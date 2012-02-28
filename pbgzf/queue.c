#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

#include "util.h"
#include "block.h"
#include "queue.h"

queue_t*
queue_init(int32_t capacity, int8_t ordered)
{
  queue_t *q = calloc(1, sizeof(queue_t));

  q->mem = capacity;
  q->queue = calloc(q->mem, sizeof(block_t*));
  q->ordered = ordered;

  q->mut = calloc(1, sizeof(pthread_mutex_t));
  q->not_full = calloc(1, sizeof(pthread_cond_t));
  q->not_empty = calloc(1, sizeof(pthread_cond_t));
    
  if(0 != pthread_mutex_init(q->mut, NULL)) {
      fprintf(stderr, "Could not create mutex");
      exit(1);
  }
  if(0 != pthread_cond_init(q->not_full, NULL)) {
      fprintf(stderr, "Could not create condition");
      exit(1);
  }
  if(0 != pthread_cond_init(q->not_empty, NULL)) {
      fprintf(stderr, "Could not create condition");
      exit(1);
  }

  return q;
}

int8_t
queue_add(queue_t *q, block_t *b, int8_t wait)
{
  safe_mutex_lock(q->mut);
  while(q->n == q->mem) {
      if(wait && !q->eof) {
          if(0 != pthread_cond_wait(q->not_full, q->mut)) {
              fprintf(stderr, "Could not condition wait");
              exit(1);
          }
      }
      else {
          return 0;
      }
  }
  if(1 == q->ordered) {
      while(q->id - q->n + q->mem <= b->id) {
          if(wait && !q->eof) {
              if(0 != pthread_cond_wait(q->not_full, q->mut)) {
                  fprintf(stderr, "Could not condition wait");
                  exit(1);
              }
          }
          else {
              return 0;
          }
      }
      q->queue[b->id % q->mem] = b;
  }
  else {
      b->id = q->id;
      q->id++;
      q->queue[q->tail++] = b;
      if(q->tail == q->mem) q->tail = 0;
  }
  q->n++;
  pthread_cond_signal(q->not_empty);
  safe_mutex_unlock(q->mut);
  return 1;
}

block_t*
queue_get(queue_t *q, int8_t wait)
{
  block_t *b = NULL;
  safe_mutex_lock(q->mut);
  while(0 == q->n) {
      if(wait && !q->eof) {
          if(0 != pthread_cond_wait(q->not_empty, q->mut)) {
              fprintf(stderr, "Could not condition wait");
              exit(1);
          }
      }
      else {
          return NULL;
      }
  }
  b = q->queue[q->head];
  if(q->ordered) {
      while(NULL == b) {
          if(wait && !q->eof) {
              if(0 != pthread_cond_wait(q->not_empty, q->mut)) {
                  fprintf(stderr, "Could not condition wait");
                  exit(1);
              }
          }
          else {
              return NULL;
          }
          b = q->queue[q->head];
      }
  }
  q->queue[q->head] = NULL;
  q->head++;
  if(q->head == q->mem) q->head = 0;
  if(q->ordered) q->id++;
  q->n--;
  pthread_cond_signal(q->not_full);
  safe_mutex_unlock(q->mut);
  return b;
}

void
queue_close(queue_t *q)
{
  if(q->eof) return;
  safe_mutex_lock(q->mut);
  q->eof = 1;
  pthread_cond_broadcast(q->not_full);
  pthread_cond_broadcast(q->not_empty);
  safe_mutex_unlock(q->mut);
}

void
queue_destroy(queue_t *q)
{
  int32_t i;
  if(NULL == q) return;
  queue_close(q);
  for(i=0;i<q->mem;i++) {
      block_destroy(q->queue[i]);
  }
  free(q->queue);
  free(q->mut);
  free(q->not_full);
  free(q->not_empty);
  free(q);
}

