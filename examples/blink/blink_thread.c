#include <string.h>

#define ENABLE_DEBUG    (0)
#include "debug.h"
#include "thread.h"
#include "xtimer.h"
#include "blink_thread.h"

typedef struct {
    const uint8_t *data;
    const uint16_t *data_len;
} blink_msg_state_t;

static char _stack[THREAD_STACKSIZE_DEFAULT];
static volatile bool  _do_blink = false; /* not sure if this must be volatile */
static pid_t _my_pid = -1;
static blink_msg_state_t _msg;

static
void *_blink_thread(void *arg) {
    blink_msg_state_t msg = *((blink_msg_state_t *)arg);
    while (1) {
        while (_do_blink) {
            blink_sync();
            blink_data(msg.data, *msg.data_len);
        }
        thread_sleep();
    }
    return NULL;
}

int blink_thread_create(const uint8_t *data, const uint16_t *data_len) {
    /* there shall only be one blink thread */
    if (_my_pid < 0) {
        _do_blink = true;
        _msg = (blink_msg_state_t){ data, data_len };
        _my_pid = thread_create(_stack, sizeof(_stack),
                                THREAD_PRIORITY_MAIN,
                                THREAD_CREATE_STACKTEST,
                                _blink_thread, &_msg,
                                "blink");
    }
    return _my_pid;
}

void blink_thread_halt(void) {
    _do_blink = false;
}

void blink_thread_continue(void) {
    _do_blink = true;
    thread_wakeup(_my_pid);
}
