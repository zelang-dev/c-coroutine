//%LICENSE////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2015 Devchandra M. Leishangthem (dlmeetei at gmail dot com)
//
// Distributed under the MIT License (See accompanying file LICENSE)
//
//////////////////////////////////////////////////////////////////////////
//
//%///////////////////////////////////////////////////////////////////////////

#ifndef UV_TLS_H
#define UV_TLS_H

#ifdef __cplusplus
extern "C" {
#endif
#include "uv/evt_tls.h"
#include "uv.h"
#if defined(_WIN32) || defined(_WIN64)
    #include "compat/unistd.h"
#else
    #include <unistd.h>
#endif

//copied gladly from libuv
#define CONTAINER_OF(ptr, type, member)                                       \
    ((type *) ((char *) (ptr) - offsetof(type, member)))


typedef struct uv_tls_s uv_tls_t;

typedef void (*uv_handshake_cb)(uv_tls_t*, int);
typedef void (*uv_tls_write_cb)(uv_tls_t*, int);
typedef void (*uv_tls_read_cb)(uv_tls_t*, ssize_t, const uv_buf_t*);
typedef void (*uv_tls_close_cb)(uv_tls_t*);

struct uv_tls_s {
   int type;
   uv_tcp_t *tcp_hdl;
   evt_tls_t *tls;
   void *data;

   uv_tls_read_cb tls_rd_cb;
   uv_tls_close_cb tls_cls_cb;
   uv_handshake_cb tls_hsk_cb;
   uv_tls_write_cb tls_wr_cb;
};

//implementation of network writer for libuv using uv_try_write
int uv_tls_writer(evt_tls_t *t, void *bfr, int sz);

//int uv_tls_init(uv_loop_t *loop, evt_ctx_t *ctx, uv_tls_t *endpt);
int uv_tls_init(evt_ctx_t *ctx, uv_tcp_t *tcp, uv_tls_t *endpt);

int uv_tls_connect(uv_tls_t *t, uv_handshake_cb cb);
int uv_tls_accept(uv_tls_t *tls, uv_handshake_cb cb);
int uv_tls_read(uv_tls_t *tls, uv_tls_read_cb on_read);
int uv_tls_close(uv_tls_t* session, uv_tls_close_cb close_cb);


int uv_tls_write(uv_tls_t *stream, uv_buf_t* buf, uv_tls_write_cb cb);
#ifdef __cplusplus
}
#endif //extern C

#endif //UV_TLS_H
