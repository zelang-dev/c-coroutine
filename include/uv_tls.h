
/*//////////////////////////////////////////////////////////////////////////////

 * Copyright (c) 2015 libuv-tls contributors

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
**////////////////////////////////////////////////////////////////////////////*/


#ifndef __UV_TLS_H__
#define __UV_TLS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32) || defined(_WIN64)
    #include "compat/unistd.h"
#else
    #include <unistd.h>
#endif
#include <assert.h>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/conf.h>
#include <openssl/engine.h>

enum uv_tls_state {
    STATE_INIT = 0x0,
    STATE_HANDSHAKING = 0x1,
    STATE_IO = 0x2, //read or write mode
    STATE_CLOSING = 0x4,  // This means closed state also
};

//TODO: improve the error handling
enum uv_tls_error {
    ERR_TLS_ERROR = -1, //use OpenSSL error handling technique for this
    ERR_TLS_OK,
};

typedef struct tls_engine_s tls_engine;

struct tls_engine_s {
    BIO *app_bio_; //Our BIO, All IO should be through this
    SSL *ssl;
    SSL_CTX *ctx;
    BIO *ssl_bio_; //the ssl BIO used only by openSSL
};

static tls_engine the_engine;
static tls_engine *ptr_engine;

int tls_engine_inhale(char *, char *, int);

//copied gladly from libuv
#define CONTAINER_OF(ptr, type, member)                  \
  ((type *) ((char *) (ptr) - offsetof(type, member)))

typedef struct uv_tls_s uv_tls_t;

typedef void (*tls_rd_cb)(uv_tls_t* h, int nrd, uv_buf_t* dcrypted);
typedef void (*tls_close_cb)(uv_tls_t* h);
typedef void (*tls_connect_cb)(uv_connect_t* req, int status);

//Most used members are put first
struct uv_tls_s {
    uv_tcp_t              socket_; //handle that encapsulate the socket
    tls_engine            tls_eng;  //the tls engine representation
    void                  *data;
    int                   oprn_state; // operational state
    tls_rd_cb             rd_cb;
    tls_close_cb          close_cb;
    tls_connect_cb        on_tls_connect;
    uv_connect_t          *con_req;
};


/*
 *Initialize the common part of SSL startup both for client and server
 Only uv_tls_init at max will return TLS engine related issue other will have
 libuv error
 */
int uv_tls_init(uv_loop_t* loop, uv_tls_t* stream);
int uv_tls_listen(uv_tls_t *server, const int bk, uv_connection_cb on_connect );
int uv_tls_accept(uv_tls_t* server, uv_tls_t* client);
int uv_tls_read(uv_tls_t* client, uv_alloc_cb alloc_cb , tls_rd_cb on_read);
int uv_tls_write(uv_write_t* req, uv_tls_t *client, uv_buf_t* buf, uv_write_cb cb);
int uv_tls_close(uv_tls_t* session, tls_close_cb close_cb);
//shutdown should go away
int uv_tls_shutdown(uv_tls_t* session);

int uv_tls_connect(
      uv_connect_t *req
      ,uv_tls_t* hdl
      ,const struct sockaddr* addr
      ,uv_connect_cb cb
);

//verification mode unused currently, SSL_VERIFY_NONE set
int tls_enginge_inhale(char *cert, char *key, int verify_mode);

void tls_engine_stop(void);

SSL_CTX *get_tls_ctx(void);

//Auxilary functions
uv_stream_t *uv_tls_get_stream(uv_tls_t *tls);
#ifdef __cplusplus
}
#endif

#endif
