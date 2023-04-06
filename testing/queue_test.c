//
// Created by chiro on 23-4-6.
//

#include <stdio.h>
#include "queue.h"

int main(int argc, char **argv) {
  int data[] = {1, 2, 3};
  queue_t *q = queue_new(data);
  queue_push(q, data + 2);
  queue_push(q, data + 1);
  int *d;
  while (true) {
    d = (int *) queue_pop(q);
    if (!d) break;
    printf("pop: %d\n", *d);
  }
  queue_free(q, false);
  return 0;
}