#include <cstdio>
#include "DB.h"
#include <ctime>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>

/***********************************************************************************************
* DB.cpp: class handles all SQL and message/error logging interactions                         *
*                                                                                              *
* Version: 0.3                                                                                 *
* Last updated 14/07/2026 17:02                                                                *
* Author: Jim Gunther                                                                          *
*                                                                                              *
***********************************************************************************************/
DB::DB() { };

void DB::begin() {
    mysql_library_init(0, NULL, NULL);
}

bool DB::openConnection() {
    char errBuf[ERR_LEN];
    _myConn = mysql_init(NULL);
    if ( mysql_real_connect( _myConn, "localhost", _userid.c_str(), _passwd.c_str(), _dbName.c_str(), 0, NULL, 0 ) == NULL)
        {
		    sprintf( errBuf, "SQL Connect Failed : %i", mysql_error(_myConn) );
		    _errMsg = "Open connection failed.";
            mysql_close(_myConn);
            return false;
        }
        return true;
    }

    bool DB::readPrefs() {
        /*readPrefs(): method to read Prefs data table: note this must be caalled before any other methods in this class (to read .env variables)
        parameters: none
        returns: bool: true if successful, otherwise false
        */
        std::string fLine, fKey, fVal, delim;
        size_t pos, len;

        delim = ",";
        std::ifstream f("Prefs.csv");
        getline(f, fLine); // ignore header line
        while (getline(f, fLine)) {
            pos = fLine.find(delim);
            fKey = fLine.substr(1, pos - 2);
            fVal = fLine.substr(pos + 2);
            len = fVal.length();
            fVal = fVal.substr(0, len - 1);
            _prefs[fKey] = fVal;
            //std::cout << "K:V:" << fKey << ":" << fVal << std::endl;
        }
        f.close();
        _userid = _prefs["DB_USER"];
        _passwd = _prefs["PASSWORD"];
        _dbName = _prefs["DBASE"];
                
        return true;
    }

    float DB::getPrefFloat(std::string prefName) {
        std::string p = _prefs[prefName];
        return std::stof(p);
    }

    std::string DB::dtString(std::string fString) {
        auto t = std::time(NULL);
        auto tm = *std::gmtime(&t);
        std::ostringstream oss;
        oss << std::put_time(&tm, fString.c_str());
        auto sTime = oss.str();
        return sTime;
    }
    
    bool DB::addMessageEntry(std::string txt, bool isErr) {
        /*addMessageEntry(): method to add a log message to today's Message or Error Log file
        parameters:
            txt: string: message text (< 80 characters)
            isErr: bool: True if error message
        returns: bool: True if successful otherwise False
        */
        auto sTime = dtString("%H:%M:%S");
        auto sDate = dtString("%Y-%b-%d.log");

        std::string ff;
        if (isErr) ff = "ER";
        else ff = "MS";
        std::string fName = "./Logs/" + ff + sDate;
        std::ofstream log(fName, std::ios_base::app | std::ios_base::out);
        log << txt << std::endl;
        return true;
    }

    bool DB::updateLiveRow(std::string itemNm, int intvl, float itemVal) {
        /*updateLiveRow(): method to update one item in LiveValues table
        parameters:
            itemNm: 2-character code name for item
            intvl: interval since last measurement
            itemVal: int: new value for item
        returns: bool: true if successful, otherwise false
        */
        bool bOK = openConnection();
        if (!bOK) return false;
        std::string query;
        auto sTime = dtString("'%Y-%m-%d %H:%M:%S'");
        query = "UPDATE LiveValues SET Intvl = " + std::to_string(intvl) + ", Val = " + std::to_string(itemVal) + ", LastUpdated = " + sTime + " WHERE ItemName = '" + itemNm + "'";
        //std::cout << query << std::endl;
        int status = mysql_query(_myConn, query.c_str());
        if (status != 0) {
            std::string s(mysql_error(_myConn));
            _errMsg = s;
        }
        mysql_close(_myConn);
        return (status == 0);            
    }
       
    bool DB::updateLiveWD(int intvl, std::vector<int> counts) {
        /*updateLiveWD(): method to update all 17 rows in WDLive table
        parameters:
            int: interval in ms since last update
            counts: array of 17 integers (16 compass points plus dustbin)
        returns: bool: true if successful, otherwise false
        */
        int i, numPts = (int)getPrefFloat("CmpsPnts");
        bool bOK = openConnection();
        if (!bOK) return false;

        std::string query;
        auto sTime = dtString("'%Y-%m-%d %H:%M:%S'");
        for (i = 0; i <= numPts; i++) {
            query = "UPDATE WDLive SET WDCount = " + std::to_string(counts[i]) + " WHERE PK = " + std::to_string(i + 1); // PK is 1-based!
            int status = mysql_query(_myConn, query.c_str());
            if (status != 0) {
                std::string s(mysql_error(_myConn));
                _errMsg = s;
                mysql_close(_myConn);
                return false;
            }
        }
        mysql_close(_myConn);
        return true;
    }
    
    bool DB::addNowRow(std::vector<float> row) {
        /*addNowRow(): method to add a new row to NowValues table
        parameters:
            row: array of 8 float values
        returns: bool: True if successful, otherwise False
        */
        bool bOK = openConnection();
        if (!bOK) return false;

        std::string query;
        std::string sDT = dtString("'%Y-%m-%d %H:%M:%S'");
        query = "INSERT INTO NowValues(dtNow, Rain, WSpeed, Gust, Temp, Humdty, Pressure, Light) VALUES (" + sDT;
        int i;
        for (i = 0; i < NUM_NOW; i++) {
            query += ", " + std::to_string(row[i]);
        }
        query += ")";
        std::cout << query << std::endl;
        int status = mysql_query(_myConn, query.c_str());
        if (status != 0) {
            std::string s(mysql_error(_myConn));
            _errMsg = s;
            mysql_close(_myConn);
            return false;
        }
        mysql_close(_myConn);
        return true;
}

    bool DB::addWDRow(std::vector<int> row, bool bHour) {
        /*addWDRow(): method to add a new row to WDNow table
        parameters:
            row: vector of 17 values
            bHour: True if adding to WDHour table, False if WDNow table
        returns: bool: True if successful, otherwise False
        */
        bool bOK = openConnection();
        if (!bOK) return false;
        std::string strIns;
        if (bHour) strIns = "Hour (dtWD";
        else strIns = "Now (dtWDNow";
        auto sDT = dtString("'%Y-%m-%d %H:%M:%S'");
        std::string query;
        query = "INSERT INTO WD" + strIns + ", N, NNW, NW, WNW, W, WSW, SW, SSW, S, SSE, SE, ESE, E, ENE, NE, NNE, INV) VALUES (" + sDT;
        int i, numPts = (int)getPrefFloat("CmpsPnts");
        for (i = 0; i <= numPts; i++) {
            query += ", " + std::to_string(row[i]);
        }
        query += ")";
        std::cout << query << std::endl;
        int status = mysql_query(_myConn, query.c_str());
        if (status != 0) {
            std::string s(mysql_error(_myConn));
            _errMsg = s;
            mysql_close(_myConn);
            return false;
        }
        mysql_close(_myConn);
        return true;
    }

    std::vector<float> DB::hourAggregates() {
        /*hourAggregates(): gets hourly aggregates for rain wind, gust from now table
        parameters: none
        returns: vector of 3 doubles
        */
        bool bOK = openConnection();
        if (!bOK) return std::vector<float> {0.0, 0.0, 0.0};
        std::string query = "SELECT MAX(Rain) AS MaxRain, AVG(WSpeed) AS AveWSpeed, MAX(Gust) AS MaxGust FROM NowValues WHERE dtNow > DATE_SUB(CONCAT(UTC_DATE(), ' ', UTC_TIME()), INTERVAL 1 HOUR)";
        std::cout << query << std::endl;
        int status = mysql_query(_myConn, query.c_str());
        if (status != 0) {
            mysql_close(_myConn);
            return std::vector<float> {0.0, 0.0, 0.0};
        }
        MYSQL_RES* result = mysql_use_result(_myConn);
        MYSQL_ROW row = mysql_fetch_row(result);
        mysql_free_result(result);
        mysql_close(_myConn);

        if (result == NULL) return std::vector<float> {0.0, 0.0, 0.0};
        std::vector<float> rv = {(float)atof(row[0]), (float)atof(row[1]), (float)atof(row[2])}; 
        return rv; 
    }
    
    std::vector<int> DB::hourWDAggs() {
        /*hourWDAggs(): gets hourly aggregates for Wind direction
        parameters: none
            cls: this class object
        returns: vector of 17 results
        */
        bool bOK = openConnection();
        if (!bOK) return std::vector<int> {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        std::string query = "SELECT SUM(N) AS hrN, SUM(NNW) AS hrNNW, SUM(NW) AS hrNW, SUM(WNW) AS hrWNW, SUM(W) AS hrW, SUM(WSW) AS hrWSW, SUM(SW) AS hrSW, SUM(SSW) AS hrSSW, SUM(S) AS hrS, ";
        query += "SUM(SSE) AS hrSSE, SUM(SE) AS hrSE, SUM(ESE) AS hrESE, SUM(E) AS hrE, SUM(ENE) AS hrENE, SUM(NE) AS hrNE, SUM(NNE) AS hrNNE, SUM(INV) AS hrINV FROM WDNow ";
        query += "WHERE dtWDNow > DATE_SUB(CONCAT(UTC_DATE(), ' ', UTC_TIME()), INTERVAL 1 HOUR)";
        std::cout << query << std::endl;
        int status = mysql_query(_myConn, query.c_str());
        if (status != 0) return std::vector<int> {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        MYSQL_RES* result = mysql_use_result(_myConn);
        MYSQL_ROW row = mysql_fetch_row(result);
        mysql_free_result(result);
        mysql_close(_myConn);
        if (result == NULL) return std::vector<int> {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        std::vector<int> rv;
        int i;
        for (i = 0; i < 17; i++) {
             rv[i] = atoi(row[i]); 
        }
        return rv;
}
    
    bool DB::addHourRow(std::vector<float> row) {
        /*addHourRow(): adds a row to the HrValues table
        parameters:
            row: vector of 7 float values
        returns: bool: true if successful, otherwise False
        */
        bool bOK = openConnection();
        if (!bOK) return false;
        auto sDT = dtString("'%Y-%m-%d %H:%M:00'");
        std::string query = "INSERT INTO HrValues(dtHr, RainHr, WSpeedHr, GustHr, Temp, Humdty, Pressure, Light) Values(" + sDT;
        int i;
        for (i = 0; i < NUM_NOW; i++) {
            query += ", " + std::to_string(row[i]);
        }
        query += ")";
        std::cout << query << std::endl;
        int status = mysql_query(_myConn, query.c_str());
        if (status != 0) {
           std::string s(mysql_error(_myConn));
           _errMsg = s;
           mysql_close(_myConn);
            return false;
        }
        mysql_close(_myConn);
        return true;
    }

    bool DB::addDayRow(int risefall) {
        /*
        addDayRow(): adds a row to the DayValues table
        parameters:
            risefall: int: -1, 0, or +1 (fall, steady, rise)
        returns: bool: true if successful, false otherwise
        '*/
        bool bOK = openConnection();
        if (!bOK) return false;
        auto sDT = dtString("'%Y-%m-%d 00:00:00', '");
        std::string query = "CALL spHourToDay(" + sDT + std::to_string(risefall) + ")";
        std::cout << query << std::endl;
        int status = mysql_query(_myConn, query.c_str());
        if (status != 0) {
            std::string s(mysql_error(_myConn));
            _errMsg = s;
            mysql_close(_myConn);
            return false;
        }
        mysql_close(_myConn);
        return true;
    }
    
    std::vector<hr_pressure> DB::getHrPressures() {
        /*getHrPressures(): method to get 24 (or less) hourly pressure results
        parameters: none
        returns: list of tuples (hour, pressure)
        */
        bool bOK = openConnection();
        if (!bOK) return std::vector<hr_pressure> {};
        std::string fString = "'%Y-%m-%d 00:00:00'";
        time_t t = std::time(NULL) - 24*60*60;  // t is 24 hours ago
        auto tm = *std::gmtime(&t);
        std::ostringstream oss;
        oss << std::put_time(&tm, fString.c_str());
        std::string sYest = oss.str();
        std::string query = "SELECT HOUR(dtHr) AS hr, Pressure FROM HrValues WHERE dtHr > " + sYest;
        int status = mysql_query(_myConn, query.c_str());
        if (status != 0) {
            mysql_close(_myConn);
            return std::vector<hr_pressure> {};
        }
        MYSQL_RES* result = mysql_use_result(_myConn);
        if (result == NULL) {
            mysql_close(_myConn);
            return std::vector<hr_pressure> {};
        }
        MYSQL_ROW row;
        hr_pressure hp;
        std::vector<hr_pressure> rv = {};
        while ((row = mysql_fetch_row(result))) {
            hp.hour = atoi(row[0]);
            hp.press = atoi(row[1]);
            rv.push_back(hp);
        }
        mysql_free_result(result);
        mysql_close(_myConn);
        return rv;
    }
    
    std::string DB::error() { return _errMsg; }
