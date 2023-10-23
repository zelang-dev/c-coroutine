#include "../include/coroutine.h"

string_t co_itoa(int64_t number) {
#ifdef _WIN32
    snprintf(co_active()->scrape, CO_SCRAPE_SIZE, "%lld", number);
#else
    snprintf(co_active()->scrape, CO_SCRAPE_SIZE, "%ld", number);
#endif
    return co_active()->scrape;
}

int co_strpos(string_t text, string pattern) {
    int c, d, e, text_length, pattern_length, position = -1;

    text_length = strlen(text);
    pattern_length = strlen(pattern);

    if (pattern_length > text_length) {
        return -1;
    }

    for (c = 0; c <= text_length - pattern_length; c++) {
        position = e = c;
        for (d = 0; d < pattern_length; d++) {
            if (pattern[d] == text[e]) {
                e++;
            } else {
                break;
            }
        }

        if (d == pattern_length) {
            return position;
        }
    }

    return -1;
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
        and (CO_CHANNEL)
            channel_free(ptr);
        or (CO_OBJ)
            ((object_t *)ptr)->dtor(ptr);
        or (CO_OA_HASH)
            hash_free(ptr);
        otherwise {
            if (is_valid(ptr))
                CO_FREE(ptr);
            else
                CO_LOG("Pointer not freed, possible double free attempt!");
        }
    }
}

void co_delete(routine_t *co) {
    if (!co) {
        CO_LOG("attempt to delete an invalid coroutine");
    } else if (!(co->status == CO_NORMAL
                 || co->status == CO_DEAD
                 // || co->status == CO_ERRED
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
        } else {
            if (co->err_allocated)
                CO_FREE(co->err_allocated);

            if (co->results && !co->event_active)
                CO_FREE(co->results);

            //if (co->status != CO_ERRED)
            CO_FREE(co);
        }
    }
}

void_t co_user_data(routine_t *co) {
    return (co != NULL) ? co->user_data : NULL;
}

co_state co_status(routine_t *co) {
    return (co != NULL) ? co->status : CO_DEAD;
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
   // if (wgr == NULL)
   //     return;

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
    if (r < 0)
        CO_LOG("Invalid handler");

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
    if (c->wait_active && (memcmp(c->wait_group, wg, sizeof(wg)) == 0)) {
        co_pause();
        wgr = ht_result_init();
        co_deferred(c, FUNC_VOID(hash_free), wgr);
        oa_pair *pair;
        while (wg->size != 0) {
            for (int i = 0; i < wg->capacity; i++) {
                pair = wg->buckets[i];
                if (NULL != pair) {
                    if (pair->value != NULL) {
                        co = (routine_t *)pair->value;
                        if (!co_terminated(co)) {
                            if (!co->loop_active && co->status == CO_NORMAL)
                                coroutine_schedule(co);

                            coroutine_yield();
                        } else {
                            if (co->results != NULL && !co->loop_erred) {
                                hash_put(wgr, co_itoa(co->cid), co->results);
                                if(!co->event_active)
                                    CO_FREE(co->results);

                                co->results = NULL;
                            }

                            if (co->loop_erred) {
                                //return NULL;
                                //hash_remove(wg, pair->key);
                                //--c->wait_counter;
                                //break;
                                //continue;
                                hash_free(wg);
                                return wgr;
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
    return wgr;
}

value_t co_group_get_result(wait_result_t *wgr, int cid) {
  //  if (wgr == NULL)
   //     return;

    return ((values_t *)hash_get(wgr, co_itoa(cid)))->value;
}

void co_result_set(routine_t *co, void_t data) {
    if (data && data != CO_ERROR) {
        if (co->results != NULL && !co->event_active)
            CO_FREE(co->results);

        if (co->event_active) {
            co->results = data;
        } else {
            co->results = try_calloc(1, sizeof(values_t) + sizeof(data));
            memcpy(co->results, &data, sizeof(data));
        }
    }
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
signed int co_err_code() {
    return co_active()->loop_code;
}

char *co_get_name() {
    return co_active()->name;
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
CO_FORCE_INLINE bool is_status_invalid(routine_t *task) {
    return (task->status > CO_EVENT || task->status < 0);
}
