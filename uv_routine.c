#include "include/coroutine.h"

static void *fs_init(void *);

static value_t co_fs_init(uv_args_t *uv_args, co_value_t *args, uv_fs_type fs_type, size_t n_args, bool is_path)
{
    uv_args->args = args;
    uv_args->fs_type = fs_type;
    uv_args->n_args = n_args;
    uv_args->is_path = is_path;

    co_ht_group_t *wg = co_wait_group();
    int cid = co_uv(fs_init, uv_args);
    co_ht_result_t *wgr = co_wait(wg);
    return co_group_get_result(wgr, cid);
}

static void fs_cb(uv_fs_t *req)
{
    ssize_t result = uv_fs_get_result(req);
    uv_args_t *uv_args = (uv_args_t *)uv_req_get_data((uv_req_t *)req);
    co_routine_t *co = uv_args->context;
    bool override = false;

    if (result < 0)
    {
        fprintf(stderr, "Error: %s\n", uv_strerror((int)result));
    }
    else
    {
        void *fs_ptr = uv_fs_get_ptr(req);
        uv_fs_type fs_type = uv_fs_get_type(req);

        switch (fs_type)
        {
        case UV_FS_CLOSE:
        case UV_FS_SYMLINK:
        case UV_FS_LINK:
        case UV_FS_CHMOD:
        case UV_FS_RENAME:
        case UV_FS_UNLINK:
        case UV_FS_RMDIR:
        case UV_FS_MKDIR:
        case UV_FS_CHOWN:
        case UV_FS_UTIME:
        case UV_FS_FUTIME:
        case UV_FS_FCHMOD:
        case UV_FS_FCHOWN:
        case UV_FS_FTRUNCATE:
        case UV_FS_FDATASYNC:
        case UV_FS_FSYNC:
        case UV_FS_OPEN:
            break;
        case UV_FS_SCANDIR:
            break;
        case UV_FS_LSTAT:
        case UV_FS_STAT:
            break;
        case UV_FS_FSTAT:
            override = true;
            co_result_set(co, uv_fs_get_statbuf(req));
            break;
        case UV_FS_READLINK:
            break;
        case UV_FS_READ:
            break;
        case UV_FS_SENDFILE:
            break;
        case UV_FS_WRITE:
            break;
        case UV_FS_UNKNOWN:
        case UV_FS_CUSTOM:
        default:
            fprintf(stderr, "type; %d not supported.", fs_type);
            break;
        }
    }

    co->halt = true;
    if (!override)
        co_result_set(co, (co_value_t *)uv_fs_get_result(req));

    co_resuming(co->context);
    co_scheduler();
}

static void *fs_init(void *uv_args)
{
    uv_fs_t *req = co_new_by(1, sizeof(uv_fs_t));
    uv_loop_t *loop = co_loop();
    uv_args_t *fs = (uv_args_t *)uv_args;
    co_value_t *args = fs->args;
    uv_uid_t uid, gid;
    uv_file in_fd;
    size_t length;
    int64_t offset;
    uv_buf_t *bufs;
    const char *n_path;
    double atime, mtime;
    int flags, mode;
    int result = -4058;

    co_defer(uv_fs_req_cleanup, req);
    if (fs->is_path)
    {
        const char *path = args[0].value.string;
        switch (fs->fs_type)
        {
        case UV_FS_OPEN:
            flags = args[1].value.integer;
            mode = args[2].value.integer;
            result = uv_fs_open(loop, req, path, flags, mode, fs_cb);
            break;
        case UV_FS_UNLINK:
            result = uv_fs_unlink(loop, req, path, fs_cb);
            break;
        case UV_FS_MKDIR:
            mode = args[1].value.integer;
            result = uv_fs_mkdir(loop, req, path, mode, fs_cb);
            break;
        case UV_FS_RENAME:
            n_path = args[1].value.string;
            result = uv_fs_rename(loop, req, path, n_path, fs_cb);
            break;
        case UV_FS_CHMOD:
            mode = args[1].value.integer;
            result = uv_fs_chmod(loop, req, path, mode, fs_cb);
            break;
        case UV_FS_UTIME:
            atime = args[1].value.precision;
            mtime = args[2].value.precision;
            result = uv_fs_utime(loop, req, path, atime, mtime, fs_cb);
            break;
        case UV_FS_CHOWN:
            uid = args[1].value.uchar;
            gid = args[2].value.uchar;
            result = uv_fs_chown(loop, req, path, uid, gid, fs_cb);
            break;
        case UV_FS_LINK:
            n_path = args[1].value.string;
            result = uv_fs_link(loop, req, path, n_path, fs_cb);
            break;
        case UV_FS_SYMLINK:
            n_path = args[1].value.string;
            flags = args[2].value.integer;
            result = uv_fs_symlink(loop, req, path, n_path, flags, fs_cb);
            break;
        case UV_FS_RMDIR:
            result = uv_fs_rmdir(loop, req, path, fs_cb);
            break;
        case UV_FS_FSTAT:
            n_path = args[1].value.string;
            result = uv_fs_lstat(loop, req, path, fs_cb);
            break;
        case UV_FS_STAT:
            result = uv_fs_stat(loop, req, path, fs_cb);
            break;
        case UV_FS_SCANDIR:
            flags = args[1].value.integer;
            result = uv_fs_scandir(loop, req, path, flags, fs_cb);
            break;
        case UV_FS_READLINK:
            result = uv_fs_readlink(loop, req, path, fs_cb);
            break;
        case UV_FS_UNKNOWN:
        case UV_FS_CUSTOM:
        default:
            fprintf(stderr, "type; %d not supported.", fs->fs_type);
            break;
        }
    }
    else
    {
        uv_file fd = args[0].value.integer;
        switch (fs->fs_type)
        {
        case UV_FS_FSTAT:
            result = uv_fs_fstat(loop, req, fd, fs_cb);
            break;
        case UV_FS_SENDFILE:
            in_fd = args[1].value.integer;
            offset = args[2].value.long_int;
            length = args[3].value.max_int;
            result = uv_fs_sendfile(loop, req, fd, in_fd, offset, length, fs_cb);
            break;
        case UV_FS_CLOSE:
            result = uv_fs_close(loop, req, fd, fs_cb);
            break;
        case UV_FS_FSYNC:
            result = uv_fs_fsync(loop, req, fd, fs_cb);
            break;
        case UV_FS_FDATASYNC:
            result = uv_fs_fdatasync(loop, req, fd, fs_cb);
            break;
        case UV_FS_FTRUNCATE:
            offset = args[1].value.long_int;
            result = uv_fs_ftruncate(loop, req, fd, offset, fs_cb);
            break;
        case UV_FS_FCHMOD:
            mode = args[1].value.integer;
            result = uv_fs_fchmod(loop, req, fd, mode, fs_cb);
            break;
        case UV_FS_FUTIME:
            atime = args[1].value.precision;
            mtime = args[2].value.precision;
            result = uv_fs_futime(loop, req, fd, atime, mtime, fs_cb);
            break;
        case UV_FS_FCHOWN:
            uid = args[1].value.uchar;
            gid = args[2].value.uchar;
            result = uv_fs_fchown(loop, req, fd, uid, gid, fs_cb);
            break;
        case UV_FS_READ:
            offset = args[1].value.long_int;
            bufs = args[2].value.object;
            result = uv_fs_read(loop, req, fd, bufs, 1, offset, fs_cb);
            break;
        case UV_FS_WRITE:
            bufs = args[1].value.object;
            offset = args[2].value.long_int;
            result = uv_fs_write(loop, req, fd, bufs, 1, offset, fs_cb);
            break;
        case UV_FS_UNKNOWN:
        case UV_FS_CUSTOM:
        default:
            fprintf(stderr, "type; %d not supported.", fs->fs_type);
            break;
        }
    }

    if (result)
    {
        fprintf(stderr, "failed: %s\n", uv_strerror(result));
        return CO_ERROR;
    }

    fs->context = co_active();
    uv_req_set_data((uv_req_t *)req, (void *)fs);
    return 0;
}

uv_file co_fs_open(const char *path, int flags, int mode)
{
    co_value_t *args;
    uv_args_t *uv_args;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (co_value_t *)co_new_by(3, sizeof(co_value_t));

    args[0].value.string = (char *)path;
    args[1].value.integer = flags;
    args[2].value.integer = mode;

    return (uv_file)co_fs_init(uv_args, args, UV_FS_OPEN, 3, true).integer;
}

uv_stat_t *co_fs_fstat(uv_file fd)
{
    co_value_t *args;
    uv_args_t *uv_args;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (co_value_t *)co_new_by(1, sizeof(co_value_t));

    args[0].value.integer = fd;
    return (uv_stat_t *)co_fs_init(uv_args, args, UV_FS_FSTAT, 1, false).object;
}

int co_fs_close(uv_file fd)
{
    co_value_t *args;
    uv_args_t *uv_args;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (co_value_t *)co_new_by(1, sizeof(co_value_t));

    args[0].value.integer = fd;
    return co_fs_init(uv_args, args, UV_FS_CLOSE, 1, false).integer;
}
