#ifndef BLINK_H
#define BLINK_H

#include "board.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C"
{
#endif
/*
//#define MESSAGE         "SSID=Tech_D3858095;PSK=6841118259180262"
#ifndef MESSAGE
#define MESSAGE         "fabian"
#endif*/

#ifndef MESSAGE
#define MESSAGE         "ab"
#endif

#ifdef EXT_LED_PIN
#ifdef LED0_PIN
#undef LED0_PIN
#endif
#define LED0_PIN        (EXT_LED_PIN)
#endif

#ifndef BLINK_HZ
#define BLINK_HZ        (4)
#endif

#define SYNC_WORD       (0b11111111)

#define TOGGLE_DELAY    ((US_PER_SEC / BLINK_HZ) / 2)

void blink_init(void);
void blink_sync(void);
void blink_byte(uint8_t byte);
void blink_data(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* BLINK_H */
