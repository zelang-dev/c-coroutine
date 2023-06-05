/***********************************************************
* Project: Complete exception handling for pure C code.    *
* Author: Paulo H. "Taka" Torrens.                         *
* E-Mail: paulo_torrens@hotmail.com.                       *
* Date: August, 31st; 2011.                                *
* License: GNU LGPLv3+ (http://gnu.org/licenses/lgpl.html) *
*   It's free for use everyway, even in commercial         *
*   projects. The only thing that is forbidden is you to   *
*   claim this code was written by you. There is even no   *
*   need to mention my name in your project, although I    *
*   would really appreciate if you do that (or the name of *
*   the person to whom this code was dedicated to). =p     *
* Warranty: None. No one at all. Use at your own risk!     *
*                                                          *
* Language: ANSI/ISO C                                     *
* Description: This code enables the user to make use of   *
*   fully complete exception handling in pure C code.      *
*   There is a complete system, because it uses "try" to   *
*   start, "catch(err)" to figure some error, "rescue" to  *
*   handle all the remaining errors, and the "finally"     *
*   clause, called wherever there's an error or not.       *
*   As a gift we have tree more statements: the "retry",   *
*   which you may use in case of an exception to restart   *
*   the whole block (back to the start of the "try"),      *
*   "comeback", for returning to the point the exception   *
*   was thrown (insted of running the whole code again, it *
*   just assumes the error was fixed by the handler and it *
*   can return to whence it was thrown), and the "check"   *
*   statement, which makes possible check for failures in  *
*   functions that change "errno", like fopen() and so on. *
* Dedicated to: Sabrina da Silva Pereira, the girl with    *
*   the most beautiful eyes I have ever seen in my life.   *
*                                                          *
* C.S.S.M.D-N.D.S.M.D-V.R.S-N.S.M.V-S.M.Q.L-I.V.B. Amem.   *
***********************************************************/

#pragma once
#ifndef __C89_EXCEPTION_HANDLERS__
  #define __C89_EXCEPTION_HANDLERS__
  #include <stdlib.h>
  #include <stdio.h>
  #include <stddef.h>
  #include <stdbool.h>
  #include <string.h>
  #include <setjmp.h>
  #include <errno.h>
  #include <signal.h>
  /**
    \defgroup c_excpt ANSI/ISO C Exception Handling
    A totally compatible ANSI/ISO C native exception
    handling library. It has a little runtime overtime, so
    it shouldn't be used when time is an important pitfall.

    This library was written by Paulo H. "Taka" Torrens, and
    dedicated to Sabrina da Silva Pereira.

    \code
      static _Bool flag = true;
      try {
        if(flag) {
          throw("damn");
          retry;
        } else {
          throw(42);
        };
      } catch("damn") {
        flag = false;
        comeback;
      } rescue {
        printf("Hello, Exceptions!\n");
      } finally {
        printf("Enjoy!\n");
      };
    \endcode
  */
  #if __STDC_VERSION__ < 199900L
    #define __func__ NULL
  #endif
  #if __STDC_VERSION__ < 201000L
    #ifdef __MSCV__
      #define _Thread_local __declspec(thread_local)
    #else
      #define _Thread_local __thread
    #endif
  #endif
  #define exception_concat2(x, y) x ## y
  #define exception_concat(x, y) exception_concat2(x, y)
  #define exception_local exception_concat(_, __LINE__)
  /**
    A pseud-keyword to start an exception handler.
    \ingroup c_excpt
  */
  #define try \
    bool exception_local; \
    for(exception_init(&exception_local); exception_local; exception_propagate()) \
      if(exception_try())
  /**
    A pseudo-keyword to be used after a try block to check
    for any thrown exceptions, which this will save.
    It will always catch a word, check throw for examples.
    \see try
    \see throw
    \see rescue
    \ingroup c_excpt
  */
  #define catch(x) else if(exception_catch((int)(x))) \
    for((void)0; exception_get(); exception_clear())
  /**
    A pseudo-keyword for general error catching, i.e., every
    single exception may be caught here, and so we avoid a
    possible termination of the program.
    \see catch
    \ingroup c_excpt
  */
  #define rescue else if(exception_rescue()) \
    for((void)0; exception_get(); exception_clear())
  /**
    A pseudo-keyword for running a block after the handler
    is finished, being it by right execution or by an
    exception thrown.
    \ingroup c_excpt
  */
  #define finally else if(exception_finally())
  /**
    Used for raising an exception. The library will then see
    if are there any handlers that might get it, and, if we
    fall into an exception that couldn't be handled, then
    exception_panic is called.
    You can throw any variable with a word-size. It means
    that you usually will want to throw numbers, but you may
    throw strings or other objects (pointers) safelly, even
    pointers to functions.
    \code
      try {
        throw(42);
      } catch(42) {
        throw("foo");
      } catch("foo") {
        throw(&foo);
      } catch(&foo) {
        // We've thrown the three exceptions and fallen here
      };
    \endcode
    \see exception_panic
    \warning Zero/NULL should can not be thrown.
    \ingroup c_excpt
  */
  #define throw(x) exception_throw((int)(x), __FILE__, __LINE__, __func__)
  /**
    Start again the inner-most try block.
    \see comeback
    \warning As we are returning, an unwanted behaviour must
             have been executed, so all local variables from
             the scope on which the try block was made will
             return to their original values for safety.
    \bug I couldn't find any ways to avoid from restoring
         the local variables when the library must restore
         the stack because of the function calls (being
         still ANSI/ISO, i.e., portable). Any ideas?
         \code
           static _Bool x = true;
           int a = 10;
           try {
             // We fall here twice and, in both of them, a
             // will be 10, as it is local to the try's
             // scope... but, as x is static, it will be
             // true once, and then false
             a = 20;
             if(x) throw(1);
           } catch(1) {
             x = false;
             retry;
           };
         \endcode
    \ingroup c_excpt
  */
  #define retry exception_retry()
  /**
    Go back and continue the execution from the position the
    last exception was thrown, as if it didn't occour.
    \see retry
    \warning Different from retry, which restore the local
             variables at return type, as we came back from
             an exception, the local variables are hold all
             their modifications, although it loses them if
             a finally statement is to be called.
             Consequently, all the modification to those
             same variables in a caught/rescue statement are
             lost as we go back to the throw.
    \bug I couldn't find any ways to avoid from restoring
         the local variables when the library must restore
         the stack because of the function calls (being
         still ANSI/ISO, i.e., portable). Any ideas?
         \code
           int a = 10;
           try {
             a = 20;
             throw(1);
             // We fall here first, and a is 20
           } finally {
             // Then we fall here, but a is 10 once again
           } catch(1) {
             comeback;
           };
         \endcode
    \ingroup c_excpt
  */
  #define comeback exception_comeback()
  /**
    We check for modifications on the errno variable. If so,
    it throws an exception (which may be returned with
    comeback too). E.g.:
    \code
      FILE *f = NULL;
      try {
        check {
          f = fopen("foo.txt", "r")
        };
        // Successfully opened the file
        fclose(f);
      } catch(-2) {
        // Could not open the file (errno was set to -2)
      };
    \endcode
    \see throw
    \ingroup c_excpt
  */
  #define check \
    bool exception_local = true; \
    for((void)0; exception_local; exception_check(&exception_local, __FILE__, __LINE__, __func__))
  /**
    The running status of an exception handler. We use it to
    define the behaviour of the handler inside of the block.
    \ingroup c_excpt
  */
  typedef enum exception_state {
    /**
      The block has started and it's calling it's try block.
      \see try
    */
    EXCEPTION_TRYING = 0x01,
    /**
      The block've had an exception, which was successfully
      rescued and handled.
      \see rescue
    */
    EXCEPTION_RESCUE = 0x02,
    /**
      The try block has already run, so now we have to check
      if we have any finally statement to handle.
      \see finally
    */
    EXCEPTION_FINISH = 0x04,
    /**
      This handler is the memory backup of an exception,
      which we may want to recover if we call comeback.
      \see throw
      \see comeback
    */
    EXCEPTION_THROWN = 0x08,
    /**
      This handler was an exception, but it was marked to be
      returned as a come back.
      \see comeback
    */
    EXCEPTION_RETURN = 0x10,
    /**
      This handler is dead-like, i.e., we had an exception
      inside of it that could not be handled.
    */
    EXCEPTION_ZOMBIE = 0x20,
    /**
      A living handler is the one that is currently running,
      if it is trying for the first time or just was saved
      from being a zombie.
    */
    EXCEPTION_LIVING = EXCEPTION_TRYING | EXCEPTION_RESCUE
  } exception_state;
  /**
    The reason will exception_panic was called.
    \ingroup c_excpt
  */
  typedef enum exception_reason {
    /**
      An uncaught exception was found.
    */
    EXCEPTION_UNCAUGHT,
    /**
      An unrecheable point of the code was, ironically,
      reached... bug?
    */
    EXCEPTION_UNRECHEABLE,
    /**
      Could not allocate enough memory for the handlers.
    */
    EXCEPTION_UNALLOCATED
  } exception_reason;
  /**
    An exception handler, the object which controls the flux
    of code.
    \ingroup c_excpt
  */
  typedef struct exception_handler {
    /**
      The previous handler in the stack (which is a FILO
      container).
      \see exception_stack
      \see exception_set_handler
      \see exception_pop_handler
    */
    struct exception_handler *previous;
    /**
      The flag at the start of the try block: we have to set
      it to false after popping it, to avoid calling it once
      again.
    */
    bool *flag;
    /**
      The start position in the program stack from which we
      will save our state. We save it in order to return to
      the handler from anywhere, restoring the call stack.
    */
    void *start;
    /**
      The program state in which the handler was created,
      and the one to which it shall return.
    */
    jmp_buf nest;
    /**
      Which is our active status?
    */
    exception_state status;
    /**
      The size of the backup memory stack.
      \see start
      \see stack
    */
    size_t stack_size;
    /**
      Our backup memory stack.
      \see start
      \see stack_size
    */
    unsigned char *stack;
    /**
      The file from whence this handler was made, in order
      to backtrace it later (if we want to).
    */
    const char *file;
    /**
      The line from whence this handler was made, in order
      to backtrace it later (if we want to).
    */
    int line;
    /**
      The function from whence this handler was made, in
      order to backtrace it later (if we want to). In C89 we
      have no function, so this will be just the NULL
      terminator.
    */
    char function[];
  } exception_handler;
  /**
    Clears the exception flag and kills the dead handlers if
    we have rescued some exception.
    \ingroup c_excpt
  */
  void exception_clear(void);
  /**
    Sets the current exception to the first argument. It
    cannot be zero/NULL.
    \param[in] - The new exception.
    \ingroup c_excpt
  */
  void exception_set(register int);
  /**
    Gets the current running exception.
    \ingroup c_excpt
  */
  int exception_get(void);
  /**
    Starts a new handler and setup all the necessary
    information for it to run.
    \param[inout] - The flag used for calling this handler.
    \see try
    \ingroup c_excpt
  */
  void exception_init(register bool *);
  /**
    Propagates an exception or just clear the already used
    handlers as needed, as well as control their states.
    This makes the exception handling go to the next step it
    should take.
    \see try
    \ingroup c_excpt
  */
  void exception_propagate(void);
  /**
    Are we to run a try statement now?
    \see try
    \ingroup c_excpt
  */
  bool exception_try(void);
  /**
    Can we catch an exception? If the first argument equals
    to the current exception, the exception is then clean
    and it returns true, allowing the catch block to run.
    \param[in] - The possible exception to get.
    \see catch
    \ingroup c_excpt
  */
  bool exception_catch(register int);
  /**
    Should we rescue and save the life of the current
    handler?
    \see rescue
    \ingroup c_excpt
  */
  bool exception_rescue(void);
  /**
    Can we call the terminator of the block already?
    \see finally
    \ingroup c_excpt
  */
  bool exception_finally(void);
  /**
    Throws a new exception and then try handling it.
    \param[in] - The exception to throw.
    \param[in] - The file from whence it was thrown.
    \param[in] - The line from whence it was thrown.
    \param[in] - The function from whence it was thrown.
    \see throw
    \ingroup c_excpt
  */
  void exception_throw(register int, register const char *, register int, register const char *);
  /**
    Retries the execution of the inner-most try block.
    \see retry
    \ingroup c_excpt
  */
  void exception_retry(void);
  /**
    Go back to the last thrown point.
    \see comeback
    \ingroup c_excpt
  */
  void exception_comeback(void);
  /**
    Check for changes in errno.
    \param[out] - The calling flag, in order to run it once.
    \param[in] - The file from whence it was thrown.
    \param[in] - The line from whence it was thrown.
    \param[in] - The function from whence it was thrown.
    \see check
    \ingroup c_excpt
  */
  void exception_check(register bool *, register const char *, register int, register const char *);
  /**
    Gets the top of the handlers' stack.
    \ingroup c_excpt
  */
  exception_handler exception_last_handler(void);
  /**
    Gets a handler with the desired state(s). You may binary
    or them in order to seek for more than one type.
    Calls exception_panic if there are no more handlers of
    this kind.
    \see exception_state
    \see exception_find_handler
    \ingroup c_excpt
  */
  exception_handler exception_get_handler(register exception_state);
  /**
    Gets a handler with the desired state(s). You may binary
    or them in order to seek for more than one type.
    Different from exception_get_handler, we return NULL
    instead of calling exception_panic here if no handler is
    found.
    \param[in] - The state(s) to look for.
    \see exception_state
    \see exception_get_handler
    \ingroup c_excpt
  */
  exception_handler exception_find_handler(register exception_state);
  /**
    Sets the handler to be the first argument. If it is the
    second handler in the stack, then the first will be
    freed (deallocated) and the second assumes it's place.
    Otherwise, just set a new top for the stack, putting
    the first in second, instead of deleting it.
    \param[in] - The new stack top
    \ingroup c_excpt
  */
  exception_handler exception_set_handler(register exception_handler);
  /**
    Deletes the top of the stack (by setting the handler to
    the second one).
    \see exception_set_handler
    \ingroup c_excpt
  */
  void exception_pop_handler(void);
  /**
    Creates a new handler.
    \param[in] - The state of the new handler.
    \param[inout] - The flag which controls the execution of the handler.
    \param[in] - The origin file.
    \param[in] - The origin line.
    \param[in] - The origin function.
    \ingroup c_excpt
  */
  exception_handler exception_add_handler(register exception_state, register bool *, register const char *, register int, register const char *);
  /**
    Returns to the creation point of a handler, restoring
    it's memory.
    \param[in] - The handler which to return.
    \see exception_add_handler
    \ingroup c_excpt
  */
  void exception_return_home(register exception_handler);
  /**
    Save all zombies (for checking for a new exception).
    \ingroup c_excpt
  */
  void exception_save_zombies(void);
  /**
    The function called when no handler could catch some
    exception.
    \see throw
    \ingroup c_excpt
  */
  extern void (*exception_panic)(exception_reason);
  /**
    The exception stack. It is a per-thread variable, so
    avoiding any crashes. Each thread has it's own handlers.
    \ingroup c_excpt
  */
  extern _Thread_local exception_handler exception_stack;
#endif
