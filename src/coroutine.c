#include "coroutine.h"

thrd_static(co_collector_t, coroutine_list, NULL)

void co_collector(routine_t *co) {
    wait_group_t wg;
    if (is_coroutine_list_empty()) {
        wg = ht_wait_init(HASH_INIT_CAPACITY/2);
        wg->resize_free = false;
        coroutine_list_update(wg);
    }

    if (co->magic_number == CO_MAGIC_NUMBER)
        hash_put(coroutine_list(), co_itoa(co->cid), co);
}

CO_FORCE_INLINE void co_collector_free(void) {
    if (!is_coroutine_list_empty())
        hash_free(coroutine_list());
}

CO_FORCE_INLINE co_collector_t *co_collector_list(void) {
    return coroutine_list();
}

args_t args_ex(size_t num_of_args, ...) {
    va_list ap;
    size_t i;

    args_t params = args_for_ex(co_scope(), 0);
    va_start(ap, num_of_args);
    for (i = 0; i < num_of_args; i++)
        vector_push_back(params, va_arg(ap, void_t));
    va_end(ap);

    return params;
}

args_t args_deferred(void_t args) {
    args_t arg = (args_t)args;
    args_deferred_set(arg, co_scope());

    return arg;
}

CO_FORCE_INLINE future thrd_launch(thrd_func_t fn, void_t args) {
    return thrd_async_ex(co_scope(), fn, args);
}

CO_FORCE_INLINE void thrd_until(future fut) {
    thrd_wait(fut, co_yield_info);
}

void delete(void_t ptr) {
    match(ptr) {
        and (CO_CHANNEL)
            channel_free(ptr);
        or (CO_OBJ)
            ((object_t *)ptr)->dtor(ptr);
        or (CO_OA_HASH)
            hash_free(ptr);
        otherwise {
            if (is_valid(ptr)) {
                memset(ptr, 0, sizeof(ptr));
                CO_FREE(ptr);
            } else
                RAII_LOG("Pointer not freed, possible double free attempt!");
        }
    }
}

void co_delete(routine_t *co) {
    if (!co) {
        RAII_LOG("attempt to delete an invalid coroutine");
    } else if (!(co->status == CO_NORMAL
                 || co->status == CO_DEAD
                 || co->status == CO_ERRED
                 || co->status == CO_EVENT_DEAD)
               && !co->exiting
               ) {
        co_info(co, 1);
        co->flagged = true;
    } else {
#ifdef USE_VALGRIND
        if (co->vg_stack_id != 0) {
            VALGRIND_STACK_DEREGISTER(co->vg_stack_id);
            co->vg_stack_id = 0;
        }
#endif
        if (co->interrupt_active) {
            co->status = CO_EVENT_DEAD;
            co->interrupt_active = false;
            co->is_waiting = false;
        } else if (co->magic_number == CO_MAGIC_NUMBER) {
            co->magic_number = -1;
            CO_FREE(co);
        }
    }
}

CO_FORCE_INLINE void_t co_user_data(routine_t *co) {
    return !is_empty(co) ? co->user_data : NULL;
}

CO_FORCE_INLINE co_states co_status(routine_t *co) {
    return !is_empty(co) ? co->status : CO_DEAD;
}

values_t *co_var(var_t *data) {
    if (data)
        return ((values_t *)data->value);

    RAII_LOG("attempt to get value on null");
    return ((values_t *)0);
}

value_t co_value(void_t data) {
    return co_data((values_t *)data);
}

value_t co_data(values_t *data) {
    return raii_value(data);
}

CO_FORCE_INLINE void co_suspend(void) {
    co_yielding(co_current());
}

CO_FORCE_INLINE void co_yielding(routine_t *co) {
    co_stack_check(0);
    co_switch(co);
}

CO_FORCE_INLINE bool co_terminated(routine_t *co) {
    return co->halt;
}

CO_FORCE_INLINE void launch(func_t fn, void_t arg) {
    go((callable_t)fn, arg);
    co_yield();
}

value_t co_await(callable_t fn, void_t arg) {
    wait_group_t wg = wait_group_by(2);
    u32 cid = go(fn, arg);
    wait_result_t wgr = wait_for(wg);
    if (is_empty(wgr))
        return;

    return wait_result(wgr, cid);
}

value_t co_event(callable_t fn, void_t arg) {
    routine_t *co = co_active();
    co->interrupt_active = true;
    return co_await(fn, arg);
}

void co_handler(func_t fn, void_t handle, func_t dtor) {
    routine_t *co = co_active();
    wait_group_t eg = ht_wait_init(2);
    eg->grouping = ht_result_init();
    eg->resize_free = false;

    co->event_group = eg;
    co->event_active = true;

    u32 cid = go((callable_t)fn, handle);
    string_t key = co_itoa(cid);
    routine_t *c = (routine_t *)hash_get(eg, key);

    co_deferred(c, dtor, handle);
    int r = snprintf(c->name, sizeof(c->name), "handler #%s", key);
    if (r == 0)
        RAII_LOG("Invalid handler");

    co->event_group = NULL;
    hash_remove(eg, key);
    hash_free(eg);
}

void co_process(func_t fn, void_t args) {
    routine_t *co = co_active();
    wait_group_t eg = ht_wait_init(2);
    eg->resize_free = false;

    co->event_group = eg;
    co->event_active = true;

    u32 cid = go((callable_t)fn, args);
    string_t key = co_itoa(cid);
    routine_t *c = (routine_t *)hash_get(eg, key);
    c->process_active = true;

    int r = snprintf(c->name, sizeof(c->name), "process #%s", key);
    if (r == 0)
        RAII_LOG("Invalid process");

    co->event_group = NULL;
    hash_remove(eg, key);
    hash_free(eg);
}

CO_FORCE_INLINE void wait_capacity(u32 size) {
    hash_capacity(size + (size * 0.0025));
}

static wait_group_t wait_group_ex(u32 capacity) {
    routine_t *c = co_active();
    if (!is_zero(capacity) && (capacity > gq_sys.thread_count * 2))
        wait_capacity(capacity);

    if (atomic_flag_load(&gq_sys.is_multi) && is_empty(gq_sys.deque_run_queue)) {
        gq_sys.deque_run_queue = try_calloc(1, sizeof(deque_t));
        deque_init(gq_sys.deque_run_queue, gq_sys.capacity);
    } else if (atomic_flag_load(&gq_sys.is_multi) && atomic_load_explicit(&gq_sys.deque_run_queue->array->size, memory_order_relaxed) < gq_sys.capacity) {
        deque_array_t *a = (deque_array_t *)atomic_load_explicit(&gq_sys.deque_run_queue->array, memory_order_acquire);
        a = try_realloc(a, sizeof(deque_array_t) + sizeof(routine_t *) * gq_sys.capacity);
        atomic_store(&a->size, gq_sys.capacity);
        atomic_store_explicit(&gq_sys.deque_run_queue->array, a, memory_order_release);
    }

    wait_group_t wg = ht_wait_init(capacity + (capacity * 0.0025));
    wg->grouping = ht_result_init();
    if (sched_is_assignable(capacity))
        wg->group_capacity = capacity / gq_sys.thread_count;

    wg->resize_free = false;
    c->wait_active = true;
    c->wait_group = wg;
    c->is_group_finish = false;

    return wg;
}

CO_FORCE_INLINE wait_group_t wait_group(void) {
    return wait_group_ex(0);
}

CO_FORCE_INLINE wait_group_t wait_group_by(u32 capacity) {
    return wait_group_ex(capacity);
}

wait_result_t wait_for(wait_group_t wg) {
    routine_t *co, *c = co_active();
    wait_result_t wgr = NULL;
    wait_group_t *wait_group;
    void_t key;
    oa_pair *pair;
    u32 i, *gi, group_capacity, capacity, group_id = 0, id = 0, begin = 0;
    bool has_erred = false, has_completed = false, is_multi = atomic_flag_load(&gq_sys.is_multi);

    if (c->wait_active && (memcmp(c->wait_group, wg, sizeof(wg)) == 0)) {
        c->is_group_finish = true;
        co_yield();
        if (is_multi && atomic_flag_load(&gq_sys.is_threading_waitable)) {
            group_id = atomic_load_explicit(&gq_sys.group_id, memory_order_relaxed);
            if (group_id) {
                id = group_id - 1;
                gi = (u32 *)atomic_load_explicit(&gq_sys.group_index[id], memory_order_relaxed);
                begin = gi[sched_id()];
                wait_group = (wait_group_t *)atomic_load_explicit(&gq_sys.wait_group, memory_order_acquire);
                wait_group[id] = wg;
                atomic_store_explicit(&gq_sys.wait_group, wait_group, memory_order_release);
            }
            group_capacity = wg->group_capacity;
        }

        wgr = wg->grouping;
        co_deferred(c, VOID_FUNC(hash_free), wgr);
        while (atomic_load_explicit(&wg->size, memory_order_relaxed) != 0 && !has_completed) {
            capacity = (u32)atomic_load_explicit(&wg->capacity, memory_order_relaxed);
            for (i = begin; i < capacity; i++) {
                pair = (oa_pair *)atomic_load_explicit(&wg->buckets[i], memory_order_relaxed);
                if (!is_empty(pair) && !is_empty(pair->value)) {
                    co = (routine_t *)pair->value;
                    key = pair->key;
                    if (is_multi && group_id && group_capacity == 0) {
                        has_completed = true;
                        break;
                    } else if (is_multi && co->tid != sched_id()) {
                        continue;
                    } else if (!co_terminated(co)) {
                        if (!co->interrupt_active && co->status == CO_NORMAL)
                            sched_enqueue(co);

                        co_info(c, 1);
                        sched_yielding();
                    } else {
                        if (is_multi && group_id)
                            group_capacity--;

                        if ((!is_empty(co->results) && !co->is_event_err) || co->is_plain) {
                            if (co->is_plain)
                                hash_put(wgr, co_itoa(co->cid), &co->plain_results);
                            else
                                hash_put(wgr, co_itoa(co->cid), (co->is_address ? &co->results : co->results));

                            if (!co->event_active && !co->is_plain && !co->is_address) {
                                CO_FREE(co->results);
                                co->results = NULL;
                            }
                        }

                        if (co->is_event_err) {
                            wg = (wait_group_t)atomic_load_explicit(&gq_sys.wait_group[id], memory_order_acquire);
                            wg->has_erred = true;
                            atomic_store_explicit(&gq_sys.wait_group[id], wg, memory_order_release);
                            hash_remove(wg, key);
                            continue;
                        }

                        if (co->interrupt_active)
                            co_deferred_free(co);

                        hash_delete(wg, key);
                    }
                }
            }
            i = begin;
        }

        has_erred = wg->has_erred;
        c->wait_active = false;
        c->wait_group = NULL;
        if (is_multi && group_id) {
            while (atomic_load_explicit(&wg->size, memory_order_relaxed) != 0)
                co_yield();

            gi = (u32 *)atomic_load_explicit(&gq_sys.group_index[id], memory_order_acquire);
            atomic_store_explicit(&gq_sys.group_index[id], NULL, memory_order_release);
            wg = (wait_group_t)atomic_load_explicit(&gq_sys.wait_group[id], memory_order_acquire);
            wgr = wg->grouping;
            atomic_store_explicit(&gq_sys.wait_group[id], NULL, memory_order_release);
            CO_FREE(gi);
        }

        sched_dec();
        hash_free(wg);
        return has_erred ? NULL : wgr;
    }

    return NULL;
}

value_t wait_result(wait_result_t wgr, u32 cid) {
    if (is_empty(wgr))
        return;

    return ((values_t *)hash_get(wgr, co_itoa(cid)))->value;
}

value_t await(awaitable_t task) {
    if (!is_empty(task) && is_type(task, CO_ROUTINE)) {
        task->type = -1;
        u32 cid = task->cid;
        wait_group_t wg = task->wg;
        CO_FREE(task);
        co_active()->wait_group = wg;
        wait_result_t wgr = wait_for(wg);
        if (!is_empty(wgr))
            return wait_result(wgr, cid);
    }

    return;
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

void co_plain_set(routine_t *co, size_t data) {
    co->is_plain = true;
    co->plain_results = data;
}

#if defined(_WIN32) || defined(_WIN64)
struct tm *gmtime_r(const time_t *timer, struct tm *buf) {
    int r = gmtime_s(buf, timer);
    if (r)
        return NULL;

    return buf;
}
#endif

string_t co_state(int status) {
    switch (status) {
        case CO_DEAD:
            return "Dead/Not initialized";
        case CO_NORMAL:
            return "Active/Not running";
        case CO_RUNNING:
            return "Active/Running";
        case CO_SUSPENDED:
            return "Suspended/Not started";
        case CO_EVENT:
            return "External/Event callback";
        case CO_EVENT_DEAD:
            return "External/Event deleted/uninitialized";
        case CO_ERRED:
            return "Erred/Exception generated";
        default:
            return "Unknown";
    }
}

CO_FORCE_INLINE void co_info(routine_t *t, int pos) {
#ifdef USE_DEBUG
    bool line_end = false;
    if (is_empty(t)) {
        line_end = true;
        t = co_active();
    }

    char line[SCRAPE_SIZE];
    snprintf(line, SCRAPE_SIZE, "\033[0K\n\r\033[%dA", pos);
    fprintf(stderr, "\t\t - Thrd #%zx, cid: %u (%s) %s cycles: %zu%s",
            thrd_self(),
            t->cid,
            (!is_empty(t->name) && t->cid > 0 ? t->name : !t->is_channeling ? "" : "channel"),
            co_state(t->status),
            t->cycles,
            (line_end ? "\033[0K\n" : line)
    );
#endif
}

CO_FORCE_INLINE void co_info_active(void) {
    co_info(nullptr, 0);
}

CO_FORCE_INLINE u32 co_id(void) {
    return co_active()->cid;
}

CO_FORCE_INLINE signed int co_err_code(void) {
    return co_active()->event_err_code;
}

CO_FORCE_INLINE char *co_get_name(void) {
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
    raii_deferred_init(&coro->scope->defer);
}

CO_FORCE_INLINE size_t co_defer(func_t func, void_t data) {
    return raii_deferred(co_scope(), func, data);
}

CO_FORCE_INLINE void co_recover(func_t func, void_t data) {
    raii_recover_by(co_scope(), func, data);
}

CO_FORCE_INLINE bool co_catch(string_t err) {
    return raii_is_caught(co_scope(), err);
}

CO_FORCE_INLINE string_t co_message(void) {
    return  raii_message_by(co_scope());
}

CO_FORCE_INLINE size_t co_deferred(routine_t *coro, func_t func, void_t data) {
    return raii_deferred(coro->scope, func, data);
}

CO_FORCE_INLINE void co_defer_cancel(size_t index) {
    raii_deferred_cancel(co_scope(), index);
}

CO_FORCE_INLINE void co_defer_fire(size_t index) {
    raii_deferred_fire(co_scope(), index);
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

CO_FORCE_INLINE void co_yield_info(void) {
    co_info(co_active(), 1);
    co_yield();
}
