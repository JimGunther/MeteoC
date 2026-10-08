#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <wiringPi.h>
#include <wiringPiI2C.h>
//#include <wiringSerial.h>
#include <pcf8574.h>
#include <cstring>
//#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
//using namespace std;

#include "Devi.h"
#include "Sensors.h"
/*********************************************************************************************************
 * Devi class implementation file
 * Version of 08/10/2026 15:48
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
    //_rainCount = 1;
    //_rainTare = 0.0;
    _vaneCount = 1;
    _bHXWorking = false;
}

// Function to open the serial port
int Devi::openSerialPort(const char* portname) {
    int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        std::cerr << "Error opening " << portname << ": "
             << strerror(errno) << std::endl;
        return -1;
    }
    return fd;
}

// Function to configure the serial port
bool Devi::configureSerialPort(int fd, int speed) {
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "Error from tcgetattr: " << strerror(errno) << std::endl;
        return false;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit characters
    tty.c_iflag &= ~IGNBRK; // disable break processing
    tty.c_lflag = 0; // no signaling chars, no echo, no
                     // canonical processing
    tty.c_oflag = 0; // no remapping, no delays
    tty.c_cc[VMIN] = 0; // read doesn't block
    tty.c_cc[VTIME] = 5; // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

    tty.c_cflag |= (CLOCAL | CREAD); // ignore modem controls, enable reading
    tty.c_cflag &= ~(PARENB | PARODD); // shut off parity
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "Error from tcsetattr: " << strerror(errno) << std::endl;
        return false;
    }
    return true;
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
	_rainRatio = _db.getPrefFloat("RainRatio");
	_rainBank = 0.0f;
	_rainFD = openSerialPort("/dev/serial0"); // NAME OF DEVICE UNCERTAIN!!
    bool bOK = (_rainFD >= 0);
	bOK = bOK && configureSerialPort(_rainFD, 115200);
	if (bOK) _deviStatus += RAIN_STATUS;
    if (bOK) std::cout << "Rain gauge setup completed." << std::endl;
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
    //std::cout << "VaneID:" << _vaneID << std::endl;
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

//float Devi::getVal(std::string nm) { return _vals[nm]; }
scoreboard Devi::getScoreboard() { return _scBd; }

std::vector<int> Devi::getWDCounts(bool bHourly) {     if (bHourly) return _dirHrCounts;
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
        _db.updateLiveRow("Gu", gu);
        _vals["Gu"] = gu;
    }
    
    if (_anemCount == 0) { // every 15secs
        std::cout << "<AB:" << std::flush;
        millisNow = millis();
        intvl = millisNow - _prevAnemMillis;
        if (intvl > 0) ws = r * revs15Inc / intvl;
        else ws = 0.0;
        _prevAnemMillis = millisNow;
        _db.updateLiveRow("Rv", ws);
        _vals["Rv"] = ws;
		unsigned long millisEnd = millis();
        std::cout << (millisEnd - millisNow) << "AE>" << std::flush;
    }
    _anemCount = (_anemCount + 1) % ANEM_LOOPS;
}

// RAIN "LOOP" METHODS================================================================================================

void Devi::rainTasks() { // called every second(?)
	std::cout << "<RS:";
	int num = read(_rainFD, _rainBuf, sizeof(_rainBuf));
	if (num > 0) {
		//Read it!
		std::string s = _rainBuf;
		std::stringstream ss(s);
		std::string itm;
		std::vector<std::string> vect;
		while (getline(ss, itm, ',')) {
			vect.push_back(itm);
		}
		_currTips = stoi(vect[2]);
		_rainWeight = stof(vect[7]) * _rainRatio + _rainBank;
		if (_currTips > _prevTips) {	// bucket has emptied
			_rainBank += _prevRainWeight;
		}
		_prevRainWeight = _rainWeight;
		_prevTips = _currTips;		
	}       
    std::cout << "RE>";
}

void Devi::resetRainBank() { _rainBank = 0.0f; }
	

// SENSOR "LOOP" METHODS===========================================================================================

void Devi::sensTasks() { // called every 30 secs
    int intvl;
    unsigned long millisNow = millis();
    intvl = millisNow - _prevSensMillis;
    _prevSensMillis = millisNow;
    std::cout << "<SB:" << std::flush;
    float lt = 0.5 * ( _lightA.getLux() + _lightB.getLux());
    bme_280_values vals = _bme.getValues();
    // Load scoreboard values
	_scBd.temp = vals.temp;
    _scBd.humdty = vals.humdty;
    _scBd.press = vals.press;
    _scBd.light = lt;
	unsigned long millisEnd = millis();
    std::cout << (millisEnd - millisNow) << "SE>" << std::flush;
}

// VANE "LOOP" METHODS==============================================================================================

void Devi::vaneTasks() { // called every 100ms

    unsigned int p = wiringPiI2CRead(_vaneID); // ASSUMED TO BE SAME AS RICHARD'S pcf.digitalReadByte(); (Checked with Richard)
    
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
		std::cout << "<VB:" << std::flush;
        // Update the database with new counts
        int intvl;
        unsigned long millisNow = millis();
        intvl = millisNow - _prevVaneMillis;
        _prevVaneMillis = millisNow;
        _db.updateLiveWD(intvl, _dirCounts);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "VE>" << std::flush;
    }
    _vaneCount = (_vaneCount + 1) % VANE_LOOPS;
}

