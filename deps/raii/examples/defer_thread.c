#include "raii.h"

/**
 ** @file
 **
 ** This is a test program for the defer mechanism that v
 ** shows some compliant solutions using defer
 ** for https://wiki.sei.cmu.edu/confluence/x/LdYxBQ
 **/

mtx_t lock;

unsigned int completed = 0;
enum { max_threads = 5 };
char number[20];

static unsigned int names[max_threads];
static thrd_t threads[max_threads];

void defer_capture(void *arg) {
    thrd_t thrd = (thrd_t)raii_value(arg)->max_size;
    if (thrd_success != thrd_join(thrd, NULL)) {
        puts("main: thrd_join failure");
        thrd_exit(EXIT_FAILURE);
    }

    printf("main: thread %lu joined.\n", thrd);
}

void do_print(void *name) {
    printf("thread %u deferred mtx_unlock = %.03u\n", raii_value(name)->integer, completed);
    if (thrd_success != mtx_unlock(&lock)) {
        snprintf(number, 20, "%d", thrd_error);
        _panic(number);
    }
}

int do_work(void *namep) {
    guard{
    unsigned int *name = namep;
      printf("thread #%lu . %u: starting do_work guarded block\n", thrd_current(), *name);
      if (thrd_success != mtx_lock(&lock)) {
          thrd_exit(thrd_error);
      }
      _defer(do_print, name);
      /* Access data protected by the lock */
      completed += 1;
    } guarded;

    // these last two lines should do the same thing
    // return thrd_success;
    thrd_exit(EXIT_SUCCESS);
    return EXIT_SUCCESS;
}

static void cleanup_threads(void) {
    printf(".cleanup: %p\n", (void *)threads);
    *threads = 0;
}

int main(int argc, char **argv) {
    // install the overall cleanup handlers
    atexit(cleanup_threads);
    // compliant solution using threads array
    guard{
      puts("main: starting main guarded block");
      if (thrd_success != mtx_init(&lock, mtx_plain)) {
          exit(EXIT_FAILURE);
      }
      _defer(mtx_destroy, &lock);
      // In that case, a panic will be triggered, but
      // the above `mtx_destroy` should still be called, followed by the
      // atexit cleanup.
      for (unsigned int i = 0; i < max_threads; i++) {
        names[i] = i;
        if (thrd_success != thrd_create(&threads[i], do_work, &names[i])) {
          exit(EXIT_FAILURE);
        }
        _defer(defer_capture, &threads[i]);
      } // end for
    } guarded;// end guard

    printf("main: completed = %u.\n", completed);
    return EXIT_SUCCESS;
}
