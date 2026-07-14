#include <stdio.h>
#include <iostream>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <wiringShift.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include "Sensors.h"


BH1750::BH1750() { }

bool BH1750::begin(int addr) {
  _addr = addr;

  int fd = wiringPiI2CSetup(addr);
  if (fd == -1) {
        printf("I2C device not found!\n");
        return false;
  }
  _fd = fd;

  // Send measurement mode command
  wiringPiI2CWrite(fd, CONTINUOUS_HIGH_RES_MODE);
  delay(180); // Wait for measurement

  return true;
}

float BH1750::getLux() {
  float final_lux;  
  // Read 16-bit data
  int data = wiringPiI2CReadReg16(_fd, 0);
  //std::cout << "Lux raw data:" << data << std::endl;
    if (data != -1) {
      // BH1750 registers are LSB and MSB swapped
      int lux = ((data >> 8) & 0x00FF) | ((data << 8) & 0xFF00);
            
      // Calculate actual lux based on datasheet formula
      final_lux = lux / 1.2;
    }
    else {
      printf("Lux read error\n");
      return -1.0;
    }
    sleep(1);
    return final_lux;
}


//===================================================================================================================

BME280::BME280() { };

bool BME280::begin() {
  int fd = wiringPiI2CSetup(BME280_ADDRESS);
  if(fd < 0) {
    printf("Device not found");
    return false;
  }
  _fd = fd;

  readCalibrationData(_fd, &_cal);

  wiringPiI2CWriteReg8(fd, 0xf2, 0x01);   // humidity oversampling x 1
  wiringPiI2CWriteReg8(fd, 0xf4, 0x25);   // pressure and temperature oversampling x 1, mode normal
  return true;
}

bme_280_values BME280::getValues() {
  bme280_raw_data raw;
  bme_280_values vals;
  getRawData(_fd, &raw);

  int32_t t_fine = getTemperatureCalibration(&_cal, raw.temperature);
  float t = compensateTemperature(t_fine); // C
  float p = compensatePressure(raw.pressure, &_cal, t_fine) / 100; // hPa
  float h = compensateHumidity(raw.humidity, &_cal, t_fine);       // %

  vals.temp = t;
  vals.humdty = h;
  vals.press = p;
  return vals;

}

// Private helper methods__________________________________________________________________________________

int32_t BME280::getTemperatureCalibration(bme280_calib_data *cal, int32_t adc_T) {
  int32_t var1  = ((((adc_T>>3) - ((int32_t)cal->dig_T1 <<1))) *
     ((int32_t)cal->dig_T2)) >> 11;

  int32_t var2  = (((((adc_T>>4) - ((int32_t)cal->dig_T1)) *
       ((adc_T>>4) - ((int32_t)cal->dig_T1))) >> 12) *
     ((int32_t)cal->dig_T3)) >> 14;

  return var1 + var2;
}

void BME280::readCalibrationData(int fd, bme280_calib_data *data) {
  data->dig_T1 = (uint16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_T1);
  data->dig_T2 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_T2);
  data->dig_T3 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_T3);

  data->dig_P1 = (uint16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_P1);
  data->dig_P2 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_P2);
  data->dig_P3 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_P3);
  data->dig_P4 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_P4);
  data->dig_P5 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_P5);
  data->dig_P6 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_P6);
  data->dig_P7 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_P7);
  data->dig_P8 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_P8);
  data->dig_P9 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_P9);

  data->dig_H1 = (uint8_t)wiringPiI2CReadReg8(fd, BME280_REGISTER_DIG_H1);
  data->dig_H2 = (int16_t)wiringPiI2CReadReg16(fd, BME280_REGISTER_DIG_H2);
  data->dig_H3 = (uint8_t)wiringPiI2CReadReg8(fd, BME280_REGISTER_DIG_H3);
  data->dig_H4 = (wiringPiI2CReadReg8(fd, BME280_REGISTER_DIG_H4) << 4) | (wiringPiI2CReadReg8(fd, BME280_REGISTER_DIG_H4+1) & 0xF);
  data->dig_H5 = (wiringPiI2CReadReg8(fd, BME280_REGISTER_DIG_H5+1) << 4) | (wiringPiI2CReadReg8(fd, BME280_REGISTER_DIG_H5) >> 4);
  data->dig_H6 = (int8_t)wiringPiI2CReadReg8(fd, BME280_REGISTER_DIG_H6);
}

float BME280::compensateTemperature(int32_t t_fine) {
  float T  = (t_fine * 5 + 128) >> 8;
  return T/100;
}

float BME280::compensatePressure(int32_t adc_P, bme280_calib_data *cal, int32_t t_fine) {
  int64_t var1, var2, p;

  var1 = ((int64_t)t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)cal->dig_P6;
  var2 = var2 + ((var1*(int64_t)cal->dig_P5)<<17);
  var2 = var2 + (((int64_t)cal->dig_P4)<<35);
  var1 = ((var1 * var1 * (int64_t)cal->dig_P3)>>8) +
    ((var1 * (int64_t)cal->dig_P2)<<12);
  var1 = (((((int64_t)1)<<47)+var1))*((int64_t)cal->dig_P1)>>33;

  if (var1 == 0) {
    return 0;  // avoid exception caused by division by zero
  }
  p = 1048576 - adc_P;
  p = (((p<<31) - var2)*3125) / var1;
  var1 = (((int64_t)cal->dig_P9) * (p>>13) * (p>>13)) >> 25;
  var2 = (((int64_t)cal->dig_P8) * p) >> 19;

  p = ((p + var1 + var2) >> 8) + (((int64_t)cal->dig_P7)<<4);
  return (float)p/256;
}


float BME280::compensateHumidity(int32_t adc_H, bme280_calib_data *cal, int32_t t_fine) {
  int32_t v_x1_u32r;

  v_x1_u32r = (t_fine - ((int32_t)76800));

  v_x1_u32r = (((((adc_H << 14) - (((int32_t)cal->dig_H4) << 20) -
      (((int32_t)cal->dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) *
         (((((((v_x1_u32r * ((int32_t)cal->dig_H6)) >> 10) *
        (((v_x1_u32r * ((int32_t)cal->dig_H3)) >> 11) + ((int32_t)32768))) >> 10) +
      ((int32_t)2097152)) * ((int32_t)cal->dig_H2) + 8192) >> 14));

  v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
           ((int32_t)cal->dig_H1)) >> 4));

  v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
  v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
  float h = (v_x1_u32r>>12);
  return  h / 1024.0;
}

void BME280::getRawData(int fd, bme280_raw_data *raw) {
  wiringPiI2CWrite(fd, 0xf7);

  raw->pmsb = wiringPiI2CRead(fd);
  raw->plsb = wiringPiI2CRead(fd);
  raw->pxsb = wiringPiI2CRead(fd);

  raw->tmsb = wiringPiI2CRead(fd);
  raw->tlsb = wiringPiI2CRead(fd);
  raw->txsb = wiringPiI2CRead(fd);

  raw->hmsb = wiringPiI2CRead(fd);
  raw->hlsb = wiringPiI2CRead(fd);

  raw->temperature = 0;
  raw->temperature = (raw->temperature | raw->tmsb) << 8;
  raw->temperature = (raw->temperature | raw->tlsb) << 8;
  raw->temperature = (raw->temperature | raw->txsb) >> 4;

  raw->pressure = 0;
  raw->pressure = (raw->pressure | raw->pmsb) << 8;
  raw->pressure = (raw->pressure | raw->plsb) << 8;
  raw->pressure = (raw->pressure | raw->pxsb) >> 4;

  raw->humidity = 0;
  raw->humidity = (raw->humidity | raw->hmsb) << 8;
  raw->humidity = (raw->humidity | raw->hlsb);
}

//============================================================================================

HX711::HX711() {
  _clockPin  = RN_CLOCK_PIN;
  _outPin  = RN_DATA_PIN;
  _gain = std::byte{1};
  _pinsConfigured = false;
}

HX711::~HX711() {
}

bool HX711::readyToSend() {
  if (!_pinsConfigured) {
    // We need to set the pin mode once, but not in the constructor
    pinMode(_clockPin, OUTPUT);
    pinMode(_outPin, INPUT);
    _pinsConfigured = true;
  }
  return digitalRead(_outPin) == LOW;
}

void HX711::setGain(int gain) {
  switch (gain) {
    case 128:
      _gain = std::byte{1};
      break;
    case 64:
      _gain = std::byte{3};
      break;
    case 32:
      _gain = std::byte{2};
      break;
  }

  digitalWrite(_clockPin, LOW);
  read();
}

long HX711::read() {
   while (!readyToSend());

  std::byte data[3];

  for (int j = 3; j--;) {
      data[j] = std::byte{shiftIn(_outPin,_clockPin, MSBFIRST)};
  }

  // set gain
  for (int i = 0; i < (int)_gain; i++) {
    digitalWrite(_clockPin, HIGH);
    digitalWrite(_clockPin, LOW);
  }

  data[2] = std::byte((int)data[2] ^ 0x80);
  return ((uint32_t) data[2] << 16) | ((uint32_t) data[1] << 8) | (uint32_t) data[0];
}
