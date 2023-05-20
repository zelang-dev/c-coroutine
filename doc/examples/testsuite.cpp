#include "c_coro.h"
#include <assert.h>
#include <stdio.h>

void *coro_entry2(void* args) {
  co_routine_t* co = NULL;
  co_routine_t *co2 = co_active();

  assert(co_status(co2) == CO_RUNNING);
  assert(co_pop(co2, &co, sizeof(co)) == CO_SUCCESS);
  assert(co_pop(co2, NULL, co_get_bytes_stored(co2)) == CO_SUCCESS);
  assert(co_status(co) == CO_NORMAL);
  assert(co_get_bytes_stored(co2) == 0);
  printf("hello 2\n");
  co_suspend();
  printf("world! 2\n");
  return 0;
}

void *coro_entry(void* args) {
  char buffer[128] = {0};
  int ret;
  co_routine_t *co2;
  co_routine_t *co = co_active();

  /* Startup checks */
  assert(co_status(co) == CO_RUNNING);

  /* Get storage 1 */
  assert(co_get_bytes_stored(co) == 6);
  assert(co_peek(co, buffer, co_get_bytes_stored(co)) == CO_SUCCESS);
  assert(strcmp(buffer, "hello") == 0);
  assert(co_pop(co, NULL, co_get_bytes_stored(co)) == CO_SUCCESS);
  puts(buffer);

  /* Set storage 1 */
  ret = 1;
  assert(co_push(co, &ret, sizeof(ret)) == CO_SUCCESS);

  /* Yield 1 */
  co_suspend();

  /* Get storage 2 */
  assert(co_pop(co, buffer, co_get_bytes_stored(co)) == CO_SUCCESS);
  puts(buffer);

  /* Set storage 2 */
  ret = 2;
  assert(co_push(co, &ret, sizeof(ret)) == CO_SUCCESS);

  /* Nested coroutine test */
  co2 = co_start(coro_entry2, &co2);
  assert(co_push(co2, &co, sizeof(co)) == CO_SUCCESS);
  co_switch(co2);
  co_switch(co2);
  assert(co_get_bytes_stored(co2) == 0);
  co_delete(co2);
  assert(co_status(co2) == CO_DEAD);
  assert(co_active() == co);
  assert(co_status(co) == CO_RUNNING);
  return 0;
}

int main(void) {
  co_routine_t* co;
  int ret = 0;

  /* Create coroutine */
  co = co_start(coro_entry, NULL);
  assert(co_status(co) == CO_SUSPENDED);

  /* Set storage 1 */
  const char first_word[] = "hello";
  assert(co_push(co, first_word, sizeof(first_word)) == CO_SUCCESS);

  /* Resume 1 */
  co_switch(co);
  assert(co_status(co) == CO_RUNNING);

  /* Get storage 1 */
  assert(co_pop(co, &ret, sizeof(ret)) == CO_SUCCESS);
  assert(ret == 1);

  /* Set storage 2 */
  const char second_word[] = "world!";
  assert(co_push(co, second_word, sizeof(second_word)) == CO_SUCCESS);

  /* Resume 2 */
  co_switch(co);
  assert(co_status(co) == CO_NORMAL);

  /* Get storage 2 */
  assert(co_pop(co, &ret, sizeof(ret)) == CO_SUCCESS);
  assert(ret == 2);
  co_switch(co);

  assert(co_terminated(co) == 1);
  assert(co_results(co) == NULL);

  /* Destroy */
  printf("Test suite succeeded!\n");
  return 0;
}
