/**************************************************************************************** 
Devi.h: manages all device-handling code
Version of 13/07/2026
Written by Jim Gunther
****************************************************************************************/
#ifndef DEVI_H
#define DEVI_H

#include <vector>
#include <map>
#include <deque>
#include "Sensors.h"
#include "DB.h"

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
#define RAIN_LOOPS 30
#define VANE_LOOPS 300
#define DEBNCE_MARGIN 5 // milliseconds
#define ANEM_PIN 23 // arbitrary
#define EMPTY_MARGIN 0.02
#define RAIN_DEQ_LEN 25
#define GUSTPOLL_COUNT 12

class Devi {
    public:
        Devi();
        unsigned int setupDevices(DB dBase);
        float getVal(std::string nm);
        std::vector<int> getWDCounts(bool bHourly);
        void resetWDCounts();
        void anemTasks();
        void rainTasks();
        void sensTasks();
        void vaneTasks();

    private:
        bool updateMaxGust();
        void startEmptying(double weight_mg);
        void stopEmptying(double weight_mg);
        double aveRainWeight();

        DB _db;
        BH1750 _lightA;
        BH1750 _lightB;
        BME280 _bme;
        HX711 _hx;
        std::map<std::string, float> _vals;
        unsigned int _deviStatus;
        int _anemCount;
        int _gusts[GUSTPOLL_COUNT];
        int _ixGust;
        int _maxGust;
        int _prevWSRevs;
        unsigned long _prevAnemMillis;
        unsigned long _prevSensMillis;
        bool _bHXWorking;
        std::deque<double> _rainWeights;
        double _rainFull;
        float _rainRatio;
        double _rainTare;
        double _prevRainWeight;
        unsigned long _prevRainMillis;
        double _startEmpWeight;
        double _rainBank;
        int _rainCount;
        bool _bEmptying;
        int _vaneCount;
        int _vaneID;
        std::vector<int> _dirCounts;
        std::vector<int> _dirHrCounts;
        int _numCmpPts;
        unsigned long _prevVaneMillis;

};
#endif