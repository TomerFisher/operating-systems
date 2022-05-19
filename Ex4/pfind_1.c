#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <linux/limits.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define SUCCESS 0

// FIFO queue and queue items definition plus queue utility functions

typedef struct q_directory {
    char path[PATH_MAX];
    struct q_directory *next;
} q_directory;

typedef struct directory_queue {
    struct q_directory *head;
    struct q_directory *tail;
} directory_queue;

int add_dir(directory_queue *q, q_directory *new_directory){
    if(q->head==NULL){
        // inserting to empty queue
        q->head=new_directory;
        q->tail=new_directory;
        return 1;
    }else{
        // inserting to queue's tail
        q->tail->next=new_directory;
        q->tail=new_directory;
        return 0;
    }
}

int remove_head_dir(directory_queue *q,char *path){
    q_directory *tmp;
    if(q->head==NULL){
        // should not happen because we will not try to dequeue from empty queue
        return -1;
    }else{
        // copy the directory path and removes it from queue
        strcpy(path,q->head->path);
        tmp=q->head;
        if (q->head->next==NULL){
            q->head=NULL;
            q->tail=NULL;
        }else{
            q->head=q->head->next;  
        }
        free(tmp);
        return SUCCESS;
    }
}

// global variables including locks and conds to be used by all threads
pthread_mutex_t inc_lock;
pthread_mutex_t start_lock;
pthread_cond_t threads_ready;
pthread_cond_t start;
pthread_mutex_t qlock;
pthread_cond_t notEmpty;
pthread_cond_t handoff_done;
directory_queue *q;        // should only be accessed when qlock is acquired
int waiting_threads = 0;   // should only be accessed when qlock is acquired
int err_threads = 0;       // should only be accessed when qlock is acquired
int handoff_needed = 0;    // should only be accessed when qlock is acquired
int total_matches = 0;     // should only be accessed when inc_lock is acquired
int initiated_threads = 0; // should only be accessed when start_lock is acquired
int thread_num;
char *search_term;

int search_directory(char *path){
    char new_path[PATH_MAX];
    int was_empty;
    struct dirent *entry;
    struct stat entry_stats;
    DIR *dir = opendir(path);
    if(dir == NULL){
        fprintf(stderr, "error opening directory %s: %s\n", path,strerror(errno));
        return -1;
    }
    // starting iteration on the directory
    while((entry=readdir(dir)) != NULL){
        sprintf(new_path, "%s/%s", path, entry->d_name); // create string of file path
        if (lstat(new_path, &entry_stats) != SUCCESS){
            fprintf(stderr, "error getting stats on %s: %s\n", path,strerror(errno));
            return -1;
        }
        if (S_ISDIR(entry_stats.st_mode)){
            if ((strcmp(entry->d_name, ".") == 0) || (strcmp(entry->d_name, "..") == 0)){
                // ignoring when the file is one of the directories "." or ".."
            }else if (entry_stats.st_mode & S_IXUSR && entry_stats.st_mode & S_IRUSR){
                // add the new searchable directory found to the queue's tail
                q_directory *new_directory = malloc(sizeof(q_directory));
                if (new_directory==NULL){
                    fprintf(stderr, "malloc directory for q failed: %s\n", strerror(errno));
                return -1;
                }
                strcpy(new_directory->path,new_path);
                new_directory->next=NULL;
                pthread_mutex_lock(&qlock);
                was_empty = add_dir(q,new_directory);
                if (was_empty == 1){ 
                    // handoff might be needed and threads waiting for work should wake up
                    if (waiting_threads > 0){
                        handoff_needed = 1;
                    }
                    pthread_cond_signal(&notEmpty);
                }
                pthread_mutex_unlock(&qlock);
            }else{
                // we do not have permission to read and execute the directory (can't be searched)
                printf("Directory %s: Permission denied.\n", new_path);
            }
        
        }else if (strstr(entry->d_name, search_term) != NULL){
            // file isn't a directory and the file name contains the search term
            pthread_mutex_lock(&inc_lock);
            total_matches++;
            pthread_mutex_unlock(&inc_lock);
            printf("%s\n", new_path);
        }
    }
    closedir(dir);
    return SUCCESS;
}

void *thread_search(void *i){
    char path[PATH_MAX];
    int wait_flag =0;
    // waiting for all threads to be created and for 'start' signal from main
    pthread_mutex_lock(&start_lock);
    initiated_threads++;
    if(initiated_threads == thread_num){
        pthread_cond_broadcast(&threads_ready);
    }
    pthread_cond_wait(&start, &start_lock);
    pthread_mutex_unlock(&start_lock);
    // trying to dequeue from the directory queue, waiting if 'empty' (logicly for the thread)
    while (1){
        wait_flag =0;
        pthread_mutex_lock(&qlock);
        // if handoff is needed make running threads wait until a sleeping thread wakes up and dequeues
        if (waiting_threads > 0 && handoff_needed){
            waiting_threads++;
            wait_flag =1;
            pthread_cond_wait(&handoff_done,&qlock);
        }
        while(q->head==NULL){
            // wait for queue to become non-empty, exit if all threads are idle
            if (!wait_flag){
                waiting_threads++;
                wait_flag =1;
            }
            if (waiting_threads + err_threads == thread_num){
                // all threads are idle and the directory queue is empty
                // broadcasts will wake everyone up so they will exit too
                pthread_cond_broadcast(&notEmpty);
                pthread_cond_broadcast(&handoff_done);
                pthread_mutex_unlock(&qlock);
                pthread_exit(NULL);
            }
            pthread_cond_wait(&notEmpty,&qlock);
        }
        if (wait_flag){
            waiting_threads--;
        }
        // dequeue head directory from the FIFO queue
        if (remove_head_dir(q,path)!=SUCCESS){
            // unexcpected return value, should not happen
            // updating 'err_threads' marks thread as idle so we will know when to exit search
            // as well as make main exit value 1 indicating error in a search thread
            fprintf(stderr, "dequeue Error: %s\n", strerror(errno));
            err_threads++;
            pthread_mutex_unlock(&qlock);
            pthread_exit(NULL);
        }
        // if this was a sleeping thread that dequeued wake up all threads waiting for handoff
        if (wait_flag && handoff_needed){
            handoff_needed = 0;
            pthread_cond_broadcast(&handoff_done);
        }
        pthread_mutex_unlock(&qlock);
        // iterate on the directory's files, searching for matches and other directories
        if (search_directory(path)!=SUCCESS){
            // return value from function indicating an error has occurred
            fprintf(stderr, "directory search Error: %s\n", strerror(errno));
            pthread_mutex_lock(&qlock);
            err_threads++;
            pthread_mutex_unlock(&qlock);
            pthread_exit(NULL);
        }
    }
}

int main(int argc, char** argv){
    long i;
    int rt;
    q_directory *root_directory;
    struct stat root_stats;
    // reading and validating command line arguments
    if (argc != 4) {
        fprintf(stderr, "invalid number of arguments Error: %s\n", strerror(EINVAL));
        exit(1);
    }
    search_term = argv[2];
    thread_num = atoi(argv[3]);
    if (lstat(argv[1], &root_stats) != SUCCESS){
        fprintf(stderr, "error getting stats on root - %s: %s\n", argv[1],strerror(errno));
        exit(1);
    }
    if (!(root_stats.st_mode & S_IXUSR && root_stats.st_mode & S_IRUSR)){
        printf("Directory %s: Permission denied.\n", argv[1]);
        fprintf(stderr, "error no permission on root - %s: %s\n", argv[1],strerror(EACCES));
        exit(1);
    }
    // creating the FIFO queue and adding the search root directory to it
    q = malloc(sizeof(directory_queue));
    if (q==NULL){
        fprintf(stderr, "malloc for q failed: %s\n", strerror(errno));
        exit(1);
    }
    q->head=NULL;
    q->tail=NULL;
    root_directory = malloc(sizeof(q_directory));
    if (root_directory==NULL){
        fprintf(stderr, "malloc root for q failed: %s\n", strerror(errno));
        exit(1);
    }
    strcpy(root_directory->path,argv[1]);
    root_directory->next=NULL;
    rt = add_dir(q, root_directory);
    if(rt!=1){
        fprintf(stderr, "unexpected behavior while adding root: %s\n", strerror(EBADE));
        exit(1);
    }
    // init locks and conds
    pthread_mutex_init(&inc_lock,NULL);
    pthread_mutex_init(&start_lock,NULL);
    pthread_mutex_init(&qlock,NULL);
    pthread_cond_init(&threads_ready, NULL);
    pthread_cond_init(&start, NULL);
    pthread_cond_init(&notEmpty, NULL);
    pthread_cond_init(&handoff_done, NULL);
    // create appropriate amount of threads
    pthread_mutex_lock(&start_lock);
    pthread_t thread_arr[thread_num];
    for (i = 0; i < thread_num; i++) {
        int rc = pthread_create(&thread_arr[i], NULL, thread_search,  (void *)i);
        if (rc) {
            fprintf(stderr, "Failed creating thread %ld: %s\n",i , strerror(rc));
            exit(1);
        }
    }
    // waiting for all locks to be ready to start the search and then signaling them all to start
    pthread_cond_wait(&threads_ready, &start_lock);
    pthread_cond_broadcast(&start); // broadcasting 'start' after signal 'threads_ready' ensures no lost wakeup for search threads
    pthread_mutex_unlock(&start_lock);
    // wait for all threads to finish
    for (i = 0; i < thread_num; i++) {
        pthread_join(thread_arr[i], NULL);
    }
    printf("Done searching, found %d files\n", total_matches);
    // cleaning up and exiting
    pthread_mutex_destroy(&inc_lock);
    pthread_mutex_destroy(&start_lock);
    pthread_mutex_destroy(&qlock);
    pthread_cond_destroy(&threads_ready);
    pthread_cond_destroy(&start);
    pthread_cond_destroy(&notEmpty);
    pthread_cond_destroy(&handoff_done);
    free(q);
    if (err_threads != 0){
        exit(1);
    }
    exit(SUCCESS);
}