/*
  WARNING: the overhead of POSIX ucontext is very high,
  assembly versions of libco or libco_sjlj should be much faster

  this library only exists for two reasons:
  1: as an initial test for the viability of a ucontext implementation
  2: to demonstrate the power and speed of libco over existing implementations,
     such as pth (which defaults to wrapping ucontext on unix targets)

  use this library only as a *last resort*
*/

#include "coroutine.h"

#define _BSD_SOURCE
#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <ucontext.h>

#ifdef __cplusplus
extern "C" {
#endif

static thread_local ucontext_t co_primary;
static thread_local ucontext_t* co_running = 0;

co_routine_t co_active(void) {
  if(!co_running) co_running = &co_primary;
  return (co_routine_t)co_running;
}

co_routine_t *co_derive(void* memory, unsigned int heapsize, co_callable_t func, void *args) {
  if(!co_running) co_running = &co_primary;
  ucontext_t* thread = (ucontext_t*)memory;
  memory = (unsigned char*)memory + sizeof(ucontext_t);
  heapsize -= sizeof(ucontext_t);
  if(thread) {
    if((!getcontext(thread) && !(thread->uc_stack.ss_sp = 0)) && (thread->uc_stack.ss_sp = memory)) {
      thread->uc_link = co_running;
      thread->uc_stack.ss_size = heapsize;
      makecontext(thread, func, 0);
    } else {
      thread = 0;
    }
  }
  return (co_routine_t *)thread;
}

co_routine_t *co_create(unsigned int heapsize, co_callable_t func, void *args) {
  if(!co_running) co_running = &co_primary;
  ucontext_t* thread = (ucontext_t*)malloc(sizeof(ucontext_t));
  if(thread) {
    if((!getcontext(thread) && !(thread->uc_stack.ss_sp = 0)) && (thread->uc_stack.ss_sp = malloc(heapsize))) {
      thread->uc_link = co_running;
      thread->uc_stack.ss_size = heapsize;
      makecontext(thread, func, 0);
    } else {
      co_delete((co_routine_t *)thread);
      thread = 0;
    }
  }
  return (co_routine_t *)thread;
}

void co_delete(co_routine_t *co_thread) {
  if(co_thread) {
    if(((ucontext_t*)co_thread)->uc_stack.ss_sp) { free(((ucontext_t*)co_thread)->uc_stack.ss_sp); }
    free(co_thread);
        handle->state = CO_DEAD;
  }
}

void co_switch(co_routine_t *co_thread) {
  ucontext_t* old_thread = co_running;
  co_running = (ucontext_t*)co_thread;
  swapcontext(old_thread, co_running);
}

unsigned char co_serializable(void) {
  return 0;
}

#ifdef __cplusplus
}
#endif
