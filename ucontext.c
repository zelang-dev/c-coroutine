/*
  WARNING: the overhead of POSIX ucontext is very high,
  assembly versions of libco or libco_sjlj should be much faster

  this library only exists for two reasons:
  1: as an initial test for the viability of a ucontext implementation
  2: to demonstrate the power and speed of libco over existing implementations,
     such as pth (which defaults to wrapping ucontext on unix targets)

  use this library only as a *last resort*
*/

#include "include/coroutine.h"

#define _BSD_SOURCE
#define _XOPEN_SOURCE 500

#if __APPLE__ && __MACH__
    #include <sys/ucontext.h>
#elif defined(_WIN32) || defined(_WIN64)
    #include "ucontext_windows.c"
#else
    #include <ucontext.h>
#endif

static thread_local ucontext_t co_primary;
static thread_local ucontext_t *co_running_handle = 0;

co_routine_t *co_active(void) {
  if(!co_running_handle) co_running_handle = &co_primary;
  return (co_routine_t *)co_running_handle;
}

co_routine_t *co_derive(void* memory, unsigned int heapsize, co_callable_t func, void *args) {
  if(!co_running_handle) co_running_handle = &co_primary;
  ucontext_t* thread = (ucontext_t*)memory;
  memory = (unsigned char*)memory + sizeof(ucontext_t);
  heapsize -= sizeof(ucontext_t);
  if(thread) {
    if((!getcontext(thread) && !(thread->uc_stack.ss_sp = 0)) && (thread->uc_stack.ss_sp = memory)) {
      thread->uc_link = co_running_handle;
      thread->uc_stack.ss_size = heapsize;
      makecontext(thread, func, 0);
    } else {
      thread = 0;
    }
  }
  return (co_routine_t *)thread;
}

co_routine_t *co_create(unsigned int heapsize, co_callable_t func, void *args) {
  if(!co_running_handle) co_running_handle = &co_primary;
  ucontext_t* thread = (ucontext_t*)CO_MALLOC(sizeof(ucontext_t));
  memset(thread, 0, heapsize);
  if(thread) {
    if((!getcontext(thread) && !(thread->uc_stack.ss_sp = 0)) && (thread->uc_stack.ss_sp = CO_MALLOC(heapsize))) {
      thread->uc_link = co_running_handle;
      thread->uc_stack.ss_size = heapsize;
      makecontext(thread, func, 0);
    } else {
      co_delete((co_routine_t *)thread);
      thread = 0;
    }
  }
  return (co_routine_t *)thread;
}

void co_delete(co_routine_t *handle) {
  if(handle) {
    if(((ucontext_t*)handle)->uc_stack.ss_sp) { CO_FREE(((ucontext_t*)handle)->uc_stack.ss_sp); }
    CO_FREE(handle);
        handle->status = CO_DEAD;
  }
}

void co_switch(co_routine_t *handle) {
  ucontext_t* old_thread = co_running_handle;
  co_running_handle = (ucontext_t*)handle;
  swapcontext(old_thread, co_running_handle);
}
