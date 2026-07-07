/* 
Devi.h: manages all device-handling code
Version of 06/07/2026
Written by Jim Gunther
*/
#ifndef DEVI_H
#define DEVI_H

#include "Sensors.h"
#include "DB.h"

#define BH1750_ADDR_A 0x23
#define BH1750_ADDR_B 0x5c
#define ANEM_LOOPS 60
#define VANE_LOOPS 150
#define DEB_MARGIN 5 // milliseconds
#define ANEM_PIN 23
#define GUSTPOLL_COUNT 12

class Devi {
    public:
        Devi();
        bool setupDevices(DB dBase);
        void anemTasks();
        void rainTasks();
        void sensTasks();
        void vaneTasks();

    private:
        void updateMaxGust();

        DB _db;
        BH1750 _lightA;
        BH1750 _lightB;
        BME280 _bme;
        int _anemCount;
        int _gusts[GUSTPOLL_COUNT];
        int _ixGust;
        int _maxGust;
        int _prevWSRevs;
        unsigned long _prevAnemMillis;
        int _vaneCount;

};
#endif