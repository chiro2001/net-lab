//
// Created by chiro on 23-4-6.
//

#ifndef NET_QUEUE_H
#define NET_QUEUE_H

#include <stdbool.h>

struct queue_node {
  void *item;
  // pointer to next node
  struct queue_node *next;
};

typedef struct queue_node queue_node_t;

typedef struct queue {
  queue_node_t *head;
  queue_node_t *tail;
} queue_t;

queue_t *queue_new(void *item);
void queue_free(queue_t *q, bool free_items);
void queue_free_data(queue_t *q, bool free_items);
void queue_push(queue_t *q, void *item);
void *queue_pop(queue_t *q);
void queue_copy(void *dst, const void *src, size_t len);

#endif //NET_QUEUE_H
