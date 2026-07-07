#include <iostream>
#include <chrono>
#include <thread>
#include <functional>
//#include <vector>
//#include <string>
#include <wiringPi.h>
#include "Devi.h"
#include "DB.h"
/*
MeteoC.cpp: C++ version of code to run Meteo WS
Version as at 05/07/2026
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
#define RAIN_INTVL 15000 // PROVISIONAL
#define SENS_INTVL 30000
#define VANE_INTVL 100

Devi dv;
DB db;
void anem() { dv.anemTasks(); }
void rain() { dv.rainTasks(); }
void sens() { dv.sensTasks(); }
void vane() { dv.vaneTasks(); }

int main()
{
    Tim tAnem, tRain, tSens, tVane, tMaster;
    dv = Devi();
    db = DB();
    bool bOK = db.readPrefs();
    if (bOK) {
      std::cout << "Preference file read OK." << std::endl;
    }
    
    bOK = dv.setupDevices(db);
    if (bOK) {
      tAnem.timer_start(anem, ANEM_INTVL);
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      tRain.timer_start(rain, RAIN_INTVL);
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      tSens.timer_start(sens, SENS_INTVL);
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      tVane.timer_start(vane, VANE_INTVL);
      while (true)
        ;
    }
    return 0;
}

