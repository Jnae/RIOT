#include <string.h>

#define ENABLE_DEBUG    (0)
#include "debug.h"
#include "xtimer.h"
#include "blink_interrupt.h"
#include "periph/gpio.h"

typedef struct {
    xtimer_t *timer;
    const uint8_t *data;
    uint16_t data_len;
    uint16_t byte_index;
    uint8_t bit_index;
} blink_msg_state_t;

static void _isr_blink_even(void *arg);
static void _isr_blink_odd(void *arg);

static uint8_t _buffer[MSG_MAX_LEN];
static blink_msg_state_t _msg;
static xtimer_t _timer;

static void _isr_blink_even(void *arg) {
    blink_msg_state_t *state = arg;
    if (state->timer->callback) {
        state->timer->callback = _isr_blink_odd;
        xtimer_set(state->timer, TOGGLE_DELAY);
    }
    uint8_t byte = state->data[state->byte_index];
    DEBUG("byte: %u\n", byte);
    if (byte & (1 << (7 - (state->bit_index)))) {
        /* blink 1, first signal */
        gpio_clear(LED0_PIN);
    }
    else {
        /* blink 0, first signal */
        gpio_set(LED0_PIN);
    }
}

static void _isr_blink_odd(void *arg) {
    blink_msg_state_t *state = arg;
    if (state->timer->callback) {
        state->timer->callback = _isr_blink_even;
        xtimer_set(state->timer, TOGGLE_DELAY);
    }
    uint8_t byte = state->data[state->byte_index];
    if (byte & (1 << (7 - (state->bit_index)))) {
        /* blink 1, second signal */
        gpio_set(LED0_PIN);
    }
    else {
        /* blink 0, second signal */
        gpio_clear(LED0_PIN);
    }

    assert(state->byte_index < state->data_len);
    if (state->bit_index < 7) {
        state->bit_index++;
    }
    else if (state->byte_index < state->data_len - 1){
        state->byte_index++;
        state->bit_index = 0;
    }
    else {
        state->byte_index = 0;
        state->bit_index = 0;
    }
}

int blink_interrupt_start(const uint8_t *data, const uint16_t *data_len) {
    if (MSG_MAX_LEN + 1 < *data_len) {
        return -1;
    }
    _buffer[0] = SYNC_WORD;
    memcpy(_buffer + 1, data, *data_len);
    _timer = (xtimer_t) {
        .next = NULL,
        .offset = 0,
        .long_offset = 0,
        .start_time = 0,
        .long_start_time = 0,
        .callback = _isr_blink_even,
        .arg = &_msg
    };
    _msg = (blink_msg_state_t) {
        .timer = &_timer,
        .data = _buffer,
        .data_len = *data_len + 1,
        .byte_index = 0,
        .bit_index = 0
    };
    xtimer_set(_msg.timer, 100);
    return 0;
}

void blink_interrupt_stop(void) {
    _timer.callback = NULL;
}
