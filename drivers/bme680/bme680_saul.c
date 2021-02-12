/*
 * Copyright (C) 2020 Gunar Schorcht
 *               2020 OVGU Magdeburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_bme680
 * @brief       SAUL adaption for BME680 devices
 * @author      Gunar Schorcht <gunar@schorcht.net>
 * @author      Jana Eisoldt <jana.eisoldt@ovgu.de>
 * @file
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "phydat.h"
#include "saul.h"
#include "bme680_spi.h"
#include "bme680_spi_params.h"
#include "bme680_i2c.h"
#include "bme680_i2c_params.h"
#include "xtimer.h"

static uint8_t _valid_flags[(BME680_COMMON_NUMOF + 3) / 4] = { 0 };
static int16_t _temp[BME680_COMMON_NUMOF];
static int16_t _press[BME680_COMMON_NUMOF];
static int16_t _hum[BME680_COMMON_NUMOF];
static uint32_t _gas[BME680_COMMON_NUMOF];

static bool _is_gas_valid(unsigned dev_idx) {
    return (_valid_flags[dev_idx >> 2] & (1 << (dev_idx & 0x3)));
}
static bool _is_other_valid(unsigned dev_idx) {
    return (_valid_flags[dev_idx >> 2] & (1 << ((dev_idx & 0x3) + 1)));
}
static void _set_gas_valid(unsigned dev_idx) {
    _valid_flags[dev_idx >> 2] |= (1 << (dev_idx & 0x3));
}
static void _set_other_valid(unsigned dev_idx) {
    _valid_flags[dev_idx >> 2] |= (1 << ((dev_idx & 0x3) + 1));
}
static void _clear_gas_valid(unsigned dev_idx) {
    _valid_flags[dev_idx >> 2] &= ~(1 << (dev_idx & 0x3));
}
static void _clear_other_valid(unsigned dev_idx) {
    _valid_flags[dev_idx >> 2] &= ~(1 << ((dev_idx & 0x3) + 1));
}

#ifdef MODULE_BME680_I2C
extern bme680_spi_t bme680_common_devs_saul[BME680_COMMON_NUMOF];
#endif

#ifdef MODULE_BME680_SPI
extern bme680_spi_t bme680_common_devs_saul[BME680_COMMON_NUMOF];
#endif

static bool _is_spi_attached(const void *unknown_dev)
{
    if (!IS_USED(MODULE_BME680_SPI)) {
        return false;
    }
    if (!IS_USED(MODULE_BME680_I2C)) {
        return true;
    }
    /* both I2C and SPI attached devices are used */
    if (((uintptr_t)unknown_dev >= (uintptr_t)&bme680_common_devs_saul[0])
         && ((uintptr_t)unknown_dev < (uintptr_t)&bme680_common_devs_saul[BME680_COMMON_NUMOF])) {
         return true;
    }
    return false;
}

static bool _is_i2c_attached(const void *unknown_dev)
{
    if (!IS_USED(MODULE_BME680_SPI)) {
        return true;
    }
    if (!IS_USED(MODULE_BME680_I2C)) {
        return false;
    }
    /* both I2C and SPI attached devices are used */
    if (((uintptr_t)unknown_dev >= (uintptr_t)&bme680_common_devs_saul[0])
         && ((uintptr_t)unknown_dev < (uintptr_t)&bme680_common_devs_saul[BME680_COMMON_NUMOF])) {
         return true;
    }
    return false;
}

static unsigned _dev2index(const void *dev)
{
    uintptr_t offset = (uintptr_t)dev - (uintptr_t)&bme680_common_devs_saul[0];

    if (_is_spi_attached(dev)) {
        return offset / sizeof(bme680_spi_t);
    }

    if (_is_i2c_attached(dev)) {
        return offset / sizeof(bme680_i2c_t);
    }
    return -ECANCELED;
}

static int _read(const void *unknown_dev)
{
    bme680_data_t data;
    unsigned dev_index = _dev2index(unknown_dev);

    if (_is_spi_attached(unknown_dev)) {
        const bme680_spi_t *dev = unknown_dev;
        bme680_spi_read(dev, &data);
    }
    else {
        const bme680_i2c_t *dev = unknown_dev;
        bme680_i2c_read(dev, &data);
    }

    _temp[dev_index] = data.temperature;
    _press[dev_index] = data.pressure / 100;
    _hum[dev_index] = data.humidity / 10;
    _gas[dev_index] = data.gas_resistance;

    /* mark sensor values as valid */
    _set_other_valid(dev_index);
    _set_gas_valid(dev_index);
    return 0;
}

static int read_temp(const void *dev, phydat_t *data)
{
    /* find the device index */
    unsigned dev_index = _dev2index(dev);
    if (dev_index == BME680_COMMON_NUMOF) {
        /* return error if device index could not be found */
        return -ECANCELED;
    }

    /* either local variable is valid or fetching it was successful */
    if (_is_other_valid(dev_index) || _read(dev) == 0) {
        /* mark local variable as invalid */
        _clear_other_valid(dev_index);

        data->val[0] = _temp[dev_index];
        data->unit = UNIT_TEMP_C;
        data->scale = -2;
        return 1;
    }
    return -ECANCELED;
}

static int read_press(const void *dev, phydat_t *data)
{
    /* find the device index */
    unsigned dev_index = _dev2index(dev);
    if (dev_index == BME680_COMMON_NUMOF) {
        /* return with error if device index could not be found */
        return -ECANCELED;
    }

    /* either local variable is valid or fetching it was successful */
    if (_is_other_valid(dev_index)  || _read(dev) == 0) {
        /* mark local variable as invalid */
        _clear_other_valid(dev_index);

        data->val[0] = _press[dev_index];
        data->unit = UNIT_PA;
        data->scale = 2;
        return 1;
    }
    return -ECANCELED;
}

static int read_hum(const void *dev, phydat_t *data)
{
    /* find the device index */
    unsigned dev_index = _dev2index(dev);
    if (dev_index == BME680_COMMON_NUMOF) {
        /* return with error if device index could not be found */
        return -ECANCELED;
    }

    /* either local variable is valid or fetching it was successful */
    if (_is_other_valid(dev_index)  || _read(dev) == 0) {
        /* mark local variable as invalid */
        _clear_other_valid(dev_index);

        data->val[0] = _hum[dev_index];
        data->unit = UNIT_PERCENT;
        data->scale = -2;
        return 1;
    }
    return -ECANCELED;
}

static int read_gas(const void *dev, phydat_t *data)
{
    /* find the device index */
    unsigned dev_index = _dev2index(dev);
    if (dev_index == BME680_COMMON_NUMOF) {
        /* return with error if device index could not be found */
        return -ECANCELED;
    }

    /* either local variable is valid or fetching it was successful */
    if (_is_gas_valid(dev_index)  || _read(dev) == 0) {
        /* mark local variable as invalid */
        _clear_gas_valid(dev_index);

        if (_gas[dev_index] > INT16_MAX) {
            data->val[0] = _gas[dev_index] / 1000;
            data->scale = 3;
        }
        else {
            data->val[0] = _gas[dev_index];
            data->scale = 0;
        }
        data->unit = UNIT_OHM;
        return 1;
    }
    return -ECANCELED;
}

const saul_driver_t bme680_common_saul_driver_temperature = {
    .read = read_temp,
    .write = saul_notsup,
    .type = SAUL_SENSE_TEMP
};

const saul_driver_t bme680_common_saul_driver_pressure = {
    .read = read_press,
    .write = saul_notsup,
    .type = SAUL_SENSE_PRESS
};

const saul_driver_t bme680_common_saul_driver_humidity = {
    .read = read_hum,
    .write = saul_notsup,
    .type = SAUL_SENSE_HUM
};

const saul_driver_t bme680_common_saul_driver_gas = {
    .read = read_gas,
    .write = saul_notsup,
    .type = SAUL_SENSE_GAS
};
