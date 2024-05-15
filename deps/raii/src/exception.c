#if defined(__GNUC__) || !defined(_WIN32)
#undef _FORTIFY_SOURCE
#endif
#include "raii.h"

/* Some common exception */
EX_EXCEPTION(invalid_type);
EX_EXCEPTION(range_error);
EX_EXCEPTION(divide_by_zero);
EX_EXCEPTION(division_by_zero);
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
EX_EXCEPTION(bad_alloc);

static thread_local ex_context_t ex_context_buffer;
thread_local ex_context_t *ex_context = NULL;
thread_local char ex_message[256] = {0};
static volatile sig_atomic_t got_signal = false;
static volatile sig_atomic_t got_uncaught_exception = false;
static volatile sig_atomic_t got_ctrl_c = false;
static volatile sig_atomic_t can_terminate = true;

ex_setup_func exception_setup_func = NULL;
ex_unwind_func exception_unwind_func = NULL;
ex_terminate_func exception_ctrl_c_func = NULL;
ex_terminate_func exception_terminate_func = NULL;
bool exception_signal_set = false;

static void ex_handler(int sig);
#if !defined(_WIN32)
static struct sigaction ex_sig_sa = {0}, ex_sig_osa = {0};
#endif

enum {
    max_ex_sig = 32
};

static struct {
    const char *ex;
    int sig;
#ifdef _WIN32
    DWORD seh;
#endif
} ex_sig[max_ex_sig];

ex_ptr_t ex_protect_ptr(ex_ptr_t *const_ptr, void *ptr, void (*func)(void *)) {
    ex_context_t *ctx = ex_init();
    const_ptr->next = ctx->stack;
    const_ptr->func = func;
    const_ptr->ptr = ptr;
    const_ptr->type = ex_protected_st;
    ctx->stack = const_ptr;
    return *const_ptr;
}

void ex_unprotected_ptr(ex_ptr_t *const_ptr) {
    const_ptr->type = -1;
    ex_context->stack = const_ptr->next;
}

static void ex_unwind_stack(ex_context_t *ctx) {
    ex_ptr_t *p = ctx->stack;
    void *temp = NULL;

    ctx->unstack = 1;

    if (ctx->is_unwind && exception_unwind_func) {
        exception_unwind_func(ctx->data);
    } else if (ctx->is_unwind && ctx->is_raii && ctx->data == (void *)raii_context) {
        raii_deferred_clean();
    } else {
        while (p && p->type == ex_protected_st) {
            if ((got_uncaught_exception = (temp == *p->ptr)))
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

static void ex_print(ex_context_t *exception, const char *message) {
    fflush(stdout);
#ifndef NDEBUG
    fprintf(stderr, "\nFatal Error: %s in function(%s)\n\n",
                  ((void *)exception->panic != NULL ? exception->panic : exception->ex), exception->function);
#else
    fprintf(stderr, "\n%s: %s\n", message, (exception->panic != NULL ? exception->panic : exception->ex));
    if (exception->file != NULL) {
        if (exception->function != NULL) {
            fprintf(stderr, "    thrown in %s at (%s:%d)\n\n", exception->function, exception->file, exception->line);
        } else {
            fprintf(stderr, "    thrown at %s:%d\n\n", exception->file, exception->line);
        }
    }
#endif
    fflush(stderr);
}

void ex_unwind_set(ex_context_t *ctx, bool flag) {
    ctx->is_unwind = flag;
}

void ex_data_set(ex_context_t *ctx, void *data) {
    ctx->data = data;
}

void *ex_data_get(ex_context_t *ctx, void *data) {
    return ctx->data;
}

void ex_swap_set(ex_context_t *ctx, void *data) {
    ctx->prev = ctx->data;
    ctx->data = data;
}

void ex_swap_reset(ex_context_t *ctx) {
    ctx->data = ctx->prev;
}

void ex_flags_reset(void) {
    got_signal = false;
    got_ctrl_c = false;
}

ex_context_t *ex_init(void) {
    if (ex_context != NULL)
        return ex_context;

    ex_signal_block(all);
    ex_context = &ex_context_buffer;
    ex_context->is_unwind = false;
    ex_context->is_rethrown = false;
    ex_context->is_guarded = false;
    ex_context->is_raii = false;
    ex_context->caught = -1;
    ex_context->type = ex_context_st;
    ex_signal_unblock(all);

    return ex_context;
}

int ex_uncaught_exception(void) {
    return ex_init()->unstack;
}

void ex_terminate(void) {
    if (ex_init()->is_raii)
        raii_destroy();

    if (ex_uncaught_exception() || got_uncaught_exception)
        ex_print(ex_context, "\nException during stack unwinding leading to an undefined behavior");
    else
        ex_print(ex_context, "\nExiting with uncaught exception");

    if (got_ctrl_c && exception_ctrl_c_func) {
        got_ctrl_c = false;
        exception_ctrl_c_func();
    }

    if (can_terminate && exception_terminate_func) {
        can_terminate = false;
        exception_terminate_func();
    }

    if (got_signal)
        _Exit(EXIT_FAILURE);
    else
        exit(EXIT_FAILURE);
}

void ex_throw(const char *exception, const char *file, int line, const char *function, const char *message) {
    ex_context_t *ctx = ex_init();

    if (ctx->unstack)
        ex_terminate();

    ex_signal_block(all);
    ctx->ex = exception;
    ctx->file = file;
    ctx->line = line;
    ctx->function = function;
    ctx->panic = message;

    if (exception_setup_func)
        exception_setup_func(ctx, ctx->ex, ctx->panic);
    else if (ctx->is_raii)
        raii_unwind_set(ctx, ctx->ex, ctx->panic);

    ex_unwind_stack(ctx);
    ex_signal_unblock(all);

    if (ctx == &ex_context_buffer)
        ex_terminate();

#ifdef _WIN32
    RaiseException(EXCEPTION_PANIC, 0, 0, NULL);
#endif
    ex_longjmp(ctx->buf, ctx->state | ex_throw_st);
}

#ifdef _WIN32
int catch_seh(const char *exception, DWORD code, struct _EXCEPTION_POINTERS *ep) {
    ex_context_t *ctx = ex_init();
    const char *ex = 0;
    int i;

    if (!is_str_eq(ctx->ex, exception))
        return EXCEPTION_EXECUTE_HANDLER;

    for (i = 0; i < max_ex_sig; i++) {
        if (ex_sig[i].seh == code
            || ctx->caught == ex_sig[i].seh
            || is_str_eq(ctx->ex, exception)
            ) {
            ctx->state = ex_throw_st;
            ctx->is_rethrown = true;
            if (got_signal) {
                ctx->ex = ex_sig[i].ex;
                ctx->file = "unknown";
                ctx->line = 0;
                ctx->function = NULL;
            }

            if (exception_setup_func)
                exception_setup_func(ctx, ctx->ex, ctx->panic);
            else if (ctx->is_raii)
                raii_unwind_set(ctx, ctx->ex, ctx->panic);
            return EXCEPTION_EXECUTE_HANDLER;
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

int catch_filter_seh(DWORD code, struct _EXCEPTION_POINTERS *ep) {
    ex_context_t *ctx = ex_init();
    const char *ex = 0;
    int i;

    if (code == EXCEPTION_PANIC) {
        ctx->state = ex_throw_st;
        return EXCEPTION_EXECUTE_HANDLER;
    }

    for (i = 0; i < max_ex_sig; i++) {
        if (ex_sig[i].seh == code || ctx->caught == ex_sig[i].seh) {
            ctx->state = ex_throw_st;
            ctx->is_rethrown = true;
            ctx->ex = ex_sig[i].ex;
            ctx->panic = NULL;
            ctx->file = "unknown";
            ctx->line = 0;
            ctx->function = NULL;

            if (exception_setup_func)
                exception_setup_func(ctx, ctx->ex, ctx->panic);
            else if (ctx->is_raii)
                raii_unwind_set(ctx, ctx->ex, ctx->panic);

            if (!ctx->is_guarded)
                ex_unwind_stack(ctx);
            return EXCEPTION_EXECUTE_HANDLER;
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

void ex_signal_seh(DWORD sig, const char *ex) {
    int i;

    for (i = 0; i < max_ex_sig; i++)
        if (!ex_sig[i].ex || ex_sig[i].seh == sig)
            break;

    if (i == max_ex_sig)
        fprintf(stderr,
            "Cannot install exception handler for signal no %d (%s), "
            "too many signal exception handlers installed (max %d)\n",
            sig, ex, max_ex_sig);
    else
        ex_sig[i].ex = ex, ex_sig[i].seh = sig;
}
#endif

void ex_handler(int sig) {
    const char *ex = NULL;
    int i;

    got_signal = true;
    if (sig == SIGINT)
        got_ctrl_c = true;

#ifdef _WIN32
    /*
     * Make signal handlers persistent.
     */
    if (signal(sig, ex_handler) == SIG_ERR)
        fprintf(stderr, "Cannot reinstall handler for signal no %d (%s)\n",
                sig, ex);
#endif

    for (i = 0; i < max_ex_sig; i++) {
        if (ex_sig[i].sig == sig) {
            ex = ex_sig[i].ex;
            ex_init()->caught = sig;
            break;
        }
    }

    ex_throw(ex, "unknown", 0, NULL, NULL);
}

void ex_signal_reset(int sig) {
#if defined(_WIN32) || defined(_WIN64)
    if (signal(sig, SIG_DFL) == SIG_ERR)
        fprintf(stderr, "Cannot install handler for signal no %d\n",
                sig);
#else
    /*
     * Make signal handlers persistent.
     */
    ex_sig_sa.sa_handler = SIG_DFL;
    if (sigemptyset(&ex_sig_sa.sa_mask) != 0)
        fprintf(stderr, "Cannot setup handler for signal no %d\n", sig);
    else if (sigaction(sig, &ex_sig_sa, NULL) != 0)
        fprintf(stderr, "Cannot restore handler for signal no %d\n", sig);
#endif
    exception_signal_set = false;
}

void ex_signal(int sig, const char *ex) {
    int i;

    for (i = 0; i < max_ex_sig; i++) {
        if (!ex_sig[i].ex || ex_sig[i].sig == sig)
            break;
    }

    if (i == max_ex_sig) {
        fprintf(stderr,
                "Cannot install exception handler for signal no %d (%s), "
                "too many signal exception handlers installed (max %d)\n",
                sig, ex, max_ex_sig);
        return;
    }

#if defined(_WIN32) || defined(_WIN64)
    if (signal(sig, ex_handler) == SIG_ERR)
        fprintf(stderr, "Cannot install handler for signal no %d (%s)\n",
                      sig, ex);
    else
        ex_sig[i].ex = ex, ex_sig[i].sig = sig;
#else
    /*
     * Make signal handlers persistent.
     */
    ex_sig_sa.sa_handler = ex_handler;
    ex_sig_sa.sa_flags = SA_RESTART;
    if (sigemptyset(&ex_sig_sa.sa_mask) != 0)
        fprintf(stderr, "Cannot setup handler for signal no %d (%s)\n",
                      sig, ex);
    else if (sigaction(sig, &ex_sig_sa, NULL) != 0)
        fprintf(stderr, "Cannot install handler for signal no %d (%s)\n",
                sig, ex);
    else
        ex_sig[i].ex = ex, ex_sig[i].sig = sig;

    exception_signal_set = true;
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

#if !defined(_WIN32)
#ifdef SIGQUIT
    ex_signal(SIGQUIT, EX_NAME(sig_quit));
#endif

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

void ex_signal_default(void) {
#ifdef SIGSEGV
    ex_signal_reset(SIGSEGV);
#endif

#if defined(SIGABRT)
    ex_signal_reset(SIGABRT);
#endif

#ifdef SIGFPE
    ex_signal_reset(SIGFPE);
#endif

#ifdef SIGILL
    ex_signal_reset(SIGILL);
#endif

#ifdef SIGBREAK
    ex_signal_reset(SIGBREAK);
#endif

#ifdef SIGINT
    ex_signal_reset(SIGINT);
#endif

#ifdef SIGTERM
    ex_signal_reset(SIGTERM);
#endif

#if !defined(_WIN32)
#ifdef SIGQUIT
    ex_signal_reset(SIGQUIT);
#endif

#ifdef SIGHUP
    ex_signal_reset(SIGHUP);
#endif

#ifdef SIGWINCH
    ex_signal_reset(SIGWINCH);
#endif
#ifdef SIGTRAP
    ex_signal_reset(SIGTRAP);
#endif
#endif

#ifdef SIGALRM
    ex_signal_reset(SIGALRM);
#endif

#ifdef SIGBUS
    ex_signal_reset(SIGBUS);
#elif SIG_BUS
    ex_signal_reset(SIG_BUS);
#endif
}
