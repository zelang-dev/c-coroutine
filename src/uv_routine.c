#include "../include/coroutine.h"

static void_t fs_init(void_t);
static void_t uv_init(void_t);

static void _close_cb(uv_handle_t *handle) {
    CO_FREE(handle);
}

static void uv_close_free(void_t handle) {
    uv_close((uv_handle_t *)handle, _close_cb);
}

static value_t fs_start(uv_args_t *uv_args, values_t *args, uv_fs_type fs_type, size_t n_args, bool is_path) {
    uv_args->args = args;
    uv_args->type = CO_EVENT_ARG;
    uv_args->fs_type = fs_type;
    uv_args->n_args = n_args;
    uv_args->is_path = is_path;

    return co_event(fs_init, uv_args);
}

static value_t uv_start(uv_args_t *uv_args, values_t *args, int type, size_t n_args, bool is_request) {
    uv_args->args = args;
    uv_args->type = CO_EVENT_ARG;
    uv_args->is_request = is_request;
    if (is_request)
        uv_args->req_type = type;
    else
        uv_args->handle_type = type;

    uv_args->n_args = n_args;

    return co_event(uv_init, uv_args);
}

static void close_cb(uv_handle_t *handle) {
    uv_args_t *uv = (uv_args_t *)uv_handle_get_data(handle);
    routine_t *co = uv->context;

    co->halt = true;
    co_result_set(co, NULL);
    co_resuming(co->context);
    co_scheduler();
}

static void write_cb(uv_write_t *req, int status) {
    uv_args_t *uv = (uv_args_t *)uv_req_get_data((uv_req_t *)req);
    routine_t *co = uv->context;
    if (status < 0) {
        fprintf(stderr, "Error: %s\n", uv_strerror(status));
    }

    co->halt = true;
    co_result_set(co, (status < 0 ? &status : 0));
    co_resuming(co->context);
    co_scheduler();
}

static void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    uv_args_t *uv = (uv_args_t *)uv_handle_get_data(handle);
    routine_t *co = uv->context;

    buf->base = (string)co_calloc_full(co, 1, suggested_size + 1, CO_FREE);
    buf->len = suggested_size;
}

static void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    uv_args_t *uv = (uv_args_t *)uv_handle_get_data((uv_handle_t *)stream);
    routine_t *co = uv->context;

    co->halt = true;
    if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "Error: %s\n", uv_strerror(nread));

        co_result_set(co, (nread == UV_EOF ? 0 : &nread));
    } else if (nread == UV_EAGAIN) {
        co_result_set(co, (void *)UV_EAGAIN);
    } else {
        co_result_set(co, (nread > 0 ? buf->base : NULL));
    }

    uv_read_stop(stream);
    co_resuming(co->context);
    co_scheduler();
}

static void fs_cb(uv_fs_t *req) {
    ssize_t result = uv_fs_get_result(req);
    uv_args_t *fs = (uv_args_t *)uv_req_get_data((uv_req_t *)req);
    routine_t *co = fs->context;
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
            case UV_FS_WRITE:
            case UV_FS_SENDFILE:
                break;
            case UV_FS_SCANDIR:
                break;
            case UV_FS_STATFS:
            case UV_FS_LSTAT:
            case UV_FS_STAT:
            case UV_FS_FSTAT:
                override = true;
                memcpy(fs->stat, &req->statbuf, sizeof(fs->stat));
                co_result_set(co, fs->stat);
                break;
            case UV_FS_READLINK:
                break;
            case UV_FS_READ:
                override = true;
                co_result_set(co, fs->buffer);
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
            case UV_FS_STATFS:
                result = uv_fs_statfs(loop, req, path, fs_cb);
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
                result = uv_fs_read(loop, req, fd, &fs->bufs, 1, offset, fs_cb);
                break;
            case UV_FS_WRITE:
                offset = var_long_long(args[1]);
                result = uv_fs_write(loop, req, fd, &fs->bufs, 1, offset, fs_cb);
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

static void_t uv_init(void_t uv_args) {
    uv_args_t *uv = (uv_args_t *)uv_args;
    values_t *args = uv->args;
    int result = -4083;

    uv_handle_t *stream = var_cast(uv_handle_t, args[0]);
    uv->context = co_active();
    if (uv->is_request) {
        uv_req_t *req;
        switch (uv->req_type) {
            case UV_WRITE:
                req = co_new_by(1, sizeof(uv_write_t));
                result = uv_write((uv_write_t *)req, (uv_stream_t *)stream, &uv->bufs, 1, write_cb);
                break;
            case UV_CONNECT:
            case UV_SHUTDOWN:
            case UV_UDP_SEND:
            case UV_WORK:
            case UV_GETADDRINFO:
            case UV_GETNAMEINFO:
            case UV_RANDOM:
                break;
            case UV_UNKNOWN_REQ:
            default:
                fprintf(stderr, "type; %d not supported.\n", uv->req_type);
                break;
        }

        uv_req_set_data(req, (void_t)uv);
    } else {
        uv_handle_set_data(stream, (void_t)uv);
        switch (uv->handle_type) {
            case UV_ASYNC:
            case UV_CHECK:
            case UV_FS_EVENT:
            case UV_FS_POLL:
            case UV_IDLE:
            case UV_NAMED_PIPE:
            case UV_POLL:
            case UV_PREPARE:
            case UV_PROCESS:
            case UV_STREAM:
                result = uv_read_start((uv_stream_t *)stream, alloc_cb, read_cb);
                break;
            case UV_TCP:
            case UV_HANDLE:
                result = 0;
                uv_close(stream, close_cb);
                break;
            case UV_TIMER:
            case UV_TTY:
            case UV_UDP:
            case UV_SIGNAL:
            case UV_FILE:
                break;
            case UV_UNKNOWN_HANDLE:
            default:
                fprintf(stderr, "type; %d not supported.\n", uv->handle_type);
                break;
        }
    }

    if (result) {
        fprintf(stderr, "failed: %s\n", uv_strerror(result));
        return CO_ERROR;
    }

    return 0;
}

uv_file fs_open(string_t path, int flags, int mode) {
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(3, sizeof(values_t));

    args[0].value.char_ptr = (string)path;
    args[1].value.integer = flags;
    args[2].value.integer = mode;

    return (uv_file)fs_start(uv_args, args, UV_FS_OPEN, 3, true).integer;
}

int fs_unlink(string_t path) {
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(1, sizeof(values_t));
    args[0].value.char_ptr = (string)path;

    return (uv_file)fs_start(uv_args, args, UV_FS_UNLINK, 1, true).integer;
}

uv_stat_t *fs_fstat(uv_file fd) {
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(1, sizeof(values_t));
    args[0].value.integer = fd;

    return (uv_stat_t *)fs_start(uv_args, args, UV_FS_FSTAT, 1, false).object;
}

int fs_fsync(uv_file fd) {
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(1, sizeof(values_t));
    args[0].value.integer = fd;

    return fs_start(uv_args, args, UV_FS_FSYNC, 1, false).integer;
}

string fs_read(uv_file fd, int64_t offset) {
    uv_stat_t *stat = fs_fstat(fd);
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    uv_args->buffer = co_new_by(1, stat->st_size + 1);
    uv_args->bufs = uv_buf_init(uv_args->buffer, stat->st_size);

    args = (values_t *)co_new_by(2, sizeof(values_t));
    args[0].value.integer = fd;
    args[1].value.u_int = offset;

    return fs_start(uv_args, args, UV_FS_READ, 2, false).char_ptr;
}

int fs_write(uv_file fd, string_t text, int64_t offset) {
    size_t size = sizeof(text) + 1;
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    uv_args->buffer = co_new_by(1, size);
    memcpy(uv_args->buffer, text, size);
    uv_args->bufs = uv_buf_init(uv_args->buffer, size);

    args = (values_t *)co_new_by(2, sizeof(values_t));
    args[0].value.integer = fd;
    args[1].value.u_int = offset;

    return fs_start(uv_args, args, UV_FS_WRITE, 2, false).integer;
}

int fs_close(uv_file fd) {
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(1, sizeof(values_t));

    args[0].value.integer = fd;
    return fs_start(uv_args, args, UV_FS_CLOSE, 1, false).integer;
}

string fs_readfile(string_t path) {
    uv_file fd = fs_open(path, O_RDONLY, 0);
    string file = fs_read(fd, 0);
    fs_close(fd);

    return file;
}

int stream_write(uv_stream_t *handle, string_t text) {
    size_t size = strlen(text) + 1;
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    uv_args->buffer = co_new_by(1, size);
    memcpy(uv_args->buffer, text, size);
    uv_args->bufs = uv_buf_init(uv_args->buffer, size);

    args = (values_t *)co_new_by(1, sizeof(values_t));
    args[0].value.object = handle;

    return uv_start(uv_args, args, UV_WRITE, 1, true).integer;
}

string stream_read(uv_stream_t *handle) {
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(1, sizeof(values_t));
    args[0].value.object = handle;

    return uv_start(uv_args, args, UV_STREAM, 1, false).char_ptr;
}

void coro_uv_close(uv_handle_t *handle) {
    values_t *args = NULL;
    uv_args_t *uv_args = NULL;

    uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
    args = (values_t *)co_new_by(1, sizeof(values_t));
    args[0].value.object = handle;

    uv_start(uv_args, args, UV_HANDLE, 1, false);
}

uv_pipe_t *pipe_create(bool is_ipc) {
    uv_pipe_t *pipe = (uv_pipe_t *)co_calloc_full(co_active(), 1, sizeof(uv_pipe_t), uv_close_free);
    int r = uv_pipe_init(co_loop(), pipe, (int)is_ipc);
    if (r) {
        co_panic(uv_strerror(r));
    }

    return pipe;
}

uv_tcp_t *tcp_create(void) {
    uv_tcp_t *tcp = (uv_tcp_t *)co_calloc_full(co_active(), 1, sizeof(uv_tcp_t), uv_close_free);
    int r = uv_tcp_init(co_loop(), tcp);
    if (r) {
        co_panic(uv_strerror(r));
    }

    return tcp;
}

uv_tty_t *tty_create(uv_file fd) {
    uv_tty_t *tty = (uv_tty_t *)co_calloc_full(co_active(), 1, sizeof(uv_tty_t), uv_close_free);
    int r = uv_tty_init(co_loop(), tty, fd, 0);
    if (r) {
        co_panic(uv_strerror(r));
    }

    return tty;
}
