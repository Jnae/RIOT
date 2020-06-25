#ifndef BLINK_THREAD_H
#define BLINK_THREAD_H

#include "blink.h"

#ifdef __cplusplus
extern "C"
{
#endif

int blink_thread_create(const uint8_t *data, const uint16_t *data_len);
void blink_thread_halt(void);
void blink_thread_continue(void);

#ifdef __cplusplus
extern "C"
}
#endif

#endif /* BLINK_THREAD_H */
