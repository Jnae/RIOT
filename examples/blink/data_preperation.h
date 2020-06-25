#ifndef DATA_PREPERATION_H
#define DATA_PREPERATION_H

#include "blink.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define BITSTUFFING_MSG_LEN        (sizeof(MESSAGE) + sizeof(MESSAGE)/sizeof(SYNC_WORD))

void bit_stuffing(uint8_t *data, uint16_t *data_len);

#ifdef __cplusplus
}
#endif

#endif /* DATA_PREPERATION_H */