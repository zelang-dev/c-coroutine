#include "c_coro.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

enum { Iterations = 500000000 };

namespace thread {
  co_routine_t *x;
  co_routine_t *y;
  volatile int counter;
}

void *co_timingtest(void *any) {
  for(;;) {
      thread::counter++;
      co_suspend();
  }
}

void sub_timingtest() {
  thread::counter++;
}

int main() {
  printf("context-switching timing test\n\n");
  time_t start, end;
  int i, t1, t2;

  start = clock();
  for(thread::counter = 0, i = 0; i < Iterations; i++) {
    sub_timingtest();
  }
  end = clock();

  t1 = (int)difftime(end, start);
  printf("%2.3f seconds per  50 million subroutine calls (%d iterations)\n", (float)t1 / CLOCKS_PER_SEC, thread::counter);

  thread::y = co_start(co_timingtest, NULL);

  start = clock();
  for(thread::counter = 0, i = 0; i < Iterations; i++) {
    co_switch(thread::y);
  }
  end = clock();

  co_delete(thread::y);

  t2 = (int)difftime(end, start);
  printf("%2.3f seconds per 100 million co_switch  calls (%d iterations)\n", (float)t2 / CLOCKS_PER_SEC, thread::counter);

  printf("co_switch skew = %fx\n\n", (double)t2 / (double)t1);
  return 0;
}
