#ifndef __ADS_SENSOR_H__
#define __ADS_SENSOR_H__

#include "ads111x.h"
#include <sys/time.h>
#include <string.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_random.h>
#include <stdio.h>


//-----I2C ---------------------------------------
#define I2C_PORT 0
#define SDA_IO_NUM 21
#define SCL_IO_NUM 22
//------------------------------------------------
#define TAG_TC "Ads_sensor"
#define MIN_READ_TIME 100


typedef struct{
    uint8_t id;
    int16_t value; //last measured value
    uint64_t timestamp;//last measured value timestamp
    uint32_t time_between_reads;//last measured value timestamp
    i2c_dev_t *ads;
    ads111x_mux_t mux;
    ads111x_gain_t gain;
    char name[12];
    char full_current_file_path[40];
} ads_sensor;

typedef struct {
    uint8_t id;
    int16_t value;
    uint64_t timestamp;
    char full_current_file_path[40];
} sensor_data;

typedef struct{
    ads_sensor* tc;
    ads_sensor* cj;
} compensated_thermocouple;


typedef struct {
    float T0, V0, P1, P2, P3, P4, Q1, Q2, Q3;
} conversion_parameters;


typedef enum{
    RANGE_N6V404_N3V554 = 0,
    RANGE_N3V554_4V096,
    RANGE_4V096_16V397,
    RANGE_16V397_33V275,
    RANGE_33V275_69V553
} voltage_range;


typedef enum{
    RANGE_N0V778_2V851 = 0
} inverse_voltage_range;






/*

Voltage:	-6.404 to -3.554 mV	-3.554 to 4.096mV 4.096 to 16.397 mV	16.397 to 33.275 mV	 33.275 to 69.553 mV
Temperature:	-250 to -100°C	-100 to 100°C	    100 to 400°C	        400 to 800°C	    800 to 1200°C
Coefficients    
T0	            -1.2147164E+02	-8.7935962E+00	    3.1018976E+02	        6.0572562E+02	    1.0184705E+03
V0	            -4.1790858E+00	-3.4489914E-01	    1.2631386E+01	        2.5148718E+01	    4.1993851E+01
p1	            3.6069513E+01	2.5678719E+01	    2.4061949E+01	        2.3539401E+01	    2.5783239E+01
p2	            3.0722076E+01	-4.9887904E-01	    4.0158622E+00	        4.6547228E-02	    -1.8363403E+00
p3	            7.7913860E+00	-4.4705222E-01	    2.6853917E-01	        1.3444400E-02	    5.6176662E-02
p4	            5.2593991E-01	-4.4869203E-02	    -9.7188544E-03	        5.9236853E-04	    1.8532400E-04
q1	            9.3939547E-01	2.3893439E-04	    1.6995872E-01	        8.3445513E-04	    -7.4803355E-02
q2	            2.7791285E-01	-2.0397750E-02	    1.1413069E-02	        4.6121445E-04	    2.3841860E-03
q3	            2.5163349E-02	-1.8424107E-03	    -3.9275155E-04	        2.5488122E-05	    0.0
*/



conversion_parameters select_mv_to_temperature_parameters(double v);

double mv_to_temperature(double v);

double temperature_to_mv(double t);

double raw_to_mv(int16_t raw, ads111x_gain_t gain);

double _get_compensated_temperature(int16_t raw_T,ads111x_gain_t themocouple_gain,int16_t raw_CJ,ads111x_gain_t could_junction_gain);

double get_compensated_temperature(compensated_thermocouple* tt);

i2c_dev_t* ads_create(uint8_t address, ads111x_data_rate_t data_rate, ads111x_mode_t mode);

ads_sensor* ads_sensor_create(char * name, uint8_t id, i2c_dev_t* ads , ads111x_gain_t gain, ads111x_mux_t mux,uint64_t time_interval );
compensated_thermocouple * themocouple_create(ads_sensor *tc, ads_sensor *cj);


esp_err_t ads_sensor_read(ads_sensor* sensor);

void compensated_thermocouple_read(compensated_thermocouple* t);

sensor_data extract_data(ads_sensor* sensor);


esp_err_t fake_ads_sensor_read(ads_sensor* sensor);
i2c_dev_t* fake_ads_create(uint8_t address, ads111x_data_rate_t data_rate, ads111x_mode_t mode);

uint64_t get_timestamp();

int getRandomInteger(int minValue, int maxValue);

#endif