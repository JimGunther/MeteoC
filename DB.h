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
* Last updated: 08/07/2026 09:14                                                               *
* Author: Jim Gunther                                                                          *
*                                                                                              *
***********************************************************************************************/
    
#define ERR_LEN 32
#define NUM_NOW 7

class DB {
  
  public:
    DB();
    bool readPrefs();
    void begin();
    float getPrefFloat(std::string prefName);
    std::string dtString(std::string fString);
    bool updateLiveRow(std::string itemNm, int intvl, float itemVal);
    bool updateLiveWD(int intvl, std::vector<int> counts);
    bool addNowRow(float* row);
    bool addMessageEntry(std::string txt,bool isErr);
  
  private:
    bool openConnection();
    void logError(const char* errMsg); 
    
    std::map<std::string, std::string> _prefs;
    std::string _userid, _passwd, _dbName;
    MYSQL* myConn;
    FILE* f;
  
}; 
#endif