
#include <stdio.h>
#include <assert.h>
#include "coroutine.h"

static void fail(const char* message, co_result res) {
  printf("%s: %s\n", message, co_result_description(res));
  exit(-1);
}

static void *fibonacci_coro(void* args)
{
  /* Retrieve max value. */
  unsigned long max = co_value(args).integer;
  unsigned long m = 1;
  unsigned long n = 1;
  co_result res;

  while(1) {
    /* Yield the next Fibonacci number. */
    res = co_push(co_active(), &m, sizeof(m));
    if(res != CO_SUCCESS)
      fail("Failed to push to coroutine", res);
    co_suspend();

    unsigned long tmp = m + n;
    m = n;
    n = tmp;
    if(m >= max)
      break;
  }

  /* Yield the last Fibonacci number. */
  co_push(co_active(), &m, sizeof(m));

  return (void *)"hello world\0";
}

int main() {
  /* Create the coroutine. */
  co_routine_t *co;
  co_result res;

  /* Set storage. */
  unsigned long max = 1000000000;

  co = co_start(fibonacci_coro, &max);

  int counter = 1;
  unsigned long ret = 0;
  while(ret < max) {
    /* Resume the coroutine. */
    co_switch(co);

    /* Retrieve storage set in last coroutine yield. */
    ret = 0;
    res = co_pop(co, &ret, sizeof(ret));
    if(res != CO_SUCCESS)
      fail("Failed to retrieve coroutine storage", res);
    printf("fib %d = %li\n", counter, ret);
    counter = counter + 1;
  }

  assert(co_terminated(co) == 1);
  assert(strcmp((char *)co_results(co), "hello world") == 0);

  /* Destroy the coroutine. */
  co_delete(co);

  return 0;
}
