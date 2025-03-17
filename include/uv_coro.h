#ifndef UV_CORO_H
#define UV_CORO_H

#ifndef CERTIFICATE
    #define CERTIFICATE "localhost"
#endif

#include "uv_tls.h"
#include "raii.h"

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

#if defined(_MSC_VER)
    #define S_IRUSR S_IREAD  /* read, user */
    #define S_IWUSR S_IWRITE /* write, user */
    #define S_IXUSR 0 /* execute, user */
    #define S_IRGRP 0 /* read, group */
    #define S_IWGRP 0 /* write, group */
    #define S_IXGRP 0 /* execute, group */
    #define S_IROTH 0 /* read, others */
    #define S_IWOTH 0 /* write, others */
    #define S_IXOTH 0 /* execute, others */
    #define S_IRWXU 0
    #define S_IRWXG 0
    #define S_IRWXO 0
#endif

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
C_API uv_stream_t *ipc_in(spawn_t *);
C_API uv_stream_t *ipc_out(spawn_t *);
C_API uv_stream_t *ipc_err(spawn_t *);

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

#define UV_TLS UV_HANDLE + UV_STREAM + UV_POLL
#define UV_SERVER_LISTEN UV_STREAM + UV_NAMED_PIPE + UV_TCP + UV_UDP
#define UV_CTX UV_SERVER_LISTEN + UV_POLL

typedef struct uv_args_s {
    raii_type type;
    bool is_path;
    bool is_request;
    int bind_type;
    uv_fs_type fs_type;
    uv_req_type req_type;
    uv_handle_type handle_type;
    uv_dirent_type_t dirent_type;
    uv_tty_mode_t tty_mode;
    uv_stdio_flags stdio_flag;
    uv_errno_t errno_code;

    /* total number of args in set */
    size_t n_args;

    /* allocated array of arguments */
    values_type *args;
    routine_t *context;

    string buffer;
    uv_buf_t bufs;
    uv_stat_t stat[1];
    uv_statfs_t statfs[1];
    evt_ctx_t ctx;
    char ip[32];
    struct sockaddr name[1];
    struct sockaddr_in in4[1];
    struct sockaddr_in6 in6[1];
} uv_args_t;

C_API uv_loop_t *ze_loop(void);

C_API value_t co_event(callable_t, void_t arg);
C_API value_t co_await(callable_t fn, void_t arg);
C_API void co_handler(func_t fn, void_t handle, func_t dtor);
C_API void co_process(func_t fn, void_t args);

/* Returns Cpu core count, library version, and OS system info from `uv_os_uname()`. */
C_API string_t ze_uname(void);

C_API uv_args_t *interrupt_listen_args(void);
C_API void interrupt_listen_set(uv_args_t);
C_API bool is_tls(uv_handle_t *);

#define c_int(data) co_value((data)).integer
#define c_long(data) co_value((data)).s_long
#define c_int64(data) co_value((data)).long_long
#define c_unsigned_int(data) co_value((data)).u_int
#define c_unsigned_long(data) co_value((data)).u_long
#define c_size_t(data) co_value((data)).max_size
#define c_const_char(data) co_value((data)).const_char
#define c_char(data) co_value((data)).schar
#define c_char_ptr(data) co_value((data)).char_ptr
#define c_bool(data) co_value((data)).boolean
#define c_float(data) co_value((data)).point
#define c_double(data) co_value((data)).precision
#define c_unsigned_char(data) co_value((data)).uchar
#define c_char_ptr_ptr(data) co_value((data)).array
#define c_unsigned_char_ptr(data) co_value((data)).uchar_ptr
#define c_short(data) co_value((data)).s_short
#define c_unsigned_short(data) co_value((data)).u_short
#define c_void_ptr(data) co_value((data)).object
#define c_callable(data) co_value((data)).func
#define c_cast(type, data) (type *)co_value((data)).object

#define c_integer(value) co_data((value)).integer
#define c_signed_long(value) co_data((value)).s_long
#define c_long_long(value) co_data((value)).long_long
#define c_unsigned_integer(value) co_data((value)).u_int
#define c_unsigned_long_int(value) co_data((value)).u_long
#define c_unsigned_long_long(value) co_data((value)).max_size
#define c_string(value) co_data((value)).const_char
#define c_signed_chars(value) co_data((value)).schar
#define c_const_chars(value) co_data((value)).char_ptr
#define c_boolean(value) co_data((value)).boolean
#define c_point(value) co_data((value)).point
#define c_precision(value) co_data((value)).precision
#define c_unsigned_chars(value) co_data((value)).uchar
#define c_chars_array(value) co_data((value)).array
#define c_unsigned_chars_ptr(value) co_data((value)).uchar_ptr
#define c_signed_shorts(value) co_data((value)).s_short
#define c_unsigned_shorts(value) co_data((value)).u_short
#define c_object(value) co_data((value)).object
#define c_func(value) co_data((value)).func
#define c_cast_of(type, value) (type *)co_data((value)).object

#ifdef __cplusplus
}
#endif

#endif
