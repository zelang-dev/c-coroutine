#include "exception.h"

/* A default panic handler for the library */
static void default_panic(exception_reason reason) {
  /* A simple handling as a example for panic functions */
  int exception = exception_get();
  printf("Panic! ");
  switch(reason) {
    /* If we fall here, it must be a bug */
    case EXCEPTION_UNRECHEABLE: {
      printf("Unrecheable code run. Bug?\n");
    } break;
    /* Here we know that an exception was thrown, but couldn't be caught */
    case EXCEPTION_UNCAUGHT: {
      exception_handler handler = exception_find_handler(EXCEPTION_THROWN);
      printf("Uncaught exception run on ");
      if(handler) {
        printf("%s@%s:%d [error code: %d].\n", handler->function, handler->file, handler->line, exception);
      } else {
        printf("a unknown position.\n");
      };
    } break;
    /* If we couldn't allocate enough memory... */
    case EXCEPTION_UNALLOCATED: {
      printf("Handler allocation failed.\n");
    } break;
  };
  /* Finally, exit with the error code */
  exit(exception ? exception : -1);
};

/* Cleans the current error flag */
void exception_clear(void) {
  /* Here we have to set the exception to zero (NULL) */
  exception_set(0);
  /* And remove everything if we just rescued an error */
  exception_handler handler = exception_find_handler(EXCEPTION_RESCUE);
  if(handler) {
    while(handler != exception_last_handler()) {
      exception_pop_handler();
    };
  };
};

/* Sets the current error flag */
void exception_set(register int n) {
  /* Set's the exception itself */
  errno = n;
};

/* Gets the current error flag */
int exception_get(void) {
  /* Gets the exception value */
  return errno;
};

/* Handler opener */
void exception_init(register bool *flag) {
  /* At the beginning of a try block, we clean the exception */
  exception_clear();
  /* And create a new handler */
  exception_add_handler(EXCEPTION_TRYING, flag, NULL, 0, NULL);
  /* We have here to start the loop flag for the handler */
  *flag = true;
};

/* Handler closer */
void exception_propagate(void) {
  /* Let's check the current last handler */
  exception_handler handler = exception_last_handler();
  switch(handler->status) {
    /* If we were just trying... */
    case EXCEPTION_TRYING:
    case EXCEPTION_RESCUE: {
      /* Ok, once we have tried (successfully), let's just try our final block */
      handler->status = EXCEPTION_FINISH;
    } break;
    /* If we've already called the final execution... */
    case EXCEPTION_FINISH: {
      /* We can get out of this block */
      exception_pop_handler();
    } break;
    /* If our last exception if a thrown handler... */
    case EXCEPTION_THROWN: {
      /* If we still have the exception, it wasn't caught */
      if(exception_get()) {
        /* Then we have to semi-kill a handler, turning it into a brain-eating zombie */
        handler = exception_get_handler(EXCEPTION_LIVING);
        handler->status = EXCEPTION_ZOMBIE;
        exception_return_home(exception_get_handler(EXCEPTION_LIVING));
      };
      /* The exception is done, to remove it and than try again */
      exception_pop_handler();
      exception_propagate();
    } break;
    /* If we found a zombie, it means we came back from an outter handler to an inner one */
    case EXCEPTION_ZOMBIE: {
      /* So we have to finish it before leaving */
      handler->status = EXCEPTION_FINISH;
      exception_return_home(handler);
    } break;
  };
};

/* Try blocks */
bool exception_try(void) {
  /* We have to check here if we are to call the try block */
  exception_handler handler = exception_last_handler();
  switch(handler->status) {
    /* If our handler is beginning, hence we are trying it */
    case EXCEPTION_TRYING: {
      return true;
    } break;
  };
  /* Otherwise, don't execute it and go ahead */
  return false;
};

/* Catch blocks */
bool exception_catch(register int x) {
  /* First we've to be sure we are not trying to catch NULL */
  if(x) {
    /* If we have a thrown exception, let's check if we can catch it */
    exception_handler handler = exception_last_handler();
    if(handler->status == EXCEPTION_THROWN) {
      return exception_get() == x;
    };
  };
  /* Now we avoid calling it as default, 'cause it wasn't caught */
  return false;
};

/* Rescue blocks */
bool exception_rescue(void) {
  /* Is there an alive handler and, at the same time, an uncaught exception? */
  exception_handler handler = exception_find_handler(EXCEPTION_LIVING);
  if(handler) {
    if(exception_get()) {
      /* We do, so let's tell the handler it was rescued and then call the block */
      handler->status = EXCEPTION_RESCUE;
      return true;
    };
  };
  /* No rescue for now */
  return false;
};

/* Finally blocks */
bool exception_finally(void) {
  /* If the top handler is finishing, then call the finish block */
  return exception_last_handler()->status == EXCEPTION_FINISH;
};

/* Throw exceptions */
void exception_throw(register int x, register const char *file, register int line, register const char *function) {
  /* Let's assure we won't throw zero, shall we? */
  if(x) {
    /* First let's backup our data and get a new handler */
    exception_handler handler;
    handler = exception_add_handler(EXCEPTION_THROWN, (bool *)&handler, file, line, function);
    /* Are we inside any try block? */
    if(exception_last_handler()) {
      /* Check it's state */
      if(handler->status == EXCEPTION_RETURN) {
        /* If we've returned, act as there were no exception at all, deleting it */
        exception_pop_handler();
        /* Make sure we're error-free */
        exception_clear();
      } else {
        /* We have to set the exception itself */
        exception_set(x);
        /* We can go back to check newer handlers, if any */
        exception_save_zombies();
        /* And we have to return to the previous handler */
        exception_return_home(exception_get_handler(EXCEPTION_LIVING));
      };
    } else {
      /* Ooops! Called throw outside try! */
      (*exception_panic)(EXCEPTION_UNCAUGHT);
    };
  };
};

/* Retry expressions */
void exception_retry(void) {
  /* We want to return to the start of the try block */
  exception_handler handler = exception_get_handler(~EXCEPTION_THROWN);
  /* So first we have to kill newer things */
  while(exception_last_handler() != handler) {
    exception_pop_handler();
  };
  /* We are trying again, let's return to it */
  handler->status = EXCEPTION_TRYING;
  exception_return_home(handler);
};

/* Comeback expressions */
void exception_comeback(void) {
  /* We have to come back to our last thrown exception */
  exception_handler handler = exception_get_handler(EXCEPTION_THROWN);
  handler->status = EXCEPTION_RETURN; 
  exception_return_home(handler);
};

/* Check expressions */
void exception_check(register bool *flag, register const char *file, register int line, register const char *function) {
  /* If errno was set, then we have an exception here */
  if(errno) {
    exception_throw(errno, file, line, function);
  };
  /* Just set the flag used in the check syntax sugar */
  *flag = false;
};

/* Gets the top of the stack, regardless of the status */
exception_handler exception_last_handler(void) {
  /* Gets the top of the stack */
  return exception_stack;
};

/* Gets a handler with the desired status */
exception_handler exception_get_handler(register exception_state status) {
  /* We must get a handler and be sure it exists */
  exception_handler handler = exception_find_handler(status);
  if(!handler) {
    (*exception_panic)(EXCEPTION_UNCAUGHT);
  };
  return handler;
};

/* Gets a handler with the desired status, if any */
exception_handler exception_find_handler(register exception_state status) {
  /* We have to circle all the handlers and check it's state */
  exception_handler handler;
  for(handler = exception_last_handler(); handler; handler = handler->previous) {
    if(handler->status & status) {
      /* If we found it, then return it */
      return handler;
    };
  };
  /* Otherwise... return NULL, nothing was found */
  return NULL;
};

/* Set's a new handler (and deletes the top if setting to the second) */
exception_handler exception_set_handler(register exception_handler handler) {
  /* We have to set a new active handler */
  exception_handler last = exception_last_handler();
  /* First we have to be sure we're not popping back to the old one */
  if(last && last->previous == handler) {
    /* Yeah, we are, so remove the current and set it to the older */
    if(last->status & EXCEPTION_FINISH) {
      /* Of course, if we've finished it , we won't go back */
      *last->flag = false;
    };
    /* Here we just free the used memory */
    free(last->stack);
    free(last);
  } else {
    /* We have a new handler, we are not just returning */
    if(handler) {
      handler->previous = last;
    };
  };
  /* Now just set the stack and return it */
  exception_stack = handler;
  return handler;
};

/* Removes the top of the handler stack */
void exception_pop_handler(void) {
  /* Are there any handlers? */
  exception_handler handler = exception_last_handler();
  if(handler) {
    /* If so, set the current handler to the previous one and free the old one */
    exception_set_handler(handler->previous);
  } else {
    /* Should never be here... */
    (*exception_panic)(EXCEPTION_UNRECHEABLE);
  };
};

/* Creates a new handler, or gives the returned one */
exception_handler exception_add_handler(register exception_state status, register bool *flag, register const char *file, register int line, register const char *function) {
  /* Do we know the function that made the handler? */
  int function_size = function ? strlen(function) : 0;
  /* We make here a new handler */
  exception_handler handler = malloc(sizeof(struct exception_handler) + function_size + 1);
  if(handler) {
    handler->flag = flag;
    handler->start = exception_last_handler() ? exception_get_handler(EXCEPTION_LIVING)->start : flag;
    handler->status = status;
    handler->stack_size = (unsigned long)handler->start - (unsigned long)&function_size;
    handler->stack = malloc(handler->stack_size);
    handler->file = file;
    handler->line = line;
    if(function) {
      memcpy(handler->function, function, function_size);
    } else {
      *handler->function = (char)0;
    };
    if(handler->stack) {
      memcpy(handler->stack, &function_size, handler->stack_size);
      /* We push it to the stack */
      exception_set_handler(handler);
      /* And start it's execution... Warning: it is to here that it will return later on */
      exception_handler young = (exception_handler)setjmp(handler->nest);
      if(young) {
        /* If we've returned, let's restaure the old stack values */
        memcpy(young->start - young->stack_size, young->stack, young->stack_size);
        /* We return then the returned value, as if it was added now (which it "was") */
        return young;
      };
      /* We return the returned handler (may be at any position on the stack now) */
      return handler;
    };
  };
  /* Allocation have failed! */
  (*exception_panic)(EXCEPTION_UNALLOCATED);
  return NULL;
};

/* Returns to where the given handler was made */
void exception_return_home(exception_handler handler) {
  /* We will return to the handler's nest (where it was saved) */
  if(handler) {
    longjmp(handler->nest, (int)handler);
  };
};

/* Save all zombie exceptions, because we may need to return to them */
void exception_save_zombies(void) {
  /* We run through all active handlers */
  exception_handler handler;
  for(handler = exception_last_handler(); handler; handler = handler->previous) {
    /* If we found a zombie, heal it back to life */
    if(handler->status == EXCEPTION_ZOMBIE) {
      handler->status = EXCEPTION_TRYING;
    };
  };
};

/* Exception final handler (panic!) */
void (*exception_panic)(exception_reason) = &default_panic;

/* Exception handler stack */
_Thread_local exception_handler exception_stack = NULL;
