#ifndef UV_ROUTINE_H
#define UV_ROUTINE_H

#include <ctype.h>
#include "uv.h"

/* Public API qualifier. */
#ifndef C_API
#define C_API extern
#endif

#define stream(handle) ((uv_stream_t *)handle)

#ifdef __cplusplus
extern "C"
{
#endif

typedef union
{
    uv_alloc_cb alloc;
    uv_read_cb read;
    uv_write_cb write;
    uv_connect_cb connect;
    uv_shutdown_cb shutdown;
    uv_connection_cb connection;
    uv_close_cb close;
    uv_poll_cb poll;
    uv_timer_cb timer;
    uv_async_cb async;
    uv_prepare_cb prepare;
    uv_check_cb _check;
    uv_idle_cb idle;
    uv_exit_cb exit;
    uv_walk_cb walk;
    uv_fs_cb fs;
    uv_work_cb work;
    uv_after_work_cb after_work;
    uv_getaddrinfo_cb getaddrinfo;
    uv_getnameinfo_cb getnameinfo;
    uv_random_cb randoms;
    uv_fs_poll_cb fs_poll;
    uv_fs_event_cb fs_event;
    uv_udp_recv_cb udp_recv;
    uv_udp_send_cb udp_send;
    uv_signal_cb signal;
} uv_routine_cb;

C_API void coro_uv_close(uv_handle_t *);

C_API char *fs_readfile(const char *path);
C_API uv_file fs_open(const char *path, int flags, int mode);
C_API int fs_close(uv_file fd);
C_API uv_stat_t *fs_fstat(uv_file fd);
C_API char *fs_read(uv_file fd, int64_t offset);
C_API int fs_write(uv_file fd, const char *text, int64_t offset);
C_API int fs_unlink(const char *path);
C_API int fs_fsync(uv_file file);
C_API int fs_fdatasync(uv_file file);
C_API int fs_ftruncate(uv_file file, int64_t offset);
C_API int fs_sendfile(uv_file out_fd, uv_file in_fd, int64_t in_offset, size_t length);
C_API int fs_fchmod(uv_file file, int mode);
C_API int fs_fchown(uv_file file, uv_uid_t uid, uv_gid_t gid);
C_API int fs_futime(uv_file file, double atime, double mtime);

C_API int fs_copyfile(const char *path, const char *new_path, int flags);
C_API int fs_mkdir(const char *path, int mode);
C_API int fs_mkdtemp(const char *tpl);
C_API int fs_mkstemp(const char *tpl);
C_API int fs_rmdir(const char *path);
C_API int fs_scandir(const char *path, int flags);
C_API int fs_opendir(const char *path);
C_API int fs_readdir(uv_dir_t *dir);
C_API int fs_closedir(uv_dir_t *dir);
C_API int fs_stat(const char *path);
C_API int fs_rename(const char *path, const char *new_path);
C_API int fs_access(const char *path, int mode);
C_API int fs_chmod(const char *path, int mode);
C_API int fs_utime(const char *path, double atime, double mtime);
C_API int fs_lutime(const char *path, double atime, double mtime);
C_API int fs_lstat(const char *path);
C_API int fs_link(const char *path, const char *new_path);
C_API int fs_symlink(const char *path, const char *new_path, int flags);
C_API int fs_readlink(const char *path);
C_API int fs_realpath(const char *path);
C_API int fs_chown(const char *path, uv_uid_t uid, uv_gid_t gid);
C_API int fs_lchown(const char *path, uv_uid_t uid, uv_gid_t gid);
C_API uv_statfs_t *fs_statfs(const char *path);

C_API void fs_poll_start(const char *path, int interval);
C_API void fs_event_start(const char *path, int flags);

C_API char *stream_read(uv_stream_t *);
C_API int stream_write(uv_stream_t *, const char *text);
C_API uv_stream_t *stream_connect(uv_handle_type scheme, const char *address, int port);
C_API uv_stream_t *stream_listen(uv_stream_t *, int backlog);
C_API uv_stream_t *stream_bind(uv_handle_type scheme, const char *address, int port_flag);
C_API void stream_handler(void (*connected)(uv_stream_t *), uv_stream_t *client);

C_API int stream_write2(uv_stream_t *, const char *text, uv_stream_t *send_handle);

C_API void stream_shutdown(uv_shutdown_t *, uv_stream_t *, uv_shutdown_cb cb);

C_API uv_tty_t *tty_create(uv_file fd);

C_API uv_udp_t *udp_create(void);

C_API uv_pipe_t *pipe_create(bool is_ipc);
C_API void pipe_connect(uv_pipe_t *, const char *name);

C_API uv_tcp_t *tcp_create(void);
C_API void tcp_connect(uv_tcp_t *handle, const struct sockaddr *addr);

C_API void coro_async_init(uv_loop_t *, uv_async_t *async, uv_async_cb callback);

/** @return int */
C_API void coro_poll_start(uv_poll_t *, int events, uv_poll_cb callback);

/** @return int */
C_API void coro_timer_start(uv_timer_t *timer, uv_timer_cb callback, uint64_t timeout, uint64_t repeat);

/** @return int */
C_API void coro_signal_start(uv_signal_t *, uv_signal_cb signal_cb, int signum);

/** @return int */
C_API void coro_signal_start_oneshot(uv_signal_t *, uv_signal_cb signal_cb, int signum);

/** @return int */
C_API void coro_spawn(uv_loop_t *, uv_process_t *, uv_process_options_t *options);

/** @return int */
C_API void coro_queue_work(uv_loop_t *, uv_work_t *, uv_work_cb work_cb, uv_after_work_cb after_work_cb);

/** @return int */
C_API void coro_prepare_start(uv_prepare_t *, uv_prepare_cb cb);

/** @return int */
C_API void coro_check_start(uv_check_t *check, uv_check_cb callback);

/** @return int */
C_API void coro_idle_start(uv_idle_t *idle, uv_idle_cb callback);

/** @return int */
C_API void coro_getaddrinfo(uv_loop_t *loop,
                          uv_getaddrinfo_t *req,
                          uv_getaddrinfo_cb getaddrinfo_cb,
                          const char *node,
                          const char *service,
                          const struct addrinfo *hints);

/** @return int */
C_API void coro_getnameinfo(uv_loop_t *loop,
                          uv_getnameinfo_t *req,
                          uv_getnameinfo_cb getnameinfo_cb,
                          const struct sockaddr *addr,
                          int flags);

/** @return int */
C_API void coro_udp_recv_start(uv_udp_t *, uv_alloc_cb alloc_cb, uv_udp_recv_cb recv_cb);

/** @return int */
C_API void coro_udp_send(uv_udp_send_t *req,
                       uv_udp_t *handle,
                       const uv_buf_t bufs[],
                       unsigned int nbufs,
                       const struct sockaddr *addr,
                       uv_udp_send_cb send_cb);

/** @return void */
C_API void coro_walk(uv_loop_t, uv_walk_cb walk_cb, void *arg);

/** @return int */
C_API void coro_thread_create(uv_thread_t *tid, uv_thread_cb entry, void *arg);

typedef struct parse_s {
    char *scheme;
    char *user;
    char *pass;
    char *host;
    unsigned short port;
    char *path;
    char *query;
    char *fragment;
} url_parse_t;

typedef enum {
    URL_SCHEME,
    URL_HOST,
    URL_PORT,
    URL_USER,
    URL_PASS,
    URL_PATH,
    URL_QUERY,
    URL_FRAGMENT,
} url_key;

typedef enum {
    /* Request Methods */
    HTTP_DELETE,
    HTTP_GET,
    HTTP_HEAD,
    HTTP_POST,
    HTTP_PUT,
    /* pathological */
    HTTP_CONNECT,
    HTTP_OPTIONS,
    HTTP_TRACE,
    /* webdav */
    HTTP_COPY,
    HTTP_LOCK,
    HTTP_MKCOL,
    HTTP_MOVE,
    HTTP_PROPFIND,
    HTTP_PROPPATCH,
    HTTP_SEARCH,
    HTTP_UNLOCK,
    /* subversion */
    HTTP_REPORT,
    HTTP_MKACTIVITY,
    HTTP_CHECKOUT,
    HTTP_MERGE,
    /* upnp */
    HTTP_MSEARCH,
    HTTP_NOTIFY,
    HTTP_SUBSCRIBE,
    HTTP_UNSUBSCRIBE,
    /* RFC-5789 */
    HTTP_PATCH,
    HTTP_PURGE
} http_method;

typedef enum {
    HTTP_REQUEST,
    HTTP_RESPONSE,
    HTTP_BOTH
} http_parser_type;

typedef enum {
    F_CHUNKED = 1 << 0,
    F_CONNECTION_KEEP_ALIVE = 1 << 1,
    F_CONNECTION_CLOSE = 1 << 2,
    F_TRAILING = 1 << 3,
    F_UPGRADE = 1 << 4,
    F_SKIP_BODY = 1 << 5
} http_flags;

/*
Parse a URL and return its components, return `NULL` for malformed URLs.

Modifed C code from PHP userland function
see https://php.net/manual/en/function.parse-url.php
*/
C_API url_parse_t *parse_url(char const *str);
C_API url_parse_t *url_parse_ex(char const *str, size_t length);
C_API url_parse_t *url_parse_ex2(char const *str, size_t length, bool *has_port);
C_API char *url_decode(char *str, size_t len);
C_API char *url_encode(char const *s, size_t len);

/*
Returns valid HTTP status codes reasons.

Verified 2020-05-22

see https://www.iana.org/assignments/http-status-codes/http-status-codes.xhtml
*/
C_API const char *url_status_str(uint16_t const status);

#ifdef __cplusplus
}
#endif

#endif
