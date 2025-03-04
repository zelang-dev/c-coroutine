#ifndef UV_ROUTINE_H
#define UV_ROUTINE_H

#include <ctype.h>
#include "uv_tls.h"
#include "parson.h"
#include <stdbool.h>

/* Public API qualifier. */
#ifndef C_API
#define C_API extern
#endif

/* Cast ~libuv~ `obj` to `uv_stream_t` ptr. */
#define streamer(obj) ((uv_stream_t *)obj)

/* Cast ~libuv~ `obj` to `uv_handle_t` ptr. */
#define handler(obj) ((uv_handle_t *)obj)

/* Cast ~libuv~ `obj` to `uv_req_t` ptr. */
#define requester(obj) ((uv_req_t *)obj)

#define var_int(arg) (arg).value.integer
#define var_long(arg) (arg).value.s_long
#define var_long_long(arg) (arg).value.long_long
#define var_unsigned_int(arg) (arg).value.u_int
#define var_unsigned_long(arg) (arg).value.u_long
#define var_size_t(arg) (arg).value.max_size
#define var_const_char(arg) (string_t)(arg).value.buffer
#define var_char(arg) (arg).value.schar
#define var_char_ptr(arg) (arg).value.char_ptr
#define var_bool(arg) (arg).value.boolean
#define var_float(arg) (arg).value.point
#define var_double(arg) (arg).value.precision
#define var_unsigned_char(arg) (arg).value.uchar
#define var_char_array(arg) (arg).value.array
#define var_unsigned_char_ptr(arg) (arg).value.uchar_ptr
#define var_signed_short(arg) (arg).value.s_short
#define var_unsigned_short(arg) (arg).value.u_short
#define var_ptr(arg) (arg).value.object
#define var_func(arg) (arg).value.func
#define var_cast(type, arg) (type *)(arg).value.object

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
typedef void (*spawn_cb)(int64_t status, int signal);
typedef void (*stdio_cb)(const char *buf);
typedef stdio_cb stdin_cb;
typedef stdio_cb stdout_cb;
typedef stdio_cb stderr_cb;

typedef struct {
    int type;
    void *data;
    int stdio_count;
    spawn_cb exiting_cb;
    uv_stdio_container_t stdio[3];
    uv_process_options_t options[1];
} spawn_options_t;

typedef struct {
    int type;
    bool is_detach;
    spawn_options_t *handle;
    uv_process_t process[1];
} spawn_t;

/**
*@param stdio fd
* -The convention stdio[0] points to `fd 0` for stdin, `fd 1` is used for stdout, and `fd 2` is stderr.
* -Note: On Windows file descriptors greater than 2 are available to the child process only if
*the child processes uses the MSVCRT runtime.
*
*@param flag specify how stdio `uv_stdio_flags` should be transmitted to the child process.
* -`UV_IGNORE`
* -`UV_CREATE_PIPE`
* -`UV_INHERIT_FD`
* -`UV_INHERIT_STREAM`
* -`UV_READABLE_PIPE`
* -`UV_WRITABLE_PIPE`
* -`UV_NONBLOCK_PIPE`
* -`UV_OVERLAPPED_PIPE`
*/
C_API uv_stdio_container_t *stdio_fd(int fd, int flags);

/**
*@param stdio streams
* -The convention stdio[0] points to `fd 0` for stdin, `fd 1` is used for stdout, and `fd 2` is stderr.
* -Note: On Windows file descriptors greater than 2 are available to the child process only if
*the child processes uses the MSVCRT runtime.
*
*@param flag specify how stdio `uv_stdio_flags` should be transmitted to the child process.
* -`UV_IGNORE`
* -`UV_CREATE_PIPE`
* -`UV_INHERIT_FD`
* -`UV_INHERIT_STREAM`
* -`UV_READABLE_PIPE`
* -`UV_WRITABLE_PIPE`
* -`UV_NONBLOCK_PIPE`
* -`UV_OVERLAPPED_PIPE`
*/
C_API uv_stdio_container_t *stdio_stream(void *handle, int flags);

/**
 * @param env Environment for the new process. Key=value, separated with semicolon like:
 * `"Key1=Value1;Key=Value2;Key3=Value3"`. If NULL the parents environment is used.
 *
 * @param cwd Current working directory for the subprocess.
 * @param flags  Various process flags that control how `uv_spawn()` behaves:
 * - On Windows this uses CreateProcess which concatenates the arguments into a string this can
 * cause some strange errors. See the UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS flag on uv_process_flags.
 * - `UV_PROCESS_SETUID`
 * - `UV_PROCESS_SETGID`
 * - `UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS`
 * - `UV_PROCESS_DETACHED`
 * - `UV_PROCESS_WINDOWS_HIDE`
 * - `UV_PROCESS_WINDOWS_HIDE_CONSOLE`
 * - `UV_PROCESS_WINDOWS_HIDE_GUI`
 *
 * @param uid options
 * @param gid options
 * Can change the child process’ user/group id. This happens only when the appropriate bits are set in the flags fields.
 * - Note:  This is not supported on Windows, uv_spawn() will fail and set the error to UV_ENOTSUP.
 *
 * @param no_of_stdio Number of `uv_stdio_container_t` for each stream or file descriptors to be passed to a child process. Use `stdio_stream()` or `stdio_fd()` functions to create.
 */
C_API spawn_options_t *spawn_opts(char *env, const char *cwd, int flags, uv_uid_t uid, uv_gid_t gid, int no_of_stdio, ...);

/**
 * Initializes the process handle and starts the process.
 * If the process is successfully spawned, this function will return `spawn_t`
 * handle. Otherwise, the negative error code corresponding to the reason it couldn’t
 * spawn is returned.
 *
 * Possible reasons for failing to spawn would include (but not be limited to) the
 * file to execute not existing, not having permissions to use the setuid or setgid
 * specified, or not having enough memory to allocate for the new process.
 *
 * @param command Program to be executed.
 * @param args Command line arguments, separate with comma like: `"arg1,arg2,arg3,..."`
 * @param options Use `spawn_opts()` function to produce `uv_stdio_container_t` and `uv_process_options_t` options.
 * If `NULL` defaults `stderr` of subprocess to parent.
 */
C_API spawn_t *spawn(const char *command, const char *args, spawn_options_t *options);
C_API int spawn_exit(spawn_t *, spawn_cb exit_func);
C_API int spawn_in(spawn_t *, stdin_cb std_func);
C_API int spawn_out(spawn_t *, stdout_cb std_func);
C_API int spawn_err(spawn_t *, stderr_cb std_func);
C_API int spawn_pid(spawn_t *child);
C_API int spawn_signal(spawn_t *, int sig);
C_API int spawn_detach(spawn_t *);
C_API uv_stream_t *ipc_in(spawn_t *in);
C_API uv_stream_t *ipc_out(spawn_t *out);
C_API uv_stream_t *ipc_err(spawn_t *err);

C_API void coro_uv_close(uv_handle_t *);
C_API void interrupt_cleanup(void *handle);
C_API void interrupt_notify(void *event);
C_API void interrupt_resume(uv_async_t *handle);

C_API char *fs_readfile(const char *path);
C_API int fs_write_file(const char *path, const char *text);
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

C_API uv_tcp_t *tls_tcp_create(void *extra);

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
#define UV_TLS UV_HANDLE + UV_STREAM + UV_POLL
#define UV_SERVER_LISTEN UV_STREAM + UV_NAMED_PIPE + UV_TCP + UV_UDP
#define UV_CTX UV_SERVER_LISTEN + UV_POLL

#ifdef _WIN32
#include <sys/utime.h>

#define SLASH '\\'
#define DIR_SEP	';'
#define IS_SLASH(c)	((c) == '/' || (c) == '\\')
#define IS_SLASH_P(c)	(*(c) == '/' || \
        (*(c) == '\\' && !IsDBCSLeadByte(*(c-1))))

/* COPY_ABS_PATH is 2 under Win32 because by chance both regular absolute paths
   in the file system and UNC paths need copying of two characters */
#define COPY_ABS_PATH(path) 2
#define IS_UNC_PATH(path, len) \
	(len >= 2 && IS_SLASH(path[0]) && IS_SLASH(path[1]))
#define IS_ABS_PATH(path, len) \
	(len >= 2 && (/* is local */isalpha(path[0]) && path[1] == ':' || /* is UNC */IS_SLASH(path[0]) && IS_SLASH(path[1])))

#else
#include <dirent.h>

#define SLASH '/'

#ifdef __riscos__
    #define DIR_SEP  ';'
#else
    #define DIR_SEP  ':'
#endif

#define IS_SLASH(c)	((c) == '/')
#define IS_SLASH_P(c)	(*(c) == '/')
#endif

#ifndef COPY_ABS_PATH
#define COPY_ABS_PATH(path) 0
#endif

#ifndef IS_ABS_PATH
#define IS_ABS_PATH(path, len) \
	(IS_SLASH(path[0]))
#endif

typedef struct fileinfo_s {
    const char *dirname;
    const char *base;
    const char *extension;
    const char *filename;
} fileinfo_t;

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
    double version;

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
C_API http_t *http_for(http_parser_type action, char *hostname, double protocol);

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
