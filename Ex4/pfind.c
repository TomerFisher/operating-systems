#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <thread.h>
#include <linux/limits.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

struct directory_node {
	char path[PATH_MAX];
	struct directory_node *next;
};

struct directory_queue {
	struct directory_node *head;
	struct directory_node *tail;
};

// global locks and conds
pthread_mutex_t queue_lock;
pthread_mutex_t initialize_thread_lock;
pthread_mutex_t start_threads_lock;
pthread_mutex_t increase_total_match_search_term_lock;
pthread_mutex_t increase_total_waiting_threads_lock;
pthread_cond_t all_threads_initialized;
pthread_cond_t start_threads;
pthread_cond_t not_empty;

// global variables
struct directory_queue *dir_queue;
char *search_term;
int total_match_search_term = 0;
int total_initialized_threads = 0;
int total_waiting_threads = 0;
int total_error_threads = 0;
int total_threads;

void add_to_queue(struct directory_queue *dir_queue, struct directory_node *dir_node) {
	if(dir_queue->head == NULL && dir_queue->tail == NULL) {
		dir_queue->head = dir_node;
		dir_queue->tail = dir_node;
	}
	else {
		dir_queue->tail->next = dir_node;
		dir_queue->tail = dir_node;
	}
}

struct directory_node* remove_from_queue(struct directory_queue *dir_queue) {
	struct directory_node* dir_node;
	if(dir_queue->head == NULL)
		exit(1);
	dir_node = dir_queue->head;
	if(dir_queue->head == dir_queue->tail) {
		dir_queue->head = NULL;
		dir_queue->tail = NULL;
	}
	else {
		dir_queue->head = dir_queue->head->next;
	}
	return dir_node;
}

void enqueue(struct directories_queue *dir_queue, struct directory_node *dir_node) {
	pthread_mutex_lock(&queue_lock);
	add_to_queue(dir_queue, dir_node);
	pthread_cond_signal(&not_empty);
	pthread_mutex_unlock(&queue_lock);
}

void searching_in_directory(char *path) {
	char *full_path;
	struct dirent *current_dirent;
	struct directory_node *dir_node;
	struct stat statbuf;
	DIR *dir = opendir(path);
	if (dir == NULL) {
		printf(stderr, "Directory %s: Permission denied.\n", path);
		return;
	}
	current_dirent = readdir(dir);
	while(current_dirent != NULL) {
		sprintf(full_path, "%s/%s", path, current_dirent->d_name);
		//get stat of the directory
		if (stat(path, &statbuf) != 0) {
			printf(stderr, "stat for %s failed\n", full_path);
			exit(1);
		}
		if(S_ISDIR(statbuf.st_mode)) {
			if(!strcmp(current_dirent->d_name, ".") && !strcmp(current_dirent->d_name, "..")) {
				if(opendir(full_path) == NULL) {
					printf(stderr, "Directory %s: Permission denied.\n", path);
					exit(1);
				}
				dir_node = malloc(sizeof(struct directory_node));
				if (dir_node == NULL) {
					printf(stderr, "malloc for root directory node failed\n");
					exit(1);
				}
				strcpy(dir_node->path, full_path);
				dir_node->next = NULL;
				enqueue(dir_queue, dir_node);
			}
		}
		else if(strstr(current_dirent->d_name, search_term)) {
			pthread_mutex_lock(&increase_total_match_search_term_lock);
			total_match_search_term++;
			pthread_mutex_lock(&increase_total_match_search_term_lock);
			printf("%s\n", full_path);
		}
	}
}

void thread_func() {
	struct directory_node *dir_node;
	pthread_mutex_lock(&initialize_thread_lock);
	total_initialized_threads++;
	printf("total_initialized_threads: %d\n", total_initialized_threads);
	if(total_initialized_threads == total_threads) {
		pthread_cond_broadcast(&all_threads_initialized);
	}
	pthread_cond_wait(&start_threads, &initialize_thread_lock);
	pthread_mutex_unlock(&initialize_thread_lock);
	printf("thread start working\n");
	while(1) {
		
		pthread_mutex_lock(&queue_lock);
		if(dir_queue->head != NULL) {
			printf("thread got dir from the queue\n");
			dir_node = remove_from_queue(dir_queue);
			pthread_mutex_unlock(&queue_lock);
			searching_in_directory(dir_node->path);
			free(dir_node);
		}
		else {
			pthread_mutex_lock(&increase_total_waiting_threads_lock);
			total_waiting_threads++;
			printf("thread in waiting status");
			pthread_mutex_unlock(&increase_total_waiting_threads_lock);
			if(total_waiting_threads == total_threads) {
				// exit all threads.
			}
			else {
				pthread_cond_wait(&not_empty, &queue_lock);
			}
		}
	}
}

int initialize_threads(pthread_t* threads, int total_threads) {
	int i, rc;
    for (i = 0; i < total_threads; i++) {
		printf("create thread %d\n", i);
        rc = thrd_create(&threads[i], thread_func, (void *)i);
        if (rc) {
            fprintf(stderr, "Failed creating thread\n");
            exit(1);
        }
    }
}

void initialize_locks_and_conds() {
	pthread_mutex_init(&queue_lock, NULL);
	pthread_mutex_init(&initialize_thread_lock, NULL);
	pthread_mutex_init(&start_threads_lock, NULL);
	pthread_mutex_init(&increase_total_match_search_term_lock, NULL);
	pthread_mutex_init(&increase_total_waiting_threads_lock, NULL);
	pthread_cond_init(&all_threads_initialized, NULL);
	pthread_cond_init(&start_threads, NULL);
	pthread_cond_init(&not_empty, NULL);
}

void destroy_locks_and_conds() {
	pthread_mutex_destroy(&queue_lock);
	pthread_mutex_destroy(&initialize_thread_lock);
	pthread_mutex_destroy(&start_threads_lock);
	pthread_mutex_destroy(&increase_total_match_search_term_lock);
	pthread_mutex_destroy(&increase_total_waiting_threads_lock);
	pthread_cond_destroy(&all_threads_initialized);
	pthread_cond_destroy(&start_threads);
	pthread_cond_destroy(&not_empty);
}

void initialize_fifo_queue(char *root_dir_path) {
	struct directory_node* root_dir;
	dir_queue = malloc(sizeof(struct directory_queue));
	if (dir_queue == NULL) {
		printf(stderr, "malloc for directory queue failed\n");
		exit(1);
	}
	dir_queue->head = NULL;
	dir_queue->tail = NULL;
	root_dir = malloc(sizeof(struct directory_node));
	if (root_dir == NULL) {
		printf(stderr, "malloc for root directory node failed\n");
		exit(1);
	}
	strcpy(root_dir->path, root_dir_path);
	root_dir->next = NULL;
	add_to_queue(dir_queue, root_dir);
}

int main(int argc, char *argv[]) {
	char* root_dir_path;
	int i;
	//command line arguments validation
	if(argc != 4) {
		printf(stderr, "invalid number of arguments\n");
		exit(1);
	}
	root_dir_path = argv[1];
	if(opendir(root_dir_path) == NULL) {
		printf(stderr, "search root directory cannot be searched\n");
		exit(1);
	}
	search_term = argv[2];
    total_threads = atoi(argv[3]);
	printf("finish command line arguments validation\n");
	//FIFO queue creation
	initialize_fifo_queue(root_dir_path);
	printf("finish init fifo queue\n");
	//locks and conditions creation
	initialize_locks_and_conds();
	printf("finish init locks and conds\n");
	//initialize all threads
	pthread_mutex_lock(&initialize_thread_lock);
	pthread_t threads[total_threads];
	initialize_threads(threads, total_threads);
	pthread_cond_wait(&all_threads_initialized, &initialize_thread_lock);
	pthread_mutex_unlock(&initialize_thread_lock);
	printf("finish init threads\n");
	//waiting for all threads to be ready and then start the searching
	//waiting for all threads to finish
	for (i = 0; i < total_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("Done searching, found %d files\n", total_match_search_term);
	//locks and conditions destroying
	destroy_locks_and_conds();
	//free the queue
	free(dir_queue);
	if(total_error_threads > 0)
		exit(1);
	exit(0);
}