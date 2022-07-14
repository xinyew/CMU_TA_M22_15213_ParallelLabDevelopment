// A generic thread safe linked list
#include "thread-safe-linked-list.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>

typedef struct mutex_list_elem {
    pthread_mutex_t *mutex;
    int mutex_num;
    struct mutex_list_elem *prev;
    struct mutex_list_elem *next;
} mutex_list_elem_t;

typedef struct {
    int next_mutex_num;
    mutex_list_elem_t *front;
} mutex_list_t;

typedef struct linked_list_elem {
    void *elem;
    struct linked_list_elem *prev;
    struct linked_list_elem *next;
} linked_list_elem_t;

typedef struct {
    int list_num;
    linked_list_elem_t *front;
} linked_list_t;

// Globals
static pthread_mutex_t mutex_list_mutex;
static mutex_list_t *mutex_list = NULL;

static pthread_mutex_t *mutex_list_get(int mutex_num) {
    pthread_mutex_lock(&mutex_list_mutex);
    mutex_list_elem_t *res = mutex_list->front;
    while (res != NULL) {
        if (res->mutex_num == mutex_num)
            return res->mutex;
        res = res->next;
    }
    pthread_mutex_unlock(&mutex_list_mutex);
    
    // Double free must have occured
    fprintf(stderr, "double free in thread-safe-linked-list");
    exit(1);
}

linked_list_t *linked_list_new() {
    // Add a new mutex to back of mutex list
    mutex_list_elem_t *node;
    if ((node = malloc(sizeof(mutex_list_elem_t))) == NULL) {
        perror("malloc failed in thread-safe-linked-list");
        exit(1);
    }
    node->next = NULL;
    if ((node->mutex = calloc(1, sizeof(pthread_mutex_t))) == NULL) {
        perror("malloc failed in thread-safe-linked-list");
        exit(1);
    }
    pthread_mutex_lock(&mutex_list_mutex);
    if (mutex_list == NULL) {
        if (mutex_list = malloc(sizeof(mutex_list_t)) == NULL) {
            perror("malloc failed in thread-safe-linked-list");
            exit(1);
        }
        mutex_list->front = node;
        mutex_list->next_mutex_num = 0;
    }
    else {
        mutex_list->next_mutex_num++;
        node->mutex_num = mutex_list->next_mutex_num;
        node->next = mutex_list->front;
        mutex_list->front->prev = node;
        mutex_list->front = node;
    }
    pthread_mutex_unlock(&mutex_list_mutex);

    // Actually allocate the list
    linked_list_t *list; 
    if (list = malloc(sizeof(linked_list_t)) == NULL) {
        perror("malloc failed in thread-safe-linked-list");
        exit(1);
    }
    list->list_num = mutex_list->next_mutex_num;
    list->front = NULL;
    return list;
}

void linked_list_insert_front(linked_list_t *list, void *elem) {
    linked_list_elem_t *node = malloc(sizeof(linked_list_elem_t));
    pthread_mutex_t *mutex = mutex_list_get(list->list_num);
    pthread_mutex_lock(mutex);
    node->elem = elem;
    node->next = list->front;
    list->front->prev = node;
    list->front = node;
    pthread_mutex_unlock(mutex);
}

void *linked_list_remove(linked_list_t *list, comparator_fn comp) {
    pthread_mutex_t *mutex = mutex_list_get(list->list_num);
    pthread_mutex_lock(mutex);

    // Remove from front
    if (comp == NULL) {
        linked_list_elem_t *list_elem = list->front;
        list->front = list->front->next;
        void *res = list_elem->elem;
        free(list_elem);
        pthread_mutex_unlock(mutex);
        return res;
    }
    
    // Remove an element satisfying cond
    for (linked_list_elem_t *elem = list->front; elem != NULL; elem = elem->next) {
        if (comp(elem->elem)) {
            linked_list_elem_t *prev = elem->prev;
            linked_list_elem_t *next = elem->next;
            if (prev != NULL)
                prev-> next = next;
            if (next != NULL)
                next->prev = prev;
            void *res = elem->elem;
            free(elem);
            pthread_mutex_unlock(mutex);
            return res;
        }
    }

    // No element satisfying cond found
    pthread_mutex_unlock(mutex);
    return NULL;
}

void linked_list_free(linked_list_t *list, free_fn elem_free) {
    // free linked list
    pthread_mutex_t *list_mutex = mutex_list_get(list->list_num);
    pthread_mutex_lock(list_mutex);
    linked_list_elem_t *list_elem = list->front;
    while (list_elem != NULL) {
        linked_list_elem_t *temp = list_elem;
        list_elem = list_elem->next;
        (*elem_free)(list_elem->elem);
        free(temp);
    }
    pthread_mutex_unlock(list_mutex);

    // Remove from mutex list
    pthread_mutex_lock(&mutex_list_mutex);
    mutex_list_elem_t *elem = mutex_list->front;
    int list_num = list->list_num;
    while (elem != NULL) {
        if (elem->mutex_num == list_num) {
            mutex_list_elem_t *prev = elem->prev;
            mutex_list_elem_t *next = elem->next;
            if (prev)
                prev->next = next;
            if (next)
                next->prev = prev;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_list_mutex);
}
