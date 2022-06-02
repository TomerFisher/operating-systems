#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
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
mtx_t queue_lock;
mtx_t initialize_thread_lock;
mtx_t start_threads_lock;
mtx_t incr_total_error_threads;
mtx_t incr_total_match_search_term_lock;
cnd_t queue_not_empty;
cnd_t all_threads_initialized;
cnd_t start_threads;

// global variables
struct directory_queue *dir_queue;
char *search_term;
int total_match_search_term = 0;
int total_initialized_threads = 0;
int total_waiting_threads = 0;
int total_error_threads = 0;
int total_threads;

void add_to_queue(struct directory_queue *dir_queue, struct directory_node *dir_node) {
	if(dir_queue->head == NULL) {
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
		return NULL;
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

int searching_in_directory(char *path) {
	char full_path[PATH_MAX];
	struct dirent *current_dirent;
	struct directory_node *dir_node;
	struct stat statbuf;
	DIR *dir = opendir(path);
	if (dir == NULL) {
		fprintf(stderr, "opendir for %s failed\n", path);
		return -1;
	}
	//check all dir entries 
	while((current_dirent = readdir(dir)) != NULL) {
		sprintf(full_path, "%s/%s", path, current_dirent->d_name);
		if (lstat(full_path, &statbuf) != 0) {
			fprintf(stderr, "stat for %s failed\n", full_path);
			return -1;
		}
		if(S_ISDIR(statbuf.st_mode)) {
			if((strcmp(current_dirent->d_name, ".") != 0) && (strcmp(current_dirent->d_name, "..") != 0)) {
				//check if the dir have read and execute permissions
				if(!(statbuf.st_mode & S_IXUSR) || !(statbuf.st_mode & S_IRUSR)) {
					printf("Directory %s: Permission denied.\n", full_path);
				}
				else {
					//create new directory and add to the queue
					dir_node = malloc(sizeof(struct directory_node));
					if(dir_node == NULL) {
						fprintf(stderr, "malloc for root directory node failed\n");
						return -1;
					}
					strcpy(dir_node->path, full_path);
					dir_node->next = NULL;
					mtx_lock(&queue_lock);
					add_to_queue(dir_queue, dir_node);
					cnd_signal(&queue_not_empty);
					mtx_unlock(&queue_lock);
				}
			}
		}
		else {
			//check if the file contain the serch term
			if(strstr(current_dirent->d_name, search_term) != NULL) {
				mtx_lock(&incr_total_match_search_term_lock);
				total_match_search_term++;
				mtx_unlock(&incr_total_match_search_term_lock);
				printf("%s\n", full_path);
			}
		}
	}
	closedir(dir);
	return 0;
}

int thread_func() {
	struct directory_node *dir_node;
	mtx_lock(&initialize_thread_lock);
	total_initialized_threads++;
	if(total_initialized_threads + total_error_threads == total_threads)
		cnd_broadcast(&all_threads_initialized);
	//waiting until get 'start_threads' signal from the main thread which means all threads initialized
	cnd_wait(&start_threads, &initialize_thread_lock);
	mtx_unlock(&initialize_thread_lock);
	//if the queue not empty - remove directory from the queue
	//if the queue empty - waiting until the queue not empty
	while(1) {
		mtx_lock(&queue_lock);
		while(dir_queue->head == NULL) {
			total_waiting_threads++;
			if(total_waiting_threads + total_error_threads == total_threads) {
				//exit from all threads
				cnd_broadcast(&queue_not_empty); //for waking up all waiting threads
				mtx_unlock(&queue_lock);
				thrd_exit(0);
			}
			//waiting until the queue is not empty
			cnd_wait(&queue_not_empty, &queue_lock);
			//thread wake up
			total_waiting_threads--;
		}
		//get directory from the queue
		dir_node = remove_from_queue(dir_queue);
		mtx_unlock(&queue_lock);
		//scan the directory path
		if(searching_in_directory(dir_node->path) != 0) {
			total_error_threads++;
			thrd_exit(1);
		}
		free(dir_node);
	}
	return 0;
}

void initialize_threads(thrd_t* threads, int total_threads) {
	int i;
    for (i = 0; i < total_threads; i++) {
        if (thrd_create(&threads[i], thread_func, NULL) == thrd_error) {
            fprintf(stderr, "Failed creating thread\n");
			mtx_lock(&incr_total_error_threads);
			total_error_threads++;
			mtx_unlock(&incr_total_error_threads);
            exit(1);
        }
    }
}

void initialize_locks_and_conds() {
	mtx_init(&queue_lock, NULL);
	mtx_init(&initialize_thread_lock, NULL);
	mtx_init(&start_threads_lock, NULL);
	mtx_init(&incr_total_error_threads, NULL);
	mtx_init(&incr_total_match_search_term_lock, NULL);
	cnd_init(&queue_not_empty);
	cnd_init(&all_threads_initialized);
	cnd_init(&start_threads);
}

void destroy_locks_and_conds() {
	mtx_destroy(&queue_lock);
	mtx_destroy(&initialize_thread_lock);
	mtx_destroy(&start_threads_lock);
	mtx_destroy(&incr_total_error_threads);
	mtx_destroy(&incr_total_match_search_term_lock);
	cnd_destroy(&queue_not_empty);
	cnd_destroy(&all_threads_initialized);
	cnd_destroy(&start_threads);
}

void initialize_fifo_queue(char *root_dir_path) {
	struct directory_node* root_dir;
	dir_queue = malloc(sizeof(struct directory_queue));
	if (dir_queue == NULL) {
		fprintf(stderr, "malloc for directory queue failed\n");
		exit(1);
	}
	dir_queue->head = NULL;
	dir_queue->tail = NULL;
	root_dir = malloc(sizeof(struct directory_node));
	if (root_dir == NULL) {
		fprintf(stderr, "malloc for root directory node failed\n");
		exit(1);
	}
	strcpy(root_dir->path, root_dir_path);
	root_dir->next = NULL;
	add_to_queue(dir_queue, root_dir);
}

int main(int argc, char *argv[]) {
	int i;
	//command line arguments validation
	if(argc != 4) {
		fprintf(stderr, "invalid number of arguments\n");
		exit(1);
	}
	if(opendir(argv[1]) == NULL) {
		if (errno == EACCES)
			fprintf(stderr, "Directory %s: Permission denied.\n", argv[1]);
		else {
			fprintf(stderr, "opendir for %s failed\n", argv[1]);
		}
		exit(1);
	}
	search_term = argv[2];
    total_threads = atoi(argv[3]);
	//FIFO queue creation
	initialize_fifo_queue(argv[1]);	
	//iniitialize all locks and conditions
	initialize_locks_and_conds();	
	//initialize all threads
	mtx_lock(&initialize_thread_lock);
	thrd_t threads[total_threads];
	initialize_threads(threads, total_threads);
	//waiting for all threads to be initialized
	cnd_wait(&all_threads_initialized, &initialize_thread_lock);
	//start the threads for searcing
	cnd_broadcast(&start_threads);
	mtx_unlock(&initialize_thread_lock);	
	//waiting for all threads to finish
	for (i = 0; i < total_threads; i++)
        thrd_join(threads[i], NULL);
	//print how many files were found
    printf("Done searching, found %d files\n", total_match_search_term);
	//locks and conditions destroying
	destroy_locks_and_conds();
	//free the queue
	free(dir_queue);
	if(total_error_threads > 0)
		exit(1);
	exit(0);
}