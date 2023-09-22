#include "../include/coroutine.h"

string_t co_itoa(int64_t number) {
#ifdef _WIN32
    snprintf(co_active()->scrape, CO_SCRAPE_SIZE, "%lld", number);
#else
    snprintf(co_active()->scrape, CO_SCRAPE_SIZE, "%ld", number);
#endif
    return co_active()->scrape;
}

void co_strcpy(char *dest, string_t src, size_t len) {
#if defined(_WIN32) || defined(_WIN64)
    strcpy_s(dest, len, src);
#else
    strcpy(dest, src);
#endif
}

void delete(void_t ptr) {
    match(ptr) {
        and (CO_MAP_STRUCT)
            map_free(ptr);
        or (CO_CHANNEL)
            channel_free(ptr);
        or (CO_OBJ)
            ((object_t *)ptr)->dtor(ptr);
        otherwise {
            if (is_valid(ptr))
                CO_FREE(ptr);
            else
                CO_LOG("Pointer not freed, possible double free attempt!");
        }
    }
}

void println(int n_of_args, ...) {
    va_list argp;
    void_t list;
    int type;

    va_start(argp, n_of_args);
    for (int i = 0; i < n_of_args; i++) {
        list = va_arg(argp, void_t);
        if (is_type(((map_t *)list), CO_MAP_STRUCT)) {
            type = ((map_t *)list)->item_type;
            foreach(item in list) {
                if (type == CO_LLONG)
#ifdef _WIN32
                    printf("%ld ", (long)has(item).long_long);
#else
                    printf("%lld ", has(item).long_long);
#endif
                else if (type == CO_STRING)
                    printf("%s ", has(item).str);
                else if (type == CO_OBJ)
                    printf("%p ", has(item).object);
                else if (type == CO_BOOL)
                    printf(has(item).boolean ? "true " : "false ");
            }
        } else if (is_reflection(list)) {
            reflect_type_t *kind = (reflect_type_t *)list;
            printf("[ %d, %s, %zu, %zu, %zu ]\n",
                   reflect_type_enum(kind),
                   reflect_type_of(kind),
                   reflect_num_fields(kind),
                   reflect_type_size(kind),
                   reflect_packed_size(kind)
            );
            for (int i = 0; i < reflect_num_fields(kind); i++) {
                printf("  -  %d, %s, %s, %zu, %zu, %d, %d\n",
                       reflect_field_enum(kind, i),
                       reflect_field_type(kind, i),
                       reflect_field_name(kind, i),
                       reflect_field_size(kind, i),
                       reflect_field_offset(kind, i),
                       reflect_field_is_signed(kind, i),
                       reflect_field_array_size(kind, i)
                );
            }
        } else {
            printf("%s ", co_value(list).str);
        }
    }
    va_end(argp);
    puts("");
}

void co_delete(co_routine_t *handle) {
    if (!handle) {
        CO_LOG("attempt to delete an invalid coroutine");
    } else if (!(handle->status == CO_NORMAL || handle->status == CO_DEAD || handle->status == CO_EVENT_DEAD) && !handle->exiting) {
        CO_LOG("attempt to delete a coroutine that is not dead or suspended");
    } else {
#ifdef CO_USE_VALGRIND
        if (handle->vg_stack_id != 0) {
            VALGRIND_STACK_DEREGISTER(handle->vg_stack_id);
            handle->vg_stack_id = 0;
        }
#endif
        if (handle->loop_active) {
            handle->status = CO_EVENT_DEAD;
            handle->loop_active = false;
            handle->synced = false;
        } else {
            if (handle->err_allocated != NULL)
                CO_FREE(handle->err_allocated);

            CO_FREE(handle);
        }
    }
}

void_t co_user_data(co_routine_t *co) {
    return (co != NULL) ? co->user_data : NULL;
}

co_state co_status(co_routine_t *co) {
    return (co != NULL) ? co->status : CO_DEAD;
}

co_value_t *co_var(var_t *data) {
    if (data)
        return ((co_value_t *)data->value);

    CO_LOG("attempt to get value on null");
    return ((co_value_t *)0);
}

value_t co_value(void_t data) {
    if (data)
        return ((co_value_t *)data)->value;

    CO_LOG("attempt to get value on null");
    return ((co_value_t *)0)->value;
}

value_t co_data(co_value_t *data) {
    if (data)
        return data->value;

    return ((co_value_t *)0)->value;
}

CO_FORCE_INLINE void co_suspend() {
    co_yielding(co_current());
}

void co_yielding(co_routine_t *handle) {
    co_stack_check(0);
    co_switch(handle);
}

CO_FORCE_INLINE void co_resuming(co_routine_t *handle) {
    if (!co_terminated(handle))
        co_switch(handle);
}

CO_FORCE_INLINE bool co_terminated(co_routine_t *co) {
    return co->halt;
}

value_t co_await(callable_t fn, void_t arg) {
    wait_group_t *wg = co_wait_group();
    int cid = co_go(fn, arg);
    wait_result_t *wgr = co_wait(wg);
    return co_group_get_result(wgr, cid);
}

value_t co_event(callable_t fn, void_t arg) {

    co_routine_t *co = co_active();
    co->loop_active = true;
    return co_await(fn, arg);
}

wait_group_t *co_wait_group(void) {
    co_routine_t *c = co_active();
    wait_group_t *wg = co_ht_group_init();
    c->wait_active = true;
    c->wait_group = wg;

    return wg;
}

wait_result_t *co_wait(wait_group_t *wg) {
    co_routine_t *c = co_active();
    wait_result_t *wgr = NULL;
    co_routine_t *co;
    if (c->wait_active && (memcmp(c->wait_group, wg, sizeof(wg)) == 0)) {
        co_pause();
        wgr = co_ht_result_init();
        co_deferred(c, FUNC_VOID(co_hash_free), wgr);
        oa_pair *pair;
        while (wg->size != 0) {
            for (int i = 0; i < wg->capacity; i++) {
                pair = wg->buckets[i];
                if (NULL != pair) {
                    if (pair->value != NULL) {
                        co = (co_routine_t *)pair->value;
                        if (!co_terminated(co)) {
                            if (!co->loop_active && co->status == CO_NORMAL)
                                coroutine_schedule(co);

                            coroutine_yield();
                        } else {
                            if (co->results != NULL)
                                co_hash_put(wgr, co_itoa(co->cid), &co->results);

                            if (co->loop_active)
                                co_deferred_free(co);

                            co_hash_delete(wg, pair->key);
                            --c->wait_counter;
                        }
                    }
                }
            }
        }
        c->wait_active = false;
        c->wait_group = NULL;
        co_count_down();
    }

    co_hash_free(wg);
    return wgr;
}

value_t co_group_get_result(wait_result_t *wgr, int cid) {
    return ((co_value_t *)co_hash_get(wgr, co_itoa(cid)))->value;
}

void co_result_set(co_routine_t *co, void_t data) {
    co->results = data;
}

#if defined(_WIN32) || defined(_WIN64)
int gettimeofday(struct timeval *tp, struct timezone *tzp) {
    // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
    // This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
    // until 00:00:00 January 1, 1970
    static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);

    SYSTEMTIME system_time;
    FILETIME file_time;
    uint64_t time;

    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    time = ((uint64_t)file_time.dwLowDateTime);
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec = (long)((time - EPOCH) / 10000000L);
    tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
    return 0;
}
#endif

unsigned int co_id() {
    return co_active()->cid;
}
