#include "../coroutine.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

enum { Iterations = 500000000 };

static int counter;

void *co_timingtest(void *any) {
  co_routine_t *x = (co_routine_t *)any;
  for(;;) {
      counter++;
      co_switch(x);
  }
}

void sub_timingtest() {
  counter++;
}

int co_main(int argc, char **argv)
{
  printf("context-switching timing test\n\n");
  time_t start, end;
  co_routine_t *x, *y;
  int i, t1, t2;

  start = clock();
  for(counter = 0, i = 0; i < Iterations; i++) {
    sub_timingtest();
  }
  end = clock();

  t1 = (int)difftime(end, start);
  printf("%2.3f seconds per  50 million subroutine calls (%d iterations)\n", (float)t1 / CLOCKS_PER_SEC, counter);

  x = co_active();
  y = co_start(co_timingtest, (void *)x);

  start = clock();
  for(counter = 0, i = 0; i < Iterations; i++) {
    co_switch(y);
  }
  end = clock();

  t2 = (int)difftime(end, start);
  printf("%2.3f seconds per 100 million co_switch  calls (%d iterations)\n", (float)t2 / CLOCKS_PER_SEC, counter);

  printf("co_switch skew = %fx\n\n", (double)t2 / (double)t1);

  co_delete(y);

  return 0;
}
