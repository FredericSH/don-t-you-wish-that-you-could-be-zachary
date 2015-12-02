#include <stdlib.h>
#include <assert.h>
#include "queues.h"

queue qqcreate(){
  queue out = (queue)malloc(sizeof(queue_t));
  assert(out);
  out->first = 0;
  out->last = 0;
  out->length = 0;
  return out;
}
void qqadd(queue q, int x, int y, int direction, int layer){
  qnode newq = (qnode)malloc(sizeof(node));
  assert(newq);
  newq->x = x;
  newq->y = y;
  newq->direction = direction;
  newq->layer = layer;
  newq->next = 0;
  
  if(q->length == 0){
    q->first = newq;
    q->last = newq;
    q->length = 1;
    return;
  }
  q->first->next = newq;
  q->first = newq;
  q->length++;
}

void qqpop(queue q){
  qnode buf = q->last->next;
  free(q->last);
  q->last = buf;
  q->length--;
  if(q->length == 0)q->first = 0;
}

qnode qqfirst(queue q){
  return q->first;
}

qnode qqlast(queue q){
  return q->last;
}

int qqlength(queue q){
  return q->length;
}

void qqdestroy(queue q){
  while(q->length)qqpop(q);
  free(q);
}
