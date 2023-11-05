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

void alloc_cb(uv_handle_t *handle, size_t size, uv_buf_t *buf)
{
    buf->base = (char*)malloc(size);
    memset(buf->base, 0, size);
    buf->len = size;
    assert(buf->base != NULL && "Memory allocation failed");
}

void echo_read(uv_tls_t *server, int nread, uv_buf_t *buf) {
    fprintf(stderr, "Entering %s\n", __FUNCTION__);

    if (nread == -1) {
        fprintf(stderr, "error echo_read");
        return;
    }
     
     fprintf(stderr, "%s\n", buf->base);
}

void on_write(uv_write_t *req, int status)
{
    if(status ) {
        return;
    }

    uv_tls_read(req->handle->data, alloc_cb, echo_read);
    free(req);
    req = 0;
}

void on_close(uv_tls_t* h)
{
    free(h);
    h = 0;
}




//TEST CODE for the lib
void on_connect(uv_connect_t *req, int status)
{
    fprintf( stderr, "Entering tls_connect callback\n");
    if( status ) {
        fprintf( stderr, "TCP connection error\n");
        return;
    }
    fprintf( stderr, "TCP connection established\n");

    uv_tls_t *clnt = req->handle->data;
    uv_write_t *rq = (uv_write_t*)malloc(sizeof(*rq));
    uv_buf_t dcrypted;
    dcrypted.base = "Hello from lib-tls";
    dcrypted.len = strlen(dcrypted.base);
    assert(rq != 0);
    uv_tls_write(rq, clnt, &dcrypted, on_write);
}


int main()
{
    uv_loop_t *loop = uv_default_loop();

    uv_tls_t *client = (uv_tls_t*)malloc(sizeof *client);
    if(uv_tls_init(loop, client) < 0) {
        free(client);
        client = 0;
        fprintf( stderr, "TLS setup error\n");
        return  -1;
    }

    const int port = 8000;
    struct sockaddr_in conn_addr;
    int r = uv_ip4_addr("127.0.0.1", port, &conn_addr);
    assert(!r);

    uv_connect_t req;
    uv_tls_connect(&req, client, (const struct sockaddr*)&conn_addr, on_connect);

    uv_run(loop, UV_RUN_DEFAULT);


    tls_engine_stop();
    free (client);
    client = 0;

    return 0;
}
