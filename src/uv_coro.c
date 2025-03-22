#include "uv_coro.h"
static char uv_coro_powered_by[SCRAPE_SIZE] = {0};
static void_t fs_init(params_t);
static void_t uv_init(params_t);

static void uv_arguments_free(uv_args_t *uv_args) {
    if (is_type(uv_args, RAII_INTERRUPT_ARG)) {
        if (!uv_args->is_freeable) {
            array_delete(uv_args->args);
            memset(uv_args, RAII_ERR, sizeof(raii_type));
            RAII_FREE(uv_args);
        }
    }
}

static uv_args_t *uv_arguments(int count, bool auto_free) {
    uv_args_t *uv_args = nullptr;
    arrays_t params = nullptr;
    if (auto_free) {
        uv_args = (uv_args_t *)calloc_local(1, sizeof(uv_args_t));
        params = arrays();
    } else {
        uv_args = (uv_args_t *)try_calloc(1, sizeof(uv_args_t));
        params =  array_of(get_scope(), 0);
    }

    uv_args->n_args = count;
    uv_args->args = params;
    uv_args->is_freeable = auto_free;
    uv_args->type = RAII_INTERRUPT_ARG;
    return uv_args;
}

static void _close_cb(uv_handle_t *handle) {
    if (UV_UNKNOWN_HANDLE == handle->type)
        return;

    memset(handle, 0, sizeof(uv_handle_t));
    RAII_FREE(handle);
}

void uv_close_free(void_t handle) {
    uv_handle_t *h = (uv_handle_t *)handle;
    if (UV_UNKNOWN_HANDLE == h->type) {
        return;
    }

    if (!uv_is_closing(h))
        uv_close(h, _close_cb);
}

void tls_close_free(void_t handle) {
    uv_handle_t *h = (uv_handle_t *)handle;
    if (UV_UNKNOWN_HANDLE == h->type) {
        return;
    }

    uv_tls_close((uv_tls_t *)h, (uv_tls_close_cb)RAII_FREE);
}

static value_t fs_start(uv_args_t *uv_args, uv_fs_type fs_type, size_t n_args, bool is_path) {
    uv_args->fs_type = fs_type;
    uv_args->n_args = n_args;
    uv_args->is_path = is_path;

    return coro_interrupt(fs_init, 1, uv_args);
}

static value_t uv_start(uv_args_t *uv_args, int type, size_t n_args, bool is_request) {
    uv_args->is_request = is_request;
    if (is_request)
        uv_args->req_type = type;
    else
        uv_args->handle_type = type;

    uv_args->n_args = n_args;
    return coro_interrupt(uv_init, 1, uv_args);
}

static RAII_INLINE void close_cb(uv_handle_t *handle) {
    uv_args_t *uv = (uv_args_t *)uv_handle_get_data(handle);
    coro_interrupt_complete(uv->context, nullptr, 0, false);
}

static void uv_coro_cleanup(void_t t) {
    routine_t *co = (routine_t *)t;
    uv_loop_t *uvLoop = ze_loop();
    coro_interrupt_waitgroup_destroy(co);
    if (uv_loop_alive(uvLoop)) {
        if (uv_coro_data() && uv_coro_data()->bind_type == UV_TLS)
            uv_walk(uvLoop, (uv_walk_cb)tls_close_free, nullptr);
        else
            uv_walk(uvLoop, (uv_walk_cb)uv_close_free, nullptr);

        uv_run(uvLoop, UV_RUN_DEFAULT);
    }
}

static void error_catch(void_t uv) {
    routine_t *co = ((uv_args_t *)uv)->context;
    string_t text = err_message();

    if (!is_empty((void_t)text)) {
        raii_is_caught(get_coro_scope(co), text);
        if (uv_loop_alive(ze_loop()) && uv_coro_data()) {
            coro_halt_set(co);
            coro_interrupt_erred(co, get_coro_err(co));
            uv_coro_cleanup(co);
            memset(uv_coro_data(), 0, sizeof(uv_args_t));
        }
    }
}

static void connect_cb(uv_connect_t *client, int status) {
    uv_args_t *uv = (uv_args_t *)uv_req_get_data(requester(client));
    arrays_t args = uv->args;
    routine_t *co = uv->context;
    bool is_plain = true;

    if (status)
        fprintf(stderr, "Error: %s\n", uv_strerror(status));
    else if (!status)
        is_plain = false;

    coro_interrupt_complete(co, (status < 0 ? nullptr : args[0].object), status, is_plain);
}

RAII_INLINE void on_connect_handshake(uv_tls_t *tls, int status) {
    RAII_ASSERT(tls->tcp_hdl->data == tls);
    connect_cb((uv_connect_t *)tls->data, status);
}

void on_connect(uv_connect_t *req, int status) {
    evt_ctx_t *ctx = (evt_ctx_t *)uv_req_get_data(requester(req));
    uv_args_t *uv = (uv_args_t *)ctx->data;
    routine_t *co = uv->context;
    uv_tcp_t *tcp = (uv_tcp_t *)req->handle;

    if (!status) {
        //free on uv_tls_close
        uv_tls_t *client = try_malloc(sizeof(*client));
        if (uv_tls_init(ctx, tcp, client) < 0) {
            RAII_FREE(client);
            req->data = (void_t)uv;
            connect_cb(req, status);
            return;
        }

        RAII_ASSERT(tcp->data == client);
        client->data = (void_t)req;
        client->uv_args = (void_t)uv;
        uv_tls_connect(client, on_connect_handshake);
    } else {
        req->data = (void_t)uv;
        connect_cb(req, status);
    }
}

static void on_listen_handshake(uv_tls_t *ut, int status) {
    uv_args_t *uv = (uv_args_t *)ut->uv_args;
    routine_t *co = uv->context;
    bool is_plain = true;

    if (0 == status)
        is_plain = false;
    else
        coro_err_set(co, status);

    coro_interrupt_complete(co, (!is_plain ? streamer(ut->tcp_hdl) : nullptr), -1, is_plain);
}

static void connection_cb(uv_stream_t *server, int status) {
    uv_args_t *uv = nullptr;
    void_t check = uv_handle_get_data((uv_handle_t *)server);
    if (is_type(check, RAII_INTERRUPT_ARG))
        uv = (uv_args_t *)check;
    else
        uv = (uv_args_t *)((evt_ctx_t *)check)->data;

    routine_t *co = uv->context;
    uv_loop_t *uvLoop = ze_loop();
    void_t handle = nullptr;
    int r = status;
    bool is_ready = false, halt = true;

    if (status == 0) {
        if (uv->bind_type == UV_TLS) {
            handle = try_calloc(1, sizeof(uv_tcp_t));
            r = uv_tcp_init(uvLoop, (uv_tcp_t *)handle);
            if (!r && !(r = uv_accept(server, (uv_stream_t *)handle))) {
                uv_tls_t *client = try_malloc(sizeof(uv_tls_t)); //freed on uv_close callback
                if (!(r = uv_tls_init(&uv->ctx, handle, client))) {
                    halt = false;
                    r = uv_tls_accept(client, on_listen_handshake);
                    client->uv_args = (void_t)uv;
                } else {
                    RAII_FREE(client);
                }
            }
        } else if (uv->bind_type == UV_TCP) {
            handle = RAII_CALLOC(1, sizeof(uv_tcp_t));
            r = uv_tcp_init(uvLoop, (uv_tcp_t *)handle);
        } else if (uv->bind_type == UV_NAMED_PIPE) {
            handle = RAII_CALLOC(1, sizeof(uv_pipe_t));
            r = uv_pipe_init(uvLoop, (uv_pipe_t *)handle, 0);
        }

        if ((uv->bind_type != UV_TLS) && !r)
            r = uv_accept(server, streamer(handle));

        if (uv->bind_type != UV_TLS && !r)
            is_ready = true;
    }

    if (r) {
        fprintf(stderr, "Error: %s\n", uv_strerror(r));
        if (!is_empty(handle))
            RAII_FREE(handle);

        coro_err_set(co, r);
    }

    coro_interrupt_finisher(co, (is_ready
                                 ? streamer(handle) : nullptr),
                                 status,
                            nullptr, nullptr, halt, (uv->bind_type != UV_TLS), r);
}

static void write_cb(uv_write_t *req, int status) {
    uv_args_t *uv = (uv_args_t *)uv_req_get_data(requester(req));
    struct routine_s *co = uv->context;
    if (status < 0) {
        fprintf(stderr, "Error: %s\n", uv_strerror(status));
    }

    coro_interrupt_complete(co, nullptr, status, true);
}

static void tls_write_cb(uv_tls_t *tls, int status) {
    uv_args_t *uv = (uv_args_t *)tls->uv_args;
    routine_t *co = uv->context;
    if (status < 0) {
        fprintf(stderr, "Error: %s\n", uv_strerror(status));
    }

    coro_interrupt_complete(co, nullptr, status, true);
}

static void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    uv_args_t *uv = (uv_args_t *)uv_handle_get_data(handle);
    routine_t *co = uv->context;

    buf->base = (string)calloc_full(get_coro_scope(co), 1, suggested_size + 1, RAII_FREE);
    buf->len = (unsigned int)suggested_size;
}

static void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    uv_args_t *uv = (uv_args_t *)uv_handle_get_data((uv_handle_t *)stream);
    routine_t *co = uv->context;
    bool is_plain = true;

    if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "Error: %s\n", uv_strerror((int)nread));

        uv_read_stop(stream);
    } else if (nread > 0) {
        is_plain = false;
    }

    coro_interrupt_complete(co, (!is_plain ? buf->base : nullptr), nread, is_plain);
}

static void tls_read_cb(uv_tls_t *strm, ssize_t nread, const uv_buf_t *buf) {
    uv_args_t *uv = (uv_args_t *)strm->uv_args;
    routine_t *co = uv->context;
    bool is_plain = true;

    if (nread < 0 && nread != UV_EOF)
        fprintf(stderr, "Error: %s\n", uv_strerror((int)nread));
    else if (nread > 0)
        is_plain = false;

    coro_interrupt_complete(co, (!is_plain ? buf->base : nullptr), nread, is_plain);
}


static void fs_cb(uv_fs_t *req) {
    ssize_t result = uv_fs_get_result(req);
    uv_args_t *fs = (uv_args_t *)uv_req_get_data(requester(req));
    routine_t *co = fs->context;
    void_t data = nullptr;
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
                data = fs->stat;
                break;
            case UV_FS_READLINK:
                break;
            case UV_FS_READ:
                override = true;
                data = fs->buffer;
                break;
            case UV_FS_UNKNOWN:
            case UV_FS_CUSTOM:
            default:
                fprintf(stderr, "type; %d not supported.\n", fs_type);
                break;
        }
    }

    coro_interrupt_finisher(co, data, uv_fs_get_result(req),
                            nullptr, nullptr, true, true, !override);
}

static RAII_INLINE void fs_cleanup(uv_fs_t *req) {
    uv_fs_req_cleanup(req);
    RAII_FREE(req);
}

static void_t fs_init(params_t uv_args) {
    uv_fs_t *req = calloc_full(coro_scope(), 1, sizeof(uv_fs_t), (func_t)fs_cleanup);
    uv_loop_t *uvLoop = ze_loop();
    uv_args_t *fs = uv_args->object;
    arrays_t args = fs->args;
    uv_uid_t uid, gid;
    uv_file in_fd;
    size_t length;
    int64_t offset;
    string_t n_path;
    double atime, mtime;
    int flags, mode;
    int result = -4058;

    if (fs->is_path) {
        string_t path = args[0].char_ptr;
        switch (fs->fs_type) {
            case UV_FS_OPEN:
                flags = args[1].integer;
                mode = args[2].integer;
                result = uv_fs_open(uvLoop, req, path, flags, mode, fs_cb);
                break;
            case UV_FS_UNLINK:
                result = uv_fs_unlink(uvLoop, req, path, fs_cb);
                break;
            case UV_FS_MKDIR:
                mode = args[1].integer;
                result = uv_fs_mkdir(uvLoop, req, path, mode, fs_cb);
                break;
            case UV_FS_RENAME:
                n_path = args[1].char_ptr;
                result = uv_fs_rename(uvLoop, req, path, n_path, fs_cb);
                break;
            case UV_FS_CHMOD:
                mode = args[1].integer;
                result = uv_fs_chmod(uvLoop, req, path, mode, fs_cb);
                break;
            case UV_FS_UTIME:
                atime = args[1].precision;
                mtime = args[2].precision;
                result = uv_fs_utime(uvLoop, req, path, atime, mtime, fs_cb);
                break;
            case UV_FS_CHOWN:
                uid = (uv_uid_t)args[1].uchar;
                gid = (uv_uid_t)args[2].uchar;
                result = uv_fs_chown(uvLoop, req, path, uid, gid, fs_cb);
                break;
            case UV_FS_LINK:
                n_path = (string_t)args[1].char_ptr;
                result = uv_fs_link(uvLoop, req, path, n_path, fs_cb);
                break;
            case UV_FS_SYMLINK:
                n_path = (string_t)args[1].char_ptr;
                flags = args[2].integer;
                result = uv_fs_symlink(uvLoop, req, path, n_path, flags, fs_cb);
                break;
            case UV_FS_RMDIR:
                result = uv_fs_rmdir(uvLoop, req, path, fs_cb);
                break;
            case UV_FS_LSTAT:
                n_path = (string_t)args[1].char_ptr;
                result = uv_fs_lstat(uvLoop, req, path, fs_cb);
                break;
            case UV_FS_STAT:
                result = uv_fs_stat(uvLoop, req, path, fs_cb);
                break;
            case UV_FS_STATFS:
                result = uv_fs_statfs(uvLoop, req, path, fs_cb);
                break;
            case UV_FS_SCANDIR:
                flags = args[1].integer;
                result = uv_fs_scandir(uvLoop, req, path, flags, fs_cb);
                break;
            case UV_FS_READLINK:
                result = uv_fs_readlink(uvLoop, req, path, fs_cb);
                break;
            case UV_FS_UNKNOWN:
            case UV_FS_CUSTOM:
            default:
                fprintf(stderr, "type; %d not supported.\n", fs->fs_type);
                break;
        }
    } else {
        uv_file fd = args[0].integer;
        switch (fs->fs_type) {
            case UV_FS_FSTAT:
                result = uv_fs_fstat(uvLoop, req, fd, fs_cb);
                break;
            case UV_FS_SENDFILE:
                in_fd = args[1].integer;
                offset = args[2].long_long;
                length = args[3].max_size;
                result = uv_fs_sendfile(uvLoop, req, fd, in_fd, offset, length, fs_cb);
                break;
            case UV_FS_CLOSE:
                result = uv_fs_close(uvLoop, req, fd, fs_cb);
                break;
            case UV_FS_FSYNC:
                result = uv_fs_fsync(uvLoop, req, fd, fs_cb);
                break;
            case UV_FS_FDATASYNC:
                result = uv_fs_fdatasync(uvLoop, req, fd, fs_cb);
                break;
            case UV_FS_FTRUNCATE:
                offset = args[1].long_long;
                result = uv_fs_ftruncate(uvLoop, req, fd, offset, fs_cb);
                break;
            case UV_FS_FCHMOD:
                mode = args[1].integer;
                result = uv_fs_fchmod(uvLoop, req, fd, mode, fs_cb);
                break;
            case UV_FS_FUTIME:
                atime = args[1].precision;
                mtime = args[2].precision;
                result = uv_fs_futime(uvLoop, req, fd, atime, mtime, fs_cb);
                break;
            case UV_FS_FCHOWN:
                uid = (uv_uid_t)args[1].uchar;
                gid = (uv_uid_t)args[2].uchar;
                result = uv_fs_fchown(uvLoop, req, fd, uid, gid, fs_cb);
                break;
            case UV_FS_READ:
                offset = args[1].long_long;
                result = uv_fs_read(uvLoop, req, fd, &fs->bufs, 1, offset, fs_cb);
                break;
            case UV_FS_WRITE:
                offset = args[1].long_long;
                result = uv_fs_write(uvLoop, req, fd, &fs->bufs, 1, offset, fs_cb);
                break;
            case UV_FS_UNKNOWN:
            case UV_FS_CUSTOM:
            default:
                fprintf(stderr, "type; %d not supported.\n", fs->fs_type);
                break;
        }
    }

    if (result) {
        RAII_FREE(req);
        return coro_interrupt_erred(coro_active(), result);
    }

    fs->context = coro_active();
    uv_req_set_data(requester(req), (void_t)fs);
    return 0;
}

static void_t uv_init(params_t uv_args) {
    uv_args_t *uv = uv_args->object;
    arrays_t args = uv->args;
    int length;
    int result = -4083;

    uv_handle_t *stream = handler(args[0].object);
    uv->context = coro_active();
    if (uv->is_request) {
        uv_req_t *req;
        switch (uv->req_type) {
            case UV_WRITE:
                if (uv->bind_type == UV_TLS) {
                    ((uv_tls_t *)stream)->uv_args = uv;
                    result = uv_tls_write((uv_tls_t *)stream, &uv->bufs, tls_write_cb);
                } else {
                    req = calloc_local(1, sizeof(uv_write_t));
                    result = uv_write((uv_write_t *)req, streamer(stream), &uv->bufs, 1, write_cb);
                }
                break;
            case UV_CONNECT:
                req = calloc_local(1, sizeof(uv_connect_t));
                int scheme = args[2].integer;
                switch (scheme) {
                    case UV_NAMED_PIPE:
                        uv->handle_type = UV_NAMED_PIPE;
                        uv_pipe_connect((uv_connect_t *)req, (uv_pipe_t *)stream, (string_t)args[3].char_ptr, connect_cb);
                        result = 0;
                        break;
                    case UV_TLS:
                        uv->bind_type = UV_TLS;
                        req->data = &uv->ctx;
                        result = uv_tcp_connect((uv_connect_t *)req, (uv_tcp_t *)stream, (const struct sockaddr *)args[1].object, on_connect);
                        break;
                    default:
                        uv->handle_type = UV_TCP;
                        result = uv_tcp_connect((uv_connect_t *)req, (uv_tcp_t *)stream, (const struct sockaddr *)args[1].object, connect_cb);
                        break;
                }
                break;
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

        if (uv->bind_type == UV_TLS)
            uv->ctx.uv_args = (void_t)uv;
        else
            uv_req_set_data(req, (void_t)uv);

    } else {
        if (uv->bind_type != UV_TLS)
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
                break;
            case UV_SERVER_LISTEN:
                result = uv_listen((uv_stream_t *)stream, args[1].integer, connection_cb);
                if (!result) {
                    length = sizeof uv->name;
                    switch (uv->bind_type) {
                        case UV_NAMED_PIPE:
                            break;
                        case UV_UDP:
                            result = uv_udp_getsockname((const uv_udp_t *)stream, uv->name, &length);
                            break;
                        default:
                            result = uv_tcp_getsockname((const uv_tcp_t *)stream, uv->name, &length);
                            break;
                    }

                    if (is_str_in(args[3].char_ptr, ":")) {
                        uv_ip6_name((const struct sockaddr_in6 *)uv->name, uv->ip, sizeof uv->ip);
                    } else if (is_str_in(args[3].char_ptr, ".")) {
                        uv_ip4_name((const struct sockaddr_in *)uv->name, uv->ip, sizeof uv->ip);
                    }

                    if (is_empty(uv_coro_data()))
                        uv_coro_update(*uv);

                    fprintf(stdout,"Listening to %s:%d for%s connections, %s.\n",
                           uv->ip,
                           args[4].integer,
                           (uv->bind_type == UV_TLS ? " secure" : ""),
                           http_std_date(0)
                    );
                }
                break;
            case UV_PROCESS:
            case UV_STREAM:
                if (uv->bind_type == UV_TLS) {
                    ((uv_tls_t *)stream)->uv_args = (void_t)uv;
                    result = uv_tls_read(((uv_tls_t *)stream), tls_read_cb);
                } else {
                    result = uv_read_start((uv_stream_t *)stream, alloc_cb, read_cb);
                }
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
        coro_err_set(get_coro_context(uv->context), result);
        coro_interrupt_erred(uv->context, result);
        coro_interrupt_switch(get_coro_context(uv->context));
    }

    return 0;
}

RAII_INLINE uv_file fs_open(string_t path, int flags, int mode) {
    uv_args_t *uv_args = uv_arguments(3, true);
    $append_string(uv_args->args, path);
    $append_signed(uv_args->args, flags);
    $append_signed(uv_args->args, mode);

    return (uv_file)fs_start(uv_args, UV_FS_OPEN, 3, true).integer;
}

RAII_INLINE int fs_unlink(string_t path) {
    uv_args_t *uv_args = uv_arguments(1, true);
    $append_string(uv_args->args, path);

    return (uv_file)fs_start(uv_args, UV_FS_UNLINK, 1, true).integer;
}

RAII_INLINE uv_stat_t *fs_fstat(uv_file fd) {
    uv_args_t *uv_args = uv_arguments(1, true);
    $append_signed(uv_args->args, fd);

    return (uv_stat_t *)fs_start(uv_args, UV_FS_FSTAT, 1, false).object;
}

RAII_INLINE int fs_fsync(uv_file fd) {
    uv_args_t *uv_args = uv_arguments(1, true);
    $append_signed(uv_args->args, fd);

    return fs_start(uv_args, UV_FS_FSYNC, 1, false).integer;
}

string fs_read(uv_file fd, int64_t offset) {
    uv_stat_t *stat = fs_fstat(fd);
    uv_args_t *uv_args = uv_arguments(2, true);

    uv_args->buffer = calloc_local(1, (size_t)stat->st_size + 1);
    uv_args->bufs = uv_buf_init(uv_args->buffer, (unsigned int)stat->st_size);

    $append_signed(uv_args->args, fd);
    $append_unsigned(uv_args->args, offset);

    return fs_start(uv_args, UV_FS_READ, 2, false).char_ptr;
}

int fs_write(uv_file fd, string_t text, int64_t offset) {
    size_t size = sizeof(text) + 1;
    uv_args_t *uv_args = uv_arguments(2, true);

    uv_args->buffer = calloc_local(1, size);
    memcpy(uv_args->buffer, text, size);
    uv_args->bufs = uv_buf_init(uv_args->buffer, (unsigned int)size);

    $append_signed(uv_args->args, fd);
    $append_unsigned(uv_args->args, offset);

    return fs_start(uv_args, UV_FS_WRITE, 2, false).integer;
}

int fs_close(uv_file fd) {
    uv_args_t *uv_args = uv_arguments(1, true);
    $append_signed(uv_args->args, fd);

    return fs_start(uv_args, UV_FS_CLOSE, 1, false).integer;
}

string fs_readfile(string_t path) {
    uv_file fd = fs_open(path, O_RDONLY, 0);
    if (fd) {
        string file = fs_read(fd, 0);
        fs_close(fd);

        return file;
    }

    return nullptr;
}

int fs_write_file(string_t path, string_t text) {
    int status;
    uv_file fd = fs_open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd) {
        status = fs_write(fd, text, 0);
        if (!fs_close(fd))
            return status;
    }

    return coro_err_code();
}

RAII_INLINE void stream_handler(void (*connected)(uv_stream_t *), uv_stream_t *client) {
    if (is_tls((uv_handle_t *)client))
        coro_interrupt_event((func_t)connected, client, tls_close_free);
    else
        coro_interrupt_event((func_t)connected, client, uv_close_free);
}

int stream_write(uv_stream_t *handle, string_t text) {
    if (is_empty(handle))
        return -1;

    size_t size = strlen(text) + 1;
    uv_args_t *uv_args = uv_arguments(1, false);
    uv_args->buffer = calloc_local(1, size);
    memcpy(uv_args->buffer, text, size);
    uv_args->bufs = uv_buf_init(uv_args->buffer, (unsigned int)size);
    $append(uv_args->args, handle);

    if (is_tls((uv_handle_t *)handle))
        uv_args->bind_type = UV_TLS;

    return uv_start(uv_args, UV_WRITE, 1, true).integer;
}

string stream_read(uv_stream_t *handle) {
    if (is_empty(handle))
        return nullptr;

    uv_args_t *uv_args = uv_arguments(1, true);
    $append(uv_args->args, handle);

    if (is_tls((uv_handle_t *)handle))
        uv_args->bind_type = UV_TLS;

    return uv_start(uv_args, UV_STREAM, 1, false).char_ptr;
}

uv_stream_t *stream_connect(string_t address) {
    if (is_empty((void_t)address))
        return nullptr;

    url_t *url = parse_url(address);
    if (is_empty(url))
        return nullptr;

    return stream_connect_ex(url->type, (string_t)url->host, url->port);
}

uv_stream_t *stream_connect_ex(uv_handle_type scheme, string_t address, int port) {
    uv_args_t *uv_args = uv_arguments(4, true);
    void_t addr_set = nullptr;
    void_t handle = nullptr;
    char name[UV_MAXHOSTNAMESIZE] = CERTIFICATE;
    char crt[UV_MAXHOSTNAMESIZE];
    char key[UV_MAXHOSTNAMESIZE];
    size_t len = sizeof(name);
    int r = 0;

    if (is_str_in(address, ":")) {
        r = uv_ip6_addr(address, port, (struct sockaddr_in6 *)uv_args->in6);
        addr_set = uv_args->in6;
    } else if (is_str_in(address, ".")) {
        r = uv_ip4_addr(address, port, (struct sockaddr_in *)uv_args->in4);
        addr_set = uv_args->in4;
    }

    if (!r) {
        return coro_interrupt_erred(coro_active(), r);
    }

    switch (scheme) {
        case UV_NAMED_PIPE:
            handle = pipe_create(false);
            break;
        case UV_UDP:
            handle = udp_create();
            r = uv_udp_connect(handle, (const struct sockaddr *)addr_set);
            if (r) {
                return coro_interrupt_erred(coro_active(), r);
            }
            return streamer(handle);
        case UV_TLS:
            if (is_str_eq(name, "localhost"))
                uv_os_gethostname(name, &len);

            r = snprintf(crt, sizeof(crt), "%s.crt", name);
            if (r == 0)
                RAII_LOG("Invalid hostname");

            r = snprintf(key, sizeof(key), "%s.key", name);
            if (r == 0)
                RAII_LOG("Invalid hostname");

            evt_ctx_init_ex(&uv_args->ctx, crt, key);
            evt_ctx_set_nio(&uv_args->ctx, nullptr, uv_tls_writer);
            defer((func_t)evt_ctx_free, &uv_args->ctx);
            uv_args->bind_type = UV_TLS;
            handle = tls_tcp_create(&uv_args->ctx);
            break;
        default:
            handle = tcp_create();
            break;
    }

    $append(uv_args->args, handle);
    $append(uv_args->args, addr_set);
    $append_signed(uv_args->args, scheme);
    $append_string(uv_args->args, address);

    return (uv_stream_t *)uv_start(uv_args, UV_CONNECT, 4, true).object;
}

uv_stream_t *stream_listen(uv_stream_t *stream, int backlog) {
    if (is_empty(stream))
        return nullptr;

    uv_args_t *uv_args = nullptr;
    void_t check = uv_handle_get_data((uv_handle_t *)stream);
    if (is_type(check, RAII_INTERRUPT_ARG)) {
        uv_args = (uv_args_t *)check;
    } else {
        uv_args = (uv_args_t *)((evt_ctx_t *)check)->data;
    }

    uv_args->args[0].object = stream;
    uv_args->args[1].integer = backlog;

    value_t handle = uv_start(uv_args, UV_SERVER_LISTEN, 5, false);
    if (is_empty(handle.object))
        return nullptr;

    return (uv_stream_t *)handle.object;
}

uv_stream_t *stream_bind(string_t address, int flags) {
    if (is_empty((void_t)address))
        return nullptr;

    url_t *url = parse_url(address);
    if (is_empty(url))
        return nullptr;

    return stream_bind_ex(url->type, (string_t)url->host, url->port, flags);
}

uv_stream_t *stream_bind_ex(uv_handle_type scheme, string_t address, int port, int flags) {
    void_t addr_set, handle;
    int r = 0;
    char name[UV_MAXHOSTNAMESIZE] = CERTIFICATE;
    char crt[UV_MAXHOSTNAMESIZE];
    char key[UV_MAXHOSTNAMESIZE];
    size_t len = sizeof(name);

    uv_args_t *uv_args = uv_arguments(5, true);
    defer_recover(error_catch, uv_args);
    if (is_str_in(address, ":")) {
        r = uv_ip6_addr(address, port, (struct sockaddr_in6 *)uv_args->in6);
        addr_set = uv_args->in6;
    } else if (is_str_in(address, ".")) {
        r = uv_ip4_addr(address, port, (struct sockaddr_in *)uv_args->in4);
        addr_set = uv_args->in4;
    }

    if (!r) {
        switch (scheme) {
            case UV_NAMED_PIPE:
                handle = pipe_create(false);
                r = uv_pipe_bind(handle, address);
                break;
            case UV_UDP:
                handle = udp_create();
                r = uv_udp_bind(handle, (const struct sockaddr *)addr_set, flags);
                break;
            case UV_TLS:
                if (is_str_eq(name, "localhost"))
                    uv_os_gethostname(name, &len);

                r = snprintf(crt, sizeof(crt), "%s.crt", name);
                if (r == 0)
                    RAII_LOG("Invalid hostname");

                r = snprintf(key, sizeof(key), "%s.key", name);
                if (r == 0)
                    RAII_LOG("Invalid hostname");

                evt_ctx_init_ex(&uv_args->ctx, crt, key);
                evt_ctx_set_nio(&uv_args->ctx, nullptr, uv_tls_writer);
                defer((func_t)evt_ctx_free, &uv_args->ctx);
                handle = tls_tcp_create(&uv_args->ctx);
                r = uv_tcp_bind(handle, (const struct sockaddr *)addr_set, flags);
                break;
            default:
                handle = tcp_create();
                r = uv_tcp_bind(handle, (const struct sockaddr *)addr_set, flags);
                break;
        }
    }

    if (r) {
        return coro_interrupt_erred(coro_active(), r);
    }

    $append(uv_args->args, handle);
    $append_signed(uv_args->args, flags);
    $append(uv_args->args, addr_set);
    $append_string(uv_args->args, address);
    $append_signed(uv_args->args, port);

    uv_args->bind_type = scheme;
    if (scheme == UV_TLS)
        uv_args->ctx.data = (void_t)uv_args;
    else
        uv_handle_set_data((uv_handle_t *)handle, (void_t)uv_args);


    return streamer(handle);
}

void coro_uv_close(uv_handle_t *handle) {
    if (is_empty(handle))
        return;

    uv_args_t *uv_args = uv_arguments(1, true);
    $append(uv_args->args, handle);

    uv_start(uv_args, UV_HANDLE, 1, false);
}

uv_udp_t *udp_create(void) {
    uv_udp_t *udp = (uv_udp_t *)calloc_full(coro_scope(), 1, sizeof(uv_udp_t), uv_close_free);
    int r = uv_udp_init(ze_loop(), udp);
    if (r) {
        return coro_interrupt_erred(coro_active(), r);
    }

    return udp;
}

uv_pipe_t *pipe_create(bool is_ipc) {
    uv_pipe_t *pipe = (uv_pipe_t *)calloc_full(coro_scope(), 1, sizeof(uv_pipe_t), uv_close_free);
    int r = uv_pipe_init(ze_loop(), pipe, (int)is_ipc);
    if (r) {
        return coro_interrupt_erred(coro_active(), r);
    }

    return pipe;
}

uv_tcp_t *tcp_create(void) {
    uv_tcp_t *tcp = (uv_tcp_t *)calloc_full(coro_scope(), 1, sizeof(uv_tcp_t), uv_close_free);
    int r = uv_tcp_init(ze_loop(), tcp);
    if (r) {
        return coro_interrupt_erred(coro_active(), r);
    }

    return tcp;
}

uv_tty_t *tty_create(uv_file fd) {
    uv_tty_t *tty = (uv_tty_t *)calloc_full(coro_scope(), 1, sizeof(uv_tty_t), uv_close_free);
    int r = uv_tty_init(ze_loop(), tty, fd, 0);
    if (r) {
        return coro_interrupt_erred(coro_active(), r);
    }

    return tty;
}

uv_tcp_t *tls_tcp_create(void_t extra) {
    uv_tcp_t *tcp = (uv_tcp_t *)calloc_full(coro_scope(), 1, sizeof(uv_tcp_t), tls_close_free);
    tcp->data = extra;
    int r = uv_tcp_init(ze_loop(), tcp);
    if (r) {
        return coro_interrupt_erred(coro_active(), r);
    }

    return tcp;
}

static void spawn_free(spawn_t *child) {
    uv_handle_t *handle = (uv_handle_t *)&child->process;

    RAII_FREE(child->handle);
    RAII_FREE(child);

    if (uv_is_active(handle) || !uv_is_closing(handle))
        uv_close(handle, nullptr);
}

static void exit_cb(uv_process_t *handle, int64_t exit_status, int term_signal) {
    spawn_t *child = (spawn_t *)uv_handle_get_data((uv_handle_t *)handle);
    routine_t *co = (routine_t *)child->handle->data;

    if (!is_empty(child->handle->exiting_cb)) {
        coro_data_set(co, (void_t)child->handle->exiting_cb);
        coro_err_set(co, exit_status);
    }

    coro_interrupt_complete(co, casting(term_signal), term_signal, true);
}

static void_t stdio_handler(params_t uv_args) {
    uv_args_t *uv = uv_args->object;
    arrays_t args = uv->args;
    spawn_t *child = args[0].object;
    stdio_cb std = (stdio_cb)args[1].func;
    uv_stream_t *io = args[2].object;
    routine_t *co = (routine_t *)child->handle->data;
    uv_arguments_free(uv);

    while (true) {
        if (coro_terminated(co))
            break;

        string data = stream_read(io);
        if (!is_str_empty(data))
            std((string_t)data);
    }

    return nullptr;
}

static void spawning(void_t uv_args) {
    uv_args_t *uv = ((params_t)uv_args)->object;
    arrays_t args = uv->args;
    spawn_t *child = args[0].object;
    spawn_cb exiting_cb;
    routine_t *co = coro_active();

    uv_handle_set_data((uv_handle_t *)&child->process, (void_t)child);
    coro_err_set(co, uv_spawn(ze_loop(), child->process, child->handle->options));
    defer((func_t)spawn_free, child);
    if (!is_empty(child->handle->data))
        RAII_FREE(child->handle->data);

    child->handle->data = (void_t)co;
    RAII_FREE(args[1].object);
    uv_arguments_free(uv);

    if (!get_coro_err(co)) {
        while (true) {
            if (!coro_terminated(co)) {
                coro_info(co, 1);
                yielding();
            } else {
                if (!is_empty(get_coro_data(co))) {
                    exiting_cb = (spawn_cb)get_coro_data(co);
                    exiting_cb(get_coro_err(co), get_coro_result(co)->integer);
                }

                break;
            }
        }
    } else {
        RAII_INFO("Process launch failed with: %s\n", uv_strerror(get_coro_err(co)));
    }
}

RAII_INLINE uv_stdio_container_t *stdio_fd(int fd, int flags) {
    uv_stdio_container_t *stdio = try_calloc(1, sizeof(uv_stdio_container_t));
    stdio->flags = flags;
    stdio->data.fd = fd;

    return stdio;
}

RAII_INLINE uv_stdio_container_t *stdio_stream(void_t handle, int flags) {
    uv_stdio_container_t *stdio = try_calloc(1, sizeof(uv_stdio_container_t));
    stdio->flags = flags;
    stdio->data.stream = (uv_stream_t *)handle;

    return stdio;
}

spawn_options_t *spawn_opts(string env, string_t cwd, int flags, uv_uid_t uid, uv_gid_t gid, int no_of_stdio, ...) {
    spawn_options_t *handle = try_calloc(1, sizeof(spawn_options_t));
    uv_stdio_container_t *p;
    va_list argp;
    int i;

    handle->data = !is_empty(env) ? str_split(env, ";", nullptr) : nullptr;
    handle->exiting_cb = nullptr;
    handle->options->env = handle->data;
    handle->options->cwd = cwd;
    handle->options->flags = flags;
    handle->options->exit_cb = (flags == UV_PROCESS_DETACHED) ? nullptr : exit_cb;
    handle->stdio_count = no_of_stdio;

#ifdef _WIN32
    handle->options->uid = 0;
    handle->options->gid = 0;
#else
    handle->options->uid = uid;
    handle->options->gid = gid;
#endif

    if (no_of_stdio > 0) {
        va_start(argp, no_of_stdio);
        for (i = 0; i < no_of_stdio; i++) {
            p = va_arg(argp, uv_stdio_container_t *);
            memcpy(&handle->stdio[i], p, sizeof(uv_stdio_container_t));
            RAII_FREE(p);
        }
        va_end(argp);
    }

    handle->options->stdio = (uv_stdio_container_t *)&handle->stdio;
    handle->options->stdio_count = handle->stdio_count;

    return handle;
}

spawn_t *spawn(string_t command, string_t args, spawn_options_t *handle) {
    spawn_t *process = try_calloc(1, sizeof(spawn_t));
    int has_args = 3;

    if (is_empty(handle)) {
        handle = spawn_opts(nullptr, nullptr, 0, 0, 0, 3,
                            stdio_fd(0, UV_IGNORE),
                            stdio_fd(1, UV_IGNORE),
                            stdio_fd(2, UV_INHERIT_FD));
    }

    handle->options->file = command;
    if (is_empty((void_t)args))
        has_args = 2;

    string command_arg = str_cat_ex(nullptr, has_args, command, ",", args);
    string *command_args = str_split_ex(nullptr, command_arg, ",", nullptr);
    RAII_FREE(command_arg);
    handle->options->args = command_args;

    process->handle = handle;
    process->is_detach = false;
    process->type = RAII_PROCESS;
    uv_args_t *uv_args = uv_arguments(2, false);

    $append(uv_args->args, process);
    $append(uv_args->args, command_args);
    coro_interrupt_event(spawning, uv_args, nullptr);

    return process;
}

RAII_INLINE int spawn_signal(spawn_t *handle, int sig) {
    return uv_process_kill(&handle->process[0], sig);
}

int spawn_detach(spawn_t *child) {
    if (child->handle->options->flags == UV_PROCESS_DETACHED && !child->is_detach) {
        uv_unref((uv_handle_t *)&child->process);
        child->is_detach = true;
        // sched_dec();
        yielding();
    }

    return get_coro_err((routine_t *)child->handle->data);
}

RAII_INLINE int spawn_exit(spawn_t *child, spawn_cb exit_func) {
    child->handle->exiting_cb = exit_func;
    yielding();

    return get_coro_err((routine_t *)child->handle->data);
}

int spawn_in(spawn_t *child, stdin_cb std_func) {
    uv_args_t *uv_args = uv_arguments(3, false);

    $append(uv_args->args, child);
    $append_func(uv_args->args, std_func);
    $append(uv_args->args, ipc_in(child));
    go(stdio_handler, 1, uv_args);

    return get_coro_err((routine_t *)child->handle->data);
}

int spawn_out(spawn_t *child, stdout_cb std_func) {
    uv_args_t *uv_args = uv_arguments(3, false);

    $append(uv_args->args, child);
    $append_func(uv_args->args, std_func);
    $append(uv_args->args, ipc_out(child));
    go(stdio_handler, 1, uv_args);

    return get_coro_err((routine_t *)child->handle->data);
}

int spawn_err(spawn_t *child, stderr_cb std_func) {
    uv_args_t *uv_args = uv_arguments(3, false);

    $append(uv_args->args, child);
    $append_func(uv_args->args, std_func);
    $append(uv_args->args, ipc_err(child));
    go(stdio_handler, 1, uv_args);

    return get_coro_err((routine_t *)child->handle->data);
}

RAII_INLINE int spawn_pid(spawn_t *child) {
    return child->process->pid;
}

RAII_INLINE uv_stream_t *ipc_in(spawn_t *_in) {
    return _in->handle->stdio[0].data.stream;
}

RAII_INLINE uv_stream_t *ipc_out(spawn_t *out) {
    return out->handle->stdio[1].data.stream;
}

RAII_INLINE uv_stream_t *ipc_err(spawn_t *err) {
    return err->handle->stdio[2].data.stream;
}

RAII_INLINE bool is_tls(uv_handle_t *self) {
    return is_type(self, UV_TLS);
}

string_t ze_uname(void) {
    if (is_str_empty((string_t)uv_coro_powered_by)) {
        char scrape[SCRAPE_SIZE];
        uv_utsname_t buffer[1];
        uv_os_uname(buffer);
        string_t powered_by = str_cat_ex(nullptr, 8, "Beta - ",
                                            simd_itoa(thrd_cpu_count(), scrape), " cores, ",
                                            buffer->sysname, " ",
                                            buffer->machine, " ",
                                            buffer->release);

        str_copy(uv_coro_powered_by, powered_by, SCRAPE_SIZE);
        RAII_FREE((void_t)powered_by);
    }

    return (string_t)uv_coro_powered_by;
}

RAII_INLINE uv_loop_t *ze_loop(void) {
    if (!is_empty(interrupt_handle()))
        return (uv_loop_t *)interrupt_handle();

    return uv_default_loop();
}

RAII_INLINE uv_args_t *uv_coro_data(void) {
    return (uv_args_t *)interrupt_data();
}

RAII_INLINE void uv_coro_update(uv_args_t uv_args) {
    set_interrupt_data(&uv_args);
}

static void uv_coro_shutdown(void_t t) {
    if (atomic_flag_load(&gq_result.is_errorless))
        uv_coro_cleanup(t);

    if (interrupt_handle()) {
        uv_loop_t *handle = interrupt_handle();
        if (atomic_flag_load(&gq_result.is_errorless)) {
            uv_stop(handle);
            uv_run(handle, UV_RUN_DEFAULT);
        }

        uv_loop_close(handle);
        RAII_FREE((void_t)handle);
        set_interrupt_handle(nullptr);
    }
}

static void create_loop(void) {
    uv_loop_t *handle = try_calloc(1, sizeof(uv_loop_t));
    if (uv_loop_init(handle)) {
        fprintf(stderr, "Event loop creation failed in file %s at line # %d", __FILE__, __LINE__);
        handle = nullptr;
    }

    set_interrupt_handle(handle);
}

main(int argc, char **argv) {
    uv_replace_allocator(rp_malloc, rp_realloc, rp_calloc, rpfree);
    RAII_INFO("%s\n\n", ze_uname());
    coro_interrupt_setup((call_interrupter_t)uv_run, create_loop, uv_coro_shutdown);
    return coro_start((coro_sys_func)uv_main, argc, argv, 0);
}
