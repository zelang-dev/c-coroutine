#include "coroutine.h"

static thread_local gc_coroutine_t *coroutine_list = NULL;

void gc_coroutine(routine_t *co) {
    if (!coroutine_list)
        coroutine_list = (gc_coroutine_t *)ht_group_init();

    if (co->magic_number == CO_MAGIC_NUMBER)
        hash_put(coroutine_list, co_itoa(co->cid), co);
}

void gc_coroutine_free() {
    if (coroutine_list)
        hash_free(coroutine_list);
}

CO_FORCE_INLINE gc_coroutine_t *gc_coroutine_list() {
    return coroutine_list;
}

void args_free(args_t *params) {
    if (is_type(params, CO_ARGS)) {
        CO_FREE(params->args);
        memset(params, 0, sizeof(value_types));
        CO_FREE(params);
    }
}

value_t get_args(void_t *params, int item) {
    args_t *co_args = (args_t *)params;
    if (!co_args->defer_set) {
        defer(args_free, co_args);
        co_args->defer_set = true;
    }

    return args_in(co_args, item);
}

CO_FORCE_INLINE value_t args_in(args_t *params, int index) {
    return (index > -1 && index < (int)params->n_args)
        ? params->args[index].value
        : ((generics_t *)0)->value;
}

args_t *args_for(string_t desc, ...) {
    int count = (int)strlen(desc);
    args_t *params = (args_t *)try_calloc(1, sizeof(args_t));
    generics_t *args = (generics_t *)try_calloc(count, sizeof(generics_t));
    va_list argp;

    va_start(argp, desc);
    for (int i = 0; i < count; i++) {
        switch (*desc++) {
            case 'i':
                // unsigned integer argument
                args[i].value.max_size = va_arg(argp, size_t);
                break;
            case 'd':
                // signed integer argument
                args[i].value.long_long = va_arg(argp, int64_t);
                break;
            case 'c':
                // character argument
                args[i].value.schar = (char)va_arg(argp, int);
                break;
            case 's':
                // string argument
                args[i].value.char_ptr = va_arg(argp, char *);
                break;
            case 'a':
                // array argument
                args[i].value.array = va_arg(argp, char **);
                break;
            case 'x':
                // executable argument
                args[i].value.func = (callable_t)va_arg(argp, any_func_t);
                break;
            case 'f':
                // float argument
                args[i].value.precision = va_arg(argp, double);
                break;
            case 'p':
                // void pointer (any arbitrary pointer) argument
                args[i].value.object = va_arg(argp, void_t);
                break;
            default:
                args[i].value.object = NULL;
                break;
        }
    }
    va_end(argp);

    params->args = args;
    params->defer_set = false;
    params->n_args = count;
    params->type = CO_ARGS;
    return params;
}

void delete(void_t ptr) {
    match(ptr) {
        and (CO_CHANNEL)
            channel_free(ptr);
        or (CO_OBJ)
            ((object_t *)ptr)->dtor(ptr);
        or (CO_OA_HASH)
            hash_free(ptr);
        or (CO_ARGS)
            args_free(ptr);
        otherwise {
            if (is_valid(ptr)) {
                memset(ptr, 0, sizeof(ptr));
                CO_FREE(ptr);
            } else
                CO_LOG("Pointer not freed, possible double free attempt!");
        }
    }
}

void co_delete(routine_t *co) {
    if (!co) {
        CO_LOG("attempt to delete an invalid coroutine");
    } else if (!(co->status == CO_NORMAL
                 || co->status == CO_DEAD
                 || co->status == CO_ERRED
                 || co->status == CO_EVENT_DEAD)
               && !co->exiting
               ) {
        CO_INFO("attempt to delete a coroutine named: %s,\nthat is not dead or suspended, status is: %d\n", co->name, co->status);
    } else {
#ifdef CO_USE_VALGRIND
        if (co->vg_stack_id != 0) {
            VALGRIND_STACK_DEREGISTER(co->vg_stack_id);
            co->vg_stack_id = 0;
        }
#endif
        if (co->loop_active) {
            co->status = CO_EVENT_DEAD;
            co->loop_active = false;
            co->synced = false;
        } else if (co->magic_number == CO_MAGIC_NUMBER) {
            co->magic_number = -1;
            CO_FREE(co);
        }
    }
}

void_t co_user_data(routine_t *co) {
    return !is_empty(co) ? co->user_data : NULL;
}

co_state co_status(routine_t *co) {
    return !is_empty(co) ? co->status : CO_DEAD;
}

values_t *co_var(var_t *data) {
    if (data)
        return ((values_t *)data->value);

    CO_LOG("attempt to get value on null");
    return ((values_t *)0);
}

value_t co_value(void_t data) {
    if (data)
        return ((values_t *)data)->value;

    CO_LOG("attempt to get value on null");
    return ((values_t *)0)->value;
}

value_t co_data(values_t *data) {
    if (data)
        return data->value;

    return ((values_t *)0)->value;
}

CO_FORCE_INLINE void co_suspend() {
    co_yielding(co_current());
}

void co_yielding(routine_t *co) {
    co_stack_check(0);
    co_switch(co);
}

CO_FORCE_INLINE void co_resuming(routine_t *co) {
    if (!co_terminated(co))
        co_switch(co);
}

CO_FORCE_INLINE bool co_terminated(routine_t *co) {
    return co->halt;
}

value_t co_await(callable_t fn, void_t arg) {
    wait_group_t *wg = co_wait_group();
    int cid = co_go(fn, arg);
    wait_result_t *wgr = co_wait(wg);
    if (is_empty(wgr))
        return;

    return co_group_get_result(wgr, cid);
}

value_t co_event(callable_t fn, void_t arg) {
    routine_t *co = co_active();
    co->loop_active = true;
    return co_await(fn, arg);
}

void co_handler(func_t fn, void_t handle, func_t dtor) {
    routine_t *co = co_active();
    wait_group_t *eg = ht_group_init();

    co->event_group = eg;
    co->event_active = true;

    int cid = co_go((callable_t)fn, handle);
    string_t key = co_itoa(cid);
    routine_t *c = (routine_t *)hash_get(eg, key);

    co_deferred(c, dtor, handle);
    int r = snprintf(c->name, sizeof(c->name), "handler #%s", key);
    if (r == 0)
        CO_LOG("Invalid handler");

    co->event_group = NULL;
    hash_remove(eg, key);
    hash_free(eg);
}

void co_process(func_t fn, void_t args) {
    routine_t *co = co_active();
    wait_group_t *eg = ht_group_init();

    co->event_group = eg;
    co->event_active = true;

    int cid = co_go((callable_t)fn, args);
    string_t key = co_itoa(cid);
    routine_t *c = (routine_t *)hash_get(eg, key);
    c->process_active = true;

    int r = snprintf(c->name, sizeof(c->name), "process #%s", key);
    if (r == 0)
        CO_LOG("Invalid process");

    co->event_group = NULL;
    hash_remove(eg, key);
    hash_free(eg);
}

wait_group_t *co_wait_group(void) {
    routine_t *c = co_active();
    wait_group_t *wg = ht_group_init();
    c->wait_active = true;
    c->wait_group = wg;

    return wg;
}

wait_result_t *co_wait(wait_group_t *wg) {
    routine_t *c = co_active();
    wait_result_t *wgr = NULL;
    routine_t *co;
    bool has_erred = false;
    if (c->wait_active && (memcmp(c->wait_group, wg, sizeof(wg)) == 0)) {
        co_yield();
        wgr = ht_result_init();
        co_deferred(c, FUNC_VOID(hash_free), wgr);
        oa_pair *pair;
        while (wg->size != 0) {
            for (int i = 0; i < wg->capacity; i++) {
                pair = wg->buckets[i];
                if (NULL != pair) {
                    if (!is_empty(pair->value)) {
                        co = (routine_t *)pair->value;
                        if (!co_terminated(co)) {
                            if (!co->loop_active && co->status == CO_NORMAL)
                                coroutine_schedule(co);

                            coroutine_yield();
                        } else {
                            if (!is_empty(co->results) && !co->loop_erred) {
                                hash_put(wgr, co_itoa(co->cid), (co->is_address ? &co->results : co->results));
                                if (!co->event_active && !co->is_plain && !co->is_address) {
                                    CO_FREE(co->results);
                                    co->results = NULL;
                                }
                            }

                            if (co->loop_erred) {
                                hash_remove(wg, pair->key);
                                --c->wait_counter;
                                has_erred = true;
                                continue;
                            }

                            if (co->loop_active)
                                co_deferred_free(co);

                            hash_delete(wg, pair->key);
                            --c->wait_counter;
                        }
                    }
                }
            }
        }
        c->wait_active = false;
        c->wait_group = NULL;
        --coroutine_count;
    }

    hash_free(wg);
    return has_erred ? NULL : wgr;
}

value_t co_group_get_result(wait_result_t *wgr, int cid) {
    if (is_empty(wgr))
        return;

    return ((values_t *)hash_get(wgr, co_itoa(cid)))->value;
}

void co_result_set(routine_t *co, void_t data) {
    if (!is_empty(data)) {
        if (!is_empty(co->results) && !co->event_active)
            CO_FREE(co->results);

        if (co->event_active || co->is_address) {
            co->results = data;
        } else {
            co->results = try_calloc(1, sizeof(values_t) + sizeof(data));
            memcpy(co->results, data, sizeof(data));
        }
    }
}

#if defined(_WIN32) || defined(_WIN64)
struct tm *gmtime_r(const time_t *timer, struct tm *buf) {
    int r = gmtime_s(buf, timer);
    if (r)
        return NULL;

    return buf;
}
#endif

CO_FORCE_INLINE unsigned int co_id() {
    return co_active()->cid;
}

CO_FORCE_INLINE signed int co_err_code() {
    return co_active()->loop_code;
}

CO_FORCE_INLINE char *co_get_name() {
    return co_active()->name;
}

CO_FORCE_INLINE bool is_status_invalid(routine_t *task) {
    return (task->status > CO_EVENT || task->status < 0);
}

CO_FORCE_INLINE bool is_tls(uv_handle_t *self) {
    return ((var_t *)self)->type == UV_TLS;
}

CO_FORCE_INLINE size_t co_deferred_count(routine_t *coro) {
    return raii_deferred_count(coro->scope);
}

CO_FORCE_INLINE void co_deferred_free(routine_t *coro) {
    raii_deferred_free(coro->scope);
}

CO_FORCE_INLINE size_t co_defer(func_t func, void_t data) {
    return raii_deferred(co_active()->scope, func, data);
}

CO_FORCE_INLINE void co_recover(func_t func, void_t data) {
    raii_recover_by(co_active()->scope, func, data);
}

CO_FORCE_INLINE bool co_catch(string_t err) {
    return raii_is_caught(co_active()->scope, err);
}

CO_FORCE_INLINE string_t co_message(void) {
    return  raii_message_by(co_active()->scope);
}

CO_FORCE_INLINE size_t co_deferred(routine_t *coro, func_t func, void_t data) {
    return raii_deferred(coro->scope, func, data);
}

CO_FORCE_INLINE void co_defer_cancel(size_t index) {
    raii_deferred_cancel(co_active()->scope, index);
}

CO_FORCE_INLINE void co_defer_fire(size_t index) {
    raii_deferred_fire(co_active()->scope, index);
}

CO_FORCE_INLINE void_t co_malloc_full(routine_t *coro, size_t size, func_t func) {
    return malloc_full(coro->scope, size, func);
}

CO_FORCE_INLINE void_t co_calloc_full(routine_t *coro, int count, size_t size, func_t func) {
    return calloc_full(coro->scope, count, size, func);
}

CO_FORCE_INLINE void_t co_new_by(int count, size_t size) {
    return co_calloc_full(co_active(), count, size, CO_FREE);
}

CO_FORCE_INLINE void_t co_new(size_t size) {
    return co_malloc_full(co_active(), size, CO_FREE);
}

CO_FORCE_INLINE void_t co_malloc(routine_t *coro, size_t size) {
    return co_malloc_full(coro, size, CO_FREE);
}

CO_FORCE_INLINE void_t co_memdup(routine_t *coro, const_t src, size_t len) {
    void_t ptr = co_new_by(1, len + 1);

    return LIKELY(ptr) ? memcpy(ptr, src, len) : NULL;
}
