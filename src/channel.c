#include "../include/coroutine.h"

static thread_local int channel_id_generate = 0;

channel_t *channel_create(int elem_size, int bufsize) {
    channel_t *c = CO_CALLOC(1, sizeof(channel_t) + bufsize * elem_size);
    values_t *s = CO_CALLOC(1, sizeof(values_t));

    if (c == NULL || s == NULL)
        co_panic("channel_create failed");

    c->id = channel_id_generate++;
    c->type = CO_CHANNEL;
    c->elem_size = elem_size;
    c->bufsize = bufsize;
    c->nbuf = 0;
    c->tmp = s;
    c->select_ready = false;
    c->buf = (unsigned char *)(c + 1);

    gc_channel(c);
    return c;
}

CO_FORCE_INLINE channel_t *channel() {
    return channel_create(sizeof(values_t), 0);
}

CO_FORCE_INLINE channel_t *channel_buf(int elem_count) {
    return channel_create(sizeof(values_t), elem_count);
}

void channel_free(channel_t *c) {
    if (c == NULL)
        return;

    if (is_type(c, CO_CHANNEL)) {
        int id = c->id;
        if (c->name != NULL)
            CO_FREE(c->name);

        CO_FREE(c->tmp);
        CO_FREE(c->a_recv.a);
        CO_FREE(c->a_send.a);
        CO_FREE(c);

       hash_remove(gc_channel_list(), co_itoa(id));
    }
}

static void add_msg(msg_queue_t *a, channel_co_t *alt) {
    if (a->n == a->m) {
        a->m += 16;
        a->a = CO_REALLOC(a->a, a->m * sizeof(a->a[ 0 ]));
    }
    a->a[ a->n++ ] = alt;
}

static void del_msg(msg_queue_t *a, int i) {
    --a->n;
    a->a[ i ] = a->a[ a->n ];
}

/*
 * doesn't really work for things other than CHANNEL_SEND and CHANNEL_RECV
 * but is only used as arg to channel_msg, which can handle it
 */
#define other_op(op) (CHANNEL_SEND + CHANNEL_RECV - (op))

static msg_queue_t *channel_msg(channel_t *c, unsigned int op) {
    switch (op) {
        case CHANNEL_SEND:
            return &c->a_send;
        case CHANNEL_RECV:
            return &c->a_recv;
        default:
            return NULL;
    }
}

static int channel_co_can_exec(channel_co_t *a) {
    msg_queue_t *ar;
    channel_t *c;

    if (a->op == CHANNEL_OP)
        return 0;
    c = a->c;
    if (c->bufsize == 0) {
        ar = channel_msg(c, other_op(a->op));
        return ar && ar->n;
    } else {
        switch (a->op) {
            case CHANNEL_SEND:
                return c->nbuf < c->bufsize;
            case CHANNEL_RECV:
                return c->nbuf > 0;
            default:
                return 0;
        }
    }
}

static void channel_co_enqueue(channel_co_t *a) {
    msg_queue_t *ar;

    ar = channel_msg(a->c, a->op);
    add_msg(ar, a);
}

static void channel_co_dequeue(channel_co_t *a) {
    unsigned int i;
    msg_queue_t *ar;

    ar = channel_msg(a->c, a->op);
    if (ar == NULL) {
        snprintf(ex_message, 256, "bad use of channel_co_dequeue op=%d\n", a->op);
        co_panic(ex_message);
    }

    for (i = 0; i < ar->n; i++)
        if (ar->a[ i ] == a) {
            del_msg(ar, i);
            return;
        }
    co_panic("cannot find self in channel_co_dequeue");
}

static void channel_co_all_dequeue(channel_co_t *a) {
    int i;

    for (i = 0; a[ i ].op != CHANNEL_END && a[ i ].op != CHANNEL_BLK; i++)
        if (a[ i ].op != CHANNEL_OP)
            channel_co_dequeue(&a[ i ]);
}

static void amove(void_t dst, void_t src, unsigned int n) {
    if (dst) {
        if (src == NULL)
            memset(dst, 0, n);
        else
            memmove(dst, src, n);
    }
}

/*
 * Actually move the data around.  There are up to three
 * players: the sender, the receiver, and the channel itself.
 * If the channel is unbuffered or the buffer is empty,
 * data goes from sender to receiver.  If the channel is full,
 * the receiver removes some from the channel and the sender
 * gets to put some in.
 */
static void channel_co_copy(channel_co_t *s, channel_co_t *r) {
    channel_co_t *t;
    channel_t *c;
    unsigned char *cp;

    /*
     * Work out who is sender and who is receiver
     */
    if (s == NULL && r == NULL)
        return;
    CO_ASSERT(s != NULL);
    c = s->c;
    if (s->op == CHANNEL_RECV) {
        t = s;
        s = r;
        r = t;
    }
    CO_ASSERT(s == NULL || s->op == CHANNEL_SEND);
    CO_ASSERT(r == NULL || r->op == CHANNEL_RECV);

    /*
     * channel_t is empty (or unbuffered) - copy directly.
     */
    if (s && r && c->nbuf == 0) {
        amove(r->v, s->v, c->elem_size);
        return;
    }

    /*
     * Otherwise it's always okay to receive and then send.
     */
    if (r) {
        cp = c->buf + c->off * c->elem_size;
        amove(r->v, cp, c->elem_size);
        --c->nbuf;
        if (++c->off == c->bufsize)
            c->off = 0;
    }
    if (s) {
        cp = c->buf + (c->off + c->nbuf) % c->bufsize * c->elem_size;
        amove(cp, s->v, c->elem_size);
        ++c->nbuf;
    }
}

static void channel_co_exec(channel_co_t *a) {
    int i;
    msg_queue_t *ar;
    channel_co_t *other;
    channel_t *c;

    c = a->c;
    ar = channel_msg(c, other_op(a->op));
    if (ar && ar->n) {
        i = rand() % ar->n;
        other = ar->a[ i ];
        channel_co_copy(a, other);
        channel_co_all_dequeue(other->x_msg);
        other->x_msg[ 0 ].x_msg = other;
        coroutine_schedule(other->co);
    } else {
        channel_co_copy(a, NULL);
    }
}

static int channel_proc(channel_co_t *a) {
    int i, j, n_can, n, can_block;
    channel_t *c;
    routine_t *t;

    co_stack_check(512);
    for (i = 0; a[ i ].op != CHANNEL_END && a[ i ].op != CHANNEL_BLK; i++);
    n = i;
    can_block = a[ i ].op == CHANNEL_END;

    t = co_coroutine();
    t->channeled = true;
    for (i = 0; i < n; i++) {
        a[ i ].co = t;
        a[ i ].x_msg = a;
    }

    CO_LOG("processed ");

    n_can = 0;
    for (i = 0; i < n; i++) {
        c = a[ i ].c;

        CO_INFO(" %c:", "esrnb"[ a[ i ].op ]);
#ifdef CO_DEBUG
        if (c->name)
            printf("%s", c->name);
        else
            printf("%p", c);
#endif
        if (channel_co_can_exec(&a[ i ])) {
            CO_LOG("*");
            n_can++;
        }
    }

    if (n_can) {
        j = rand() % n_can;
        for (i = 0; i < n; i++) {
            if (channel_co_can_exec(&a[ i ])) {
                if (j-- == 0) {
                    c = a[ i ].c;
                    CO_INFO(" => %c:", "esrnb"[ a[ i ].op ]);
#ifdef CO_DEBUG
                    if (c->name)
                        printf("%s", c->name);
                    else
                        printf("%p", c);
#endif
                    CO_LOG(" ");

                    channel_co_exec(&a[ i ]);
                    return i;
                }
            }
        }
    }

    CO_LOG(" ");
    if (!can_block)
        return -1;

    for (i = 0; i < n; i++) {
        if (a[ i ].op != CHANNEL_OP)
            channel_co_enqueue(&a[ i ]);
    }

    a[ 0 ].c->select_ready = true;
    co_suspend();
    t->channeled = false;

    /*
     * the guy who ran the op took care of dequeueing us
     * and then set a[0].x_msg to the one that was executed.
     */
    return (int)(a[ 0 ].x_msg - a);
}

void channel_print(channel_t *c) {
    printf("--- start print channel ---\n");
    printf("buf content: [nbuf: %d, off: %d] \n", c->nbuf, c->off);
    printf("buf: ");
    for (unsigned int i = 0; i < c->bufsize; i++) {
        unsigned long *p = (unsigned long *)c->buf + i;
        printf("%ld ", *p);
    }
    printf("\n");
    printf("msg queue: ");
    channel_co_t *a = *(c->a_send.a);
    for (unsigned int i = 0; i < c->a_send.n; i++) {
        unsigned long *p = (unsigned long *)a->v;
        printf("%ld ", *p);
        a = a->x_msg[ 0 ].x_msg;
    }
    printf("\n");
    printf("--- end print channel ---\n");
}

static int _channel_op(channel_t *c, unsigned int op, void_t p, unsigned int can_block) {
    channel_co_t a[ 2 ];

    a[ 0 ].c = c;
    a[ 0 ].op = op;
    a[ 0 ].v = p;
    a[ 1 ].op = can_block ? CHANNEL_END : CHANNEL_BLK;
    if (channel_proc(a) < 0)
        return -1;
    return 1;
}

int co_send(channel_t *c, void_t v) {
    return _channel_op(c, CHANNEL_SEND, v, 1);
}

value_t co_recv(channel_t *c) {
    _channel_op(c, CHANNEL_RECV, c->tmp, 1);
    return c->tmp->value;
}
