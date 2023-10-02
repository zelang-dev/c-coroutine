#ifndef UV_ROUTINE_H
#define UV_ROUTINE_H

#include "uv.h"

/* Public API qualifier. */
#ifndef C_API
#define C_API extern
#endif

#define STR_LEN(str) (str), (sizeof(str) - 1)
#define UV_BUF_STATIC(lit) uv_buf_init((char *)STR_LEN(lit))

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
} uv_routine_cb;

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

C_API int co_close(uv_handle_t *);

/** @return int */
C_API void co_write(uv_write_t *, uv_stream_t *, uv_buf_t, int, uv_write_cb callback);

/** @return int */
C_API void co_write2(uv_write_t *, uv_stream_t *, uv_buf_t, int, uv_stream_t *send_, uv_write_cb cb);

C_API void co_read_start(uv_stream_t *, uv_alloc_cb alloc_cb, uv_read_cb read_cb);

C_API void co_shutdown(uv_shutdown_t *, uv_stream_t *, uv_shutdown_cb cb);

C_API void co_async_init(uv_loop_t *, uv_async_t *async, uv_async_cb callback);

/** @return int */
C_API void co_poll_start(uv_poll_t *, int events, uv_poll_cb callback);

/** @return int */
C_API void co_timer_start(uv_timer_t *timer, uv_timer_cb callback, uint64_t timeout, uint64_t repeat);



/** @return int */
C_API void fs_copyfile(uv_loop_t *, uv_fs_t *, const char path, const char new_path, int flags, uv_fs_cb cb);

/** @return int */
C_API void fs_mkdir(uv_loop_t *, uv_fs_t *, const char path, int mode, uv_fs_cb cb);

/** @return int */
C_API void fs_mkdtemp(uv_loop_t *, uv_fs_t *, const char tpl, uv_fs_cb cb);

/** @return int */
C_API void fs_mkstemp(uv_loop_t *, uv_fs_t *, const char tpl, uv_fs_cb cb);

/** @return int */
C_API void fs_rmdir(uv_loop_t *, uv_fs_t *, const char path, uv_fs_cb cb);

/** @return int */
C_API void fs_scandir(uv_loop_t *, uv_fs_t *, const char path, int flags, uv_fs_cb cb);

/** @return int */
C_API void fs_opendir(uv_loop_t *, uv_fs_t *, const char path, uv_fs_cb cb);

/** @return int */
C_API void fs_readdir(uv_loop_t *, uv_fs_t *, uv_dir_t *dir, uv_fs_cb cb);

/** @return int */
C_API void fs_closedir(uv_loop_t *, uv_fs_t *, uv_dir_t *dir, uv_fs_cb cb);

/** @return int */
C_API void fs_stat(uv_loop_t *, uv_fs_t *, const char path, uv_fs_cb cb);

/** @return int */
C_API void fs_rename(uv_loop_t *, uv_fs_t *, const char path, const char new_path, uv_fs_cb cb);

/** @return int */
C_API void fs_access(uv_loop_t *, uv_fs_t *, const char path, int mode, uv_fs_cb cb);

/** @return int */
C_API void fs_chmod(uv_loop_t *, uv_fs_t *, const char path, int mode, uv_fs_cb cb);

/** @return int */
C_API void fs_utime(uv_loop_t *, uv_fs_t *, const char path, double atime, double mtime, uv_fs_cb cb);

/** @return int */
C_API void fs_lutime(uv_loop_t *, uv_fs_t *, const char path, double atime, double mtime, uv_fs_cb cb);

/** @return int */
C_API void fs_lstat(uv_loop_t *, uv_fs_t *, const char path, uv_fs_cb cb);

/** @return int */
C_API void fs_link(uv_loop_t *, uv_fs_t *, const char path, const char new_path, uv_fs_cb cb);

/** @return int */
C_API void fs_symlink(uv_loop_t *, uv_fs_t *, const char path, const char new_path, int flags, uv_fs_cb cb);

/** @return int */
C_API void fs_readlink(uv_loop_t *, uv_fs_t *, const char path, uv_fs_cb cb);

/** @return int */
C_API void fs_realpath(uv_loop_t *, uv_fs_t *, const char path, uv_fs_cb cb);

/** @return int */
C_API void fs_chown(uv_loop_t *, uv_fs_t *, const char path, uv_uid_t uid, uv_gid_t gid, uv_fs_cb cb);

/** @return int */
C_API void fs_lchown(uv_loop_t *, uv_fs_t *, const char path, uv_uid_t uid, uv_gid_t gid, uv_fs_cb cb);

/** @return int */
C_API void fs_statfs(uv_loop_t *, uv_fs_t *, const char path, uv_fs_cb cb);

/** @return int */
C_API void fs_poll_start(uv_fs_poll_t *, uv_fs_poll_cb poll_cb, const char path, int interval);

/** @return int */
C_API void co_signal_start(uv_signal_t *, uv_signal_cb signal_cb, int signum);

/** @return int */
C_API void co_signal_start_oneshot(uv_signal_t *, uv_signal_cb signal_cb, int signum);

/** @return int */
C_API void co_spawn(uv_loop_t *, uv_process_t *, uv_process_options_t *options);

/** @return void */
C_API void co_pipe_connect(uv_connect_t *, uv_pipe_t *, const char *name, uv_connect_cb cb);

/** @return int */
C_API void co_queue_work(uv_loop_t *, uv_work_t *, uv_work_cb work_cb, uv_after_work_cb after_work_cb);

/** @return int */
C_API void co_prepare_start(uv_prepare_t *, uv_prepare_cb cb);

/** @return int */
C_API void co_check_start(uv_check_t *check, uv_check_cb callback);

/** @return int */
C_API void co_idle_start(uv_idle_t *idle, uv_idle_cb callback);

/** @return int */
C_API void co_listen(uv_stream_t *stream, int backlog, uv_connection_cb cb);

/** @return int */
C_API void co_tcp_connect(uv_connect_t *req,
                          uv_tcp_t *handle,
                          const struct sockaddr *addr,
                          uv_connect_cb cb);

/** @return int */
C_API void co_getaddrinfo(uv_loop_t *loop,
                          uv_getaddrinfo_t *req,
                          uv_getaddrinfo_cb getaddrinfo_cb,
                          const char *node,
                          const char *service,
                          const struct addrinfo *hints);

/** @return int */
C_API void co_getnameinfo(uv_loop_t *loop,
                          uv_getnameinfo_t *req,
                          uv_getnameinfo_cb getnameinfo_cb,
                          const struct sockaddr *addr,
                          int flags);

/** @return int */
C_API void co_udp_recv_start(uv_udp_t *, uv_alloc_cb alloc_cb, uv_udp_recv_cb recv_cb);

/** @return int */
C_API void co_udp_send(uv_udp_send_t *req,
                       uv_udp_t *handle,
                       const uv_buf_t bufs[],
                       unsigned int nbufs,
                       const struct sockaddr *addr,
                       uv_udp_send_cb send_cb);

/** @return void */
C_API void co_walk(uv_loop_t ,  uv_walk_cb walk_cb, void *arg);

/** @return int */
C_API void fs_event_start(uv_fs_event_t *, uv_fs_event_cb cb, const char path, int flags);

/** @return int */
C_API void co_thread_create(uv_thread_t *tid, uv_thread_cb entry, void *arg);

#ifdef __cplusplus
}
#endif

#endif
