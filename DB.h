#ifndef DB_H
#define DB_H

#include <string>
#include <mysql/mysql.h>
#include <map>
/***********************************************************************************************
* DB.h: header file for DB class                                                               *
*                                                                                              *
* Version: 0.1                                                                                 *
* Last updated: 06/07/2026 15:04                                                               *
* Author: Jim Gunther                                                                          *
*                                                                                              *
***********************************************************************************************/
    
//#define USERID "mis"
//#define PASSWD "mispwd"
//#define DBASE "Meteo"
#define ERR_LEN 32

class DB {
  
  public:
    DB();
    bool readPrefs();
    void begin();
    float getPrefFloat(std::string prefName);
    bool updateLiveRow(std::string itemNm, int intvl, float itemVal);
  
  private:
    bool openConnection();
    void logError(const char* errMsg); 
    
    std::map<std::string, std::string> _prefs;
    std::string _userid, _passwd, _dbName;
    MYSQL* myConn;
    FILE* f;
  
};
// ADD HERE
#endif