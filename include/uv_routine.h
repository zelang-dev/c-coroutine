#ifndef UV_ROUTINE_H
#define UV_ROUTINE_H

#include <ctype.h>
#include "uv.h"
#include "compat/yyjson.h"

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

typedef struct sockaddr_in sock_in_t;
typedef struct sockaddr_in6 sock_in6_t;

C_API void coro_uv_close(uv_handle_t *);
C_API void uv_close_free(void *handle);

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
C_API uv_stream_t *stream_connect(const char *address);
C_API uv_stream_t *stream_connect_ex(uv_handle_type scheme, const char *address, int port);
C_API uv_stream_t *stream_listen(uv_stream_t *, int backlog);
C_API uv_stream_t *stream_bind(const char *address, int flags);
C_API uv_stream_t *stream_bind_ex(uv_handle_type scheme, const char *address, int port, int flags);
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

#define CRLF "\r\n"

typedef struct url_s {
    uv_handle_type uv_type;
    char *scheme;
    char *user;
    char *pass;
    char *host;
    unsigned short port;
    char *path;
    char *query;
    char *fragment;
} url_t;

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
    // Informational 1xx
    STATUS_CONTINUE = 100,
    STATUS_SWITCHING_PROTOCOLS = 101,
    STATUS_PROCESSING = 102,
    STATUS_EARLY_HINTS = 103,
    // Successful 2xx
    STATUS_OK = 200,
    STATUS_CREATED = 201,
    STATUS_ACCEPTED = 202,
    STATUS_NON_AUTHORITATIVE_INFORMATION = 203,
    STATUS_NO_CONTENT = 204,
    STATUS_RESET_CONTENT = 205,
    STATUS_PARTIAL_CONTENT = 206,
    STATUS_MULTI_STATUS = 207,
    STATUS_ALREADY_REPORTED = 208,
    STATUS_IM_USED = 226,
    // Redirection 3xx
    STATUS_MULTIPLE_CHOICES = 300,
    STATUS_MOVED_PERMANENTLY = 301,
    STATUS_FOUND = 302,
    STATUS_SEE_OTHER = 303,
    STATUS_NOT_MODIFIED = 304,
    STATUS_USE_PROXY = 305,
    STATUS_RESERVED = 306,
    STATUS_TEMPORARY_REDIRECT = 307,
    STATUS_PERMANENT_REDIRECT = 308,
    // Client Errors 4xx
    STATUS_BAD_REQUEST = 400,
    STATUS_UNAUTHORIZED = 401,
    STATUS_PAYMENT_REQUIRED = 402,
    STATUS_FORBIDDEN = 403,
    STATUS_NOT_FOUND = 404,
    STATUS_METHOD_NOT_ALLOWED = 405,
    STATUS_NOT_ACCEPTABLE = 406,
    STATUS_PROXY_AUTHENTICATION_REQUIRED = 407,
    STATUS_REQUEST_TIMEOUT = 408,
    STATUS_CONFLICT = 409,
    STATUS_GONE = 410,
    STATUS_LENGTH_REQUIRED = 411,
    STATUS_PRECONDITION_FAILED = 412,
    STATUS_PAYLOAD_TOO_LARGE = 413,
    STATUS_URI_TOO_LONG = 414,
    STATUS_UNSUPPORTED_MEDIA_TYPE = 415,
    STATUS_RANGE_NOT_SATISFIABLE = 416,
    STATUS_EXPECTATION_FAILED = 417,
    STATUS_IM_A_TEAPOT = 418,
    STATUS_MISDIRECTED_REQUEST = 421,
    STATUS_UNPROCESSABLE_ENTITY = 422,
    STATUS_LOCKED = 423,
    STATUS_FAILED_DEPENDENCY = 424,
    STATUS_TOO_EARLY = 425,
    STATUS_UPGRADE_REQUIRED = 426,
    STATUS_PRECONDITION_REQUIRED = 428,
    STATUS_TOO_MANY_REQUESTS = 429,
    STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
    STATUS_UNAVAILABLE_FOR_LEGAL_REASONS = 451,
    // Server Errors 5xx
    STATUS_INTERNAL_SERVER_ERROR = 500,
    STATUS_NOT_IMPLEMENTED = 501,
    STATUS_BAD_GATEWAY = 502,
    STATUS_SERVICE_UNAVAILABLE = 503,
    STATUS_GATEWAY_TIMEOUT = 504,
    STATUS_VERSION_NOT_SUPPORTED = 505,
    STATUS_VARIANT_ALSO_NEGOTIATES = 506,
    STATUS_INSUFFICIENT_STORAGE = 507,
    STATUS_LOOP_DETECTED = 508,
    STATUS_NOT_EXTENDED = 510,
    STATUS_NETWORK_AUTHENTICATION_REQUIRED = 511
} http_status;

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

typedef struct cookie_s {
    char *domain;
    char *lifetime;
    int expiry;
    bool httpOnly;
    int maxAge;
    char *name;
    char *path;
    bool secure;
    char *value;
    bool strict;
    char *sameSite;
} cookie_t;

typedef struct http_s {
    http_parser_type action;

    /* The current response status */
    http_status status;

    /* The cookie headers */
    cookie_t *cookies;

    /* The current response body */
    char *body;

    /* The unchanged data from server */
    char *raw;

    /* The current headers */
    void *headers;


    /* The protocol */
    char *protocol;

    /* The protocol version */
    float version;

    /* The requested status code */
    http_status code;

    /* The requested status message */
    char *message;

    /* The requested method */
    char *method;

    /* The requested path */
    char *path;

    /* The requested uri */
    char *uri;

    /* The request params */
    void *parameters;

    char *hostname;

    /* Response headers to send */
    void *header;
} http_t;

/*
Parse a URL and return its components, return `NULL` for malformed URLs.

Modifed C code from PHP userland function
see https://php.net/manual/en/function.parse-url.php
*/
C_API url_t *parse_url(char const *str);
C_API url_t *url_parse_ex(char const *str, size_t length);
C_API url_t *url_parse_ex2(char const *str, size_t length, bool *has_port);
C_API char *url_decode(char *str, size_t len);
C_API char *url_encode(char const *s, size_t len);

/*
Returns valid HTTP status codes reasons.

Verified 2020-05-22

see https://www.iana.org/assignments/http-status-codes/http-status-codes.xhtml
*/
C_API const char *http_status_str(uint16_t const status);

/* Return date string in standard format for http headers */
C_API char *http_std_date(time_t t);

/* Parse/prepare server headers, and store. */
C_API void parse_http(http_t *, char *headers);

/**
 * Returns `http_t` instance, for simple generic handling/constructing **http** request/response
 * messages, following the https://tools.ietf.org/html/rfc2616.html specs.
 *
 * - For use with `http_response()` and `http_request()`.
 *
 * - `action` either HTTP_RESPONSE or HTTP_REQUEST
 * - `hostname` for `Host:` header request,  will be ignored on `path/url` setting
 * - `protocol` version for `HTTP/` header
 */
C_API http_t *http_for(http_parser_type action, char *hostname, float protocol);

/**
 * Construct a new response string.
 *
 * - `body` defaults to `Not Found`, if `status` empty
 * - `status` defaults to `STATUS_NO_FOUND`, if `body` empty, otherwise `STATUS_OK`
 * - `type`
 * - `extras` additional headers - associative like "x-power-by: whatever" as `key=value;...`
 */
C_API char *http_response(http_t *, char *body, http_status status, char *type, char *extras);

/**
 * Construct a new request string.
 *
 * - `extras` additional headers - associative like "x-power-by: whatever" as `key=value;...`
 */
C_API char *http_request(http_t *,
                         http_method method,
                         char *path,
                         char *type,
                         char *connection,
                         char *body_data,
                         char *extras);

/**
 * Return a request header `content`.
 */
C_API char *get_header(http_t *, char *key);

/**
 * Return a request header content `variable` value.
 *
 * - `key` header to check for
 * - `var` variable to find
 */
C_API char *get_variable(http_t *, char *key, char *var);

/**
 * Return a request parameter `value`.
 */
C_API char *get_parameter(http_t *, char *key);

/**
 * Add or overwrite an response header parameter.
 */
C_API void put_header(http_t *, char *key, char *value, bool force_cap);

C_API bool has_header(http_t *, char *key);
C_API bool has_variable(http_t *, char *key, char *var);
C_API bool has_flag(http_t *, char *key, char *flag);
C_API bool has_parameter(http_t *, char *key);

#ifndef HTTP_AGENT
#define HTTP_AGENT "uv_client"
#endif

#ifndef HTTP_SERVER
#define HTTP_SERVER "uv_server"
#endif

#ifdef __cplusplus
}
#endif

#endif
