/*
  note this was designed for UNIX systems. Based on ideas expressed in a paper by Ralf Engelschall.
  for SJLJ on other systems, one would want to rewrite springboard() and co_create() and hack the jmb_buf stack pointer.
*/

#include "c_coro.h"

#define _BSD_SOURCE
#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  sigjmp_buf context;
  co_callable_t func;
  void* stack;
} co_thread_struct;

static thread_local co_thread_struct co_primary;
static thread_local co_thread_struct* creating;
static thread_local co_thread_struct* co_running = 0;

static void springboard(int ignored) {
  if(sigsetjmp(creating->context, 0)) {
    co_running->func();
  }
}

co_routine_t *co_active(void) {
  if(!co_running) co_running = &co_primary;
  return (co_routine_t *)co_running;
}

co_routine_t *co_derive(void *memory, unsigned int size, co_callable_t func, void *args)
{
  if(!co_running) co_running = &co_primary;

  co_thread_struct* thread = (co_thread_struct*)memory;
  memory = (unsigned char*)memory + sizeof(co_thread_struct);
  size -= sizeof(co_thread_struct);
  if(thread) {
    struct sigaction handler;
    struct sigaction old_handler;

    stack_t stack;
    stack_t old_stack;

    thread->func = thread->stack = 0;

    stack.ss_flags = 0;
    stack.ss_size = size;
    thread->stack = stack.ss_sp = memory;
    if(stack.ss_sp && !sigaltstack(&stack, &old_stack)) {
      handler.sa_handler = springboard;
      handler.sa_flags = SA_ONSTACK;
      sigemptyset(&handler.sa_mask);
      creating = thread;

      if(!sigaction(SIGUSR1, &handler, &old_handler)) {
        if(!raise(SIGUSR1)) {
          thread->func = func;
        }
        sigaltstack(&old_stack, 0);
        sigaction(SIGUSR1, &old_handler, 0);
      }
    }

    if(thread->func != func) {
      co_delete(thread);
      thread = 0;
    }
  }

  return (co_routine_t *)thread;
}

co_routine_t *co_create(unsigned int size, co_callable_t func, void *args) {
  if(!co_running) co_running = &co_primary;

  co_thread_struct* thread = (co_thread_struct*)malloc(sizeof(co_thread_struct));
  if(thread) {
    struct sigaction handler;
    struct sigaction old_handler;

    stack_t stack;
    stack_t old_stack;

    thread->func = thread->stack = 0;

    stack.ss_flags = 0;
    stack.ss_size = size;
    thread->stack = stack.ss_sp = malloc(size);
    if(stack.ss_sp && !sigaltstack(&stack, &old_stack)) {
      handler.sa_handler = springboard;
      handler.sa_flags = SA_ONSTACK;
      sigemptyset(&handler.sa_mask);
      creating = thread;

      if(!sigaction(SIGUSR1, &handler, &old_handler)) {
        if(!raise(SIGUSR1)) {
          thread->func = func;
        }
        sigaltstack(&old_stack, 0);
        sigaction(SIGUSR1, &old_handler, 0);
      }
    }

    if(thread->func != func) {
      co_delete(thread);
      thread = 0;
    }
  }

  return (co_routine_t *)thread;
}

void co_delete(co_routine_t *co_thread) {
  if(co_thread) {
    if(((co_thread_struct*)co_thread)->stack) {
      free(((co_thread_struct*)co_thread)->stack);
    }
    free(co_thread);
    co_thread->state = CO_DEAD;
  }
}

void co_switch(co_routine_t *co_thread) {
  if(!sigsetjmp(co_running->context, 0)) {
    co_running = (co_thread_struct*)co_thread;
    siglongjmp(co_running->context, 1);
  }
}

unsigned char co_serializable(void) {
  return 0;
}

#ifdef __cplusplus
}
#endif
