#include <stdbool.h>

typedef struct _future
{
    pthread_t thread;
    pthread_attr_t attr;
    void *(*func)(void *);
    int id;
} future;

typedef struct _future_arg
{
    void *(*func)(void *);
    void *arg;
} future_arg;

typedef struct _promise
{
    int result;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool done;
    int id;
} promise;

future *future_create(void *(*start_routine)(void *));
void future_start(future *f, void *arg);
void future_stop(future *f);
void future_close(future *f);

promise *promise_create();
int promise_get(promise *p);
void promise_set(promise *p, int res);
bool promise_done(promise *p);
void promise_close(promise *p);

#ifdef DEBUG
#define msg(format, ...) printf("T(%d)|" format "\n", (int)pthread_self(), ##__VA_ARGS__)
#else
#define msg(format, ...)
#endif
