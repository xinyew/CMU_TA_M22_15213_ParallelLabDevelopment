/**
Simple implementation of the unix command line tool grep, but it's 
multithreaded! 
 */

#include <stdio.h>
#include <stdlib.h>
#include <ftw.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <regex.h>
#include <pthread.h>
#include <errno.h>
#include <semaphore.h>
#include "thread-safe-linked-list.h"
#include <getopt.h>

#define MAX_FILE_NUM 4096
#define BUF_SIZE 4096
#define WORK_THREAD_NUM 8
#define FILE_THREAD_NUM 8
#define threshold 2
#define KB 1024
#define MB (1024*1024)

// typedef struct file_grep_task {
//     char *filename;
//     long start;
//     long end;
// } file_grep_task_t;

// typedef struct file_match {
//     char **matches;
//     int order;
// } file_match_t;

typedef struct {
   char *file_name;
   int task_num;
} task_t;

typedef struct {
    linked_list_t *output;
    int output_num;
} output_t;


// GLOBALS
bool recursive = false;
bool print_line_numbers = false;
char *pattern = NULL;
const char *usage = "Usage: ./pgrep [-rh] [pattern] [file] \n"
                    "-h     Show help message\n"
                    "-r     Recursively search through directory structure\n"
                    "-n     Include line numbers\n";
pthread_t thread_pool[WORK_THREAD_NUM];
linked_list_t *task_list;
linked_list_t *output_list;

int task_num = 0;

int next_output = 0;
pthread_mutex_t next_output_mut;
bool files_added_to_task_list = false;
pthread_mutex_t readers_finished_mut;
int readers_finished = 0;
pthread_mutex_t reading_mut;
sem_t reading_sem;
bool reading = false;

/**
* @brief inner function for printing out nodes in the linked list 
*/
void print_line(void *buf) {
    printf("%s", (char *)buf);
}

/**
* @brief traverse and print the output list
*/
void output_list_print(void *output) {
    linked_list_print(((output_t *)output)->output, print_line);
}

/**
* @brief parse the arguments to get flags, searching pattern and files for searching
*/
char *parse_args(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, "rhn")) != -1) {
        switch (opt) {
            case 'r':
                recursive = true;
                break;

            case 'h':
                printf("%s", usage);
                break;

            case 'n':
                print_line_numbers = true;
                break;

            case '?':
                printf("Error parsing command line arguments\n%s", usage);
                exit(1);
        }
    }

    // If there are not two arguments left (pattern and file name)
    if (argc - optind != 2) {
        fprintf(stderr, "Missing either pattern or file name in parsing command line arguments\n%s", usage);
        exit(1);
    }

    pattern = argv[optind];
    return argv[optind + 1];
}

linked_list_t *grep_file(const char *file_name) {
    FILE *file;
    char *buf = NULL;
    size_t buf_size = 10;
    size_t read;
    int regex_errorno; // value returned from regex library functions, zero indicates success
    regex_t regex; // tmp variable to conduct regex operations

    linked_list_t *output = linked_list_new();

    // compile the searching pattern to a regex object
    if (regcomp(&regex, pattern, 0)) {
        fprintf(stderr, "Regex compile failed\n");
        exit(1);
    }
    if ((file = fopen(file_name, "r")) == NULL) {
        printf("%s\n", file_name);
        perror("Error Opening File");
        exit(1);
    }

    int line_number = 1;
    while ((read = getline(&buf, &buf_size, file)) != -1) {
        regex_errorno = regexec(&regex, buf, 0, NULL, 0);
        if (regex_errorno == REG_NOMATCH) {
            continue;
        }
        /* num bytes read + size of file name + 3 bytes for colons, line
        number + 1 byte for \n + 1 byte for nul termiantor */
        char *line;
        if ((line = calloc(1, read + strlen(file_name) + 3 + 1 + 1)) == NULL) {
            perror("malloc failed in pgrep: grep_file 2");
            free(buf);
            exit(1);
        }

        if (recursive) {
            if (print_line_numbers) 
                sprintf(line, "%s:%d:%s", file_name, line_number, buf);
            else
                sprintf(line, "%s:%s", file_name, buf);
        } else {
            if (print_line_numbers)
                sprintf(line, "%d:%s", line_number, buf);
            else
                sprintf(line, "%s", buf);
        }
        linked_list_insert_back(output, line);
        line_number++;
    }
    fclose(file);
    free(buf);
    return output;
}

int add_to_task_list(const char *filename, const struct stat *statptr,
    int fileflags) {
    
    if (fileflags == FTW_F) {
        task_t *task;
        if ((task = malloc(sizeof(task_t))) == NULL) {
            perror("malloc failed in pgrep: add_to_task_list");
            exit(1);
        }
        task->task_num = task_num++;
        // printf("set num: %d\n", task->task_num);
        task->file_name = strdup(filename);
        linked_list_insert_front(task_list, task);
    }
    return 0; // Tells ftw to continue
}

// function to grep a file bigger than 2MB
// void grepFileParallel(const char *filename) {
//     struct stat info;
//     if (lstat(filename, &info) == -1) {
//         printf("Error: could not open the specified file: %s\n", filename);
//         exit(1);
//     }

//     if (info.st_size < threshold * MB) {
//         printf("The file is less than 2MB\n");
//     }

//     FILE *fp = NULL;
//     int i = 0;
//     int posAddress = 0;
//     long blockSize = info.st_size / FILE_THREAD_NUM;
// }

bool is_next_output(output_t *output) {
    pthread_mutex_lock(&next_output_mut);
    bool flag = output->output_num == next_output;
    pthread_mutex_unlock(&next_output_mut);
    return flag;
}

void print_output() {
    /* Can't test for task list being empty because of state
    where all tasks have been removed, but a thread is still parsing
    a file and hasn't yet written it to output list. files_added_to_output_list
    is only set to true after writing to output list */
    while (true) {
        if((linked_list_empty(output_list) && (readers_finished == WORK_THREAD_NUM))) {
            pthread_mutex_unlock(&readers_finished_mut);
            reading = false;
            break;
        }
        pthread_mutex_unlock(&readers_finished_mut);
        output_t *output = linked_list_remove_comp(output_list, (comparator_fn *)is_next_output);
        if (output == NULL) {
            pthread_mutex_lock(&reading_mut);
            reading = false;
            pthread_mutex_unlock(&reading_mut);
            sem_wait(&reading_sem);
            continue;
        }
        linked_list_t *list = output->output;
        while (!linked_list_empty(list)) {
            char *line = linked_list_remove_front(list);
            printf("%s", line);
            free(line);
        }
        linked_list_free(list, NULL);

        // printf("loop: %d\n", next_output);
        next_output++;
    }
    linked_list_free(output_list, NULL);
    linked_list_free(task_list, NULL);
}

void *file_reader(void *arg) {
    if (pthread_detach(pthread_self()) != 0) {
        perror("thread detach error");
        exit(1);
    }
    while (true) {
        if (linked_list_empty(task_list) && files_added_to_task_list) {
            pthread_mutex_lock(&readers_finished_mut);
            readers_finished++;
            pthread_mutex_unlock(&readers_finished_mut);
            pthread_exit(NULL); 
        }
        task_t *task = linked_list_remove_front(task_list);
        if (task == NULL)
            continue;
        linked_list_t *res = grep_file(task->file_name);
        output_t *output;
        if ((output = malloc(sizeof(output_t))) == NULL) {
            perror("malloc failed in pgrep: file_reader");
            exit(1); 
        }
        output->output = res;
        output->output_num = task->task_num;
        // printf("task num: %d\n", task->task_num);
        linked_list_insert_front(output_list, output);
        pthread_mutex_lock(&reading_mut);
        if (!reading) {
            reading = true;
            sem_post(&reading_sem);
        }
        pthread_mutex_unlock(&reading_mut);
        free(task->file_name);
        free(task);
    }
}

void init_thread_pool() {
    for (int i = 0; i < WORK_THREAD_NUM; i++) {
        if ((pthread_create(&thread_pool[i], NULL, file_reader, NULL))) {
            perror("pthread_create error");
            exit(1);
        }
    }
}

void grep_dir(char *path) {
    // initialize linked lists for producing output
    task_list = linked_list_new();
    output_list = linked_list_new();
    // Iterates over the directory structure starting at path and
    // calls grep_file_wrapper on each file
    init_thread_pool();
    ftw(path, add_to_task_list, MAX_FILE_NUM);
    files_added_to_task_list = true;
    print_output();
}

int main(int argc, char *argv[]) {
    struct stat sb;
    pthread_mutex_init(&readers_finished_mut, NULL);
    pthread_mutex_init(&next_output_mut, NULL);
    pthread_mutex_init(&reading_mut, NULL);
    sem_init(&reading_sem, 0, 0);

    char *file_name = parse_args(argc, argv);

    if (stat(file_name, &sb) == -1) {
        perror("stat");
        exit(1);
    }

    if (recursive && S_ISREG(sb.st_mode)) {
        fprintf(stderr, "%s is not a directory\n%s", file_name, usage);
        exit(1);
    }

    if (!recursive && S_ISDIR(sb.st_mode)) {
        fprintf(stderr, "%s is not a file\n%s", file_name, usage);
        exit(1);
    }

    if (!recursive)
        grep_file(file_name);
    else
        grep_dir(file_name);

    sem_destroy(&reading_sem);
}