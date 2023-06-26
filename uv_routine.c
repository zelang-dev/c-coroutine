#include "include/coroutine.h"

void open_cb(uv_fs_t *req);

uv_loop_t *co_loop()
{
    co_routine_t *co = co_active();
    if (co->loop != NULL)
        return (uv_loop_t *)co->loop;

    uv_loop_t *loop = uv_default_loop();
    co->loop = (void *)loop;

    return loop;
}

void *fs_open(void *args)
{
    uv_fs_t open_req;
    uv_loop_t *loop = co_loop();
   // const char *path = ((const char **)&args)[0];
   // printf("%s\n", path);
   // int flags = ((co_value_t **)args)[1]->value.integer;
   // int mode = ((co_value_t **)args)[2]->value.integer;
   // return (void *)uv_fs_open(loop, &open_req, path, flags, mode, open_cb);
}

void open_cb(uv_fs_t *req)
{
    int result = (int)req->result;

    if (result == -1)
    {
        fprintf(stderr, "Error at opening file: %s\n", uv_strerror(result));
    }

    uv_fs_req_cleanup(req);

    printf("Successfully opened file.\n");
}

int co_fs_open(const char *path, int flags, int mode)
{
    void **args[3];
    args[0] = &path;
//    args[1] = &flags;
 //   args[2] = &mode;

    return co_uv(fs_open, &args);
}
