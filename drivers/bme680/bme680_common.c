/*
 * Copyright (C) 2020 OVGU Magdeburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_BME680
 * @{
 *
 * @file
 * @brief       Device driver implementation for the BME680 sensor.
 *
 * @author      Jana Eisoldt <jana.eisoldt@ovgu.de>
 *
 * @}
 */

#include <assert.h>
#include <stdbool.h>
#include <errno.h>

#include "bme680_common.h"
#include "bme680_internals.h"
#include "xtimer.h"

#define ENABLE_DEBUG 0
#include "debug.h"

/* convert heating config to register value */
uint8_t bme680_common_convert_res_heat(const bme680_common_t *dev)
{
    uint8_t heatr_res;
    int32_t var1, var2, var3, var4, var5, heatr_res_x100;

    /* calculate heater resistance using datasheet formula */
    var1 = (((int32_t)(dev->params.ambient_temp) * dev->calib.par_g3) / 1000) * 256;
    var2 = (dev->calib.par_g2 + 154009) * (dev->params.gas_heating_temp) * 5;
    var2 = ((var2 / 100) + 3276800) / 10;
    var2 = var2 * (dev->calib.par_g1 + 784);
    var3 = var1 + (var2 / 2);
    var4 = (var3 / ((dev->calib.res_heat_range) + 4));
    var5 = (131 * dev->calib.res_heat_val) + 65536;
    heatr_res_x100 = (int32_t)(((var4 / var5) - 250) * 34);
    heatr_res = (uint8_t)((heatr_res_x100 + 50) / 100);

    return heatr_res;
}

/* translate heating duration to register value */
uint8_t bme680_common_calc_heater_dur(uint16_t dur)
{

    if (dur >= 0xfc0) {
        return 0xff; /* Max duration */
    }

    uint8_t factor = 0;
    while (dur > 0x3F) {
        dur = dur / 4;
        factor += 1;
    }
    return dur + (factor * 64);
}

/* calculate compensated temperature using data sheet formula */
static uint32_t calc_temp(const bme680_common_t *dev, uint32_t *t_fine, uint32_t temp_adc)
{
    int32_t var1 = ((int32_t)temp_adc >> 3) - ((int32_t)dev->calib.par_t1 << 1);
    int32_t var2 = (var1 * (int32_t)dev->calib.par_t2) >> 11;
    int32_t var3 = ((((var1 >> 1) * (var1 >> 1)) >> 12) * ((int32_t)dev->calib.par_t3 << 4)) >> 14;
    *t_fine = var2 + var3;
    int32_t temp_comp = ((*t_fine * 5) + 128) >> 8;

    //printf("calib par t1: %u, par t2: %u, par t3: %u\n", dev->calib.par_t1, dev->calib.par_t2, dev->calib.par_t3);

    return temp_comp;
}

/*static int32_t _calc_t_fine(const bme680_common_t *dev, uint32_t temp_adc)
{
    calc_temp(const bme680_common_t *dev, uint32_t *res, uint32_t *t_fine, uint32_t temp_adc)
{
    int32_t var1 = ((int32_t)temp_adc >> 3) - ((int32_t)dev->calib.par_t1 << 1);
    int32_t var2 = (var1 * (int32_t)dev->calib.par_t2) >> 11;
    int32_t var3 = ((((var1 >> 1) * (var1 >> 1)) >> 12) * ((int32_t)dev->calib.par_t3 << 4)) >> 14;
    return var2 + var3;
}


static int32_t _calc_temp(uint32_t t_fine)
{
    return ((t_fine * 5) + 128) >> 8;
}*/

/* calculate humdity using datasheet formula */
static uint32_t calc_hum(const bme680_common_t *dev, const int32_t temp_comp, uint16_t hum_adc)
{
    int32_t var1 = (int32_t)hum_adc - ((int32_t)dev->calib.par_h1 << 4);
    var1 -= ((temp_comp * dev->calib.par_h3) / 100) >> 1;
    int32_t var2 = ((temp_comp * temp_comp * dev->calib.par_h5) / 100) >> 6;
    var2 = var2 / 100 + (1LL << 14);
    var2 += (((temp_comp) * (int32_t) dev->calib.par_h4) / ((int32_t) 100));
    var2 = (int32_t) dev->calib.par_h2 * var2;
    var2 >>= 10;
    int32_t var3 = var1 * var2;
    int32_t var4 = (int32_t)dev->calib.par_h6 << 7;
    var4 = (var4 + ((temp_comp * dev->calib.par_h7) / 100)) >> 4;
    int32_t var5 = ((var3 >> 14) * (var3 >> 14)) >> 10;
    int32_t var6 = (var4 * var5) >> 1;
    int32_t hum_comp = (((var3 + var6) >> 10) * ((int32_t) 1000)) >> 12;

    if (hum_comp < 0){
        return 0;
    }
    if (hum_comp > 100000){
        return 100000;
    }

    return hum_comp;
}

/* calculate pressure using datasheet formula */
static uint32_t calc_press(const bme680_common_t *dev, uint32_t t_fine, uint32_t press_adc)
{
    int32_t var1, var2, var3, pressure_comp;

    var1 = (t_fine >> 1) - 64000;
    var2 = ((((var1 >> 2) * (var1 >> 2)) >> 11) * dev->calib.par_p6) >> 2;
    var2 = var2 + ((var1 * dev->calib.par_p5) << 1);
    var2 = (var2 >> 2) + ((int32_t)dev->calib.par_p4 << 16);
    int32_t tmp1 = ((var1 >> 2) * (var1 >> 2)) >> 13;
    int32_t tmp2 = (int32_t)dev->calib.par_p3 << 5;
    var1 = ((tmp1 + tmp2) >> 3) + (((int32_t)dev->calib.par_p2 * var1) >> 1);
    var1 = var1 >> 18;
    var1 = ((32768 + var1) * dev->calib.par_p1) >> 15;
    pressure_comp = 1048576 - press_adc;
    pressure_comp = ((pressure_comp - (var2 >> 12)) * (3125));
    if (pressure_comp >= (1 << 30)){
        pressure_comp = ((pressure_comp / (uint32_t)var1) << 1);
    }
    else {
        pressure_comp = ((pressure_comp << 1) / (uint32_t)var1);
    }
    var1 = pressure_comp >> 3;
    var1 *= var1;
    var1 >>= 13;
    var1 = (var1 * dev->calib.par_p9) >> 12;
    var2 = ((int32_t)(pressure_comp >> 2) * (int32_t)dev->calib.par_p8) >> 13;
    var3 = pressure_comp >> 8;
    var3 = var3 * var3 * var3 * dev->calib.par_p10;
    var3 >>= 17;
    pressure_comp += (var1 + var2 + var3 + ((int32_t)dev->calib.par_p7 << 7)) >> 4;

    return pressure_comp;
}

static uint32_t calc_gas(const bme680_common_t *dev, uint8_t gas_range, uint16_t gas_adc)
{
    int64_t var1;
    uint64_t var2;
    uint32_t gas_res;

    uint32_t const_array1_int[] = CONST_ARRAY1_INT;
    uint32_t const_array2_int[] = CONST_ARRAY2_INT;

    /* calculate gas resistance using datasheet formula */
    var1 = ((1340 + (5 * (int64_t)dev->calib.range_sw_error)) * const_array1_int[gas_range]) >> 16;
    var2 = ((int64_t)gas_adc << 15) - (int64_t)(1 << 24) + var1;
    gas_res = (int32_t)((((int64_t)(const_array2_int[gas_range] * (int64_t)var1) >> 9) + (var2 >> 1)) / var2);

    return gas_res;
}

void bme680_common_convert(const bme680_common_t *dev, bme680_data_t *dest, const bme680_raw_t *raw_adc_values){

    uint32_t t_fine;
    dest->temperature = calc_temp(dev, &t_fine, raw_adc_values->temp_adc);
    dest->pressure = calc_press(dev, t_fine, raw_adc_values->press_adc);
    dest->humidity = calc_hum(dev, dest->temperature, raw_adc_values->hum_adc);

    printf("temperature: %lu\n", dest->temperature);

    /* evaluate gas data if required */
    if (dev->params.meas_gas && raw_adc_values->gas_status){
        dest->gas_resistance = calc_gas(dev, raw_adc_values->gas_range, raw_adc_values->gas_adc);
        dest->flags |= (1 << BME680_FLAG_HAS_GAS_VALUE);
    }
}
