#if defined(__GNUC__) || !defined(_WIN32)
#undef _FORTIFY_SOURCE
#endif
#include "../include/coroutine.h"
/*
 o---------------------------------------------------------------------o
 |
 | Exception in C
 |
 | Copyright (c) 2001+ Laurent Deniau, laurent.deniau@cern.ch
 |
 | For more information, see:
 | http://cern.ch/laurent.deniau/oopc.html
 |
 o---------------------------------------------------------------------o
 |
 | Exception in C is free software; you can redistribute it and/or
 | modify it under the terms of the GNU Lesser General Public License
 | as published by the Free Software Foundation; either version 2.1 of
 | the License, or (at your option) any later version.
 |
 | The C Object System is distributed in the hope that it will be
 | useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 | of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 | Lesser General Public License for more details.
 |
 | You should have received a copy of the GNU Lesser General Public
 | License along with this library; if not, write to the Free
 | Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 | Boston, MA 02110-1301 USA
 |
 o---------------------------------------------------------------------o
 |
 | $Id$
 |
*/

/* Some common exception */
EX_EXCEPTION(invalid_type);
EX_EXCEPTION(range_error);
EX_EXCEPTION(divide_by_zero);
EX_EXCEPTION(out_of_memory);
EX_EXCEPTION(panic);

/* Some signal exception */
EX_EXCEPTION(sig_int);
EX_EXCEPTION(sig_abrt);
EX_EXCEPTION(sig_alrm);
EX_EXCEPTION(sig_bus);
EX_EXCEPTION(sig_fpe);
EX_EXCEPTION(sig_ill);
EX_EXCEPTION(sig_quit);
EX_EXCEPTION(sig_segv);
EX_EXCEPTION(sig_term);
EX_EXCEPTION(sig_trap);
EX_EXCEPTION(sig_hup);
EX_EXCEPTION(sig_break);
EX_EXCEPTION(sig_winch);
EX_EXCEPTION(sig_info);
EX_EXCEPTION(access_violation);
EX_EXCEPTION(array_bounds_exceeded);
EX_EXCEPTION(breakpoint);
EX_EXCEPTION(datatype_misalignment);
EX_EXCEPTION(flt_denormal_operand);
EX_EXCEPTION(flt_divide_by_zero);
EX_EXCEPTION(flt_inexact_result);
EX_EXCEPTION(flt_invalid_operation);
EX_EXCEPTION(flt_overflow);
EX_EXCEPTION(flt_stack_check);
EX_EXCEPTION(flt_underflow);
EX_EXCEPTION(illegal_instruction);
EX_EXCEPTION(in_page_error);
EX_EXCEPTION(int_divide_by_zero);
EX_EXCEPTION(int_overflow);
EX_EXCEPTION(invalid_disposition);
EX_EXCEPTION(noncontinuable_exception);
EX_EXCEPTION(priv_instruction);
EX_EXCEPTION(single_step);
EX_EXCEPTION(stack_overflow);
EX_EXCEPTION(invalid_handle);

static thread_local ex_context_t ex_context_buffer;
thread_local ex_context_t *ex_context = 0;
thread_local char ex_message[ 256 ] = { 0 };
static volatile sig_atomic_t got_signal = false;
static volatile sig_atomic_t got_uncaught_exception = false;
static volatile sig_atomic_t got_ctrl_c = false;
static volatile sig_atomic_t can_terminate = true;

#if !defined(_WIN32)
static struct sigaction ex_sig_sa = {0}, ex_sig_osa = {0};
#endif

enum {
    max_ex_sig = 32
};

static struct {
    string_t ex;
    int sig;
#ifdef _WIN32
    DWORD seh;
#endif
} ex_sig[max_ex_sig];

int ex_uncaught_exception(void);
void ex_terminate(void);
void ex_unwind_stack(ex_context_t *ctx);

static void ex_print(ex_context_t *exception, string_t message) {
#ifndef CO_DEBUG
    printf_stderr("\nFatal Error: %s in function(%s)\n\n",
            !is_empty((void_t)exception->co->panic) ? exception->co->panic : exception->ex, exception->function);
#else
    printf_stderr("\n%s: %s\n", message, !is_empty((void_t)exception->co->panic) ? exception->co->panic : exception->ex);
    if (!is_empty((void_t)exception->file)) {
        if (!is_empty((void_t)exception->function)) {
            printf_stderr("    thrown at %s (%s:%d)\n\n", exception->function, exception->file, exception->line);
        } else {
            printf_stderr("    thrown at %s:%d\n\n", exception->file, exception->line);
        }
    }
#endif
}

ex_ptr_t ex_protect_ptr(ex_ptr_t *const_ptr, void_t ptr, void (*func)(void_t)) {
    if (!ex_context)
        ex_init();

    const_ptr->next = ex_context->stack;
    const_ptr->func = func;
    const_ptr->ptr = ptr;
    ex_context->stack = const_ptr;
    const_ptr->type = CO_ERR_PTR;
    return *const_ptr;
}

void ex_unwind_stack(ex_context_t *ctx) {
    ex_ptr_t *p = ctx->stack;
    void_t temp = NULL;

    ctx->unstack = 1;

    if (ctx->co->err_protected) {
        co_deferred_free(ctx->co);
    } else {
        while (p && p->type == CO_ERR_PTR) {
            if (got_uncaught_exception = (temp == *p->ptr))
                break;

            if (*p->ptr) {
                p->type = -1;
                p->func(*p->ptr);
            }

            temp = *p->ptr;
            p = p->next;
        }
    }

    ctx->unstack = 0;
    ctx->stack = 0;
}

void ex_init(void) {
#if !defined(_WIN32)
    sigset_t block_all, defaults;
    sigfillset(&block_all);
    pthread_sigmask(SIG_SETMASK, &block_all, &defaults);
#else
    pthread_mutex_t block_all = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&block_all);
#endif

    ex_context = &ex_context_buffer;
    ex_context->type = CO_ERR_CONTEXT;

#if !defined(_WIN32)
    pthread_sigmask(SIG_SETMASK, &defaults, NULL);
#else
    pthread_mutex_unlock(&block_all);
    pthread_mutex_destroy(&block_all);
#endif
}

int ex_uncaught_exception(void) {
    if (!ex_context)
        ex_init();

    return ex_context->unstack;
}

void ex_terminate(void) {
    if (!can_terminate)
        return;

    can_terminate = false;
    if (got_ctrl_c)
        coroutine_info();

    if (ex_uncaught_exception() || got_uncaught_exception)
        ex_print(ex_context, "Exception during stack unwinding leading to an undefined behavior");
    else
        ex_print(ex_context, "Exiting with uncaught exception");

    if (!got_signal)
        coroutine_cleanup();

    if (got_signal)
        _Exit(EXIT_FAILURE);
    else
        exit(EXIT_FAILURE);
}

void ex_throw(string_t exception, string_t file, int line, string_t function, string_t message) {
    ex_context_t *ctx = ex_context;
#if !defined(_WIN32)
    sigset_t block_all, defaults;
#endif

    if (!ctx) {
        ex_init();
        ctx = ex_context;
    }

    if (ctx->unstack)
        ex_terminate();

    ctx->ex = exception;
    ctx->file = file;
    ctx->line = line;
    ctx->function = function;

    ctx->co = co_active();
    ctx->co->err = (void_t)ctx->ex;
    ctx->co->panic = message;

#if !defined(_WIN32)
    sigfillset(&block_all);
    pthread_sigmask(SIG_SETMASK, &block_all, &defaults);
#else
    pthread_mutex_t block_all = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&block_all);
#endif

    ex_unwind_stack(ctx);

#if !defined(_WIN32)
    pthread_sigmask(SIG_SETMASK, &defaults, NULL);
#else
    pthread_mutex_unlock(&block_all);
    pthread_mutex_destroy(&block_all);
#endif

    if (ctx == &ex_context_buffer || !ctx->co->err_recovered && got_signal)
        ex_terminate();
    else if (ctx->co->err_recovered && got_signal)
        got_signal = false;

#ifdef _WIN32
    if (!is_empty((void_t)message))
        RaiseException(EXCEPTION_PANIC, 0, 0, 0);
#endif
    ex_longjmp(ctx->buf, ctx->state | ex_throw_st);
}

#ifdef _WIN32
int catch_seh(string_t exception, DWORD code, struct _EXCEPTION_POINTERS *ep) {
    string_t ex = 0;
    int i;

    for (i = 0; i < max_ex_sig; i++) {
        if (ex_sig[ i ].ex == exception)
            return EXCEPTION_EXECUTE_HANDLER;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

int catch_filter_seh(DWORD code, struct _EXCEPTION_POINTERS *ep) {
    ex_context_t *ctx = ex_context;
    if (!ctx) {
        ex_init();
        ctx = ex_context;
    }

    string_t ex = 0;
    int i;

    if (code == EXCEPTION_PANIC) {
        ctx->state = ex_throw_st;
        return EXCEPTION_EXECUTE_HANDLER;
    }

    for (i = 0; i < max_ex_sig; i++) {
        if (ex_sig[ i ].seh == code) {
            ctx->state = ex_throw_st;
            ctx->ex = ex_sig[ i ].ex;
            ctx->file = "unknown";
            ctx->line = 0;
            ctx->function = NULL;

            ctx->co = co_active();
            ctx->co->err = (void_t)ctx->ex;
            return EXCEPTION_EXECUTE_HANDLER;
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

void ex_signal_seh(DWORD sig, string_t ex) {
    int i;

    for (i = 0; i < max_ex_sig; i++)
        if (!ex_sig[ i ].ex || ex_sig[ i ].seh == sig)
            break;

    if (i == max_ex_sig)
        printf_stderr(
                "Cannot install exception handler for signal no %d (%s), "
                "too many signal exception handlers installed (max %d)\n",
                sig, ex, max_ex_sig);
    else
        ex_sig[ i ].ex = ex, ex_sig[ i ].seh = sig;
}
#endif

void ex_handler(int sig) {
    string_t ex = NULL;
    int i;

    got_signal = true;
    if (sig == SIGINT)
        got_ctrl_c = true;

#ifdef _WIN32
    /*
     * Make signal handlers persistent.
     */
    if (signal(sig, ex_handler) == SIG_ERR)
        printf_stderr("Cannot reinstall handler for signal no %d (%s)\n",
                sig, ex);
#endif

    for (i = 0; i < max_ex_sig; i++) {
        if (ex_sig[i].sig == sig) {
            ex = ex_sig[i].ex;
            break;
        }
    }

    ex_throw(ex, "unknown", 0, NULL, NULL);
}

void ex_signal(int sig, string_t ex) {
    int i;

    for (i = 0; i < max_ex_sig; i++) {
        if (!ex_sig[i].ex || ex_sig[i].sig == sig)
            break;
    }

    if (i == max_ex_sig) {
        printf_stderr(
                "Cannot install exception handler for signal no %d (%s), "
                "too many signal exception handlers installed (max %d)\n",
                sig, ex, max_ex_sig);
        return;
    }

#if defined(_WIN32) || defined(_WIN64)
    if (signal(sig, ex_handler) == SIG_ERR)
        printf_stderr("Cannot install handler for signal no %d (%s)\n",
                      sig, ex);
    else
        ex_sig[i].ex = ex, ex_sig[i].sig = sig;
#else
    /*
     * Make signal handlers persistent.
     */
    ex_sig_sa.sa_handler = ex_handler;
    ex_sig_sa.sa_flags = 0;
    if (sigemptyset(&ex_sig_sa.sa_mask) != 0)
        printf_stderr("Cannot setup handler for signal no %d (%s)\n",
                      sig, ex);
    else if (sigaction(sig, &ex_sig_sa, NULL) != 0)
        printf_stderr("Cannot install handler for signal no %d (%s)\n",
                sig, ex);
    else
        ex_sig[ i ].ex = ex, ex_sig[ i ].sig = sig;
#endif
}

void ex_signal_setup(void) {
#ifdef _WIN32
    ex_signal_seh(EXCEPTION_ACCESS_VIOLATION, EX_NAME(sig_segv));
    ex_signal_seh(EXCEPTION_ARRAY_BOUNDS_EXCEEDED, EX_NAME(array_bounds_exceeded));
    ex_signal_seh(EXCEPTION_FLT_DIVIDE_BY_ZERO, EX_NAME(sig_fpe));
    ex_signal_seh(EXCEPTION_INT_DIVIDE_BY_ZERO, EX_NAME(divide_by_zero));
    ex_signal_seh(EXCEPTION_ILLEGAL_INSTRUCTION, EX_NAME(sig_ill));
    ex_signal_seh(EXCEPTION_INT_OVERFLOW, EX_NAME(int_overflow));
    ex_signal_seh(EXCEPTION_STACK_OVERFLOW, EX_NAME(stack_overflow));
    ex_signal_seh(EXCEPTION_INVALID_HANDLE, EX_NAME(invalid_handle));
    ex_signal_seh(EXCEPTION_PANIC, EX_NAME(panic));
    ex_signal_seh(CONTROL_C_EXIT, EX_NAME(sig_int));
#endif
#ifdef SIGSEGV
    ex_signal(SIGSEGV, EX_NAME(sig_segv));
#endif

#ifdef SIGABRT
    ex_signal(SIGABRT, EX_NAME(sig_abrt));
#endif

#ifdef SIGFPE
    ex_signal(SIGFPE, EX_NAME(sig_fpe));
#endif

#ifdef SIGILL
    ex_signal(SIGILL, EX_NAME(sig_ill));
#endif

#ifdef SIGBREAK
    ex_signal(SIGBREAK, EX_NAME(sig_break));
#endif

#ifdef SIGINT
    ex_signal(SIGINT, EX_NAME(sig_int));
#endif

#ifdef SIGTERM
    ex_signal(SIGTERM, EX_NAME(sig_term));
#endif

#ifdef SIGQUIT
    ex_signal(SIGQUIT, EX_NAME(sig_quit));
#endif

#if !defined(_WIN32)
#ifdef SIGHUP
    ex_signal(SIGHUP, EX_NAME(sig_hup));
#endif

#ifdef SIGWINCH
    ex_signal(SIGWINCH, EX_NAME(sig_winch));
#endif
#ifdef SIGTRAP
    ex_signal(SIGTRAP, EX_NAME(sig_trap));
#endif
#endif

#ifdef SIGALRM
    ex_signal(SIGALRM, EX_NAME(sig_alrm));
#endif

#ifdef SIGBUS
    ex_signal(SIGBUS, EX_NAME(sig_bus));
#elif SIG_BUS
    ex_signal(SIG_BUS, EX_NAME(sig_bus));
#endif
}
