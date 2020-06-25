#include <math.h>
#include <string.h>

#define ENABLE_DEBUG    (1)
#include "debug.h"
#include "data_preperation.h"

void bit_stuffing(uint8_t *data, uint16_t *data_len){
    uint8_t tmp[BITSTUFFING_MSG_LEN] = {};
    uint16_t idx = 0;
    uint8_t offset = 0, pattern = 0;
    
    /*DEBUG("[bitstuffing] previous data: ");
    for (int i = 0; i < (*data_len); ++i){
        DEBUG("%d: ", i);
        for(uint8_t j = 0; j < 8; ++j){
            uint8_t test = (data[i] & (1 << (7 - j))) >> (7 - j);
        DEBUG("%u ", test);
    }
    DEBUG("\n");
    }*/

    for (uint16_t i = 0; i < (*data_len); ++i){
        for(uint8_t j = 0; j < 8; ++j){
            //shifting one bit in
            pattern = (pattern << 1) | (data[i] & (1 << (7 - j))) >> (7 - j);
            idx = i + (j + offset) / 8;
            DEBUG("[bitstuffing] pattern: %u\n", pattern);

            if (((i > 0) | (j >= 7)) && SYNC_WORD == pattern){
                DEBUG("[bitstuffing] SYNC WORD == pattern\n");
                tmp[idx] = (tmp[idx] << 1) | (~pattern & 1);
                ++offset;
                idx = i + ( j + offset ) / 8;
                tmp[idx] = (tmp[idx] << 1) | (pattern & 1);
                pattern = (pattern << 1) | (~data[i] & (1 << (7 - j)) >> (7 - j));
                pattern = (pattern << 1) | (data[i] & (1 << (7 - j)) >> (7 - j));
            }
            else{
            tmp[idx] = (tmp[idx] << 1) | (pattern & 1);
            }
        }
    }
    //align last byte correctly
    if (offset) {
        tmp[idx] = (tmp[idx] << (8 - offset));
    }

    DEBUG("[bitstuffing] changed data: ");
    for (int i = 0; i < (*data_len + ceil( (double) offset / 8)); ++i){
        DEBUG("%d: %u ", i, tmp[i]);
    }
    DEBUG("\n");
    DEBUG("[bitstuffing] previous data: ");
    for (int i = 0; i< (*data_len); ++i){
        DEBUG("%d: %u ", i, data[i]);
    }
    DEBUG("\n");
    *data_len = *data_len + ceil((double) offset / 8);
    memcpy(data, tmp, *data_len);
}