/* Copyright (C) 2010 Tony Garnock-Jones. All rights reserved. */

#define _XOPEN_SOURCE 600

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/time.h>

#include <ucontext.h>

#define ICHECK(result, message) do { if ((result) == -1) { perror(message); exit(2); } } while (0)
#define BCHECK(result, message) do { if (!(result)) { perror(message); exit(2); } } while (0)
#define PCHECK(result, message) do { if (!(result)) { perror(message); exit(2); } } while (0)

#ifdef __APPLE__
/* Bollocks. Looks like OS X chokes unless STACK_SIZE is a multiple of 32k. */
#define STACK_SIZE 32768
#elif linux
#define STACK_SIZE 4096
#else
#error Define STACK_SIZE for your platform.
#endif

#define N 100000
#define M 20

static void report(char const *message, struct timeval const *start, double n) {
  struct timeval now;
  double delta;

  gettimeofday(&now, NULL);
  delta = (now.tv_sec - start->tv_sec) + (now.tv_usec - start->tv_usec) / 1000000.0;
  fprintf(stderr, "%s: %g s total (%g s each = %g Hz)\n", message, delta, delta / n, n / delta);
}

typedef struct Process {
  ucontext_t context;
  struct Process *link;
  void *stack_base;
} Process;

typedef struct ProcessQueue {
  Process *head;
  Process *tail;
} ProcessQueue;

static ucontext_t scheduler;
static Process *current = NULL;
static ProcessQueue runlist = { NULL, NULL };
static ProcessQueue deadlist = { NULL, NULL };

static void enqueue(ProcessQueue *pq, Process *p) {
  p->link = NULL;
  if (pq->head == NULL) {
    pq->head = p;
  } else {
    pq->tail->link = p;
  }
  pq->tail = p;
}

static Process *dequeue(ProcessQueue *pq) {
  if (pq->head == NULL) {
    return NULL;
  } else {
    Process *p = pq->head;
    pq->head = p->link;
    if (pq->head == NULL) {
      pq->tail = NULL;
    }
    return p;
  }
}

static void yield(void) {
  if (current == NULL) {
    ICHECK(setcontext(&scheduler), "yield setcontext");
  } else {
    enqueue(&runlist, current);
    ICHECK(swapcontext(&current->context, &scheduler), "yield swapcontext");
  }
}

static unsigned long alloc_count = 0;

static void killproc(void) {
  enqueue(&deadlist, current);
  current = NULL;
  yield();
}

static void driver(void (*f)(void *), void *arg) {
  f(arg);
  killproc();
}

static void spawn(void (*f)(void *), void *arg) {
  Process *p = calloc(1, sizeof(*p));
  alloc_count += sizeof(*p);
  PCHECK(p, "spawn calloc");

  p->stack_base = malloc(STACK_SIZE); /* what is a sane value here? 32k for mac... */
  alloc_count += STACK_SIZE;
  PCHECK(p->stack_base, "stack pointer malloc");

  ICHECK(getcontext(&p->context), "spawn getcontext");
  p->context.uc_link = NULL;
  p->context.uc_stack.ss_sp = p->stack_base;
  p->context.uc_stack.ss_size = STACK_SIZE;
  p->context.uc_stack.ss_flags = 0;
  makecontext(&p->context, (void (*)(void)) driver, 2, f, arg);

  enqueue(&runlist, p);
}

static void worker(void *arg) {
  int i;
  for (i = 0; i < M; i++) {
    //fprintf(stderr, "worker %d i %d\n", (int) arg, i);
    yield();
  }
  //fprintf(stderr, "worker %d done\n", (int) arg);
}

int main(int argc, char *argv[]) {
  int i;
  struct timeval start;

  ICHECK(getcontext(&scheduler), "main getcontext");

  gettimeofday(&start, NULL);
  for (i = 0; i < N; i++) {
    spawn(worker, (void *) i);
  }
  report("spawn", &start, N);

  fprintf(stderr, "alloc total = %lu\n", alloc_count);

  gettimeofday(&start, NULL);
  while ((current = dequeue(&runlist)) != NULL) {
    //fprintf(stderr, "switching to %p\n", current);
    ICHECK(swapcontext(&scheduler, &current->context), "main swapcontext");
    //fprintf(stderr, "back from %p\n", current);
    {
      Process *deadp;
      while ((deadp = dequeue(&deadlist)) != NULL) {
	free(deadp->stack_base);
	alloc_count -= STACK_SIZE;
	free(deadp);
	alloc_count -= sizeof(*deadp);
      }
    }
  }
  report("done", &start, N * M);

  return 0;
}
