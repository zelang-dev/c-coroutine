#include "coroutine.h"
/*
Small future and promise library in C using emulated C11 threads

Modified from https://gist.github.com/Geal/8f85e02561d101decf9a
*/

static unsigned long thread_id(thrd_t thread) {
#ifdef _WIN32
    return (unsigned long)thread.p;
#else
    return (unsigned long)thread;
#endif
}

future *future_create(callable_t start_routine) {
    future *f = try_calloc(1, sizeof(future));
    f->func = (thrd_func_t)start_routine;
    srand((unsigned int)time(NULL));
    f->id = rand();
    f->type = CO_FUTURE;

    return f;
}

int future_func_wrapper(void_t arg) {
    future_arg *f = (future_arg *)arg;
    f->type = CO_FUTURE_ARG;
    int res = f->func(f->arg);
    if (f->type == CO_FUTURE_ARG) {
        memset(f, 0, sizeof(value_types));
        CO_FREE(f);
    }

    thrd_exit(res);
    return res;
}

int future_wrapper(void_t arg) {
    future_arg *f = (future_arg *)arg;
    f->type = CO_FUTURE_ARG;
    int res = f->func(f->arg);
    promise_set(f->value, res);
    if (f->type == CO_FUTURE_ARG) {
        memset(f, 0, sizeof(value_types));
        CO_FREE(f);
    }

    thrd_exit(res);
    return res;
}

void async_start(future *f, promise *value, void_t arg) {
    future_arg *f_arg = try_calloc(1, sizeof(future_arg));
    f_arg->func = f->func;
    f_arg->arg = arg;
    f_arg->value = value;
    int r = thrd_create(&f->thread, future_wrapper, f_arg);
    RAII_INFO("thread #%lx created thread #%lx with status(%d) future id(%d) \n", co_async_self(), thread_id(f->thread), r, f->id);
}

future *co_async(callable_t func, void_t args) {
    future *f = future_create(func);
    promise *p = promise_create();
    f->value = p;
    async_start(f, p, args);
    return f;
}

value_t co_async_get(future *f) {
    value_t r = promise_get(f->value);
    promise_close(f->value);
    future_close(f);
    return r;
}

CO_FORCE_INLINE unsigned long co_async_self(void) {
#ifdef _WIN32
    return (unsigned long)thrd_current().p;
#else
    return (unsigned long)thrd_current();
#endif
}

void co_async_wait(future *f) {
    bool is_done = false;
    while (!is_done) {
        is_done = promise_done(f->value);
        co_yield();
    }
}

void future_start(future *f, void_t arg) {
    future_arg *f_arg = try_calloc(1, sizeof(future_arg));
    f_arg->func = f->func;
    f_arg->arg = arg;
    int r = thrd_create(&f->thread, future_func_wrapper, f_arg);
    RAII_INFO("thread #%lx created thread #%lx with status(%d) future id(%d) \n", co_async_self(), thread_id(f->thread), r, f->id);
}

void future_close(future *f) {
    int status;
    int rc = thrd_join(f->thread, &status);
    if (f->type == CO_FUTURE_ARG) {
        memset(f, 0, sizeof(value_types));
        CO_FREE(f);
    }
}

promise *promise_create(void) {
    promise *p = try_calloc(1, sizeof(promise));
    p->result = try_calloc(1, sizeof(values_t));
    mtx_init(&p->mutex, mtx_plain);
    cnd_init(&p->cond);
    srand((unsigned int)time(NULL));
    p->id = rand();
    p->done = false;
    p->type = CO_PROMISE;
    RAII_INFO("promise id(%d) created in thread #%lx\n", p->id, co_async_self());

    return p;
}

void promise_set(promise *p, int res) {
    RAII_INFO("promise id(%d) set LOCK in thread #%lx\n", p->id, co_async_self());
    mtx_lock(&p->mutex);
    p->result->value.integer = res;
    p->done = true;
    cnd_signal(&p->cond);
    RAII_INFO("promise id(%d) set UNLOCK in thread #%lx\n", p->id, co_async_self());
    mtx_unlock(&p->mutex);
}

value_t promise_get(promise *p) {
    RAII_INFO("\npromise id(%d) get LOCK in thread #%lx\n", p->id, co_async_self());
    mtx_lock(&p->mutex);
    while (!p->done) {
        RAII_INFO("promise id(%d) get WAIT in thread #%lx\n", p->id, co_async_self());
        cnd_wait(&p->cond, &p->mutex);
    }
    RAII_INFO("promise id(%d) get UNLOCK in thread #%lx\n", p->id, co_async_self());
    mtx_unlock(&p->mutex);
    return p->result->value;
}

bool promise_done(promise *p) {
    mtx_lock(&p->mutex);
    bool done = p->done;
    mtx_unlock(&p->mutex);
    return done;
}

void promise_close(promise *p) {
    if (p->type == CO_PROMISE) {
        mtx_destroy(&p->mutex);
        cnd_destroy(&p->cond);
        CO_FREE(p->result);
        memset(p, 0, sizeof(value_types));
        CO_FREE(p);
    }
}
