#include "include/coroutine.h"

static thread_local co_hast_t co_ht_loop_result;

uv_loop_t *co_loop()
{
    co_routine_t *co = co_active();
    if (co->loop != NULL)
        return (uv_loop_t *)co->loop;

    uv_loop_t *loop = uv_default_loop();
    co->loop = (void *)loop;

    return loop;
}

void fs_cb(uv_fs_t *req)
{
    ssize_t result = uv_fs_get_result(req);
    void *fs_ptr = uv_fs_get_ptr(req);
    uv_fs_type fs_type = uv_fs_get_type(req);
/*/
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
        params[0] = result;
        break;
    case UV_FS_FCHMOD:
    case UV_FS_FCHOWN:
    case UV_FS_FTRUNCATE:
    case UV_FS_FDATASYNC:
    case UV_FS_FSYNC:
        params[1] = result;
        break;
    case UV_FS_OPEN:
        if (result < 0)
            params[0] = result;
        else
            params[0] = \create_resource_object(result, uv_fSystem);
        break;
    case UV_FS_SCANDIR:
        if (result < 0)
        {
            params[0] = result;
        }
        else
        {
            zval = \zval_array(\ze_ffi()->_zend_new_array(0));
            dent = \UVDirent::init('struct uv_dirent_s');
            while (UV_EOF != uv_fs_scandir_next(req, dent()))
            {
                \ze_ffi()->add_next_index_string(zval(), dent()->name);
            }
            params[0] = \zval_native(zval);
        }
        break;
    case UV_FS_LSTAT:
    case UV_FS_STAT:
        if (!\is_null(fs_ptr))
            params[0] = \uv_stat_to_zval((\uv_cast('uv_stat_t *', fs_ptr)));
        else
            params[0] = result;
        break;
    case UV_FS_FSTAT:
        if (!\is_null(fs_ptr))
            params[1] = \uv_stat_to_zval((\uv_cast('uv_stat_t *', fs_ptr)));
        else
            params[1] = result;
        break;
    case UV_FS_READLINK:
        if (result == 0)
            params[0] = \ffi_string(fs_ptr);
        else
            params[0] = result;
        break;
    case UV_FS_READ:
        buffer = uv_fSystem->buffer();
        if (result >= 0)
            params[1] = buffer->getString(result);
        else
            params[1] = result;
        break;
    case UV_FS_SENDFILE:
        params[1] = result;
        break;
    case UV_FS_WRITE:
        params[1] = result;
        break;
    case UV_FS_UNKNOWN:
    case UV_FS_CUSTOM:
    default:
        fprintf(stderr, "type; %d not supported.", fs->fs_type);
        break;
    }
*/
    if (result == -1)
    {
        fprintf(stderr, "Error at opening file: %sn", uv_strerror(result));
    }

    uv_args_t *uv_args = (uv_args_t *)uv_req_get_data((uv_req_t *)&req);
    if (uv_args != NULL)
    {
        CO_FREE(uv_args->args);
        CO_FREE(uv_args);
    }

    uv_fs_req_cleanup(req);
}

void *fs_init(void *uv_args)
{
    uv_fs_t req;
    uv_loop_t *loop = co_loop();
    uv_args_t *fs = (uv_args_t *)uv_args;
    co_value_t *args = fs->args;
    int result = -4058;

    if (fs->is_path)
    {
        const char *path = args[0].value.string;
        switch (fs->fs_type)
        {
        case UV_FS_OPEN:
            int flags = args[1].value.integer;
            int mode = args[2].value.integer;
            result = uv_fs_open(loop, &req, path, flags, mode, fs_cb);
            break;
        case UV_FS_UNLINK:
            result = uv_fs_unlink(loop, &req, path, fs_cb);
            break;
        case UV_FS_MKDIR:
            int mode = args[1].value.integer;
            result = uv_fs_mkdir(loop, &req, path, mode, fs_cb);
            break;
        case UV_FS_RENAME:
            const char *n_path = args[1].value.string;
            result = uv_fs_rename(loop, &req, path, n_path, fs_cb);
            break;
        case UV_FS_CHMOD:
            int mode = args[1].value.integer;
            result = uv_fs_chmod(loop, &req, path, mode, fs_cb);
            break;
        case UV_FS_UTIME:
            double atime = args[1].value.precision;
            double mtime = args[2].value.precision;
            result = uv_fs_utime(loop, &req, path, atime, mtime, fs_cb);
            break;
        case UV_FS_CHOWN:
            uv_uid_t uid = args[1].value.uchar;
            uv_uid_t gid = args[2].value.uchar;
            result = uv_fs_chown(loop, &req, path, uid, gid, fs_cb);
            break;
        case UV_FS_LINK:
            const char *n_path = args[1].value.string;
            result = uv_fs_link(loop, &req, path, n_path, fs_cb);
            break;
        case UV_FS_SYMLINK:
            const char *n_path = args[1].value.string;
            int flags = args[2].value.integer;
            result = uv_fs_symlink(loop, &req, path, n_path, flags, fs_cb);
            break;
        case UV_FS_RMDIR:
            result = uv_fs_rmdir(loop, &req, path, fs_cb);
            break;
        case UV_FS_FSTAT:
            const char *n_path = args[1].value.string;
            result = uv_fs_lstat(loop, &req, path, n_path, fs_cb);
            break;
        case UV_FS_STAT:
            result = uv_fs_stat(loop, &req, path, fs_cb);
            break;
        case UV_FS_SCANDIR:
            int flags = args[1].value.integer;
            result = uv_fs_scandir(loop, &req, path, flags, fs_cb);
            break;
        case UV_FS_READLINK:
            result = uv_fs_readlink(loop, &req, path, fs_cb);
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
            result = uv_fs_fstat(loop, &req, fd, fs_cb);
            break;
        case UV_FS_SENDFILE:
            uv_file in_fd = args[1].value.integer;
            int64_t offset = args[2].value.long_int;
            size_t length = args[3].value.max_int;
            result = uv_fs_sendfile(loop, &req, fd, in_fd, offset, length, fs_cb);
            break;
        case UV_FS_CLOSE:
            result = uv_fs_close(loop, &req, fd, fs_cb);
            break;
        case UV_FS_FSYNC:
            result = uv_fs_fsync(loop, &req, fd, fs_cb);
            break;
        case UV_FS_FDATASYNC:
            result = uv_fs_fdatasync(loop, &req, fd, fs_cb);
            break;
        case UV_FS_FTRUNCATE:
            int64_t offset = args[1].value.long_int;
            result = uv_fs_ftruncate(loop, &req, fd, offset, fs_cb);
            break;
        case UV_FS_FCHMOD:
            int mode = args[1].value.integer;
            result = uv_fs_fchmod(loop, &req, fd, mode, fs_cb);
            break;
        case UV_FS_FUTIME:
            double atime = args[1].value.precision;
            double mtime = args[2].value.precision;
            result = uv_fs_futime(loop, &req, fd, atime, mtime, fs_cb);
            break;
        case UV_FS_FCHOWN:
            uv_uid_t uid = args[1].value.uchar;
            uv_uid_t gid = args[2].value.uchar;
            result = uv_fs_fchown(loop, &req, fd, uid, gid, fs_cb);
            break;
        case UV_FS_READ:
            int64_t offset = args[1].value.long_int;
            uv_buf_t *bufs = args[2].value.object;
            result = uv_fs_read(loop, &req, fd, bufs, 1, offset, fs_cb);
            break;
        case UV_FS_WRITE:
            uv_buf_t *bufs = args[1].value.object;
            int64_t offset = args[2].value.long_int;
            result = uv_fs_write(loop, &req, fd, bufs, 1, offset, fs_cb);
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

    uv_req_set_data((uv_req_t *)&req, uv_args);
    return 0;
}

int co_fs_open(const char *path, int flags, int mode)
{
    co_value_t *args;
    uv_args_t *uv_args;

    args = (co_value_t *)CO_CALLOC(3, sizeof(co_value_t));
    uv_args = (uv_args_t *)CO_CALLOC(1, sizeof(uv_args_t));

    args[0].value.string = (char *)path;
    args[1].value.integer = flags;
    args[2].value.integer = mode;

    uv_args->args = args;
    uv_args->fs_type = UV_FS_OPEN;
    uv_args->n_args = 3;
    uv_args->is_path = true;

    return co_uv(fs_init, uv_args);
}
