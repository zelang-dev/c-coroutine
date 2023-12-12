#include "../include/coroutine.h"

static void_t fs_init(void_t);
static void_t uv_init(void_t);

thread_local uv_args_t *uv_server_args = NULL;

void uv_arguments_free(uv_args_t *uv_args) {
    CO_FREE(uv_args->args);
    CO_FREE(uv_args);
}

uv_args_t *uv_arguments(int count, bool auto_free) {
    uv_args_t *uv_args = NULL;
    values_t *params = NULL;
    if (auto_free) {
        uv_args = (uv_args_t *)co_new_by(1, sizeof(uv_args_t));
        params = (values_t *)co_new_by(count, sizeof(values_t));
    } else {
        uv_args = (uv_args_t *)try_calloc(1, sizeof(uv_args_t));
        params = (values_t *)try_calloc(count, sizeof(values_t));
    }

    uv_args->args = params;
    return uv_args;
}

void_t uv_error_post(routine_t *co, int r) {
    co->loop_erred = true;
    co->loop_code = r;
    co->status = CO_ERRED;
    fprintf(stderr, "failed: %s\n", uv_strerror(r));
    return NULL;
}

static void _close_cb(uv_handle_t *handle) {
    if (UV_UNKNOWN_HANDLE == handle->type)
        return;

    memset(handle, 0, sizeof(uv_handle_t));
    CO_FREE(handle);
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

    uv_tls_close((uv_tls_t *)h, (uv_tls_close_cb)CO_FREE);
}

void coroutine_event_cleanup(void_t t) {
    routine_t *co = (routine_t *)t;
    if (co->context->wait_group)
        hash_free(co->context->wait_group);

    if (uv_loop_alive(co_loop())) {
        if (uv_server_args && uv_server_args->bind_type == UV_TLS)
            uv_walk(co_loop(), (uv_walk_cb)tls_close_free, NULL);
        else
            uv_walk(co_loop(), (uv_walk_cb)uv_close_free, NULL);

        uv_run(co_loop(), UV_RUN_DEFAULT);

        uv_stop(co_loop());
        uv_run(co_loop(), UV_RUN_DEFAULT);
    }
}

static value_t fs_start(uv_args_t *uv_args, uv_fs_type fs_type, size_t n_args, bool is_path) {
    uv_args->type = CO_EVENT_ARG;
    uv_args->fs_type = fs_type;
    uv_args->n_args = n_args;
    uv_args->is_path = is_path;

    return co_event(fs_init, uv_args);
}

static value_t uv_start(uv_args_t *uv_args, int type, size_t n_args, bool is_request) {
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
    co_resuming(co->context);
    co_scheduler();
}

static void error_catch(void_t uv) {
    routine_t *co = ((uv_args_t *)uv)->context;

    if (!is_empty((void_t)co_recover())) {
        if (uv_loop_alive(co_loop()) && uv_server_args) {
            co->halt = true;
            co->loop_erred = true;
            co->status = CO_ERRED;
            scheduler_info_log = false;
            coroutine_event_cleanup(co);
            uv_server_args = NULL;
        }
    }
}

static void connect_cb(uv_connect_t *client, int status) {
    uv_args_t *uv = (uv_args_t *)uv_req_get_data((uv_req_t *)client);
    values_t *args = uv->args;
    routine_t *co = uv->context;

    if (status) {
        fprintf(stderr, "Error: %s\n", uv_strerror(status));
    }

    co->halt = true;
    if (status < 0) {
        co->results = &status;
        co->is_plain = true;
    } else {
        co_result_set(co, args[0].value.object);
    }

    co_resuming(co->context);
    co_scheduler();
}

void on_connect_handshake(uv_tls_t *tls, int status) {
    CO_ASSERT(tls->tcp_hdl->data == tls);
    connect_cb((uv_connect_t *)tls->data, status);
}

void on_connect(uv_connect_t *req, int status) {
    evt_ctx_t *ctx = (evt_ctx_t *)uv_req_get_data((uv_req_t *)req);
    uv_args_t *uv = (uv_args_t *)ctx->data;
    values_t *args = uv->args;
    routine_t *co = uv->context;
    uv_tcp_t *tcp = (uv_tcp_t *)req->handle;

    if (!status) {
        //free on uv_tls_close
        uv_tls_t *client = try_malloc(sizeof(*client));
        if (uv_tls_init(ctx, tcp, client) < 0) {
            CO_FREE(client);
            req->data = (void_t)uv;
            connect_cb(req, status);
            return;
        }

        CO_ASSERT(tcp->data == client);
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

    co->halt = true;
    scheduler_info_log = false;
    co->is_plain = true;
    if (0 == status) {
        co_result_set(co, STREAM(ut->tcp_hdl));
    } else {
        co->loop_code = status;
        co->results = NULL;
        co->is_plain = true;
    }

    co_resuming(co->context);
    co_scheduler();
}

static void connection_cb(uv_stream_t *server, int status) {
    uv_args_t *uv = NULL;
    void_t check = uv_handle_get_data((uv_handle_t *)server);
    if (is_type(check, CO_EVENT_ARG))
        uv = (uv_args_t *)check;
    else
        uv = (uv_args_t *)((evt_ctx_t *)check)->data;

    routine_t *co = uv->context;
    uv_loop_t *uvLoop = co_loop();
    void_t handle = NULL;
    int r = status;

    co->halt = true;
    if (status == 0) {
        if (uv->bind_type == UV_TLS) {
            handle = try_calloc(1, sizeof(uv_tcp_t));
            r = uv_tcp_init(uvLoop, (uv_tcp_t *)handle);
            if (!r && !(r = uv_accept(server, (uv_stream_t *)handle))) {
                uv_tls_t *client = try_malloc(sizeof(uv_tls_t)); //freed on uv_close callback
                if (!(r = uv_tls_init(&uv->ctx, handle, client))) {
                    co->halt = false;
                    r = uv_tls_accept(client, on_listen_handshake);
                    client->uv_args = (void_t)uv;
                } else {
                    CO_FREE(client);
                }
            }
        } else if (uv->bind_type == UV_TCP) {
            handle = CO_CALLOC(1, sizeof(uv_tcp_t));
            r = uv_tcp_init(uvLoop, (uv_tcp_t *)handle);
        } else if (uv->bind_type == UV_NAMED_PIPE) {
            handle = CO_CALLOC(1, sizeof(uv_pipe_t));
            r = uv_pipe_init(uvLoop, (uv_pipe_t *)handle, 0);
        }

        if (uv->bind_type != UV_TLS && !r)
            r = uv_accept(server, STREAM(handle));

        if (uv->bind_type != UV_TLS && !r) {
            co->is_address = true;
            co_result_set(co, STREAM(handle));
        }
    }

    if (r) {
        fprintf(stderr, "Error: %s\n", uv_strerror(r));
        if (!is_empty(handle))
            CO_FREE(handle);

        co->loop_code = r;
        co->results = &status;
        co->is_plain = true;
    }

    if (uv->bind_type != UV_TLS)
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
    co->is_plain = true;
    co->results = &status;
    co_resuming(co->context);
    co_scheduler();
}

static void tls_write_cb(uv_tls_t *tls, int status) {
    uv_args_t *uv = (uv_args_t *)tls->uv_args;
    routine_t *co = uv->context;
    if (status < 0) {
        fprintf(stderr, "Error: %s\n", uv_strerror(status));
    }

    co->halt = true;
    co->is_plain = true;
    co->results = &status;
    co_resuming(co->context);
    co_scheduler();
}

static void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    uv_args_t *uv = (uv_args_t *)uv_handle_get_data(handle);
    routine_t *co = uv->context;

    buf->base = (string)co_calloc_full(co, 1, suggested_size + 1, CO_FREE);
    buf->len = (unsigned int)suggested_size;
}

static void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    uv_args_t *uv = (uv_args_t *)uv_handle_get_data((uv_handle_t *)stream);
    routine_t *co = uv->context;

    co->halt = true;
    co->is_plain = true;
    if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "Error: %s\n", uv_strerror((int)nread));

        co->results = (nread == UV_EOF ? 0 : &nread);
        uv_read_stop(stream);
    } else {
        if (nread > 0) {
            co->is_address = true;
            co_result_set(co, buf->base);
        } else {
            co->results = NULL;
        }
    }

    co_resuming(co->context);
    co_scheduler();
}

static void tls_read_cb(uv_tls_t *strm, ssize_t nread, const uv_buf_t *buf) {
    uv_args_t *uv = (uv_args_t *)strm->uv_args;
    routine_t *co = uv->context;

    co->halt = true;
    co->is_plain = true;
    if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "Error: %s\n", uv_strerror((int)nread));

        co->results = (nread == UV_EOF ? 0 : &nread);
    } else {
        if (nread > 0) {
            co->is_address = true;
            co_result_set(co, buf->base);
        } else {
            co->results = NULL;
        }
    }

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
    uv_loop_t *uvLoop = co_loop();
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
                result = uv_fs_open(uvLoop, req, path, flags, mode, fs_cb);
                break;
            case UV_FS_UNLINK:
                result = uv_fs_unlink(uvLoop, req, path, fs_cb);
                break;
            case UV_FS_MKDIR:
                mode = var_int(args[1]);
                result = uv_fs_mkdir(uvLoop, req, path, mode, fs_cb);
                break;
            case UV_FS_RENAME:
                n_path = var_char_ptr(args[1]);
                result = uv_fs_rename(uvLoop, req, path, n_path, fs_cb);
                break;
            case UV_FS_CHMOD:
                mode = var_int(args[1]);
                result = uv_fs_chmod(uvLoop, req, path, mode, fs_cb);
                break;
            case UV_FS_UTIME:
                atime = var_double(args[1]);
                mtime = var_double(args[2]);
                result = uv_fs_utime(uvLoop, req, path, atime, mtime, fs_cb);
                break;
            case UV_FS_CHOWN:
                uid = var_unsigned_char(args[1]);
                gid = var_unsigned_char(args[2]);
                result = uv_fs_chown(uvLoop, req, path, uid, gid, fs_cb);
                break;
            case UV_FS_LINK:
                n_path = var_char_ptr(args[1]);
                result = uv_fs_link(uvLoop, req, path, n_path, fs_cb);
                break;
            case UV_FS_SYMLINK:
                n_path = var_char_ptr(args[1]);
                flags = var_int(args[2]);
                result = uv_fs_symlink(uvLoop, req, path, n_path, flags, fs_cb);
                break;
            case UV_FS_RMDIR:
                result = uv_fs_rmdir(uvLoop, req, path, fs_cb);
                break;
            case UV_FS_LSTAT:
                n_path = var_char_ptr(args[1]);
                result = uv_fs_lstat(uvLoop, req, path, fs_cb);
                break;
            case UV_FS_STAT:
                result = uv_fs_stat(uvLoop, req, path, fs_cb);
                break;
            case UV_FS_STATFS:
                result = uv_fs_statfs(uvLoop, req, path, fs_cb);
                break;
            case UV_FS_SCANDIR:
                flags = var_int(args[1]);
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
        uv_file fd = var_int(args[0]);
        switch (fs->fs_type) {
            case UV_FS_FSTAT:
                result = uv_fs_fstat(uvLoop, req, fd, fs_cb);
                break;
            case UV_FS_SENDFILE:
                in_fd = var_int(args[1]);
                offset = var_long_long(args[2]);
                length = var_size_t(args[3]);
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
                offset = var_long_long(args[1]);
                result = uv_fs_ftruncate(uvLoop, req, fd, offset, fs_cb);
                break;
            case UV_FS_FCHMOD:
                mode = var_int(args[1]);
                result = uv_fs_fchmod(uvLoop, req, fd, mode, fs_cb);
                break;
            case UV_FS_FUTIME:
                atime = var_double(args[1]);
                mtime = var_double(args[2]);
                result = uv_fs_futime(uvLoop, req, fd, atime, mtime, fs_cb);
                break;
            case UV_FS_FCHOWN:
                uid = var_unsigned_char(args[1]);
                gid = var_unsigned_char(args[2]);
                result = uv_fs_fchown(uvLoop, req, fd, uid, gid, fs_cb);
                break;
            case UV_FS_READ:
                offset = var_long_long(args[1]);
                result = uv_fs_read(uvLoop, req, fd, &fs->bufs, 1, offset, fs_cb);
                break;
            case UV_FS_WRITE:
                offset = var_long_long(args[1]);
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
        return uv_error_post(co_active(), result);
    }

    fs->context = co_active();
    uv_req_set_data((uv_req_t *)req, (void_t)fs);
    return 0;
}

static void_t uv_init(void_t uv_args) {
    uv_args_t *uv = (uv_args_t *)uv_args;
    values_t *args = uv->args;
    char ip[32];
    struct sockaddr name;
    int length;
    int result = -4083;

    uv_handle_t *stream = var_cast(uv_handle_t, args[0]);
    uv->context = co_active();
    if (uv->is_request) {
        uv_req_t *req;
        switch (uv->req_type) {
            case UV_WRITE:
                if (uv->bind_type == UV_TLS) {
                    ((uv_tls_t *)stream)->uv_args = uv;
                    result = uv_tls_write((uv_tls_t *)stream, &uv->bufs, tls_write_cb);
                } else {
                    req = co_new_by(1, sizeof(uv_write_t));
                    result = uv_write((uv_write_t *)req, STREAM(stream), &uv->bufs, 1, write_cb);
                }
                break;
            case UV_CONNECT:
                req = co_new_by(1, sizeof(uv_connect_t));
                int scheme = var_int(args[2]);
                switch (scheme) {
                    case UV_NAMED_PIPE:
                        uv->handle_type = UV_NAMED_PIPE;
                        uv_pipe_connect((uv_connect_t *)req, (uv_pipe_t *)stream, var_const_char_512(args[3]), connect_cb);
                        result = 0;
                        break;
                    case UV_TLS:
                        uv->bind_type = UV_TLS;
                        req->data = &uv->ctx;
                        result = uv_tcp_connect((uv_connect_t *)req, (uv_tcp_t *)stream, var_cast(const struct sockaddr, args[1]), on_connect);
                        break;
                    default:
                        uv->handle_type = UV_TCP;
                        result = uv_tcp_connect((uv_connect_t *)req, (uv_tcp_t *)stream, var_cast(const struct sockaddr, args[1]), connect_cb);
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
                result = uv_listen((uv_stream_t *)stream, var_int(args[1]), connection_cb);
                if (!result) {
                    memset(&name, -1, sizeof length);
                    length = sizeof name;
                    switch (uv->bind_type) {
                        case UV_NAMED_PIPE:
                            break;
                        case UV_UDP:
                            result = uv_udp_getsockname((const uv_udp_t *)stream, &name, &length);
                            break;
                        default:
                            result = uv_tcp_getsockname((const uv_tcp_t *)stream, &name, &length);
                            break;
                    }

                    if (is_str_in(var_char_ptr(args[3]), ":")) {
                        uv_ip6_name((const struct sockaddr_in6 *)&name, (char *)ip, sizeof ip);
                    } else if (is_str_in(var_char_ptr(args[3]), ".")) {
                        uv_ip4_name((const struct sockaddr_in *)&name, (char *)ip, sizeof ip);
                    }

                    if (is_empty(uv_server_args)) {
                        uv_server_args = uv;
                    }

                    printf("Listening to %s:%d for%s connections, %s.\n",
                           ip,
                           var_int(args[4]),
                           (uv->bind_type == UV_TLS ? " secure" : ""),
                           http_std_date(0));

                    scheduler_info_log = false;
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
        uv->context->context->loop_code = result;
        uv_error_post(uv->context, result);
        co_resuming(uv->context->context);
    }

    return 0;
}

uv_file fs_open(string_t path, int flags, int mode) {
    uv_args_t *uv_args = uv_arguments(3, true);
    uv_args->args[0].value.char_ptr = (string)path;
    uv_args->args[1].value.integer = flags;
    uv_args->args[2].value.integer = mode;

    return (uv_file)fs_start(uv_args, UV_FS_OPEN, 3, true).integer;
}

int fs_unlink(string_t path) {
    uv_args_t *uv_args = uv_arguments(1, true);
    uv_args->args[0].value.char_ptr = (string)path;

    return (uv_file)fs_start(uv_args, UV_FS_UNLINK, 1, true).integer;
}

uv_stat_t *fs_fstat(uv_file fd) {
    uv_args_t *uv_args = uv_arguments(1, true);
    uv_args->args[0].value.integer = fd;

    return (uv_stat_t *)fs_start(uv_args, UV_FS_FSTAT, 1, false).object;
}

int fs_fsync(uv_file fd) {
    uv_args_t *uv_args = uv_arguments(1, true);
    uv_args->args[0].value.integer = fd;

    return fs_start(uv_args, UV_FS_FSYNC, 1, false).integer;
}

string fs_read(uv_file fd, int64_t offset) {
    uv_stat_t *stat = fs_fstat(fd);
    uv_args_t *uv_args = uv_arguments(2, true);

    uv_args->buffer = co_new_by(1, (size_t)stat->st_size + 1);
    uv_args->bufs = uv_buf_init(uv_args->buffer, (unsigned int)stat->st_size);

    uv_args->args[0].value.integer = fd;
    uv_args->args[1].value.u_int = (unsigned int)offset;

    return fs_start(uv_args, UV_FS_READ, 2, false).char_ptr;
}

int fs_write(uv_file fd, string_t text, int64_t offset) {
    size_t size = sizeof(text) + 1;
    uv_args_t *uv_args = uv_arguments(2, true);

    uv_args->buffer = co_new_by(1, size);
    memcpy(uv_args->buffer, text, size);
    uv_args->bufs = uv_buf_init(uv_args->buffer, (unsigned int)size);

    uv_args->args[0].value.integer = fd;
    uv_args->args[1].value.u_int = (unsigned int)offset;

    return fs_start(uv_args, UV_FS_WRITE, 2, false).integer;
}

int fs_close(uv_file fd) {
    uv_args_t *uv_args = uv_arguments(1, true);
    uv_args->args[0].value.integer = fd;

    return fs_start(uv_args, UV_FS_CLOSE, 1, false).integer;
}

string fs_readfile(string_t path) {
    uv_file fd = fs_open(path, O_RDONLY, 0);
    if (fd) {
        string file = fs_read(fd, 0);
        fs_close(fd);

        return file;
    }

    return (string)0;
}

void stream_handler(void (*connected)(uv_stream_t *), uv_stream_t *client) {
    if (is_tls((uv_handle_t *)client))
        co_handler(FUNC_VOID(connected), client, tls_close_free);
    else
        co_handler(FUNC_VOID(connected), client, uv_close_free);
}

int stream_write(uv_stream_t *handle, string_t text) {
    if (is_empty(handle))
        return -1;

    size_t size = strlen(text) + 1;
    uv_args_t *uv_args = uv_arguments(1, false);
    uv_args->buffer = co_new_by(1, size);
    memcpy(uv_args->buffer, text, size);
    uv_args->bufs = uv_buf_init(uv_args->buffer, (unsigned int)size);
    uv_args->args[0].value.object = handle;

    if (is_tls((uv_handle_t *)handle))
        uv_args->bind_type = UV_TLS;

    return uv_start(uv_args, UV_WRITE, 1, true).integer;
}

string stream_read(uv_stream_t *handle) {
    if (is_empty(handle))
        return NULL;

    uv_args_t *uv_args = uv_arguments(1, true);
    uv_args->args[0].value.object = handle;

    if (is_tls((uv_handle_t *)handle))
        uv_args->bind_type = UV_TLS;

    return uv_start(uv_args, UV_STREAM, 1, false).char_ptr;
}

uv_stream_t *stream_connect(string_t address) {
    if (is_empty((void_t)address))
        return NULL;

    url_t *url = parse_url(address);
    if (is_empty(url))
        return NULL;

    return stream_connect_ex(url->uv_type, (string_t)url->host, url->port);
}

uv_stream_t *stream_connect_ex(uv_handle_type scheme, string_t address, int port) {
    uv_args_t *uv_args = uv_arguments(4, true);
    void_t addr_set = NULL;
    void_t handle = NULL;
    char name[UV_MAXHOSTNAMESIZE] = CERTIFICATE;
    char crt[UV_MAXHOSTNAMESIZE];
    char key[UV_MAXHOSTNAMESIZE];
    size_t len = sizeof(name);
    int r = 0;

    uv_args->type = CO_EVENT_ARG;
    if (is_str_in(address, ":")) {
        r = uv_ip6_addr(address, port, (struct sockaddr_in6 *)uv_args->in6);
        addr_set = uv_args->in6;
    } else if (is_str_in(address, ".")) {
        r = uv_ip4_addr(address, port, (struct sockaddr_in *)uv_args->in4);
        addr_set = uv_args->in4;
    }

    if (!r) {
        return uv_error_post(co_active(), r);
    }

    switch (scheme) {
        case UV_NAMED_PIPE:
            handle = pipe_create(false);
            break;
        case UV_UDP:
            handle = udp_create();
            r = uv_udp_connect(handle, (const struct sockaddr *)addr_set);
            if (r) {
                return uv_error_post(co_active(), r);
            }
            return STREAM(handle);
        case UV_TLS:
            if (is_str_eq(name, "localhost"))
                uv_os_gethostname(name, &len);

            r = snprintf(crt, sizeof(crt), "%s.crt", name);
            if (r == 0)
                CO_LOG("Invalid hostname");

            r = snprintf(key, sizeof(key), "%s.key", name);
            if (r == 0)
                CO_LOG("Invalid hostname");

            evt_ctx_init_ex(&uv_args->ctx, crt, key);
            evt_ctx_set_nio(&uv_args->ctx, NULL, uv_tls_writer);
            defer(evt_ctx_free, &uv_args->ctx);
            uv_args->bind_type = UV_TLS;
            handle = tls_tcp_create(&uv_args->ctx);
            break;
        default:
            handle = tcp_create();
            break;
    }

    uv_args->args[0].value.object = handle;
    uv_args->args[1].value.object = addr_set;
    uv_args->args[2].value.integer = scheme;
    uv_args->args[3].value.char_ptr = (string)address;

    return (uv_stream_t *)uv_start(uv_args, UV_CONNECT, 4, true).object;
}

uv_stream_t *stream_listen(uv_stream_t *stream, int backlog) {
    if (is_empty(stream))
        return NULL;

    uv_args_t *uv_args = NULL;
    void_t check = uv_handle_get_data((uv_handle_t *)stream);
    if (is_type(check, CO_EVENT_ARG)) {
        uv_args = (uv_args_t *)check;
    } else {
        uv_args = (uv_args_t *)((evt_ctx_t *)check)->data;
    }

    uv_args->args[0].value.object = stream;
    uv_args->args[1].value.integer = backlog;

    value_t handle = uv_start(uv_args, UV_SERVER_LISTEN, 5, false);
    if (is_empty(handle.object))
        return NULL;

    return (uv_stream_t *)handle.object;
}

uv_stream_t *stream_bind(string_t address, int flags) {
    if (is_empty((void_t)address))
        return NULL;

    url_t *url = parse_url(address);
    if (is_empty(url))
        return NULL;

    return stream_bind_ex(url->uv_type, (string_t)url->host, url->port, flags);
}

uv_stream_t *stream_bind_ex(uv_handle_type scheme, string_t address, int port, int flags) {
    void_t addr_set, handle;
    int r = 0;
    char name[UV_MAXHOSTNAMESIZE] = CERTIFICATE;
    char crt[UV_MAXHOSTNAMESIZE];
    char key[UV_MAXHOSTNAMESIZE];
    size_t len = sizeof(name);

    uv_args_t *uv_args = uv_arguments(5, true);
    uv_args->type = CO_EVENT_ARG;
    co_defer_recover(error_catch, uv_args);
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
                    CO_LOG("Invalid hostname");

                r = snprintf(key, sizeof(key), "%s.key", name);
                if (r == 0)
                    CO_LOG("Invalid hostname");

                evt_ctx_init_ex(&uv_args->ctx, crt, key);
                evt_ctx_set_nio(&uv_args->ctx, NULL, uv_tls_writer);
                defer(evt_ctx_free, &uv_args->ctx);
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
        return uv_error_post(co_active(), r);
    }

    uv_args->args[0].value.object = handle;
    uv_args->args[2].value.object = addr_set;
    uv_args->args[3].value.char_ptr = (string)address;
    uv_args->args[4].value.integer = port;

    uv_args->bind_type = scheme;
    if (scheme == UV_TLS)
        uv_args->ctx.data = (void_t)uv_args;
    else
        uv_handle_set_data((uv_handle_t *)handle, (void_t)uv_args);


    return STREAM(handle);
}

void coro_uv_close(uv_handle_t *handle) {
    if (is_empty(handle))
        return;

    uv_args_t *uv_args = uv_arguments(1, true);
    uv_args->args[0].value.object = handle;

    uv_start(uv_args, UV_HANDLE, 1, false);
}

uv_udp_t *udp_create() {
    uv_udp_t *udp = (uv_udp_t *)co_calloc_full(co_active(), 1, sizeof(uv_udp_t), uv_close_free);
    int r = uv_udp_init(co_loop(), udp);
    if (r) {
        return uv_error_post(co_active(), r);
    }

    return udp;
}

uv_pipe_t *pipe_create(bool is_ipc) {
    uv_pipe_t *pipe = (uv_pipe_t *)co_calloc_full(co_active(), 1, sizeof(uv_pipe_t), uv_close_free);
    int r = uv_pipe_init(co_loop(), pipe, (int)is_ipc);
    if (r) {
        return uv_error_post(co_active(), r);
    }

    return pipe;
}

uv_tcp_t *tcp_create() {
    uv_tcp_t *tcp = (uv_tcp_t *)co_calloc_full(co_active(), 1, sizeof(uv_tcp_t), uv_close_free);
    int r = uv_tcp_init(co_loop(), tcp);
    if (r) {
        return uv_error_post(co_active(), r);
    }

    return tcp;
}

uv_tty_t *tty_create(uv_file fd) {
    uv_tty_t *tty = (uv_tty_t *)co_calloc_full(co_active(), 1, sizeof(uv_tty_t), uv_close_free);
    int r = uv_tty_init(co_loop(), tty, fd, 0);
    if (r) {
        return uv_error_post(co_active(), r);
    }

    return tty;
}

uv_tcp_t *tls_tcp_create(void_t extra) {
    uv_tcp_t *tcp = (uv_tcp_t *)co_calloc_full(co_active(), 1, sizeof(uv_tcp_t), tls_close_free);
    tcp->data = extra;
    int r = uv_tcp_init(co_loop(), tcp);
    if (r) {
        return uv_error_post(co_active(), r);
    }

    return tcp;
}

static void spawn_free(spawn_t *child) {
    uv_handle_t *handle = (uv_handle_t *)&child->process;

    CO_FREE(child->handle);
    CO_FREE(child);

    if (uv_is_active(handle) || !uv_is_closing(handle))
        uv_close(handle, NULL);
}

static void exit_cb(uv_process_t *handle, int64_t exit_status, int term_signal) {
    spawn_t *child = (spawn_t *)uv_handle_get_data((uv_handle_t *)handle);
    routine_t *co = (routine_t *)child->handle->data;

    if (!is_empty(child->handle->exiting_cb)) {
        co->user_data = (void_t)child->handle->exiting_cb;
        co->loop_code = (signed int)exit_status;
        co->results = &term_signal;
    }

    co->halt = true;
    co_switch(co);
    co_scheduler();
}

static void_t stdio_handler(void_t uv_args) {
    uv_args_t *uv = (uv_args_t *)uv_args;
    values_t *args = uv->args;
    spawn_t *child = var_cast(spawn_t, args[0]);
    stdio_cb std = (stdio_cb)var_func(args[1]);
    uv_stream_t *io = var_cast(uv_stream_t, args[2]);
    routine_t *co = (routine_t *)child->handle->data;

    uv_arguments_free(uv);

    while (true) {
        if (co_terminated(co))
            break;

        string data = stream_read(io);
        if (!is_str_empty(data))
            std((string_t)data);
    }

    return NULL;
}

static void spawning(void_t uv_args) {
    uv_args_t *uv = (uv_args_t *)uv_args;
    values_t *args = uv->args;
    spawn_t *child = var_cast(spawn_t, args[0]);
    spawn_cb exiting_cb;
    routine_t *co = co_active();

    uv_handle_set_data((uv_handle_t *)&child->process, (void_t)child);
    co->loop_code = uv_spawn(co_loop(), child->process, child->handle->options);
    defer(spawn_free, child);
    if (!is_empty(child->handle->data))
        CO_FREE(child->handle->data);

    child->handle->data = (void_t)co;
    CO_FREE(var_ptr(args[1]));
    uv_arguments_free(uv);

    if (!co->loop_code) {
        while (true) {
            if (!co_terminated(co)) {
                coroutine_yield();
            } else {
                if (!is_empty(co->user_data)) {
                    exiting_cb = (spawn_cb)co->user_data;
                    exiting_cb(co->loop_code, co_value(co->results).integer);
                }

                break;
            }
        }
    } else {
        CO_INFO("Process launch failed with: %s\n", uv_strerror(co->loop_code));
    }
}

uv_stdio_container_t *stdio_fd(int fd, int flags) {
    uv_stdio_container_t *stdio = try_calloc(1, sizeof(uv_stdio_container_t));
    stdio->flags = flags;
    stdio->data.fd = fd;

    return stdio;
}

uv_stdio_container_t *stdio_stream(void_t handle, int flags) {
    uv_stdio_container_t *stdio = try_calloc(1, sizeof(uv_stdio_container_t));
    stdio->flags = flags;
    stdio->data.stream = (uv_stream_t *)handle;

    return stdio;
}

spawn_options_t *spawn_opts(string env, string_t cwd, int flags, uv_uid_t uid, uv_gid_t gid, int no_of_stdio, ...) {
    spawn_options_t *handle = try_calloc(1, sizeof(spawn_options_t));
    uv_stdio_container_t *p;
    va_list argp;

    handle->data = !is_empty(env) ? str_split(env, ";", NULL) : NULL;
    handle->exiting_cb = NULL;
    handle->options->env = handle->data;
    handle->options->cwd = cwd;
    handle->options->flags = flags;
    handle->options->exit_cb = (flags == UV_PROCESS_DETACHED) ? NULL : exit_cb;
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
        for (int i = 0; i < no_of_stdio; i++) {
            p = va_arg(argp, uv_stdio_container_t *);
            memcpy(&handle->stdio[i], p, sizeof(uv_stdio_container_t));
            CO_FREE(p);
        }
        va_end(argp);
    }

    handle->options->stdio = (uv_stdio_container_t *)&handle->stdio;
    handle->options->stdio_count = handle->stdio_count;

    return handle;
}

spawn_t *spawn(const char *command, const char *args, spawn_options_t *handle) {
    spawn_t *process = try_calloc(1, sizeof(spawn_t));
    int has_args = 3;

    if (is_empty(handle)) {
        handle = spawn_opts(NULL, NULL, 0, 0, 0, 0);
    }

    handle->options->file = command;
    if (is_empty((void_t)args))
        has_args = 2;

    string command_arg = str_concat_by(has_args, command, ",", args);
    string *command_args = str_split(command_arg, ",", NULL);
    CO_FREE(command_arg);
    handle->options->args = command_args;

    process->type = CO_PROCESS;
    process->handle = handle;
    process->is_detach = false;

    uv_args_t *uv_args = uv_arguments(2, false);
    uv_args->args[0].value.object = process;
    uv_args->args[1].value.object = command_args;
    co_process(spawning, uv_args);

    return process;
}

CO_FORCE_INLINE int spawn_signal(spawn_t *handle, int sig) {
    return uv_process_kill(&handle->process[0], sig);
}

CO_FORCE_INLINE void spawn_detach(spawn_t *child) {
    if (child->handle->options->flags == UV_PROCESS_DETACHED && !child->is_detach) {
        uv_unref((uv_handle_t *)&child->process);
        child->is_detach = true;
        --coroutine_count;
    }
}

CO_FORCE_INLINE int spawn_exit(spawn_t *child, spawn_cb exit_func) {
    child->handle->exiting_cb = exit_func;
    co_pause();

    return ((routine_t *)child->handle->data)->loop_code;
}

int spawn_in(spawn_t *child, stdin_cb std_func) {
    uv_args_t *uv_args = uv_arguments(3, false);

    uv_args->args[0].value.object = child;
    uv_args->args[1].value.func = (callable_t)std_func;
    uv_args->args[2].value.object = ipc_in(child);
    co_go(stdio_handler, uv_args);

    return ((routine_t *)child->handle->data)->loop_code;
}

int spawn_out(spawn_t *child, stdout_cb std_func) {
    uv_args_t *uv_args = uv_arguments(3, false);

    uv_args->args[0].value.object = child;
    uv_args->args[1].value.func = (callable_t)std_func;
    uv_args->args[2].value.object = ipc_out(child);
    co_go(stdio_handler, uv_args);

    return ((routine_t *)child->handle->data)->loop_code;
}

int spawn_err(spawn_t *child, stderr_cb std_func) {
    uv_args_t *uv_args = uv_arguments(3, false);

    uv_args->args[0].value.object = child;
    uv_args->args[1].value.func = (callable_t)std_func;
    uv_args->args[2].value.object = ipc_err(child);
    co_go(stdio_handler, uv_args);

    return ((routine_t *)child->handle->data)->loop_code;
}

CO_FORCE_INLINE int spawn_pid(spawn_t *child) {
    return child->process->pid;
}

CO_FORCE_INLINE uv_stream_t *ipc_in(spawn_t *in) {
    return in->handle->stdio[0].data.stream;
}

CO_FORCE_INLINE uv_stream_t *ipc_out(spawn_t *out) {
    return out->handle->stdio[1].data.stream;
}

CO_FORCE_INLINE uv_stream_t *ipc_err(spawn_t *err) {
    return err->handle->stdio[2].data.stream;
}
