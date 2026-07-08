#include <iostream>
#include <chrono>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <pcf8574.h>
#include "Devi.h"
#include "Sensors.h"
/*********************************************************************************************************
 * Devi class implementation file
 * Version of 07/07/2026
 * Written by Jim Gunther
**********************************************************************************************************/
volatile int isrRevs;
volatile unsigned int prevISR;

void anemISR() {
    unsigned int nowTime = millis();

    if ((nowTime - prevISR) > DEBNCE_MARGIN) { // DEBOUNCE GUARD
        isrRevs++;
        prevISR = nowTime;
    }
}


Devi::Devi() {
    _anemCount = 1;
    _vaneCount = 1;
}

bool Devi::setupDevices(DB dBase) {
    int i;
    // Generic setup
    if (wiringPiSetupGpio() == -1) {
        printf("WiringPi setup failed!\n");
        return false;
    }
    _db = dBase;

    // Anemometer setup
    bool b = true;
    isrRevs = 0;
    prevISR = millis();
    pinMode(23, INPUT);
    pullUpDnControl(ANEM_PIN, PUD_UP);
    wiringPiISR(ANEM_PIN, INT_EDGE_FALLING, anemISR);
    for (i = 0; i < GUSTPOLL_COUNT; i++) _gusts[i] = 0;
    _ixGust = 0;
    _maxGust = 0;
    _prevWSRevs = 0;
    _prevAnemMillis = _prevSensMillis = millis();

    // ADD HERE

    // Rain setup
    b == b && b;
    // ADD HERE

    // Sensors setup
    b = b && _lightA.begin(BH1750_ADDR_A);
    b = b && _lightB.begin(BH1750_ADDR_B);
    b = b && _bme.begin();

    // Vane setup
    _vaneID = pcf8574Setup(PCF8574_BASE, PCF8574_ADDR);
    b = b && (_vaneID > 0);
    float fcp = _db.getPrefFloat("CmpsPnts");
    _numCmpPts = (int)fcp;   // initiate direction counts vector
    for (i = 0; i <= _numCmpPts; i++) {
        _dirCounts.push_back(0);
    }
    _prevVaneMillis = millis();
    
    return b;
}

void Devi::updateMaxGust() {
    int revsNow = isrRevs;
    int revsInc = revsNow - _gusts[_ixGust];
    revsInc = std::max(0, revsInc);  // no -ve values!
    _gusts[_ixGust] = revsNow;
  
    // Update maxGust prev max exceeded
    if (revsInc > _maxGust) {
        _maxGust = revsInc;
    }
    _ixGust = (_ixGust + 1) % GUSTPOLL_COUNT;    
}

void Devi::anemTasks() { // called every 250ms
    int revsNow, revs15Inc;
    unsigned long millisNow;
    int intvl;
    updateMaxGust();
    if (_anemCount == 0) { // every 15secs
        float r, gu, ws;
        std::cout << "I am doing anem" << std::endl;
        revsNow = isrRevs;
        revs15Inc = revsNow - _prevWSRevs;
        r = _db.getPrefFloat("WSMult");
        gu = r * _maxGust / 3.0;
        _db.updateLiveRow("Gu", 3000, gu);
        millisNow = millis();
        intvl = millisNow - _prevAnemMillis;
        if (intvl > 0) ws = r * revs15Inc / intvl;
        else ws = 0.0;
        _prevAnemMillis = millisNow;
        _db.updateLiveRow("Rv", intvl, ws);
    }
    _anemCount = (_anemCount + 1) % ANEM_LOOPS;
}

void Devi::rainTasks() { // called every 30 secs
    std::cout << "I am doing rain" << std::endl;
}

void Devi::sensTasks() { // called every 30 secs
    int intvl;
    unsigned long millisNow = millis();
    intvl = millisNow - _prevSensMillis;
    _prevSensMillis = millisNow;
    float lt = 0.5 * ( _lightA.getLux() + _lightB.getLux());
    bme_280_values vals = _bme.getValues();
    std::cout << "L:" << lt << " T:" << vals.temp << " H:"<< vals.humdty << " P:" << vals.press << std::endl;
    _db.updateLiveRow("Tp", intvl, vals.temp);
    _db.updateLiveRow("Hm", intvl, vals.humdty);
    _db.updateLiveRow("Pr", intvl, vals.press);
    _db.updateLiveRow("Lt", intvl, lt);
}

void Devi::vaneTasks() { // called every 100ms
    unsigned int p = wiringPiI2CRead(_vaneID); // ASSUMED TO BE SAME AS RICHARD'S pcf.digitalReadByte(); CHECK WITH RICHARD!
    int d;
    switch( p ) {
      case 1:     d = 0;     break;    //  1
      case 3:     d = 1;     break;    //  1 + 2
      case 2:     d = 2;     break;    //  2
      case 6:     d = 3;     break;    //  2 + 4
      case 4:     d = 4;     break;    //  4
      case 12:    d = 5;     break;    //  4 + 8
      case 8:     d = 6;     break;    //  8
      case 24:    d = 7;     break;    //  8 + 16
      case 16:    d = 8;     break;    // 16
      case 48:    d = 9;     break;    // 16 + 32
      case 32:    d = 10;    break;    // 32
      case 96:    d = 11;    break;    // 32 + 64
      case 64:    d = 12;    break;    // 64
      case 192:   d = 13;    break;    // 64 + 128
      case 128:   d = 14;    break;    //128
      case 129:   d = 15;    break;    //128 + 1
      default:    d = 16;    break;    //"dustbin"
    }
    _dirCounts[d]++;
    
    if (_vaneCount == 0) {
        // Update the database with new counts
        int intvl;
        unsigned long millisNow = millis();
        intvl = millisNow - _prevVaneMillis;
        _prevVaneMillis = millisNow;
        _db.updateLiveWD(intvl, _dirCounts);
        // reset vector values to 0
        int i;
        for (i = 0; i <= _numCmpPts; i++) _dirCounts[i] = 0;
        std::cout << "I am doing vane" << std::endl;
    }
    _vaneCount = (_vaneCount + 1) % VANE_LOOPS;
}

