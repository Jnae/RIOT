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

#include "bme680_spi.h"
#include "bme680_common.h"
#include "bme680_spi_params.h"
#include "bme680_internals.h"
#include "stdbool.h"
#include "periph/spi.h"
#include "xtimer.h"

#define ENABLE_DEBUG 0
#include "debug.h"

#define NSS_PIN    (dev->spi_intf.nss_pin)
#define DEV_SPI    (dev->spi_intf.dev)

unsigned int bme680_spi_devs_numof = 0;

bme680_spi_t *bme680_spi_devs[BME680_COMMON_NUMOF] = { };


/* check if memory page needs to be changed */
static uint16_t _set_memory_page(const bme680_spi_t *dev, uint8_t reg){

    uint8_t prev = 0;
    uint8_t mem_page = (reg & BME680_SPI_PAGE_0_MASK) ? BME680_SPI_PAGE_0 : BME680_SPI_PAGE_1;

    spi_acquire(DEV_SPI, NSS_PIN, BME680_SPI_MODE, BME680_SPI_SPEED);

    /* read old register value and compare */
    spi_transfer_regs(DEV_SPI, NSS_PIN, 0xF3, NULL, &prev, 1);

    if ((prev & BME680_SPI_MEM_PAGE_MASK) != mem_page){
        mem_page = (prev & (~BME680_SPI_MEM_PAGE_MASK)) | mem_page;
        spi_transfer_regs(DEV_SPI, NSS_PIN, 0x73, &mem_page, NULL, 1);
    }
    spi_release(DEV_SPI);

    return 0;
}

 static int _read_regs(const bme680_spi_t *dev, uint8_t reg, void *res, uint8_t len){
    _set_memory_page(dev, reg);
    reg = reg | (0x80);

    spi_acquire(DEV_SPI, NSS_PIN, BME680_SPI_MODE, BME680_SPI_SPEED);
    spi_transfer_regs(DEV_SPI, NSS_PIN, reg, NULL, res, len);
    spi_release(DEV_SPI);

    return 0;
 }

 static int _read_reg(const bme680_spi_t *dev, uint8_t reg, void *res){
    _set_memory_page(dev, reg);
    reg = reg | (0x80);

    spi_acquire(DEV_SPI, NSS_PIN, BME680_SPI_MODE, BME680_SPI_SPEED);
    spi_transfer_regs(DEV_SPI, NSS_PIN, reg, NULL, res, 1);
    spi_release(DEV_SPI);

    return 0;
 }

static int _write_reg(const bme680_spi_t *dev, uint8_t reg, const uint8_t *res){
    _set_memory_page(dev, reg);
    reg = reg & (0x7F);

    spi_acquire(DEV_SPI, NSS_PIN, BME680_SPI_MODE, BME680_SPI_SPEED);
    spi_transfer_regs(DEV_SPI, NSS_PIN, reg, res, NULL, 1);
    spi_release(DEV_SPI);

    return 0;
}

int bme680_spi_init(bme680_spi_t *dev, const bme680_spi_params_t *params)
{
    assert(dev && params);
    assert(bme680_spi_devs_numof < BME680_COMMON_NUMOF);
    dev->spi_intf = params->spi_params;
    dev->common.params = params->common_params;

    bme680_spi_devs[bme680_spi_devs_numof] = dev;

    uint8_t reset = BME680_RESET, reset_status, chip_id, os_set, hum_set, filter_set;
    bme680_calib_t calib_data;

    /* initalize spi */
    uint8_t tmp = spi_init_cs(DEV_SPI, NSS_PIN);
    if (tmp != SPI_OK) {
        puts("error: unable to initialize the given chip select line\n");
        return -EIO;
    }

    /* reset device */
    if (_write_reg(dev, BME680_REGISTER_RESET, &reset) < 0){
        DEBUG("[bme680] error writing reset register\n");
        return -EIO;
    }

    /* check reset status */
    if (_read_reg(dev, BME680_REGISTER_RESET, &reset_status) < 0){
        DEBUG("[bme680] error reading BME680_REGISTER_RESET register\n");
        return -EIO;
    }

    if (reset_status != 0){
        DEBUG("[bme680] error on reset\n");
        return -EIO;
    }

    if (_read_reg(dev, BME680_REGISTER_CHIP_ID, &chip_id) < 0){
        DEBUG("[bme680] error reading BME680_REGISTER_CHIP_ID register\n");
        return -EIO;
    }

    if (chip_id != BME680_CHIP_ID){
        DEBUG("[bme680] wrong chip id\n");
        return -EIO;
    }

    /* set temperature os and pressure os */
    os_set = (dev->common.params.temp_os << 5) | (dev->common.params.press_os << 2);

    if (_write_reg(dev, BME680_REGISTER_CTRL_MEAS, &os_set) < 0){
        DEBUG("[bme680] error writing BME680_REGISTER_CTRL_MEAS register\n");
        return -EIO;
    }

    if (_read_reg(dev, BME680_REGISTER_CTRL_HUM, &hum_set) < 0){
        DEBUG("[bme680] error writing BME680_REGISTER_CTRL_HUM register\n");
        return -EIO;
    }
    hum_set = (hum_set & BME680_HUM_SETTINGS_MASK) | dev->common.params.hum_os;
    if (_write_reg(dev, BME680_REGISTER_CTRL_HUM, &hum_set) < 0){
        DEBUG("[bme680] error writing BME680_REGISTER_CTRL_HUM register\n");
        return -EIO;
    }

    /* set IIR Filter */
    if (_read_reg(dev, BME680_REGISTER_CONFIG, &filter_set) < 0){
        DEBUG("[bme680] error writing BME680_REGISTER_CONFIG register\n");
        return -EIO;
    }

    filter_set = (filter_set & BME680_FILTER_SETTINGS_MASK) | (dev->common.params.filter << 2);

    if (_write_reg(dev, BME680_REGISTER_CONFIG, &filter_set) < 0){
        DEBUG("[bme680] error writing BME680_REGISTER_CONFIG register\n");
        return -EIO;
    }

    /* wait, otherwise reading the values is not working */
    xtimer_msleep(5);

    /* read calibration parameters from sensor */
    bme680_calib_chunk1_t calib_chunk1;
    bme680_calib_chunk2_t calib_chunk2;

    if (_read_regs(dev, BME680_REGISTER_CALIB_1, &calib_chunk1, sizeof(calib_chunk1)) < 0){
        DEBUG("[bme680] error reading BME680_REGISTER_CALIB_1 register\n");
        return -EIO;
    }

    if (_read_regs(dev, BME680_REGISTER_CALIB_2, &calib_chunk2, sizeof(calib_chunk2)) <0){
        DEBUG("[bme680] error reading BME680_REGISTER_CALIB_2 register\n");
        return -EIO;
    }

    calib_data.par_t1 = calib_chunk1.par_t1;
    calib_data.par_t2 = calib_chunk2.par_t2;
    calib_data.par_h1 = (calib_chunk1.h1_msb << 4) | (calib_chunk1.h1_h2_lsb  & BME680_H1_H2_MASK);
    calib_data.par_h2 = (calib_chunk1.h2_msb << 4) | (calib_chunk1.h1_h2_lsb  & BME680_H1_H2_MASK);
    calib_data.par_t3 = calib_chunk2.par_t3;
    calib_data.par_p1 = calib_chunk2.par_p1;
    calib_data.par_p2 = calib_chunk2.par_p2;
    calib_data.par_p4 = calib_chunk2.par_p4;
    calib_data.par_p5 = calib_chunk2.par_p5;
    calib_data.par_p8 = calib_chunk2.par_p8;
    calib_data.par_p9 = calib_chunk2.par_p9;
    calib_data.par_g2 = calib_chunk1.par_g2;
    calib_data.par_h3 = calib_chunk1.par_h3;
    calib_data.par_h4 = calib_chunk1.par_h4;
    calib_data.par_h5 = calib_chunk1.par_h5;
    calib_data.par_h6 = calib_chunk1.par_h6;
    calib_data.par_h7 = calib_chunk1.par_h7;
    calib_data.par_p3 = calib_chunk2.par_p3;
    calib_data.par_p6 = calib_chunk2.par_p6;
    calib_data.par_p7 = calib_chunk2.par_p7;
    calib_data.par_p10 = calib_chunk2.par_p10;
    calib_data.par_g1 = calib_chunk1.par_g1;
    calib_data.par_g3 = calib_chunk1.par_g3;

    /* set gas settings and read gas calibration if enabled */
    if (dev->common.params.meas_gas){
        bme680_calib_chunk3_t calib_chunk3;
        if (_read_regs(dev, BME680_REGISTER_CALIB_3, &calib_chunk3, sizeof(calib_chunk3)) < 0){
            DEBUG("[bme680] error reading BME680_REGISTER_CALIB_2 register\n");
            return -EIO;
        }

        calib_data.res_heat_val = calib_chunk3.res_heat_val;
        calib_data.res_heat_range = (calib_chunk3.res_heat_range & BME680_RES_HEAT_RANGE_MASK) >> 4;
        calib_data.range_sw_error = calib_chunk3.range_sw_error >> 4;

        dev->common.calib = calib_data;

        /* calculate register values for gas heating temperature and duration */
        uint8_t res_heat_0, heat_duration, set_gas;

        res_heat_0 = bme680_common_convert_res_heat(&dev->common);
        heat_duration = bme680_common_calc_heater_dur(dev->common.params.gas_heating_time);

        /* set calculated heating temperature */
        if (_write_reg(dev, BME680_REGISTER_RES_HEAT_0, &res_heat_0) < 0){
            DEBUG("[bme680] error writing BME680_REGISTER_RES_HEAT_0 register\n");
            return -EIO;
        }

        /* set gas wait to calculated heat up duration */
        if (_write_reg(dev, BME680_REGISTER_GAS_WAIT_0, &heat_duration) < 0){
            DEBUG("[bme680] error writing BME680_REGISTER_GAS_WAIT_0 register\n");
            return -EIO;
        }

        /* enable gas and select heater settings */
        if (_read_reg(dev, BME680_REGISTER_CTRL_GAS_1, &set_gas) < 0){
            DEBUG("[bme680] error writing BME680_REGISTER_CTRL_GAS_1 register\n");
            return -EIO;
        }
        set_gas = (set_gas & (BME680_GAS_SETTINGS_MASK)) | BME680_RUN_GAS;
        if (_write_reg(dev, BME680_REGISTER_CTRL_GAS_1, &set_gas) < 0){
            DEBUG("[bme680] error writing BME680_REGISTER_CTRL_GAS_1 register\n");
            return -EIO;
        }
    }

    else{
        dev->common.calib = calib_data;
    }

    return 0;
}

int bme680_spi_read(const bme680_spi_t *dev, bme680_data_t *dest)
{
    assert(dev && dest);
    uint8_t new_data, reg_ctrl_meas;

    /* read os settings and set forced mode */
    if (_read_reg(dev, BME680_REGISTER_CTRL_MEAS, &reg_ctrl_meas) < 0){
        DEBUG("[bme680] error reading BME680_REGISTER_CTRL_MEAS register\n");
        return -EIO;
    }

    reg_ctrl_meas = reg_ctrl_meas | BME680_FORCED_MODE;

    if (_write_reg(dev, BME680_REGISTER_CTRL_MEAS, &reg_ctrl_meas) < 0){
        DEBUG("[bme680] error writing BME680_REGISTER_CTRL_MEAS register\n");
        return -EIO;
    }

    /* check if new data is available */
    xtimer_msleep(400);
    do {
        if (_read_reg(dev, BME680_REGISTER_MEAS_STATUS_0, &new_data) < 0){
            DEBUG("[bme680] error reading BME680_REGISTER_MEAS_STATUS register\n");
            return -EIO;
        }
        xtimer_msleep(10);
    } while (!(new_data & BME680_NEW_DATA));

    /* read uncompensated temperature, pressure, humidity */
    bme680_adc_readout_t adc_readout;
    if (_read_regs(dev, BME680_REGISTER_ADC, &adc_readout, sizeof(adc_readout)) < 0){
        DEBUG("[bme680] error reading BME680_REGISTER_ADC register\n");
        return -EIO;
    }

    bme680_raw_t raw_adc_values;

    raw_adc_values.temp_adc = (uint32_t) (((uint32_t) adc_readout.temp_adc_msb << 12) | ((uint32_t) adc_readout.temp_adc_lsb << 4)
            | ((uint32_t) adc_readout.temp_adc_xlsb >> 4));

    raw_adc_values.hum_adc = (uint16_t) (((uint16_t) adc_readout.hum_adc_msb << 8) | (uint16_t) adc_readout.hum_adc_lsb);

    raw_adc_values.press_adc = (uint32_t) (((uint32_t) adc_readout.press_adc_msb << 12) | ((uint32_t) adc_readout.press_adc_lsb << 4)
            | ((uint32_t) adc_readout.press_adc_xlsb >> 4));

    printf("temp adc: %lu\n", raw_adc_values.temp_adc);

    if (dev->common.params.meas_gas){

        xtimer_msleep(dev->common.params.gas_heating_time);

        bme680_adc_readout_gas_t adc_readout_gas;
        if (_read_regs(dev, BME680_REGISTER_ADC_GAS, &adc_readout_gas, sizeof(adc_readout_gas)) < 0){
            DEBUG("[bme680] error reading BME680_REGISTER_ADC_GAS register\n");
            return -EIO;
        }

        /* check if gas measurement was successful */
        if ((adc_readout_gas.gas_adc_lsb & BME680_GAS_MEASUREMENT_SUCCESS) != BME680_GAS_MEASUREMENT_SUCCESS){
            DEBUG("[bme680] gas measurement not successful\n");
            raw_adc_values.gas_status = 0;
        }
        else{
            raw_adc_values.gas_range = adc_readout_gas.gas_adc_lsb & BME680_GAS_RANGE_MASK;
            raw_adc_values.gas_adc = (uint16_t) (uint16_t) (adc_readout_gas.gas_adc_msb << 2)
                |  ((uint16_t) adc_readout_gas.gas_adc_lsb >> 6);
            raw_adc_values.gas_status = 1;
        }
    }

    bme680_common_convert(&dev->common, dest, &raw_adc_values);

    return 0;
}
