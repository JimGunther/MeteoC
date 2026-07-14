#ifndef DB_H
#define DB_H

#include <string>
#include <mysql/mysql.h>
#include <map>
#include <vector>

/***********************************************************************************************
* DB.h: header file for DB class                                                               *
*                                                                                              *
* Version: 0.2                                                                                 *
* Last updated: 10/07/2026 12:25                                                               *
* Author: Jim Gunther                                                                          *
*                                                                                              *
***********************************************************************************************/
    
#define ERR_LEN 32
#define NUM_NOW 7

struct hr_pressure{
  int hour;
  float press;

};

class DB {
  
  public:
    DB();
    bool readPrefs();
    void begin();
    float getPrefFloat(std::string prefName);
    std::string dtString(std::string fString);
    bool updateLiveRow(std::string itemNm, int intvl, float itemVal);
    bool updateLiveWD(int intvl, std::vector<int> counts);
    bool addNowRow(std::vector<float> row);
    bool addWDRow(std::vector<int> row, bool bHour);
    bool addMessageEntry(std::string txt,bool isErr);
    std::vector<float> hourAggregates();
    std::vector<int> hourWDAggs();
    bool addHourRow(std::vector<float> row);
    bool addDayRow(int riseFall);
    std::vector<hr_pressure> getHrPressures();
    std::string error();
  
  private:
    bool openConnection();
    void logError(const char* errMsg); 
    
    std::map<std::string, std::string> _prefs;
    std::string _userid, _passwd, _dbName;
    MYSQL* _myConn;
    FILE* f;
    std::string _errMsg;
  
}; 
#endif