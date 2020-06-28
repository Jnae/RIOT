#include <math.h>
#include <string.h>

#define ENABLE_DEBUG    (1)
#include "debug.h"
#include "data_preperation.h"

void bit_stuffing(uint8_t *data, uint16_t *data_len){
    uint8_t tmp[BITSTUFFING_MSG_LEN] = {};
    uint16_t idx = 0;
    uint8_t offset = 0, pattern = 0, mask = 0;
    uint8_t sync_cpy = SYNC_WORD;

    //count consecutive 1s in sync pattern and set mask accordingly
    int count_ones = 0;
    while (sync_cpy){
        sync_cpy = (sync_cpy & (sync_cpy << 1));
        mask = (mask << 1) | 1;
        ++count_ones;
    }
    mask &= ~(1 << (count_ones - 1));
    DEBUG("[bitstuffing] mask: %u, count ones:%d\n", mask, count_ones);
    
    for (uint16_t i = 0; i < (*data_len); ++i){
        for(uint8_t j = 0; j < 8; ++j){
            //shifting one bit from data in
            pattern = (pattern << 1) | (data[i] & (1 << (7 - j))) >> (7 - j);
            idx = i + (j + offset) / 8;
            tmp[idx] = (tmp[idx] << 1) | (pattern & 1);

            if ((mask & pattern) == mask){
                DEBUG("[bitstuffing] SYNC WORD match in data i=%u j=%u\n", i,j);
                ++offset;
                idx = i + ( j + offset ) / 8;
                tmp[idx] = (tmp[idx] << 1) | (~pattern & 1);
                pattern = (pattern << 1) | (~pattern & 1);
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