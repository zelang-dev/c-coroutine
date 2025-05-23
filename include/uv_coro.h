#ifndef _UV_CORO_H
#define _UV_CORO_H

#define INTERRUPT_MODE UV_RUN_NOWAIT
#ifndef CERTIFICATE
    #define CERTIFICATE "localhost"
#endif

#include "uv_tls.h"
#include "url_http.h"
#include "reflection.h"

#ifdef _WIN32
#define INVALID_FD -EBADF
#else
#define INVALID_FD -EBADF
#endif

/* Cast ~libuv~ `obj` to `uv_stream_t` ptr. */
#define streamer(obj) ((uv_stream_t *)obj)

/* Cast ~libuv~ `obj` to `uv_handle_t` ptr. */
#define handler(obj) ((uv_handle_t *)obj)

/* Cast ~libuv~ `obj` to `uv_req_t` ptr. */
#define requester(obj) ((uv_req_t *)obj)

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

typedef enum {
    ai_family = RAII_COUNTER + 1,
    ai_socktype,
    ai_protocol,
    ai_flags,
    ai_unknown
} ai_hints_types;

typedef enum {
    UV_CORO_DNS = ai_unknown + 1,
    UV_CORO_NAME
} uv_coro_types;

typedef struct addrinfo addrinfo_t;
typedef const struct sockaddr sockaddr_t;
typedef struct sockaddr_in sock_in_t;
typedef struct sockaddr_in6 sock_in6_t;
typedef void (*spawn_cb)(int64_t status, int signal);
typedef void (*stdio_cb)(string_t buf);
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

typedef struct nameinfo_s {
    uv_coro_types type;
    string_t host;
    string_t service;
} nameinfo_t;

typedef void (*event_cb)(string_t filename, int events, int status);
typedef void (*poll_cb)(int status, const uv_stat_t *prev, const uv_stat_t *curr);

typedef struct scandir_s {
    bool started;
    size_t count;
    uv_fs_t *req;
    uv_dirent_t item[1];
} scandir_t;

typedef struct dnsinfo_s {
    uv_coro_types type;
    size_t count;
    string ip_addr, ip6_addr, ip_name;
    addrinfo_t *addr, original[1];
    nameinfo_t info[1];
    struct sockaddr name[1];
    struct sockaddr_in in4[1];
    struct sockaddr_in6 in6[1];
    char ip[INET6_ADDRSTRLEN + 1];
} dnsinfo_t;

typedef struct uv_args_s {
    raii_type type;
    int bind_type;
    bool is_path;
    bool is_request;
    bool is_freeable;
    bool is_timer;
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
    arrays_t args;
    routine_t *context;

    evt_ctx_t ctx;
    string buffer;
    uv_buf_t bufs;
    uv_stat_t stat[1];
    uv_statfs_t statfs[1];
    uv_fs_t req[1];
    scandir_t dir[1];
    dnsinfo_t dns[1];
} uv_args_t;

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
C_API spawn_options_t *spawn_opts(string env, string_t cwd, int flags, uv_uid_t uid, uv_gid_t gid, int no_of_stdio, ...);

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
C_API spawn_t *spawn(string_t command, string_t args, spawn_options_t *options);
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

C_API string fs_readfile(string_t path);
C_API int fs_writefile(string_t path, string_t text);
C_API uv_file fs_open(string_t path, int flags, int mode);
C_API int fs_close(uv_file fd);
C_API uv_stat_t *fs_fstat(uv_file fd);
C_API string fs_read(uv_file fd, int64_t offset);
C_API int fs_write(uv_file fd, string_t text, int64_t offset);
C_API int fs_unlink(string_t path);
C_API int fs_mkdir(string_t path, int mode);
C_API int fs_rmdir(string_t path);
C_API int fs_rename(string_t path, string_t new_path);
C_API int fs_fsync(uv_file file);
C_API scandir_t *fs_scandir(string_t path, int flags);
C_API uv_dirent_t *fs_scandir_next(scandir_t *dir);

C_API void fs_poll(string_t path, poll_cb pollfunc, int interval);
C_API void fs_watch(string_t, event_cb watchfunc);

C_API int fs_fdatasync(uv_file file);
C_API int fs_ftruncate(uv_file file, int64_t offset);
C_API int fs_sendfile(uv_file out_fd, uv_file in_fd, int64_t in_offset, size_t length);
C_API int fs_fchmod(uv_file file, int mode);
C_API int fs_fchown(uv_file file, uv_uid_t uid, uv_gid_t gid);
C_API int fs_futime(uv_file file, double atime, double mtime);

C_API int fs_copyfile(string_t path, string_t new_path, int flags);
C_API int fs_mkdtemp(string_t tpl);
C_API int fs_mkstemp(string_t tpl);
C_API int fs_stat(string_t path);
C_API int fs_access(string_t path, int mode);
C_API int fs_chmod(string_t path, int mode);
C_API int fs_utime(string_t path, double atime, double mtime);
C_API int fs_lutime(string_t path, double atime, double mtime);
C_API int fs_lstat(string_t path);
C_API int fs_link(string_t path, string_t new_path);
C_API int fs_symlink(string_t path, string_t new_path, int flags);
C_API int fs_readlink(string_t path);
C_API int fs_realpath(string_t path);
C_API int fs_chown(string_t path, uv_uid_t uid, uv_gid_t gid);
C_API int fs_lchown(string_t path, uv_uid_t uid, uv_gid_t gid);
C_API uv_statfs_t *fs_statfs(string_t path);

C_API string stream_read(uv_stream_t *);
C_API int stream_write(uv_stream_t *, string_t text);
C_API uv_stream_t *stream_connect(string_t address);
C_API uv_stream_t *stream_connect_ex(uv_handle_type scheme, string_t address, int port);
C_API uv_stream_t *stream_listen(uv_stream_t *, int backlog);
C_API uv_stream_t *stream_bind(string_t address, int flags);
C_API uv_stream_t *stream_bind_ex(uv_handle_type scheme, string_t address, int port, int flags);
C_API void stream_handler(void (*connected)(uv_stream_t *), uv_stream_t *client);

C_API int stream_write2(uv_stream_t *, string_t text, uv_stream_t *send_handle);
C_API void stream_shutdown(uv_shutdown_t *, uv_stream_t *, uv_shutdown_cb cb);

C_API uv_fs_poll_t *fs_poll_create(void);
C_API uv_fs_event_t *fs_event_create(void);
C_API uv_timer_t *time_create(void);
C_API uv_tty_t *tty_create(uv_file fd);
C_API uv_udp_t *udp_create(void);
C_API uv_pipe_t *pipe_create(bool is_ipc);
C_API uv_tcp_t *tcp_create(void);
C_API uv_tcp_t *tls_tcp_create(void_t extra);

C_API dnsinfo_t *get_addrinfo(string_t address, string_t service, u32 numhints_pair, ...);
C_API addrinfo_t *addrinfo_next(dnsinfo_t *);
C_API nameinfo_t *get_nameinfo(string_t addr, int port, int flags);

C_API string udp_recv(uv_udp_t *);
C_API int udp_send(uv_udp_t *, string_t, string_t addr);

#define UV_TLS                  UV_HANDLE + UV_STREAM + UV_POLL
#define UV_SERVER_LISTEN        UV_STREAM + UV_NAMED_PIPE + UV_TCP + UV_UDP
#define UV_CTX                  UV_SERVER_LISTEN + UV_POLL

#define foreach_in_dir(X, S)    uv_dirent_t *(X) = nil; \
    for(X = fs_scandir_next((scandir_t *)S); X != nullptr; X = fs_scandir_next((scandir_t *)S))
#define foreach_scandir(...)    foreach_xp(foreach_in_dir, (__VA_ARGS__))

#define foreach_in_info(X, S)   addrinfo_t *(X) = nil; \
    for (X = ((dnsinfo_t *)S)->original; X != nullptr; X = addrinfo_next((dnsinfo_t *)S))
#define foreach_addrinfo(...)   foreach_xp(foreach_in_info, (__VA_ARGS__))

C_API uv_loop_t *uv_coro_loop(void);

/* For displaying Cpu core count, library version, and OS system info from `uv_os_uname()`. */
C_API string_t uv_coro_uname(void);
C_API void uv_coro_close(uv_handle_t *);

C_API uv_args_t *uv_coro_data(void);
C_API void uv_coro_update(uv_args_t);
C_API bool is_tls(uv_handle_t *);

/* This library provides its own ~main~,
which call this function as an coroutine! */
C_API int uv_main(int, char **);

#ifdef __cplusplus
}
#endif

#endif /* _UV_CORO_H */
