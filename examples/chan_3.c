#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32) || defined(_WIN64)
#include "../../compat/unistd.h"
#else
#include <unistd.h>
#endif
#include "coroutine.h"

Channel *c;

void task_2(void *arg)
{
    printf("task_2 start\n");
    for (int i = 0; i < 10; i++)
    {
        unsigned long v = chanrecvul(c);
        printf("received: %lu\n", v);
    }
    printf("task_2 end\n");
}

void taskmain(int argc, char **argv)
{
    c = chancreate(sizeof(unsigned long), 3);
    taskcreate(task_2, c, 32768);
    for (unsigned long i = 0; i < 10; i++)
    {
        printf("going to send number: %lu\n", i);
        chansendul(c, i);
        printf("send success: %lu\n", i);
    }
    printf("taskmain end\n");
}
