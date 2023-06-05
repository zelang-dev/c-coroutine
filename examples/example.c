
#include <stdio.h>
#include <assert.h>
#include "../coroutine.h"

static void *fibonacci_coro(void* args)
{
  /* Retrieve max value. */
  unsigned long max = co_value(args).integer;
  unsigned long m = 1;
  unsigned long n = 1;

  while(1) {
    /* Yield the next Fibonacci number. */
    co_suspend_set(&m);

    unsigned long tmp = m + n;
    m = n;
    n = tmp;
    if(m >= max)
      break;
  }

  /* Yield the last Fibonacci number. */
  co_suspend_set(&m);

  return (void *)"hello world\0";
}

int co_main(int argc, char **argv)
{
  /* Create the coroutine. */
  co_routine_t *co;

  /* Set storage. */
  unsigned long max = 1000000000;

  co = co_start(fibonacci_coro, &max);

  int counter = 1;
  unsigned long ret = 0;
  while(ret < max) {
    /* Resume the coroutine. */
    /* Retrieve storage set in last coroutine yield. */
    ret = co_value(co_resuming(co)).big_int;

    printf("fib %d = %li\n", counter, ret);
    counter = counter + 1;
  }

  co_resuming(co);

  assert(co_terminated(co) == true);
  assert(strcmp((char *)co_results(co), "hello world") == 0);

  /* Destroy the coroutine. */
  co_delete(co);

  return 0;
}
