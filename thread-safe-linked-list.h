#ifndef THREAD_SAFE_LINKED_LIST_INCLUDED
#define THREAD_SAFE_LINKED_LIST_INCLUDED

#include <stdbool.h>

typedef struct linked_list linked_list_t;

typedef bool (comparator_fn)(void *);
typedef void (free_fn)(void *);

linked_list_t *linked_list_new();
void linked_list_insert_front(linked_list_t *list, void *elem);
void *linked_list_remove(linked_list_t *list, comparator_fn comp);
void linked_list_free(linked_list_t *list, free_fn elem_free);

#endif