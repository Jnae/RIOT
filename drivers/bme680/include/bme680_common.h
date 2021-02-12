/*
 * Copyright (C) 2019 Mesotic SAS
 *               2020 Gunar Schorcht
 *               2020 OVGU Magdeburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    drivers_BME680 BME680
 * @ingroup     drivers_sensors
 * @brief       Driver for the Bosch BME680 sensor.
 *
 * @{
 *
 * @file
 * @author      Dylan Laduranty <dylan.laduranty@mesotic.com>
 * @author      Gunar Schorcht <gunar@schorcht.net>
 * @author      Jana Eisoldt <jana.eisoldt@ovgu.de>

 */

#ifndef BME680_COMMON_H
#define BME680_COMMON_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Declare the common API for SPI and I2C for the driver */

/**
 * @brief   Device initialization parameters
 */
typedef struct {
    uint16_t gas_heating_time;   /**< heating time of gas sensor */
    uint16_t gas_heating_temp;   /**< target heating temperature of gas sensor */
    uint8_t ambient_temp;        /**< ambient temperature */
    uint8_t temp_os;             /**< oversampling setting of temperature sensor */
    uint8_t press_os;            /**< oversampling setting of pressure sensor */
    uint8_t hum_os;              /**< oversampling setting of humidity sensor */
    uint8_t filter;              /**< IIR filter for short time fluctuations in temperature and pressure */
    bool meas_gas;               /**< disable/enable gas measurement */
} bme680_common_params_t;

/**
 * @brief   Device calibration data
 */
typedef struct {
    uint16_t par_t1;
    int16_t par_t2;
    uint16_t par_h1;
    uint16_t par_h2;
    int8_t par_t3;
    uint16_t par_p1;
    int16_t par_p2;
    int16_t par_p4;
    int16_t par_p5;
    int16_t par_p8;
    int16_t par_p9;
    int16_t par_g2;
    int8_t par_h3;
    int8_t par_h4;
    int8_t par_h5;
    uint8_t par_h6;
    int8_t par_h7;
    int8_t par_p3;
    int8_t par_p6;
    int8_t par_p7;
    uint8_t par_p10;
    int8_t par_g1;
    int8_t par_g3;
    uint8_t res_heat_val;
    uint8_t res_heat_range;
    uint8_t range_sw_error;
} bme680_calib_t;

typedef struct {
    uint32_t temp_adc;
    uint32_t press_adc;
    uint16_t hum_adc;
    uint8_t gas_status;
    uint8_t gas_range;
    uint16_t gas_adc;
} bme680_raw_t;

/**
 * @brief   Oversampling modes for temperature, pressure, humidity
 */
enum {
    BME680_OVERSAMPLING_1 = 0b001,
    BME680_OVERSAMPLING_2 = 0b010,
    BME680_OVERSAMPLING_4 = 0b011,
    BME680_OVERSAMPLING_8 = 0b100,
    BME680_OVERSAMPLING_16 = 0b101
};

/**
 * @brief   Filter coefficients
 */
enum {
    BME680_FILTER_COEFFICIENT_0 = 0b000,
    BME680_FILTER_COEFFICIENT_1 = 0b001,
    BME680_FILTER_COEFFICIENT_3 = 0b010,
    BME680_FILTER_COEFFICIENT_7 = 0b011,
    BME680_FILTER_COEFFICIENT_15 = 0b100,
    BME680_FILTER_COEFFICIENT_31 = 0b101,
    BME680_FILTER_COEFFICIENT_63 = 0b110,
    BME680_FILTER_COEFFICIENT_127 = 0b111
};

/**
 * @brief   Result data
 */
typedef struct {
    int32_t temperature;            /**< temperature in degree Celsius x 100 */
    uint32_t humidity;              /**< humidity in % */
    uint32_t pressure;              /**< pressure in Pascal */
    uint32_t gas_resistance;        /**< gas resistance in Ohm */
    uint8_t flags;                  /**< Flags e.g. indicating presence of gas resistance value */
} bme680_data_t;

/**
 * @brief Flags for use in @ref bme680_data_t::flags
 */
enum {
    BME680_FLAG_HAS_GAS_VALUE,      /**< Value for gas resistance is valid */
};

/**
 * @brief   Device descriptor for the driver
 */
typedef struct {
    /** Device initialization parameters */
    bme680_common_params_t params;
    /** Device calibration data */
    bme680_calib_t calib;
} bme680_common_t;

/**
 * @brief   Converts heating temperature to register value
 *
 * @param[inout] dev                Initialized device descriptor of BME680 device
 *
 * @return                          resulting register value
 */
uint8_t bme680_common_convert_res_heat(const bme680_common_t *dev);


/**
 * @brief   Converts adc values to @ref bme680_data_t result
 *
 * @param[inout] dev                Initialized device descriptor of BME680 device
 * @param[inout] dest               resulting data
 * @param[inout] raw_adc_values     raw adc values read from device
 *
 * @return                          0 on success
 * @return -EIO                     Error in reading/writing data
 */
void bme680_common_convert(const bme680_common_t *dev, bme680_data_t *dest, const bme680_raw_t *raw_adc_values);

/**
 * @brief   Calculation of heating duration register value
 *
 * @param[inout] dur                Target heating duration
 * @return                          register value of heating duration
 */
uint8_t bme680_common_calc_heater_dur(uint16_t dur);

#ifdef __cplusplus
}
#endif

#endif /* BME680_COMMON_H */
/** @} */
