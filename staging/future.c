#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#if defined(_WIN64) || defined(_WIN32)
#include "unistd.h"
#else
#include <unistd.h>
#endif
#include <stdbool.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>

#include "future.h"

future *future_create(void *(*start_routine)(void *))
{
    future *f = malloc(sizeof(future));

    pthread_attr_init(&f->attr);
    pthread_attr_setdetachstate(&f->attr, PTHREAD_CREATE_JOINABLE);

    f->func = start_routine;
    srand(time(NULL));
    f->id = rand();

    return f;
}

void *future_func_wrapper(void *arg)
{
    // msg("WRAP");
    future_arg *f = (future_arg *)arg;
    void *res = f->func(f->arg);
    free(f);
    // msg("WRAPPED");
    pthread_exit(res);
    return res;
}
void future_start(future *f, void *arg)
{
    future_arg *farg = malloc(sizeof(future_arg));
    farg->func = f->func;
    farg->arg = arg;
    pthread_create(&f->thread, &f->attr, future_func_wrapper, farg);
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
    free(f);
}

promise *promise_create()
{
    promise *p = malloc(sizeof(promise));
    pthread_mutex_init(&p->mutex, NULL);
    pthread_cond_init(&p->cond, NULL);
    srand(time(NULL));
    p->id = rand();
    msg("P(%d) created", p->id);

    return p;
}

void promise_set(promise *p, int res)
{
    msg("P(%d) set LOCK", p->id);
    pthread_mutex_lock(&p->mutex);
    p->result = res;
    p->done = true;
    pthread_cond_signal(&p->cond);
    msg("P(%d) set UNLOCK", p->id);
    pthread_mutex_unlock(&p->mutex);
}

int promise_get(promise *p)
{
    msg("P(%d) get LOCK", p->id);
    pthread_mutex_lock(&p->mutex);
    while (!p->done)
    {
        msg("P(%d) get WAIT", p->id);
        pthread_cond_wait(&p->cond, &p->mutex);
    }
    msg("P(%d) get UNLOCK", p->id);
    pthread_mutex_unlock(&p->mutex);
    return p->result;
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
    free(p);
}
