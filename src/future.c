#include "../include/coroutine.h"
/*
Small future and promise library in C with pthreads

Modified from https://gist.github.com/Geal/8f85e02561d101decf9a
*/

static unsigned long thread_id(pthread_t thread) {
#ifdef _WIN32
    return (long *)thread.p;
#else
    return thread;
#endif
}

future *future_create(callable_t start_routine) {
    future *f = try_calloc(1, sizeof(future));

    pthread_attr_init(&f->attr);
    pthread_attr_setdetachstate(&f->attr, PTHREAD_CREATE_JOINABLE);

    f->func = start_routine;
    f->type = CO_FUTURE;
    srand((unsigned int)time(NULL));
    f->id = rand();

    return f;
}

void_t future_func_wrapper(void_t arg) {
    future_arg *f = (future_arg *)arg;
    f->type = CO_FUTURE_ARG;
    void_t res = f->func(f->arg);
    CO_FREE(f);
    pthread_exit(res);
    return res;
}

void_t future_wrapper(void_t arg) {
    future_arg *f = (future_arg *)arg;
    f->type = CO_FUTURE_ARG;
    void_t res = f->func(f->arg);
    promise_set(f->value, res);
    CO_FREE(f);
    pthread_exit(res);
    return res;
}

void async_start(future *f, promise *value, void_t arg) {
    future_arg *f_arg = try_calloc(1, sizeof(future_arg));
    f_arg->func = f->func;
    f_arg->arg = arg;
    f_arg->value = value;
    int r = pthread_create(&f->thread, &f->attr, future_wrapper, f_arg);
    CO_INFO("thread #%lx created thread #%lx with status(%d) future id(%d) \n", co_async_self(), thread_id(f->thread), r, f->id);
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

CO_FORCE_INLINE unsigned long co_async_self() {
#ifdef _WIN32
    return (long)pthread_self().p;
#else
    return pthread_self();
#endif
}

void co_async_wait(future *f) {
    bool is_done = false;
    while (!is_done) {
        is_done = promise_done(f->value);
        co_pause();
    }
}

void future_start(future *f, void_t arg) {
    future_arg *f_arg = try_calloc(1, sizeof(future_arg));
    f_arg->func = f->func;
    f_arg->arg = arg;
    int r = pthread_create(&f->thread, &f->attr, future_func_wrapper, f_arg);
    CO_INFO("thread #%lx created thread #%lx with status(%d) future id(%d) \n", co_async_self(), thread_id(f->thread), r, f->id);
}

void future_stop(future *f) {
    pthread_cancel(f->thread);
}

void future_close(future *f) {
    void_t status;
    int rc = pthread_join(f->thread, &status);
    pthread_attr_destroy(&f->attr);
    CO_FREE(f);
}

promise *promise_create() {
    promise *p = try_calloc(1, sizeof(promise));
    p->result = try_calloc(1, sizeof(values_t));
    pthread_mutex_init(&p->mutex, NULL);
    pthread_cond_init(&p->cond, NULL);
    srand((unsigned int)time(NULL));
    p->type = CO_PROMISE;
    p->id = rand();
    p->done = false;
    CO_INFO("promise id(%d) created in thread #%lx\n", p->id, co_async_self());

    return p;
}

void promise_set(promise *p, void_t res) {
    CO_INFO("promise id(%d) set LOCK in thread #%lx\n", p->id, co_async_self());
    pthread_mutex_lock(&p->mutex);
    p->result->value.object = res;
    p->done = true;
    pthread_cond_signal(&p->cond);
    CO_INFO("promise id(%d) set UNLOCK in thread #%lx\n", p->id, co_async_self());
    pthread_mutex_unlock(&p->mutex);
}

value_t promise_get(promise *p) {
    CO_INFO("promise id(%d) get LOCK in thread #%lx\n", p->id, co_async_self());
    pthread_mutex_lock(&p->mutex);
    while (!p->done) {
        CO_INFO("promise id(%d) get WAIT in thread #%lx\n", p->id, co_async_self());
        pthread_cond_wait(&p->cond, &p->mutex);
    }
    CO_INFO("promise id(%d) get UNLOCK in thread #%lx\n", p->id, co_async_self());
    pthread_mutex_unlock(&p->mutex);
    return p->result->value;
}

bool promise_done(promise *p) {
    pthread_mutex_lock(&p->mutex);
    bool done = p->done;
    pthread_mutex_unlock(&p->mutex);
    return done;
}

void promise_close(promise *p) {
    pthread_mutex_destroy(&p->mutex);
    pthread_cond_destroy(&p->cond);
    CO_FREE(p->result);
    CO_FREE(p);
}
