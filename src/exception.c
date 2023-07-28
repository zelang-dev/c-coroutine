#include "../include/coroutine.h"

/* Some common exception */
EX_EXCEPTION(invalid_type);
EX_EXCEPTION(range_error);
EX_EXCEPTION(division_by_zero);
EX_EXCEPTION(out_of_memory);
EX_EXCEPTION(panic);

/* Some signal exception */
EX_EXCEPTION(sig_abrt);
EX_EXCEPTION(sig_alrm);
EX_EXCEPTION(sig_bus);
EX_EXCEPTION(sig_fpe);
EX_EXCEPTION(sig_ill);
EX_EXCEPTION(sig_quit);
EX_EXCEPTION(sig_segv);
EX_EXCEPTION(sig_term);
EX_EXCEPTION(sig_hup);
EX_EXCEPTION(sig_break);
EX_EXCEPTION(sig_winch);

static thread_local ex_context_t ex_context_buffer;
thread_local ex_context_t *ex_context = 0;

static void ex_print(ex_context_t *exception, const char *message)
{
#ifndef CO_DEBUG
    fprintf(stderr, "\nFatal Error: %s in function(%s)\n\n",
            (exception->co->panic != NULL) ? exception->co->panic : exception->ex, exception->function);
#else
    fprintf(stderr, "\n%s: %s\n", message, (exception->co->panic != NULL) ? exception->co->panic : exception->ex);
    if (exception->file != NULL)
    {
        if (exception->function != NULL)
        {
            fprintf(stderr, "    thrown at %s (%s:%d)\n\n", exception->function, exception->file, exception->line);
        }
        else
        {
            fprintf(stderr, "    thrown at %s:%d\n\n", exception->file, exception->line);
        }
    }
#endif

    (void)fflush(stderr);
}

ex_ptr_t ex_protect_ptr(ex_ptr_t *const_ptr, void *ptr, void (*func)(void *))
{
    if (!ex_context)
        ex_init();

    const_ptr->next = ex_context->stack;
    const_ptr->func = func;
    const_ptr->ptr = ptr;
    ex_context->stack = const_ptr;
    return *const_ptr;
}

static void unwind_stack(ex_context_t *ctx)
{
    ex_ptr_t *p = ctx->stack;

    ctx->unstack = 1;

    if (ctx->co->err_protected)
    {
        co_deferred_free(ctx->co);
    }
    else
    {
        while (p)
        {
            if (*p->ptr)
                p->func(*p->ptr);
            p = p->next;
        }
    }

    ctx->unstack = 0;
    ctx->stack = 0;
}

void ex_init(void)
{
    ex_context = &ex_context_buffer;
}

int ex_uncaught_exception(void)
{
    if (!ex_context)
        ex_init();

    return ex_context->unstack;
}

void ex_terminate(void)
{
    fflush(stdout);
    if (ex_uncaught_exception())
    {
        ex_print(ex_context, "Coroutine-system, exception during stack unwinding leading to an undefined behavior");
        abort();
    }
    else
    {
        ex_print(ex_context, "Coroutine-system, exiting with uncaught exception");
        exit(EXIT_FAILURE);
    }
}

void ex_throw(const char *exception, const char *file, int line, const char *function, const char *message)
{
    ex_context_t *ctx = ex_context;

    if (!ctx)
    {
        ex_init();
        ctx = ex_context;
    }

    ctx->ex = exception;
    ctx->file = file;
    ctx->line = line;
    ctx->function = function;

    ctx->co = co_active();
    ctx->co->err = (void *)ctx->ex;
    ctx->co->panic = message;

    if (ctx->unstack)
        ex_terminate();

    unwind_stack(ctx);

    if (ctx == &ex_context_buffer)
        ex_terminate();

    ex_longjmp(ctx->buf, ctx->state | ex_throw_st);
}

enum
{
    max_ex_sig = 32
};

static struct
{
    const char *ex;
    int sig;
} ex_sig[max_ex_sig];

static void ex_handler(int sig)
{
    void (*old)(int) = ex_signaling(sig);
    const char *ex = 0;
    int i;

    for (i = 0; i < max_ex_sig; i++)
        if (ex_sig[i].sig == sig)
        {
            ex = ex_sig[i].ex;
            break;
        }

    if (old == SIG_ERR)
        fprintf(stderr, "Coroutine-system, cannot reinstall handler for signal no %d (%s)\n",
                sig, ex);

    ex_throw(ex, "unknown", 0, NULL, NULL);
}

void (*ex_signaling(int sig))(int)
{
#ifdef _WIN32
    return signal(sig, ex_handler);
#else
    static struct sigaction sa;
    // Setup the handler
    sa.sa_handler = &ex_handler;
    // Restart the system call, if at all possible
    sa.sa_flags = SA_RESTART;
    // Block every signal during the handler
    sigfillset(&sa.sa_mask);
    return sigaction(sig, &sa, NULL);
#endif
}

void (*ex_signal(int sig, const char *ex))(int)
{
    void (*old)(int);
    int i;

    for (i = 0; i < max_ex_sig; i++)
        if (!ex_sig[i].ex || ex_sig[i].sig == sig)
            break;

    if (i == max_ex_sig)
    {
        fprintf(stderr,
                "Coroutine-system, cannot install exception handler for signal no %d (%s), "
                "too many signal exception handlers installed (max %d)\n",
                sig, ex, max_ex_sig);
        return SIG_ERR;
    }

    old = ex_signaling(sig);
    if (old == SIG_ERR)
        fprintf(stderr, "Coroutine-system, cannot install handler for signal no %d (%s)\n",
                sig, ex);
    else
        ex_sig[i].ex = ex, ex_sig[i].sig = sig;

    return old;
}

void ex_signal_std(void)
{
#if SIGABRT
    ex_signal(SIGABRT, EX_NAME(sig_abrt));
#endif

#if SIGALRM
    ex_signal(SIGALRM, EX_NAME(sig_alrm));
#endif

#if SIGBUS
    ex_signal(SIGBUS, EX_NAME(sig_bus));
#elif SIG_BUS
    ex_signal(SIG_BUS, EX_NAME(sig_bus));
#endif

#if SIGFPE
    ex_signal(SIGFPE, EX_NAME(sig_fpe));
#endif

#if SIGILL
    ex_signal(SIGILL, EX_NAME(sig_ill));
#endif

#if SIGBREAK
    ex_signal(SIGBREAK, EX_NAME(sig_break));
#endif

#if !defined(_WIN32)
    #if SIGQUIT
        ex_signal(SIGQUIT, EX_NAME(sig_quit));
    #endif

    #if SIGHUP
        ex_signal(SIGHUP, EX_NAME(sig_hup));
    #endif

    #if SIGWINCH
        ex_signal(SIGWINCH, EX_NAME(sig_winch));
    #endif
#endif

#if SIGSEGV
    ex_signal(SIGSEGV, EX_NAME(sig_segv));
#endif

#if SIGTERM
    ex_signal(SIGTERM, EX_NAME(sig_term));
#endif
}
