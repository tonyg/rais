/* Copyright (C) 2010 Tony Garnock-Jones. All rights reserved. */
/* Not very fast, as it happens. < 20kHz. */
/* Compare with ucontext_t (uc.c), > 150 kHz on osx, > 500 kHz on linux in a VM on osx. */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/time.h>

#include <pth.h>

#define ICHECK(result, message) do { if ((result) == -1) { perror(message); exit(2); } } while (0)
#define BCHECK(result, message) do { if (!(result)) { perror(message); exit(2); } } while (0)
#define PCHECK(result, message) do { if (!(result)) { perror(message); exit(2); } } while (0)

#define N 1000
#define M 100

static pth_barrier_t bar = PTH_BARRIER_INIT(N+1);

static void report(char const *message, struct timeval const *start, double n) {
  struct timeval now;
  double delta;

  gettimeofday(&now, NULL);
  delta = (now.tv_sec - start->tv_sec) + (now.tv_usec - start->tv_usec) / 1000000.0;
  printf("%s: %g s total (%g s each = %g Hz)\n", message, delta, delta / n, n / delta);
}

static void *worker(void *arg) {
  int i;
  for (i = 0; i < M; i++) {
    pth_yield(NULL);
  }
  pth_barrier_reach(&bar);
  return NULL;
}

int main(int argc, char *argv[]) {
  int i;
  struct timeval start;

  BCHECK(pth_init(), "pth_init");

  gettimeofday(&start, NULL);
  for (i = 0; i < N; i++) {
    pth_spawn(PTH_ATTR_DEFAULT, worker, NULL);
  }
  report("spawn", &start, N);

  gettimeofday(&start, NULL);
  pth_barrier_reach(&bar);
  report("done", &start, N * M);

  pth_exit(NULL);
  return 0;
}
