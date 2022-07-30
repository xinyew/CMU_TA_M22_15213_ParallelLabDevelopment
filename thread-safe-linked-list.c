/* 
A generic, thread safe, linked list.
Mutexes are stord in the linked list structure, and upon access
we simply lock and then unlock before returning. The linked lists
are doubly linked and contain from and back pointers, so we can 
remove from the front, middle, and back in constant time.
*/
#include "thread-safe-linked-list.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

/** @brief The node to store each inserted element. */
typedef struct linked_list_node {
    void *elem;
    struct linked_list_node *prev;
    struct linked_list_node *next;
} linked_list_node_t;

/** @brief The linked list structure the user receives */
typedef struct linked_list {
    pthread_mutex_t mutex; // For locking before access
    linked_list_node_t *front;
    linked_list_node_t *back;
} linked_list_t;

/**
 * @brief Dynamically allocates a new linked list. Exits only on 
 * malloc error.
 * 
 * @return linked_list_t* a pointer to the allocated linked list.
 */
linked_list_t *linked_list_new() {
    linked_list_t *list; 
    if ((list = calloc(1, sizeof(linked_list_t))) == NULL) {
        perror("malloc failed in thread-safe-linked-list");
        exit(1);
    }
    pthread_mutex_init(&list->mutex, NULL);
    return list;
}

/**
 * @brief Checks to see if the linked list is empty.
 * This is true when list->front is null (implying list->back is NULL also)
 * 
 * @param list the linked list supplied from the user 
 * @return true if the linked list is empty
 * @return false otherwise
 */
bool linked_list_empty(linked_list_t *list) {
    pthread_mutex_lock(&list->mutex);
    bool res = list->front == NULL;
    pthread_mutex_unlock(&list->mutex);
    return res;
}

/**
 * @brief Inserts an element into the front of the list
 * 
 * @param list the linked list supplied from the user
 * @param elem the element to insert
 */
void linked_list_insert_front(linked_list_t *list, void *elem) {
    linked_list_node_t *node = calloc(1, sizeof(linked_list_node_t));
    node->elem = elem;
    pthread_mutex_lock(&list->mutex);
    // If the list is empty
    if (list->front == NULL) {
        list->front = node;
        list->back = node;
        pthread_mutex_unlock(&list->mutex);
        return;
    }
    // The list is not empty
    node->next = list->front;
    list->front->prev = node;
    list->front = node;
    pthread_mutex_unlock(&list->mutex);
}

/**
 * @brief Inserts an element into the back of the list
 * 
 * @param list the linked list supplied from the user
 * @param elem the element to insert
 */
void linked_list_insert_back(linked_list_t *list, void *elem) {
    linked_list_node_t *node = calloc(1, sizeof(linked_list_node_t));
    node->elem = elem;
    pthread_mutex_lock(&list->mutex);
    // If the list is empty
    if (list->front == NULL) {
        list->front = node;
        list->back = node;
        pthread_mutex_unlock(&list->mutex);
        return;
    }
    // The list is not empty
    list->back->next = node;
    node->prev = list->back;
    list->back = node;
    pthread_mutex_unlock(&list->mutex);
}

/**
 * @brief Removes an element from the front of the linked list
 * 
 * @param list the linked list supplied from the user
 * @return The element at the front of the list, if the list is
 * not empty, or NULL otherwise
 */
void *linked_list_remove_front(linked_list_t *list) {
    pthread_mutex_lock(&list->mutex);
    // If the list is empty
    if (list->front == NULL) {
        pthread_mutex_unlock(&list->mutex);
        return NULL;
    }

    // The list is not empty
    linked_list_node_t *node = list->front;
    list->front = list->front->next;
    // If the list is empty after removal
    if (list->front == NULL) {
        // Back points to the element we just removed
        list->back = NULL;
    }
    else {
        // Also points to the element we just removed
        list->front->prev = NULL;
    }
    void *res = node->elem;
    free(node);
    pthread_mutex_unlock(&list->mutex);
    return res; 
}

/**
 * @brief Removes an element from the back of the linked list
 * 
 * @param list the linked list supplied from the user
 * @return The element at the back of the list, if the list is
 * not empty, or NULL otherwise
 */
void *linked_list_remove_back(linked_list_t *list) {
    pthread_mutex_lock(&list->mutex);
    // The list is empty
    if (list->back == NULL) {
        pthread_mutex_unlock(&list->mutex);
        return NULL;
    }
        
    linked_list_node_t *node = list->back;
    list->back = list->back->prev;
    // If the list is empty after removal
    if (list->back == NULL) {
        // Points to the element we just removed
        list->front = NULL;
    } 
    else {
        // Also points to the element we just removed
        list->back->next = NULL;
    }
    void *res = node->elem;
    free(node);
    pthread_mutex_unlock(&list->mutex);
    return res; 
}

/**
 * @brief Removes the first element in the list such that comp(elem) == true
 * or the element from the front of the list if comp is NULL
 * 
 * @param list the linked list supplied from the user
 * @param comp a function that takes in a user's inserted element as argument and 
 * returns true if the user wants to remove that element, or false otherwise.
 * If comp is NULL then an element is removed from the front of the list.
 * @return The first element such that comp(elem) == true, or NULL if there is
 * no such element, or NULL if the list is empty, or, the first element in the list
 * if comp is NULL
 */
void *linked_list_remove_comp(linked_list_t *list, comparator_fn comp) {
    // If comp is NULL remove from front of list
    if (comp == NULL)
        return linked_list_remove_front(list);
    
    pthread_mutex_lock(&list->mutex);
    // If the list is empty
    if (list->front == NULL) {
        pthread_mutex_unlock(&list->mutex);
        return NULL;
    }
    // Remove from front
    if (comp(list->front->elem)) {
        linked_list_node_t *node = list->front;
        list->front = list->front->next;
        // If the list is empty after removal
        if (list->front == NULL) {
            // Back points to the element we just removed
            list->back = NULL;
        }
        else {
            // Also points to the element we just removed
            list->front->prev = NULL;
        }
        void *res = node->elem;
        free(node);
        pthread_mutex_unlock(&list->mutex);
        return res;
    }

    if (list->front->next == NULL) {
        pthread_mutex_unlock(&list->mutex);
        return NULL;
    }

    // Iterate over the interior of the list
    for (linked_list_node_t *node = list->front->next; node != list->back; node = node->next) {
        if (comp(node->elem)) {
            linked_list_node_t *prev = node->prev;
            linked_list_node_t *next = node->next;
            prev->next = next;
            next->prev = prev;
            void *res = node->elem;
            free(node);
            pthread_mutex_unlock(&list->mutex);
            return res;
        }
    }
    // Remove from back
    if (comp(list->back->elem)) {
        linked_list_node_t *node = list->back;
        list->back = list->back->prev;
        // If the list is empty after removal
        if (list->back == NULL) {
            // Points to the element we just removed
            list->front = NULL;
        } 
        else {
            // Also points to the element we just removed
            list->back->next = NULL;
        }
        void *res = node->elem;
        free(node);
        pthread_mutex_unlock(&list->mutex);
        return res;
    }   

    // No element satisfying comp found
    pthread_mutex_unlock(&list->mutex);
    return NULL;
}

/**
 * @brief Frees all dynamically allocate elements associated with the
 * linked list structure
 * 
 * @param list the linked list supplied from the user
 * @param elem_free a function that takes in an element the user 
 * inserted into the list as argument and frees all dynamically allocated data
 * associated with it, or NULL if the inserted elements are not dynamically allocated
 */
void linked_list_free(linked_list_t *list, free_fn elem_free) {
    // free linked list
    pthread_mutex_lock(&list->mutex);
    linked_list_node_t *node = list->front;
    while (node != NULL) {
        linked_list_node_t *temp = node;
        node = node->next;
        if (elem_free != NULL)
            elem_free(node->elem);
        free(temp);
    }
    pthread_mutex_unlock(&list->mutex);
    pthread_mutex_destroy(&list->mutex);
    free(list);
}
