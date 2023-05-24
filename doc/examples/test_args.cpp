/*****
 * cothread parameterized function example
 *****
 * entry point to cothreads cannot take arguments.
 * this is due to portability issues: each processor,
 * operating system, programming language and compiler
 * can use different parameter passing methods, so
 * arguments to the cothread entry points were omitted.
 *
 * however, the behavior can easily be simulated by use
 * of a specialized co_switch to set global parameters to
 * be used as function arguments.
 *
 * in this way, with a bit of extra red tape, one gains
 * even more flexibility than would be possible with a
 * fixed argument list entry point, such as void (*)(void*),
 * as any number of arguments can be used.
 *
 * this also eliminates race conditions where a pointer
 * passed to co_create may have changed or become invalidated
 * before call to co_switch, as said pointer would now be set
 * when calling co_switch, instead.
 *****/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include "coroutine.h"

co_routine_t *thread[3];

typedef struct _co_arg {
  int x;
  int y;
} co_arg;

//one could also call this co_init or somesuch if they preferred ...
void co_switch(co_routine_t *thread, int param_x, int param_y, co_arg *param)
{
  char buffer[128] = {0};
  param->x = param_x;
  param->y = param_y;
  co_switch(thread);
  const char first_word[] = "hello\0";
  co_push(thread, first_word, sizeof(buffer));
}

void *co_entrypoint(void *args)
{
  co_arg *param = (co_arg *)args;
  printf("co_entrypoint(%d, %d)\n", param->x, param->y);
  int x = param->x;
  int y = param->y;
  co_suspend();
  // co_arg::param_x will change here (due to co_switch(co_routine_t, int, int) call changing values),
  // however, param_x and param_y will persist as they are thread local

  printf("co_entrypoint(%d, %d)\n", x, y);
  co_suspend();
  throw;
}

int main() {

  char buffer[128] = {0};
  co_arg *param = (co_arg *)CO_MALLOC(sizeof(co_arg));
  printf("cothread parameterized function example\n\n");

  thread[0] = co_active();
  assert(co_status(thread[0]) == CO_DEAD);

  thread[1] = co_start(co_entrypoint, param);
  thread[2] = co_start(co_entrypoint, param);

  // use specialized co_switch(co_routine_t, int, int) for initial co_switch call
  co_switch(thread[1], 1, 2, param);
  co_switch(thread[2], 4, 8, param);

  assert(co_pop(thread[2], buffer, sizeof(buffer)) == CO_SUCCESS);
  assert(strcmp(buffer, "hello") == 0);

  // after first call, entry point arguments have been initialized, standard
  // co_switch(co_routine_t) can be used from now on
  co_switch(thread[2]);
  co_switch(thread[1]);

  assert(co_pop(thread[1], buffer, sizeof(buffer)) == CO_SUCCESS);
  assert(strcmp(buffer, "hello") == 0);
  assert(co_status(thread[0]) == CO_NORMAL);

  printf("\ndone\n");

  CO_FREE(param);
  return 0;
}
