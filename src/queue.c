//
// Created by chiro on 23-4-6.
//

#include <string.h>
#include <malloc.h>
#include "queue.h"
#include "debug_macros.h"

/**
 * Create a queue with one item
 * @param item item pointer
 * @return queue
 */
queue_t *queue_new(void *item) {
  queue_t *q = malloc(sizeof(queue_t));
  memset(q, 0, sizeof(queue_t));
  queue_node_t *node = malloc(sizeof(queue_node_t));
  memset(node, 0, sizeof(queue_node_t));
  node->item = item;
  q->head = node;
  q->tail = node;
  return q;
}

/**
 * Free data in queue, but not this queue
 * @param q
 * @param free_items
 */
void queue_free_data(queue_t *q, bool free_items) {
  Assert(q, "queue null!");
  if (free_items) {
    queue_node_t *node = q->head;
    while (node) {
      if (node->item) free(node->item);
      node = node->next;
    }
  }
  queue_node_t *node = q->head;
  while (node) {
    queue_node_t *next = node->next;
    free(node);
    node = next;
  }
}

/**
 * Free a queue
 * @param q queue
 * @param free_items whether to free items
 */
void queue_free(queue_t *q, bool free_items) {
  Assert(q, "queue null!");
  queue_free_data(q, free_items);
  free(q);
}

/**
 * Push item to queue tail
 * @param q queue
 * @param item item
 */
void queue_push(queue_t *q, void *item) {
  Assert(q, "queue null!");
  queue_node_t *node = malloc(sizeof(queue_node_t));
  memset(node, 0, sizeof(queue_node_t));
  node->item = item;
  q->tail->next = node;
  q->tail = node;
}

/**
 * Pop item from queue head
 * @param q queue
 * @return item, null if empty queue
 */
void *queue_pop(queue_t *q) {
  Assert(q, "queue null!");
  if (!q->head) return NULL;
  queue_node_t *node = q->head;
  q->head = q->head->next;
  void *item = node->item;
  free(node);
  if (!q->head) q->tail = NULL;
  return item;
}

/**
 * Copy-constructor for queue. Data in the queue will not copy!
 * @param dst dest
 * @param src source
 * @param len ignored
 */
void queue_copy(void *dst, const void *src, size_t len) {
  memcpy(dst, src, sizeof(queue_t));
}