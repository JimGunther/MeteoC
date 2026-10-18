/**************************************************************************************** 
Devi.h: manages all device-handling code
Version of 18/10/2026 11:50
Written by Jim Gunther
****************************************************************************************/
#ifndef DEVI_H
#define DEVI_H

#include <vector>
#include <map>
#include <termios.h>
#include "Sensors.h"
#include "DB.h"
#include "Defines.h"

#define ANEM_STATUS 1
#define RAIN_STATUS 2
#define BME_STATUS 4
#define BH_STATUS 8
#define VANE_STATUS 16

#define BH1750_ADDR_A 0x23
#define BH1750_ADDR_B 0x5c
#define PCF8574_ADDR 0x27
#define PCF8574_BASE 0x80
#define ANEM_LOOPS 60
//#define RAIN_LOOPS 30
#define VANE_LOOPS 300
#define DEBNCE_MARGIN 5 // milliseconds
#define ANEM_PIN 23 // arbitrary
//#define EMPTY_MARGIN 0.02
//#define RAIN_DEQ_LEN 25
#define GUSTPOLL_COUNT 12

class Devi {
    public:
        Devi();
        unsigned int setupDevices(DB dBase);
		scoreboard getScoreboard();
        void getWDCounts(int *nvCounts, bool bHourly); // copies vol -> non-vol
        void resetWDCounts();
		bool updateMaxGust();
        void storeGust();
		void storeWSpeed();
        void rainTasks();
        void sensTasks();
        void vaneInc();
		void vaneStore();
		void resetRainBank();

    private:
        int openSerialPort(const char* portName);
		bool configureSerialPort(int fd, int speed);

        struct termios _oldtio, _newtio;
		DB _db;
        BH1750 _lightA;
        BH1750 _lightB;
        BME280 _bme;
        //HX711 _hx;
        std::map<std::string, float> _vals;
        unsigned int _deviStatus;
        //int _anemCount;
        int _gusts[GUSTPOLL_COUNT];
        volatile int _ixGust;
        volatile int _maxGust;
        int _prevWSRevs;
        unsigned long _prevAnemMillis;
        unsigned long _prevSensMillis;
		scoreboard _scBd;
		int _currTips, _prevTips;
		float _rainWeight, _prevRainWeight;
        float _rainRatio;
		float _rainBank;
		char _rainBuf[128];
		int _rainFD;
        int _vaneCount;
        int _vaneID;
		volatile int _dirCounts[NUM_CP];
        volatile int _dirHrCounts[NUM_CP];
        int _numCmpPts;
        unsigned long _prevVaneMillis;

};
#endif