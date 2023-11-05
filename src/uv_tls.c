
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


#include "uv_tls.h"

static void tls_begin(void) {
    SSL_library_init();
    SSL_load_error_strings();
    ERR_load_BIO_strings();
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
}

//Need to enhance
static int uv__tls_verify_peer(int ok, X509_STORE_CTX *ctx) {
    return 1;
}

//int uv_tls_ctx_init(tls_engine *tls, char *cert, char *key)
int uv_tls_ctx_init(tls_engine *tls) {
    tls_begin();
    //Currently we support only TLS, No DTLS
    tls->ctx = SSL_CTX_new(SSLv23_method());
    if (!tls->ctx) {
        return ERR_TLS_ERROR;
    }

    SSL_CTX_set_options(tls->ctx, SSL_OP_NO_SSLv2);
    SSL_CTX_set_options(tls->ctx, SSL_OP_NO_SSLv3);
    SSL_CTX_set_options(tls->ctx, SSL_OP_NO_TLSv1_3);

    SSL_CTX_set_mode(tls->ctx, SSL_MODE_AUTO_RETRY |
                     SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER |
                     SSL_MODE_ENABLE_PARTIAL_WRITE);

    SSL_CTX_set_mode(tls->ctx, SSL_MODE_RELEASE_BUFFERS);

    return ERR_TLS_OK;
}

//tls_engine* get_engine(char *cert, char *key)
tls_engine *get_engine(void) {
    if (ptr_engine) {
        return ptr_engine;
    }
    //TODO: Better error handling
    //if( ERR_TLS_OK != uv_tls_ctx_init(&the_engine, cert, key) ) {
    if (ERR_TLS_OK != uv_tls_ctx_init(&the_engine)) {
        return NULL;
    }
    ptr_engine = &the_engine;
    return  ptr_engine;
}


SSL_CTX *get_tls_ctx(void) {
    return  get_engine()->ctx;
}

//verification mode unused currently, SSL_VERIFY_NONE set
int tls_engine_inhale(char *cert, char *key, int verify_mode) {
    SSL_CTX *ctx = get_tls_ctx();

    int r = 0;
    //TODO: Change this later, no hardcoding
#define CIPHERS    "ALL:!EXPORT:!LOW"
    r = SSL_CTX_set_cipher_list(ctx, "HIGH:!SSLv2:!SSLv3");
    if (r != 1) {
        return ERR_TLS_ERROR;
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, uv__tls_verify_peer);

    r = SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM);
    if (r != 1) {
        return ERR_TLS_ERROR;
    }

    r = SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM);
    if (r != 1) {
        return ERR_TLS_ERROR;
    }

    r = SSL_CTX_check_private_key(ctx);
    if (r != 1) {
        return ERR_TLS_ERROR;
    }
    return ERR_TLS_OK;
}

void tls_engine_stop() {
    SSL_CTX *ctx = get_tls_ctx();

    SSL_CTX_free(ctx);
    ctx = NULL;

    ERR_remove_state(0);
    ENGINE_cleanup();
    CONF_modules_unload(1);
    ERR_free_strings();
    EVP_cleanup();
    //sk_SSL_COMP_free(SSL_COMP_get_compression_methods());
    //SSL_COMP_free_compression_methods();
    CRYPTO_cleanup_all_ex_data();
}

//Auxilary
uv_stream_t *uv_tls_get_stream(uv_tls_t *tls) {
    return  (uv_stream_t *)&tls->socket_;
}

int uv_tls_init(uv_loop_t *loop, uv_tls_t *strm) {

    uv_tcp_init(loop, &strm->socket_);
    strm->socket_.data = strm;

    tls_engine *ng = &(strm->tls_eng);

    ng->ctx = get_tls_ctx();
    ng->ssl = 0;
    ng->ssl_bio_ = 0;
    ng->app_bio_ = 0;
    strm->oprn_state = STATE_INIT;
    strm->rd_cb = NULL;
    strm->close_cb = NULL;
    strm->on_tls_connect = NULL;
    return 0;
}

void stay_uptodate(uv_tls_t *sec_strm, uv_alloc_cb uv__tls_alloc) {
    uv_stream_t *client = uv_tls_get_stream(sec_strm);

    int pending = BIO_pending(sec_strm->tls_eng.app_bio_);
    if (pending > 0) {

        //Need to free the memory
        uv_buf_t mybuf;

        if (uv__tls_alloc) {
            uv__tls_alloc((uv_handle_t *)client, pending, &mybuf);
        }

        int rv = BIO_read(sec_strm->tls_eng.app_bio_, mybuf.base, pending);
        assert(rv == pending);

        rv = uv_try_write(client, &mybuf, 1);
        assert(rv == pending);

        free(mybuf.base);
        mybuf.base = 0;
    }
}

static void uv__tls_alloc(uv_handle_t *handle, size_t size, uv_buf_t *buf) {
    buf->base = malloc(size);
    assert(buf->base != NULL && "Memory allocation failed");
    buf->len = size;
}

//handle only non fatal error currently
int uv__tls_err_hdlr(uv_tls_t *k, const int err_code) {
    if (err_code > 0) {
        return err_code;
    }

    switch (SSL_get_error(k->tls_eng.ssl, err_code)) {
        case SSL_ERROR_NONE: //0
        case SSL_ERROR_SSL:  // 1
            ERR_print_errors_fp(stderr);
            //don't break, flush data first

        case SSL_ERROR_WANT_READ: // 2
        case SSL_ERROR_WANT_WRITE: // 3
        case SSL_ERROR_WANT_X509_LOOKUP:  // 4
            stay_uptodate(k, uv__tls_alloc);
            break;
        case SSL_ERROR_ZERO_RETURN: // 5
        case SSL_ERROR_SYSCALL: //6
        case SSL_ERROR_WANT_CONNECT: //7
        case SSL_ERROR_WANT_ACCEPT: //8
            //ERR_print_errors_fp(stderr);
        default:
            return err_code;
    }
    return err_code;
}

void after_close(uv_handle_t *hdl) {
    uv_tls_t *s = CONTAINER_OF((uv_tcp_t *)hdl, uv_tls_t, socket_);
    if (s->close_cb) {
        s->close_cb(s);
        s = 0;
    }
}

int uv__tls_close(uv_tls_t *session) {
    tls_engine *ng = &(session->tls_eng);
    int rv = SSL_shutdown(ng->ssl);
    uv__tls_err_hdlr(session, rv);

    if (rv == 0) {
        session->oprn_state = STATE_CLOSING;
        rv = SSL_shutdown(ng->ssl);
        uv__tls_err_hdlr(session, rv);
    }

    if (rv == 1) {
        session->oprn_state = STATE_CLOSING;
    }

    BIO_free(ng->app_bio_);
    ng->app_bio_ = NULL;
    SSL_free(ng->ssl);
    ng->ssl = NULL;

    uv_close((uv_handle_t *)uv_tls_get_stream(session), after_close);

    return rv;
}

//shutdown the ssl session then stream
int uv_tls_close(uv_tls_t *session, tls_close_cb cb) {
    session->close_cb = cb;
    return  uv__tls_close(session);
}

int uv__tls_handshake(uv_tls_t *tls) {
    if (tls->oprn_state & STATE_IO) {
        return 1;
    }
    int rv = 0;
    rv = SSL_do_handshake(tls->tls_eng.ssl);
    uv__tls_err_hdlr(tls, rv);
    tls->oprn_state = STATE_HANDSHAKING;

    if (rv == 1) {
        tls->oprn_state = STATE_IO;

        if (tls->on_tls_connect) {
            assert(tls->con_req);
            tls->on_tls_connect(tls->con_req, 0);
        }
    }
    return rv;
}

int uv_tls_shutdown(uv_tls_t *session) {
    assert(session != NULL && "Invalid session");

    SSL_CTX_free(session->tls_eng.ctx);
    session->tls_eng.ctx = NULL;

    return 0;
}

uv_buf_t encode_data(uv_tls_t *sessn, uv_buf_t *data2encode) {
    //this should give me something to write to client
    int rv = SSL_write(sessn->tls_eng.ssl, data2encode->base, data2encode->len);

    int pending = 0;
    uv_buf_t encoded_data;
    if ((pending = BIO_pending(sessn->tls_eng.app_bio_)) > 0) {

        encoded_data.base = (char *)malloc(pending);
        encoded_data.len = pending;

        rv = BIO_read(sessn->tls_eng.app_bio_, encoded_data.base, pending);
        data2encode->len = rv;
    }
    return encoded_data;
}

int uv_tls_write(uv_write_t *req,
                 uv_tls_t *client,
                 uv_buf_t *buf,
                 uv_write_cb on_tls_write) {

    const uv_buf_t data = encode_data(client, buf);

    int rv = uv_write(req, uv_tls_get_stream(client), &data, 1, on_tls_write);
    free(data.base);
    return rv;
}

int uv__tls_read(uv_tls_t *tls, uv_buf_t *dcrypted, int sz) {

    if (!SSL_is_init_finished(tls->tls_eng.ssl)) {
        if (1 != uv__tls_handshake(tls)) {
            //recheck if handshake is complete now
            return STATE_HANDSHAKING;
        }
    }

    //clean the slate
    memset(dcrypted->base, 0, sz);
    int rv = SSL_read(tls->tls_eng.ssl, dcrypted->base, sz);
    uv__tls_err_hdlr(tls, rv);

    dcrypted->len = rv;
    if (tls->rd_cb) {
        tls->rd_cb(tls, rv, dcrypted);
    }

    return rv;
}

void on_tcp_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {

    uv_tls_t *parent = CONTAINER_OF(client, uv_tls_t, socket_);
    assert(parent != NULL);

    if (nread <= 0 && (parent->oprn_state & STATE_IO)) {
        parent->rd_cb(parent, nread, (uv_buf_t *)buf);
    } else {
        BIO_write(parent->tls_eng.app_bio_, buf->base, nread);
        uv__tls_read(parent, (uv_buf_t *)buf, nread);
    }
    free(buf->base);
}

//uv_alloc_cb is unused, but here for cosmetic reasons
//Need improvement
int uv_tls_read(uv_tls_t *sclient, uv_alloc_cb uv__tls_alloc, tls_rd_cb on_read) {

    sclient->rd_cb = on_read;
    return 0;
}

void on_tcp_conn(uv_connect_t *c, int status) {

    uv_tls_t *sclnt = c->handle->data;
    assert(sclnt != 0);

    if (status < 0) {
        sclnt->on_tls_connect(c, status);
    } else { //tcp connection established
        uv__tls_handshake(sclnt);
        uv_read_start((uv_stream_t *)&sclnt->socket_, uv__tls_alloc, on_tcp_read);
    }
}

static int assume_role(tls_engine *tls_ngin, int endpt_role) {
    tls_ngin->ssl = SSL_new(tls_ngin->ctx);
    if (!tls_ngin->ssl) {
        return ERR_TLS_ERROR;
    }

    if (endpt_role == 1) {
        SSL_set_accept_state(tls_ngin->ssl);
    } else {
        //set in client mode
        SSL_set_connect_state(tls_ngin->ssl);
    }

    //use default buf size for now.
    if (!BIO_new_bio_pair(&(tls_ngin->ssl_bio_), 0, &(tls_ngin->app_bio_), 0)) {
        return ERR_TLS_ERROR;
    }
    SSL_set_bio(tls_ngin->ssl, tls_ngin->ssl_bio_, tls_ngin->ssl_bio_);
    return  ERR_TLS_OK;
}

int uv_tls_connect(
    uv_connect_t *req,
    uv_tls_t *hdl, const struct sockaddr *addr,
    uv_connect_cb cb) {

    tls_engine *tls_ngin = &(hdl->tls_eng);
    int rv = assume_role(tls_ngin, 0);
    if (rv != ERR_TLS_OK) {
        return  rv;
    }

    hdl->on_tls_connect = cb;
    hdl->con_req = req;

    return uv_tcp_connect(req, &(hdl->socket_), addr, on_tcp_conn);
}

int uv_tls_accept(uv_tls_t *server, uv_tls_t *client) {
    uv_stream_t *clnt = uv_tls_get_stream(client);
    assert(clnt != 0);

    uv_stream_t *srvr = uv_tls_get_stream(server);
    assert(srvr != 0);

    int rv = uv_accept(srvr, clnt);
    if (rv < 0) {
        return rv;
    }

    srvr->data = client;
    clnt->data = server;

    tls_engine *tls_ngin = &(client->tls_eng);

    //server role
    rv = assume_role(tls_ngin, 1);
    if (rv != ERR_TLS_OK) {
        return  rv;
    }

    return uv_read_start(clnt, uv__tls_alloc, on_tcp_read);
}

int uv_tls_listen(uv_tls_t *server,
                  const int backlog,
                  uv_connection_cb on_new_connect) {
    uv_stream_t *strm = uv_tls_get_stream(server);
    assert(strm != NULL);
    return uv_listen(strm, backlog, on_new_connect);
}
