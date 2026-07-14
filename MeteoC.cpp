#include <iostream>
#include <chrono>
#include <thread>
#include <functional>
#include <map>
#include <vector>
//#include <string>
#include <wiringPi.h>
#include "Devi.h"
#include "DB.h"
/*
MeteoC.cpp: C++ version of code to run Meteo WS
Version as at 13/07/2026
Written by Jim Gunther
*/

using namespace std;

class Tim {
  public:
  Tim() { };

  void timer_start(std::function<void(void)> func, unsigned int interval)
  {
    std::thread([func, interval]()
    { 
      while (true)
      { 
        auto x = std::chrono::steady_clock::now() + std::chrono::milliseconds(interval);
        func();
        std::this_thread::sleep_until(x);
      }
    }).detach();
  };
};


#define ANEM_INTVL 250
#define RAIN_INTVL 1000 
#define SENS_INTVL 30000
#define VANE_INTVL 100
#define CLOCK_INTVL 30000

Devi dv;
DB db;
time_t setupTime;
int nowCount;
int nowWDCount;
int hrCount;

void anem() { dv.anemTasks(); }
void rain() { dv.rainTasks(); }
void sens() { dv.sensTasks(); }
void vane() { dv.vaneTasks(); }

void saveNowVals() {
    /*saveNowVals(): method to store data from map DB::_vals into 2 data tables: NowValues and WDNow
    parameters: none
    returns: void [float: percentage of "invalid" wind direction events measured ]
    */
    // First, save non-vane values
    vector<float> v;
    std::cout << "SNV start" << std::endl;
    v.push_back(dv.getVal("Ra"));
    v.push_back(dv.getVal("Rv"));
    v.push_back(dv.getVal("Gu"));
    v.push_back(dv.getVal("Tp"));
    v.push_back(dv.getVal("Hm"));
    v.push_back(dv.getVal("Pr"));
    v.push_back(dv.getVal("Lt"));
    bool ok = db.addNowRow(v);
    if (ok) nowCount++;
    else db.addMessageEntry(db.error(), true);
     
    // And now, vane values
    std::vector<int> wdCounts = dv.getWDCounts(false);
    int n = (int)db.getPrefFloat("CmpsPnts");
    int ctInvalid = wdCounts[n];
    ok = db.addWDRow(wdCounts, false);
    if (ok) nowWDCount++;
    else {
        std::string errMsg = db.error() + " (addWDRow now)";
        db.addMessageEntry(errMsg, true);
    }
    dv.resetWDCounts();
    if ((ctInvalid > 8) && (ctInvalid < 144)) {
        db.addMessageEntry("Invalid wind direction for " + std::to_string(ctInvalid) + " times", false);
    }
    std::cout << "SNV end" << std::endl;
}
    
bool doHourly(int currHr) {
    /*doHourly(): if a new hour, does hourly calculations
    parameters: none
    returns: bool: true if hour "ticked by"
    */
   // WARNING! I THINK THIS CODE CAUSES A SEGMENTATION FAULT NEEDS CHECKING (13/07)
    int ri = (int)db.getPrefFloat("RptIntvl");
    int expectedCount = int(14400 / ri);
    int diff = expectedCount - nowCount;
    if (diff > 2) {
        std::string s = "Expected records: " + std::to_string(expectedCount) + "; Actual records: " + std::to_string(nowCount) + " in hour " + std::to_string(currHr);
        db.addMessageEntry(s, false);
    }

    if (nowCount > 0) { // no "Now" records this hour
        std::vector<float> hv = db.hourAggregates(); //hv is 3-value vector
        //Construct the rest of the vector from current values
        hv.push_back(dv.getVal("Tp"));
        hv.push_back(dv.getVal("Hm"));
        hv.push_back(dv.getVal("Pr"));
        hv.push_back(dv.getVal("Lt"));
        if (db.addHourRow(hv)) std::cout << "Hour saved:" + std::to_string(currHr) << std::endl;
        else std::cout << "Hour failed:" + std::to_string(currHr) << std::endl;

        if (db.addWDRow(dv.getWDCounts(true), true)) std::cout << "WDHour saved:" + std::to_string(currHr) << std::endl;
        else std::cout << "WDHour failed:" + std::to_string(currHr) << std::endl;
        
        hrCount += 1;
        nowCount = 0;
        return true;
        }
    else {
        db.addMessageEntry("No records this hour: " + std::to_string(currHr), false);
        return false;
    }
}

int riseFall(std::vector<hr_pressure> hourlyPress) {
    /*riseFall(hourlyPress): rising / falling: calculates line of best fit through 24 hourly barometric pressure results
    parameters:
        hourlyPress: vevtor of 24 hr_pressure structs (hour, pressure) with hourly barometric pressure values (or zero if no record in database)
    returns: int: -1 (falling), 0 (steady) or 1 (rising)
    */
    float avePress = 0.0;
    float sumPress = 0.0;
    float aveHrs = 0.0;
    int sumHrs = 0;
    float sumXY = 0.0;
    int count = 0;
    int i, rv;
    hr_pressure hp;

    for (i = 0; i <24; i++) {
        hp = hourlyPress[i];
        if (hp.press > 0.0) {
            count++;
            sumHrs += hp.hour;
            sumPress += hp.press;
        }
    }
    if (count > 0) {
        aveHrs = sumHrs / count;
        avePress = sumPress / count;
    }
    else return 0;

    float hDiff;
    // Use simple linear regression to determine line of best fit and hence rising or falling pressure: MOVE THIS!
    for (i = 0; i < 24; i++) {
        hp = hourlyPress[i];
        hDiff = hp.hour - aveHrs;
        sumXY += hDiff * (hp.press - avePress);
    }
    if (sumHrs > 0) sumXY = sumXY / sumHrs; // => slope of best fit line:
    else  sumXY = 0.0;
    float minbar = db.getPrefFloat("MinBar");
    if (sumXY < 0.0) rv = -1;
    else rv = 1;
    if (abs(sumXY) < (minbar * count / 24)) rv = 0;
    return rv;
}

bool doDaily() { 
    // reset daily rainfall first:add later
    if (hrCount > 0) {
        std::vector<hr_pressure> hourlyPress = db.getHrPressures();
        int rf;
        if (hourlyPress.size() > 0) rf = riseFall(hourlyPress);
        else rf = 0;
        bool bDay = db.addDayRow(rf);
        if (bDay) db.addMessageEntry("Day record added: " + db.dtString("%Y-%m-%d"), false);
        else db.addMessageEntry(db.error() + " (addDayRow)", true);
        hrCount = 0;
        return bDay;
    }
    else {
        db.addMessageEntry("No hourly record today", true);
        return false;
    }
}

void clockTasks() { // Called every 30secs
    time_t  t = std::time(NULL);
    if ((t - setupTime) < 30) return; // Suppress clock tasks for the first 30 seconds
    
    std::cout << "<CB:";
    saveNowVals();
    
    struct tm* tim = std::gmtime(&t);
    int tSecs = tim->tm_sec;
    if (tSecs < 30) { // code below runs once a minute
        int tMin = tim->tm_min;
        if (tMin == 0) { // code below runs once an hour
            int tHr = tim->tm_hour;
            doHourly(tHr);
            if (tHr == 0){ // midnight
                doDaily();
            }
        }
    }
    std::cout << "CE>" << std::endl;
}

int main()
{
    Tim tAnem, tRain, tSens, tVane, tMaster;
    unsigned long bootMillis = millis();
    bool bOK = db.readPrefs();
    if (bOK) std::cout << "Preference file read OK." << std::endl;

    nowCount = nowWDCount = hrCount = 0;
    unsigned int status;
    
    while (millis() < bootMillis + 400) { ; }
    status = dv.setupDevices(db);
    std::cout << "Device setup status: " << status << std::endl;
    if (status > 0) {
        setupTime = std::time(NULL);
        nowCount = 0;
        tAnem.timer_start(anem, ANEM_INTVL);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        tRain.timer_start(rain, RAIN_INTVL);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        tSens.timer_start(sens, SENS_INTVL);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        tVane.timer_start(vane, VANE_INTVL);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        tMaster.timer_start(clockTasks, CLOCK_INTVL);
        while (true)
            ;
    }
    return 0;
}

