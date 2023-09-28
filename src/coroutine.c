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
        --coroutine_count;
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

CO_FORCE_INLINE value_types type_of(void_t self) {
    return ((var_t *)self)->type;
}

CO_FORCE_INLINE bool is_type(void_t self, value_types check) {
    return type_of(self) == check;
}

CO_FORCE_INLINE bool is_instance_of(void_t self, void_t check) {
    return type_of(self) == type_of(check);
}

CO_FORCE_INLINE bool is_value(void_t self) {
    return (type_of(self) > CO_NULL) && (type_of(self) < CO_NONE);
}

CO_FORCE_INLINE bool is_instance(void_t self) {
    return (type_of(self) > CO_NONE) && (type_of(self) < CO_NO_INSTANCE);
}

CO_FORCE_INLINE bool is_valid(void_t self) {
    return is_value(self) || is_instance(self);
}
