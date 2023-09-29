#include "../include/coroutine.h"

static void_t fs_init(void_t);

static value_t co_fs_init(uv_args_t *uv_args, values_t *args, uv_fs_type fs_type, size_t n_args, bool is_path) {
    uv_args->args = args;
    uv_args->type = CO_EVENT_ARG;
    uv_args->fs_type = fs_type;
    uv_args->n_args = n_args;
    uv_args->is_path = is_path;

    return co_event(fs_init, uv_args);
}

static void fs_cb(uv_fs_t *req) {
    ssize_t result = uv_fs_get_result(req);
    uv_args_t *uv_args = (uv_args_t *)uv_req_get_data((uv_req_t *)req);
    routine_t *co = uv_args->context;
    bool override = false;

    if (result < 0) {
        fprintf(stderr, "Error: %s\n", uv_strerror((int)result));
    } else {
        void_t fs_ptr = uv_fs_get_ptr(req);
        uv_fs_type fs_type = uv_fs_get_type(req);

        switch (fs_type) {
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
                values_t *stat = co_new_by(1, sizeof(req->statbuf));
                memcpy(stat, &req->statbuf, sizeof(req->statbuf));
                co_result_set(co, stat);
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
                fprintf(stderr, "type; %d not supported.\n", fs_type);
                break;
        }
    }

    co->halt = true;
    if (!override)
        co_result_set(co, (values_t *)uv_fs_get_result(req));

    co_resuming(co->context);
    co_scheduler();
}

static void_t fs_init(void_t uv_args) {
    uv_fs_t *req = co_new_by(1, sizeof(uv_fs_t));
    uv_loop_t *loop = co_loop();
    uv_args_t *fs = (uv_args_t *)uv_args;
    values_t *args = fs->args;
    uv_uid_t uid, gid;
    uv_file in_fd;
    size_t length;
    int64_t offset;
    uv_buf_t *bufs;
    string_t n_path;
    double atime, mtime;
    int flags, mode;
    int result = -4058;

    co_defer(FUNC_VOID(uv_fs_req_cleanup), req);
    if (fs->is_path) {
        string_t path = var_char_ptr(args[0]);
        switch (fs->fs_type) {
            case UV_FS_OPEN:
                flags = var_int(args[1]);
                mode = var_int(args[2]);
                result = uv_fs_open(loop, req, path, flags, mode, fs_cb);
                break;
            case UV_FS_UNLINK:
                result = uv_fs_unlink(loop, req, path, fs_cb);
                break;
            case UV_FS_MKDIR:
                mode = var_int(args[1]);
                result = uv_fs_mkdir(loop, req, path, mode, fs_cb);
                break;
            case UV_FS_RENAME:
                n_path = var_char_ptr(args[1]);
                result = uv_fs_rename(loop, req, path, n_path, fs_cb);
                break;
            case UV_FS_CHMOD:
                mode = var_int(args[1]);
                result = uv_fs_chmod(loop, req, path, mode, fs_cb);
                break;
            case UV_FS_UTIME:
                atime = var_double(args[1]);
                mtime = var_double(args[2]);
                result = uv_fs_utime(loop, req, path, atime, mtime, fs_cb);
                break;
            case UV_FS_CHOWN:
                uid = var_unsigned_char(args[1]);
                gid = var_unsigned_char(args[2]);
                result = uv_fs_chown(loop, req, path, uid, gid, fs_cb);
                break;
            case UV_FS_LINK:
                n_path = var_char_ptr(args[1]);
                result = uv_fs_link(loop, req, path, n_path, fs_cb);
                break;
            case UV_FS_SYMLINK:
                n_path = var_char_ptr(args[1]);
                flags = var_int(args[2]);
                result = uv_fs_symlink(loop, req, path, n_path, flags, fs_cb);
                break;
            case UV_FS_RMDIR:
                result = uv_fs_rmdir(loop, req, path, fs_cb);
                break;
            case UV_FS_LSTAT:
                n_path = var_char_ptr(args[1]);
                result = uv_fs_lstat(loop, req, path, fs_cb);
                break;
            case UV_FS_STAT:
                result = uv_fs_stat(loop, req, path, fs_cb);
                break;
            case UV_FS_SCANDIR:
                flags = var_int(args[1]);
                result = uv_fs_scandir(loop, req, path, flags, fs_cb);
                break;
            case UV_FS_READLINK:
                result = uv_fs_readlink(loop, req, path, fs_cb);
                break;
            case UV_FS_UNKNOWN:
            case UV_FS_CUSTOM:
            default:
                fprintf(stderr, "type; %d not supported.\n", fs->fs_type);
                break;
        }
    } else {
        uv_file fd = var_int(args[0]);
        switch (fs->fs_type) {
            case UV_FS_FSTAT:
                result = uv_fs_fstat(loop, req, fd, fs_cb);
                break;
            case UV_FS_SENDFILE:
                in_fd = var_int(args[1]);
                offset = var_long_long(args[2]);
                length = var_size_t(args[3]);
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
                offset = var_long_long(args[1]);
                result = uv_fs_ftruncate(loop, req, fd, offset, fs_cb);
                break;
            case UV_FS_FCHMOD:
                mode = var_int(args[1]);
                result = uv_fs_fchmod(loop, req, fd, mode, fs_cb);
                break;
            case UV_FS_FUTIME:
                atime = var_double(args[1]);
                mtime = var_double(args[2]);
                result = uv_fs_futime(loop, req, fd, atime, mtime, fs_cb);
                break;
            case UV_FS_FCHOWN:
                uid = var_unsigned_char(args[1]);
                gid = var_unsigned_char(args[2]);
                result = uv_fs_fchown(loop, req, fd, uid, gid, fs_cb);
                break;
            case UV_FS_READ:
                offset = var_long_long(args[1]);
                bufs = var_cast(uv_buf_t, args[2]);
                result = uv_fs_read(loop, req, fd, bufs, 1, offset, fs_cb);
                break;
            case UV_FS_WRITE:
                bufs = var_cast(uv_buf_t, args[1]);
                offset = var_long_long(args[2]);
                result = uv_fs_write(loop, req, fd, bufs, 1, offset, fs_cb);
                break;
            case UV_FS_UNKNOWN:
            case UV_FS_CUSTOM:
            default:
                fprintf(stderr, "type; %d not supported.\n", fs->fs_type);
                break;
        }
    }

    if (result) {
        fprintf(stderr, "failed: %s\n", uv_strerror(result));
        return CO_ERROR;
    }

    fs->context = co_active();
    uv_req_set_data((uv_req_t *)req, (void_t)fs);
    return 0;
}

uv_file co_fs_open(string_t path, int flags, int mode) {
    values_t *args;
    uv_args_t *uv_args;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(3, sizeof(values_t));

    args[0].value.char_ptr = (string)path;
    args[1].value.integer = flags;
    args[2].value.integer = mode;

    return (uv_file)co_fs_init(uv_args, args, UV_FS_OPEN, 3, true).integer;
}

uv_stat_t *co_fs_fstat(uv_file fd) {
    values_t *args;
    uv_args_t *uv_args;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(1, sizeof(values_t));

    args[0].value.integer = fd;
    return (uv_stat_t *)co_fs_init(uv_args, args, UV_FS_FSTAT, 1, false).object;
}

string co_fs_read(uv_file fd, int64_t offset) {
    values_t *args;
    uv_args_t *uv_args;
    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(1, sizeof(values_t));

    args[0].value.integer = fd;
    args[1].value.long_long = offset;
    return co_fs_init(uv_args, args, UV_FS_READ, 2, false).char_ptr;
}

int co_fs_close(uv_file fd) {
    values_t *args;
    uv_args_t *uv_args;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(1, sizeof(values_t));

    args[0].value.integer = fd;
    return co_fs_init(uv_args, args, UV_FS_CLOSE, 1, false).integer;
}
