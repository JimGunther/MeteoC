#include <iostream>
#include <chrono>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include "Devi.h"
#include "Sensors.h"
/*********************************************************************************************************
 * Devi class implementation file
 * Version of 06/07/2026
 * Written by Jim Gunther
**********************************************************************************************************/
volatile int isrRevs;
volatile unsigned int prevISR;

void anemISR() {
    unsigned int nowTime = millis();

    if ((nowTime - prevISR) > DEB_MARGIN) { // DEBOUNCE GUARD
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
    _prevAnemMillis = millis();

    // ADD HERE

    // Rain setup
    b == b && b;
    // ADD HERE

    // Sensors setup
    b = b && _lightA.begin(BH1750_ADDR_A);
    b = b && _lightB.begin(BH1750_ADDR_B);
    b = b && _bme.begin();
    
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

void Devi::anemTasks() {
    int revsNow, revs15Inc;
    unsigned long millisNow;
    int intvl;
    updateMaxGust();
    if (_anemCount == 0) {
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

void Devi::rainTasks() {
    std::cout << "I am doing rain" << std::endl;
}

void Devi::sensTasks() {
    float lt = 0.5 * ( _lightA.getLux() + _lightB.getLux());
    bme_280_values vals = _bme.getValues();
    std::cout << "L:" << lt << " T:" << vals.temp << " H:"<< vals.humdty << " P:" << vals.press << std::endl;
}

void Devi::vaneTasks() {
    //std::cout << "I am doing vane" << std::endl;
}

