#include <iostream>
#include <chrono>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <pcf8574.h>
#include "Devi.h"
#include "Sensors.h"
/*********************************************************************************************************
 * Devi class implementation file
 * Version of 14/07/2026
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
    _deviStatus = 0;
    _anemCount = 1;
    _rainCount = 1;
    _rainTare = 0.0;
    _vaneCount = 1;
    _bHXWorking = false;
}

double Devi::aveRainWeight() {
    int i, count = 0, lng = _rainWeights.size();
    double val = 0.0;
    for (i = 0; i < lng; i++) {
        val += _rainWeights[i];
        count++;
    }
    if (count == 0) return 0.0;
    else return val / count;
}

unsigned int Devi::setupDevices(DB dBase) {
    int i;
    double rw;
    // Generic setup
    if (wiringPiSetupGpio() == -1) {
        printf("WiringPi setup failed!\n");
        return false;
    }
    _db = dBase;
    _vals["Gu"] = _vals["Hm"] = _vals["Lt"] = _vals["Pr"] = _vals["Ra"] = _vals["Rv"] = _vals["Tp"] = 0.0;

    // Anemometer setup__________________________________________________________________________________
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
    _deviStatus = ANEM_STATUS;
    std::cout << "Anemometer setup completed." << std::endl;

    // Rain setup_____________________________________________________________________________________
    double sw = 0.0;
    _bHXWorking = _hx.readyToSend(); // CURRENTLY I ASSUME THIS WILL BE TRUE IF OK BECAUSE >400ms SINCE POWER UP
    if (_bHXWorking) {
        for (i = 0; i < RAIN_DEQ_LEN; i++) {
            rw = _hx.read() * HX711_RATIO; // HX711_RATIO is the "magic number" that converts sensor read() units to grams: TO BE CALIBRATED
            sw += rw;
        }
        _rainTare = sw / RAIN_DEQ_LEN; // in grams: START WEIGHT: ASSUMED EMPTY!
        _deviStatus += RAIN_STATUS;
    }
    _prevRainWeight = 0.0;
    _rainBank = 0.0;
    _bEmptying = false;
    double _rainFull = (double)_db.getPrefFloat("RainFull");
    float _rainRatio = _db.getPrefFloat("RainRatio"); // rain ratio converts grams weight to mm rain
    _prevRainMillis = millis();
    if (_bHXWorking) std::cout << "Rain gauge setup completed." << std::endl;
    else std::cout << "Rain gauge setup bypassed." << std::endl;

    // Sensors setup__________________________________________________________________________________
    bool b = _lightA.begin(BH1750_ADDR_A);
    b = b && _lightB.begin(BH1750_ADDR_B);
    if (b) _deviStatus += BH_STATUS;
    b = _bme.begin();
    if (b) _deviStatus += BME_STATUS;
    unsigned int mask = BH_STATUS + BME_STATUS;
    if ((_deviStatus & mask) == mask) std::cout << "Sensors setup completed." << std::endl;
    else {
        if ((_deviStatus & BH_STATUS) != 0) std::cout << "Light sensors only setup completed." << std::endl;
        else std::cout << "BME sensor only setup completed" << std::endl;
    }

    // Vane setup_____________________________________________________________________________________
    _vaneID = pcf8574Setup(PCF8574_BASE, PCF8574_ADDR);
    for (i = 0; i < 8; i++) {
        pinMode(PCF8574_BASE + i, INPUT);
        digitalWrite(PCF8574_BASE + i, HIGH); // enables internal pullup
    }
    std::cout << "VaneID:" << _vaneID << std::endl;
    b = b && (_vaneID > 0);
    float fcp = _db.getPrefFloat("CmpsPnts");
    _numCmpPts = (int)fcp;   // initiate direction counts vector
    for (i = 0; i <= _numCmpPts; i++) {
        _dirCounts.push_back(0);
        _dirHrCounts.push_back(0);
    }
    _prevVaneMillis = millis();
    if (_vaneID > 0) {
         _deviStatus += VANE_STATUS;
        std::cout << "Vane setup completed." << std::endl;
    }
    else std::cout << "Vane setup not done." << std::endl;
    
    return _deviStatus;
}

float Devi::getVal(std::string nm) { return _vals[nm]; }

std::vector<int> Devi::getWDCounts(bool bHourly) {
    if (bHourly) return _dirHrCounts;
    else return _dirCounts;
}

void Devi::resetWDCounts() {
    int i;
    for (i = 0; i < _dirCounts.size(); i++) _dirCounts[i] = 0;
}

// ANEMOMETER "LOOP" METHODS==========================================================================================

bool Devi::updateMaxGust() {
    int revsNow = isrRevs;
    int revsInc = revsNow - _gusts[_ixGust];
    revsInc = std::max(0, revsInc);  // no -ve values!
    _gusts[_ixGust] = revsNow;
  
    // Update maxGust prev max exceeded
    if (revsInc > _maxGust) {
        _maxGust = revsInc;
    }
    _ixGust = (_ixGust + 1) % GUSTPOLL_COUNT;
    return (_ixGust == 0);    
}

void Devi::anemTasks() { // called every 250ms
    int revsNow, revs15Inc;
    unsigned long millisNow;
    int intvl;
    bool bDoGust = updateMaxGust();
    float r, gu, ws;
    if (bDoGust) { // every 3secs
        //std::cout << "I am doing gusts" << std::endl;
        revsNow = isrRevs;
        revs15Inc = revsNow - _prevWSRevs;
        r = _db.getPrefFloat("WSMult");
        gu = r * _maxGust / 3.0;
        _db.updateLiveRow("Gu", 3000, gu);
        _vals["Gu"] = gu;
    }
    
    if (_anemCount == 0) { // every 15secs
        std::cout << "<AB:";
        millisNow = millis();
        intvl = millisNow - _prevAnemMillis;
        if (intvl > 0) ws = r * revs15Inc / intvl;
        else ws = 0.0;
        _prevAnemMillis = millisNow;
        _db.updateLiveRow("Rv", intvl, ws);
        _vals["Rv"] = ws;
        std::cout << "AE>" << std::endl;
    }
    _anemCount = (_anemCount + 1) % ANEM_LOOPS;
}

// RAIN "LOOP" METHODS================================================================================================

void Devi::startEmptying(double weight_g) {
    _bEmptying = true;
    _startEmpWeight = weight_g;
    // Take the necessary steps to start emptying the rain bucket (servo or valve)
    // ADD CODE
}

void Devi::stopEmptying(double weight_g) {
    // Take the necessary steps to stop emptying the rain bucket (servo or valve)
    // ADD CODE
    _rainBank += _startEmpWeight - weight_g;
    _rainTare = weight_g;
    _bEmptying = false;
}

void Devi::rainTasks() { // called every second
    int intvl;
    unsigned long nowMillis;
    // First update deque (list) of rain weight readings
    double one_rain_g = 0.0;//_hx.read() * HX711_RATIO; TEMP REM
    if (_rainWeights.size() >= RAIN_DEQ_LEN) _rainWeights.pop_front(); // restricts the list size to RAIN_LIST_LEN
    _rainWeights.push_back(one_rain_g);
    if (_rainCount == 0) {
        // The code lines below run once every RAIN_LOOPS times (currently 30secs) 
        double rain_g = aveRainWeight();
    
        double rainfall;
        if (_bEmptying) {
            std::cout << "[emptying]" ;
            if (rain_g < _rainTare + EMPTY_MARGIN) stopEmptying(rain_g);
        }
        else {
            std::cout << "<RB:";
            rainfall = rain_g - _prevRainWeight;
            if (rainfall > 0.0) {
                nowMillis = millis();
                intvl = nowMillis - _prevRainMillis;
                _prevRainMillis = nowMillis;
                _vals["Ra"] = (rain_g + _rainBank) * _rainRatio;
                _db.updateLiveRow("Ra", intvl,  _vals["Ra"]);
                _prevRainWeight = rain_g;
                if (rain_g > _rainTare + _rainFull) startEmptying(rain_g);
                std::cout << "'";
            }
        }
        std::cout << "RE>" << std::endl;
    }
    _rainCount = (_rainCount + 1) % RAIN_LOOPS;
}

// SENSOR "LOOP" METHODS===========================================================================================

void Devi::sensTasks() { // called every 30 secs
    int intvl;
    unsigned long millisNow = millis();
    intvl = millisNow - _prevSensMillis;
    _prevSensMillis = millisNow;
    std::cout << "<SB:";
    float lt = 0.5 * ( _lightA.getLux() + _lightB.getLux());
    bme_280_values vals = _bme.getValues();
    _db.updateLiveRow("Tp", intvl, vals.temp);
    _vals["Tp"] = vals.temp;
    _db.updateLiveRow("Hm", intvl, vals.humdty);
    _vals["Hm"] = vals.humdty;
    _db.updateLiveRow("Pr", intvl, vals.press);
    _vals["Pr"] = vals.press;
    _db.updateLiveRow("Lt", intvl, lt);
    _vals["Lt"] = lt;
    std::cout << "SE>" << std::endl;
}

// VANE "LOOP" METHODS==============================================================================================

void Devi::vaneTasks() { // called every 100ms
    /*std::cout << "vane begin" << std::endl;
    unsigned int p = wiringPiI2CRead(_vaneID); // ASSUMED TO BE SAME AS RICHARD'S pcf.digitalReadByte(); (Checked with Richard)
    //std::cout << "vane I2c read" << std::endl;
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
    // Keep track of both "now" and "hourly" counts
    _dirCounts[d]++;
    _dirHrCounts[d]++;
    
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
    _vaneCount = (_vaneCount + 1) % VANE_LOOPS;*/
}

