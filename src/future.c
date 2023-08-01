#include "../include/coroutine.h"

future *future_create(void *(*start_routine)(void *))
{
    future *f = CO_MALLOC(sizeof(future));

    pthread_attr_init(&f->attr);
    pthread_attr_setdetachstate(&f->attr, PTHREAD_CREATE_JOINABLE);

    f->func = start_routine;
    srand((unsigned int)time(NULL));
    f->id = rand();

    return f;
}

void *future_func_wrapper(void *arg)
{
    future_arg *f = (future_arg *)arg;
    void *res = f->func(f->arg);
    CO_FREE(f);
    pthread_exit(res);
    return res;
}
void future_start(future *f, void *arg)
{
    future_arg *f_arg = CO_MALLOC(sizeof(future_arg));
    f_arg->func = f->func;
    f_arg->arg = arg;
    int r = pthread_create(&f->thread, &f->attr, future_func_wrapper, f_arg);
    CO_INFO("thread started status(%d) future id(%d) \n", r, f->id);
}

void future_stop(future *f)
{
    pthread_cancel(f->thread);
}

void future_close(future *f)
{
    void *status;
    int rc = pthread_join(f->thread, &status);
    pthread_attr_destroy(&f->attr);
    CO_FREE(f);
}

promise *promise_create()
{
    promise *p = CO_MALLOC(sizeof(promise));
    p->result = CO_MALLOC(sizeof(co_value_t));
    pthread_mutex_init(&p->mutex, NULL);
    pthread_cond_init(&p->cond, NULL);
    srand((unsigned int)time(NULL));
    p->id = rand();
    p->done = false;
    CO_INFO("promise id(%d) created\n", p->id);

    return p;
}

void promise_set(promise *p, void *res)
{
    CO_INFO("promise id(%d) set LOCK\n", p->id);
    pthread_mutex_lock(&p->mutex);
    p->result->value.object = res;
    p->done = true;
    pthread_cond_signal(&p->cond);
    CO_INFO("promise id(%d) set UNLOCK\n", p->id);
    pthread_mutex_unlock(&p->mutex);
}

value_t promise_get(promise *p)
{
    CO_INFO("promise id(%d) get LOCK\n", p->id);
    pthread_mutex_lock(&p->mutex);
    while (!p->done)
    {
        CO_INFO("promise id(%d) get WAIT\n", p->id);
        pthread_cond_wait(&p->cond, &p->mutex);
    }
    CO_INFO("promise id(%d) get UNLOCK\n", p->id);
    pthread_mutex_unlock(&p->mutex);
    return p->result->value;
}

bool promise_done(promise *p)
{
    pthread_mutex_lock(&p->mutex);
    bool done = p->done;
    pthread_mutex_unlock(&p->mutex);
    return done;
}

void promise_close(promise *p)
{
    pthread_mutex_destroy(&p->mutex);
    pthread_cond_destroy(&p->cond);
    CO_FREE(p->result);
    CO_FREE(p);
}
