// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <linux/limits.h>

// FIFO Queu - code taken from https://lispmachine.wordpress.com/2009/05/13/queue-in-c/ and altered a bit for the homework's needs
struct my_struct
{
    char *dir;
    struct my_struct *next;
};

struct my_list
{
    struct my_struct *head;
    struct my_struct *tail;
};

// Return true if an error occured
bool list_add_element(struct my_list *s, char *dir)
{
    struct my_struct *p = malloc(1 * sizeof(*p));

    if (NULL == p)
    {
        fprintf(stderr, "Memory allocation failed for element addition to the tree\n");
        return true;
    }

    p->dir = dir;
    p->next = NULL;

    if (NULL == s)
    {
        fprintf(stderr, "Queue not initialized - developer mistake\n");
        free(p);
        return true;
    }
    else if (NULL == s->head && NULL == s->tail)
    {
        s->head = s->tail = p;
        return false;
    }
    else if (NULL == s->head || NULL == s->tail)
    {
        fprintf(stderr, "There is something seriously wrong with your assignment of head/tail to the list\n");
        free(p);
        return true;
    }
    else
    {
        s->tail->next = p;
        s->tail = p;
    }

    return false;
}

/* This is a queue and it is FIFO, so we will always remove the first element */
char *list_remove_element(struct my_list *s)
{
    struct my_struct *h = NULL;
    struct my_struct *p = NULL;

    if (NULL == s)
    {
        // printf("List is empty\n");
        return NULL;
    }
    else if (NULL == s->head && NULL == s->tail)
    {
        // printf("Well, List is empty\n");
        return NULL;
    }
    else if (NULL == s->head || NULL == s->tail)
    {
        // printf("There is something seriously wrong with your list\n");
        // printf("One of the head/tail is empty while other is not \n");
        return NULL;
    }

    h = s->head;
    p = h->next;
    char *dir = h->dir;
    free(h);
    s->head = p;
    if (NULL == s->head)
        s->tail = s->head; /* The element tail was pointing to is free(), so we need an update */

    return dir;
}

/* ---------------------- small helper fucntions ---------------------------------- */
void list_free(struct my_list *s)
{
    while (s->head)
    {
        free(s->head->dir);
        list_remove_element(s);
    }

    free(s);
}

struct my_list *list_new(void)
{
    struct my_list *p = malloc(1 * sizeof(*p));

    if (NULL == p)
    {
        fprintf(stderr, "Malloc failed while creating a new list\n");
        exit(1);
    }

    p->head = p->tail = NULL;

    return p;
}
// End of copied fifo code

bool list_is_empty(const struct my_list *s)
{
    if (NULL == s)
    {
        return true;
    }
    else if (NULL == s->head && NULL == s->tail)
    {
        return true;
    }
    return false;
}

// Definitions of locks and condition variables
// Queue lock
pthread_cond_t data_was_added_to_queue;
pthread_mutex_t queue_lock;

// Is running lock
pthread_mutex_t is_running_lock;
int is_running = 1;

// Amount of files found
pthread_mutex_t amount_found_lock;
int amount_found = 0;

// Amount of thread errors
pthread_mutex_t amount_thread_errors_lock;
int amount_thread_errors = 0;

// Amount of threads sleeping counter, lock and condition-variable
pthread_mutex_t sleeping_lock;
pthread_cond_t sleeping_amount_increased;
int amount_sleeping = 0;
// End of locks/CVs definitions

// Some more shared varialbes
bool is_sigint = false;
char *compare_to;
int threads_amount;
pthread_t *tid;

// Signal handler
void sigint_handler(int sig)
{
    is_sigint = true;
    pthread_mutex_lock(&is_running_lock);
    is_running = 0;
    pthread_mutex_unlock(&is_running_lock);
}

struct my_list *queue = NULL;

// Iterating directories code
int handle_directory(char *path, char *dir_name)
{
    if ((strcmp(dir_name, ".") == 0) || (strcmp(dir_name, "..") == 0))
        return 0;
    char *dir = malloc(sizeof(char) * PATH_MAX);
    if (!dir) {
        fprintf(stderr, "Error in thread, allocating path memory failed\n");
        return 1;
    }
    strcpy(dir, path);

    pthread_mutex_lock(&queue_lock);
    int ret_val = list_add_element(queue, dir);
    pthread_mutex_unlock(&queue_lock);

    pthread_cond_broadcast(&data_was_added_to_queue);
    return ret_val;
}

int handle_file(char *path, char *file_name)
{
    if ((strcmp(file_name, ".") == 0) || (strcmp(file_name, "..") == 0))
        return 0;

    if (strstr(file_name, compare_to) != NULL)
    {
        pthread_mutex_lock(&amount_found_lock);
        amount_found++;
        pthread_mutex_unlock(&amount_found_lock);

        printf("%s\n", path);
    }
    return 0;
}

int iterate_directory(const char *dir)
{
    struct dirent *iteratee;
    DIR *iterated_dir;
    char path[PATH_MAX];
    int ret_val = 0;

    if ((iterated_dir = opendir(dir)) == NULL)
    {
        fprintf(stderr, "Can't open %s\n", dir);
        return 1;
    }

    while ((iteratee = readdir(iterated_dir)) != NULL)
    {
        struct stat stbuf;
        if (dir[strlen(dir) - 1] == '/')
            sprintf(path, "%s%s", dir, iteratee->d_name);
        else
            sprintf(path, "%s/%s", dir, iteratee->d_name);

        if (lstat(path, &stbuf) == -1)
        {
            fprintf(stderr, "Unable to stat file: %s\n", path);
            ret_val = 1;
        }

        if (S_ISDIR(stbuf.st_mode))
        {
            if (handle_directory(path, iteratee->d_name) != 0)
                ret_val = 1;
        }
        else
        {
            if (handle_file(path, iteratee->d_name) != 0)
                ret_val = 1;
        }
    }

    closedir(iterated_dir);

    return ret_val;
}
// End of iterating directories code

bool check_if_running()
{
    bool ret_val;
    pthread_mutex_lock(&is_running_lock);
    ret_val = is_running;
    pthread_mutex_unlock(&is_running_lock);
    return ret_val;
}

// Search threads functions
void thread_exit_error()
{
    pthread_mutex_lock(&amount_thread_errors_lock);
    amount_thread_errors++;
    pthread_mutex_unlock(&amount_thread_errors_lock);

    // Wake main thread up
    pthread_cond_signal(&sleeping_amount_increased);
    pthread_exit(NULL);
}

void thread_wait_for_data()
{
    pthread_mutex_lock(&sleeping_lock);
    amount_sleeping++;
    pthread_cond_broadcast(&sleeping_amount_increased);
    sleep(0);
    pthread_mutex_unlock(&sleeping_lock);

    pthread_mutex_lock(&queue_lock);
    pthread_cond_wait(&data_was_added_to_queue, &queue_lock);

    pthread_mutex_lock(&sleeping_lock);
    amount_sleeping--;
    pthread_mutex_unlock(&sleeping_lock);

    pthread_mutex_unlock(&queue_lock);
}

void *thread_main()
{
    while (true)
    {
        if (!check_if_running())
        {
            pthread_cond_signal(&sleeping_amount_increased);
            pthread_exit(NULL);
        }

        pthread_mutex_lock(&queue_lock);
        if (!list_is_empty(queue))
        {
            char *dir = list_remove_element(queue);
            pthread_mutex_unlock(&queue_lock);

            if (iterate_directory(dir))
            {
                free(dir);
                thread_exit_error();
            }
            free(dir);
        }
        else
        {
            pthread_mutex_unlock(&queue_lock);
            thread_wait_for_data();
        }

        sleep(0);
    }

    return NULL;
}

// Main thread funcitons
void init_locks_and_cvs()
{
    if (
        (pthread_mutex_init(&queue_lock, NULL) != 0) ||
        (pthread_mutex_init(&is_running_lock, NULL) != 0) ||
        (pthread_mutex_init(&sleeping_lock, NULL) != 0) ||
        (pthread_mutex_init(&amount_thread_errors_lock, NULL) != 0) ||
        (pthread_mutex_init(&amount_found_lock, NULL) != 0))
        exit(1);

    if (
        (pthread_cond_init(&data_was_added_to_queue, NULL) != 0) ||
        (pthread_cond_init(&sleeping_amount_increased, NULL) != 0))
        exit(1);
}

void destroy_locks_and_cvs()
{
    pthread_mutex_destroy(&queue_lock);
    pthread_mutex_destroy(&is_running_lock);
    pthread_mutex_destroy(&sleeping_lock);
    pthread_mutex_destroy(&amount_thread_errors_lock);
    pthread_mutex_destroy(&amount_found_lock);

    pthread_cond_destroy(&data_was_added_to_queue);
    pthread_cond_destroy(&sleeping_amount_increased);
}

void init_sigint_handler()
{
    struct sigaction sa;
    sa.sa_handler = &sigint_handler;
    sigaction(SIGINT, &sa, NULL);
}

// Returns true if error happened and need to exit
bool use_command_line_arguments(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s <search root> <search term> <threads amount>\n", argv[0]);
        return true;
    }

    // Set search root
    char *elm;
    queue = list_new();
    elm = malloc(sizeof(char) * PATH_MAX);
    strcpy(elm, argv[1]);
    list_add_element(queue, elm);

    // Set term
    compare_to = argv[2];

    // Set amount of threads
    threads_amount = atoi(argv[3]);

    return false;
}

void start_threads()
{
    int error;
    for (int i = 0; i < threads_amount; i++)
    {
        error = pthread_create(&(tid[i]), NULL, &thread_main, NULL);
        if (error != 0)
        {
            fprintf(stderr, "\nThread can't be created: [%s]\n",
                    strerror(error));

            pthread_mutex_lock(&amount_thread_errors_lock);
            amount_thread_errors++;
            pthread_mutex_unlock(&amount_thread_errors_lock);
        }
    }
}

bool is_all_threads_error_dead()
{
    bool ret_val;
    pthread_mutex_lock(&amount_thread_errors_lock);
    ret_val = amount_thread_errors == threads_amount;
    pthread_mutex_unlock(&amount_thread_errors_lock);
    return ret_val;
}

void main_wait_for_threads()
{
    while (true)
    {
        if ((!check_if_running()) || is_all_threads_error_dead())
            break;

        pthread_mutex_lock(&sleeping_lock);
        // Check if all threads are sleeping (only happens when finished)
        if (amount_sleeping == threads_amount)
        {
            pthread_mutex_lock(&is_running_lock);
            is_running = 0;
            pthread_mutex_unlock(&is_running_lock);

            pthread_mutex_unlock(&sleeping_lock);
            break;
        }
        // Wait until a thread goes to sleep
        pthread_cond_wait(&sleeping_amount_increased, &sleeping_lock);
        pthread_mutex_unlock(&sleeping_lock);

        sleep(0);
    }
}

int main(int argc, char *argv[])
{
    if (use_command_line_arguments(argc, argv))
        exit(1);

    init_sigint_handler();
    init_locks_and_cvs();

    // Allocate thread-ids array
    tid = malloc(sizeof(pthread_t) * threads_amount);
    if (!tid)
        exit(1);

    start_threads();
    main_wait_for_threads();

    // Send cancelations to exit nicely
    for (int i = 0; i < threads_amount; i++)
        pthread_cancel(tid[i]);

    // Print message as defined in the task
    char *format;
    if (is_sigint)
        format = "Search stopped, found %d files\n";
    else
        format = "Done searching, found %d files\n";
    pthread_mutex_lock(&amount_found_lock);
    printf(format, amount_found);
    pthread_mutex_unlock(&amount_found_lock);

    // Get return value as defined in the task
    int ret_val = 0;
    pthread_mutex_lock(&amount_thread_errors_lock);
    if (amount_thread_errors == threads_amount)
        ret_val = 1;
    pthread_mutex_unlock(&amount_thread_errors_lock);

    // Free stuff
    destroy_locks_and_cvs();
    list_free(queue);
    free(tid);

    return ret_val;
}
