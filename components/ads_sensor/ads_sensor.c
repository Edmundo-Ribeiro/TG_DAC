
#include <ads_sensor.h>
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



conversion_parameters select_mv_to_temperature_parameters(double v){

    static const conversion_parameters PARAMETER_TABLE[] = {
        {-1.2147164E+02, -4.1790858E+00, 3.6069513E+01, 3.0722076E+01, 7.7913860E+00, 5.2593991E-01, 9.3939547E-01, 2.7791285E-01, 2.5163349E-02},
        {-8.7935962E+00, -3.4489914E-01, 2.5678719E+01, -4.9887904E-01, -4.4705222E-01, -4.4869203E-02, 2.3893439E-04, -2.0397750E-02, -1.8424107E-03},
        {3.1018976E+02, 1.2631386E+01, 2.4061949E+01, 4.0158622E+00, 2.6853917E-01, -9.7188544E-03, 1.6995872E-01, 1.1413069E-02, -3.9275155E-04},
        {6.0572562E+02, 2.5148718E+01, 2.3539401E+01, 4.6547228E-02, 1.3444400E-02, 5.9236853E-04, 8.3445513E-04, 4.6121445E-04, 2.5488122E-05},
        {1.0184705E+03, 4.1993851E+01, 2.5783239E+01, -1.8363403E+00, 5.6176662E-02, 1.8532400E-04, -7.4803355E-02, 2.3841860E-03, 0.0}
    };

    conversion_parameters c;

    if(-3.554 <= v && v < 4.096)
        c = PARAMETER_TABLE[RANGE_N3V554_4V096];
    else if(4.096 <= v && v < 16.397)
        c = PARAMETER_TABLE[RANGE_4V096_16V397];
    else if(16.397 <= v && v < 33.275 )
        c = PARAMETER_TABLE[RANGE_16V397_33V275];
    else if(33.275 <= v && v < 69.553 )
        c = PARAMETER_TABLE[RANGE_33V275_69V553];
    else if(-6.404 <= v && v < -3.554 )
        c = PARAMETER_TABLE[RANGE_N6V404_N3V554];
    
    return c;
}


double mv_to_temperature(double v){
    conversion_parameters c;
    double v_v0, num,den,T;

    c = select_mv_to_temperature_parameters(v);

    v_v0 = v - c.V0;

    num = v_v0*(c.P1 + v_v0*(c.P2 + v_v0 * (c.P3 + (c.P4 * (v_v0)))));
    den = 1.0 + v_v0*(c.Q1 + v_v0*(c.Q2+(c.Q3*v_v0)));
    T = c.T0 + num/den;
    return T;
}

double temperature_to_mv(double t){
    
    static const conversion_parameters INVERSE_PARAMETER_TABLE[] = {
        {2.5000000E+01, 1.0003453E+00,4.0514854E-02,-3.8789638E-05, -2.8608478E-06,-9.5367041E-10,-1.3948675E-03,-6.7976627E-05,0.0}
    };
    conversion_parameters c;
    double t_t0, num,den, mv;

    c = INVERSE_PARAMETER_TABLE[RANGE_N0V778_2V851];


    t_t0 = t - c.T0;

    num = t_t0*(c.P1 + t_t0*(c.P2 + t_t0 * (c.P3 + (c.P4 * (t_t0)))));
    den = 1.0 + t_t0*(c.Q1 + (c.Q2*t_t0));
    mv = c.V0 + num/den;
    return mv;
}

double raw_to_mv(int16_t raw, ads111x_gain_t gain){
    return (double) 1000.0* (ads111x_gain_values[gain] /ADS111X_MAX_VALUE) * raw;
}

double _get_compensated_temperature(int16_t raw_T,ads111x_gain_t themocouple_gain,int16_t raw_CJ,ads111x_gain_t could_junction_gain){

    double thermocouple_mv = raw_to_mv(raw_T,themocouple_gain); //transform raw values from ADC into a voltage in mV
    double cj_T = 0.1 * raw_to_mv(raw_CJ,could_junction_gain);// transform raw value from ADC into a temperature (LM35 sensor -> Vout = 10 C°/mV * T)
    
    //Convert could junction temperature to voltage
    double cj_mv  = temperature_to_mv(cj_T);

    printf("TC:%lfmv ______ CJ:%lfmv\n",thermocouple_mv,cj_T*10);
    //Compensate could junction
    thermocouple_mv += cj_mv;
    
    
    //Convert compensated thermocouple voltage to temperature
    double thermocouple_T = mv_to_temperature(thermocouple_mv);
    
    return thermocouple_T;

}

double get_compensated_temperature(compensated_thermocouple* tt){
    return _get_compensated_temperature(tt->tc->value, tt->tc->gain, tt->cj->value, tt->cj->gain);
}

i2c_dev_t* fake_ads_create(uint8_t address, ads111x_data_rate_t data_rate, ads111x_mode_t mode){
    i2c_dev_t *ads = malloc(sizeof(i2c_dev_t));
    memset(ads, 0, sizeof(i2c_dev_t));//MUITO IMPORTANTE !!!!

    if (ads == NULL){
        ESP_LOGE(TAG_TC,"Couldn't allocate memory for i2c device");
        return NULL;
    }
    return ads;
}

i2c_dev_t* ads_create(uint8_t address, ads111x_data_rate_t data_rate, ads111x_mode_t mode){
    i2c_dev_t *ads = malloc(sizeof(i2c_dev_t));
    memset(ads, 0, sizeof(i2c_dev_t));//MUITO IMPORTANTE !!!!

    if (ads == NULL){
        ESP_LOGE(TAG_TC,"Couldn't allocate memory for i2c device");
        return NULL;
    }
    ESP_ERROR_CHECK( ads111x_init_desc(ads, address, I2C_PORT, SDA_IO_NUM, SCL_IO_NUM));
    ESP_ERROR_CHECK( ads111x_set_data_rate(ads, data_rate));
    ESP_ERROR_CHECK( ads111x_set_mode(ads, mode));
    return ads;
}


ads_sensor* fake_ads_sensor_create(uint8_t id, i2c_dev_t* ads , ads111x_gain_t gain, ads111x_mux_t mux){
    return ads_sensor_create(id,ads,gain,mux);
}

ads_sensor* ads_sensor_create(uint8_t id, i2c_dev_t* ads , ads111x_gain_t gain, ads111x_mux_t mux){

    ads_sensor* sensor = malloc(sizeof(i2c_dev_t));

    memset(sensor, 0, sizeof(ads_sensor));
    
    if (sensor == NULL){
        ESP_LOGE(TAG_TC,"Couldn't allocate memory for sensor");
        return NULL;
    }
    
    sensor->ads = ads;
    sensor->id = id;
    sensor->mux = mux;
    sensor->gain = gain;
    
    return sensor;
}

compensated_thermocouple * themocouple_create(ads_sensor *tc, ads_sensor *cj){
    compensated_thermocouple* t =malloc(sizeof(compensated_thermocouple));
    t->tc = tc;
    t->cj = cj;
    return t;
}


void wait_while_busy(i2c_dev_t* ads){
    bool busy;
    uint16_t busy_count = 0;
    do{
        ESP_ERROR_CHECK(ads111x_is_busy(ads, &busy));
        busy_count+=1;
    }while(busy);
    ESP_LOGD(TAG_TC,"Busy count: %d\n", busy_count);
}


esp_err_t fake_ads_sensor_read(ads_sensor* sensor){
    sensor->value = rand()%32768;
    sensor->timestamp = esp_timer_get_time();
    
    // ESP_LOGI(TAG_TC,"FAKE read ADC[%u].%u raw value: {%d} - {%x}",sensor->ads->addr,sensor->mux, sensor->value, sensor->value);
    return ESP_OK;
}


esp_err_t ads_sensor_read(ads_sensor* sensor){
    int16_t raw = 0;
    bool busy;
    uint16_t busy_count = 0;

    ESP_ERROR_CHECK( ads111x_set_input_mux(sensor->ads, sensor->mux));
    wait_while_busy(sensor->ads);
    
    ESP_ERROR_CHECK( ads111x_set_gain(sensor->ads,sensor->gain));
    wait_while_busy(sensor->ads);
    
    esp_err_t err = ads111x_get_value(sensor->ads, &raw);
    
    if(err == ESP_OK){
        sensor->value = raw;
        sensor->timestamp = esp_timer_get_time();
        // ESP_LOGI(TAG_TC,"Read ADC[%u].%u raw value: {%d} - {%x}",sensor->ads->addr,sensor->mux, raw, raw);
    }
    else
        ESP_LOGE(TAG_TC,"Could not read ADC[%u].%u",sensor->ads->addr,sensor->mux);

    return err;
}

void compensated_thermocouple_read(compensated_thermocouple* t){
    ESP_ERROR_CHECK(ads_sensor_read(t->tc));
    ESP_ERROR_CHECK(ads_sensor_read(t->cj));
}

sensor_data extract_data(ads_sensor* sensor){
    sensor_data data = {
        .id = sensor->id,
        .value = sensor->value,
        .timestamp = sensor->timestamp
    };
    return data;
}
