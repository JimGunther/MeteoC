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
* Version: 0.2                                                                                 *
* Last updated 08/07/2026 09:03                                                                *
* Author: Jim Gunther                                                                          *
*                                                                                              *
***********************************************************************************************/
DB::DB() { };

void DB::begin() {
    mysql_library_init(0, NULL, NULL);
}

bool DB::openConnection() {
    char errBuf[ERR_LEN];
    myConn = mysql_init(NULL);
    if ( mysql_real_connect( myConn, "localhost", _userid.c_str(), _passwd.c_str(), _dbName.c_str(), 0, NULL, 0 ) == NULL)
        {
		    sprintf( errBuf, "SQL Connect Failed : %i", mysql_error(myConn) );
		    //logError(errBuf);
            return false;
        }
        return true;
    }
/*

// Reusable helper methods: ALL NEEDED?
    
    @classmethod
    def changeData(cls, sql : str) -> bool:
        db, curs = cls.openDB(sql)
        b = True
        try:
            db.commit()
        except Exception as ex:
            cls.errMsg = str(ex)
            print (cls.errMsg)
            db.rollback()
            b = False
        db.close()
        return b

    @classmethod
    def fetchOne(cls, sql: str) -> tuple | None:
        db, curs = cls.openDB(sql)
        result = curs.fetchone()
        db.close()
        return result

    @classmethod
    def fetchAll(cls, sql : str) -> list | None:
        db, curs = cls.openDB(sql)
        results = curs.fetchall()
        db.close()
        return results
    */

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
        std::cout << query << std::endl;
        int status = mysql_query(myConn, query.c_str());
        std::cout << "SQL status: " << status << std::endl;
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
            int status = mysql_query(myConn, query.c_str());
            if (status != 0) return false;
        }
        return true;
    }
    
    bool DB::addNowRow(float* row) {
        /*addNowRow(): method to add a new row to NowValues table
        parameters:
            row: array of 8 float values
        returns: bool: True if successful, otherwise False
        */
        bool bOK = openConnection();
        if (!bOK) return false;
        std::string query;
        std::string sDT = dtString("'%Y-%m-%d %H:%M:%S'");
        query = "INSERT INTO NowValues(dtNow, Rain, Gust, Humdty, Light, Pressure, WSpeed, Temp) VALUES (" + sDT;
        int i;
        for (i = 0; i < NUM_NOW; i++) {
            query += ", " + std::to_string(row[i]);
        }
        query += ")";
        // MODIFY FROM HERE.....
        return true;
}
/*
    @classmethod
    def addWDRow(cls, row : tuple, bHour: bool) -> bool:
        '''
        addWDRow(): method to add a new row to WDNow table
        parameters:
            cls: this class object
            row: tuple of 17 values
            bHour: True if adding to WDHour table, False if WDNow table
        returns: bool: True if successful, otherwise False
        '''
        db = MySQLdb.connect(host="localhost", user=user_id, password=passwd, db=dbase)
        c = db.cursor()
        if bHour:
            strIns = "Hour (dtWD"
        else:
            strIns = "Now (dtWDNow"
        sql = "INSERT INTO WD" + strIns + ", N, NNW, NW, WNW, W, WSW, SW, SSW, S, SSE, SE, ESE, E, ENE, NE, NNE, INV) "
        sql += "VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)"
        try:
            c.execute(sql, row)
            db.commit()
            c.close()
            return True
        except Exception as ex:
            cls.errMsg = str(ex)
            print (cls.errMsg)
            db.rollback()
            return False

    @classmethod
    def hourAggregates(cls) -> tuple:
        '''hourAggregates(): gets hourly aggregates for rain wind, gust from now table
        parameters:
            cls: this class object
        returns: tuple of results
        '''
        sql = "SELECT MAX(Rain) AS MaxRain, AVG(WSpeed) AS AveWSpeed, MAX(Gust) AS MaxGust FROM NowValues WHERE dtNow > DATE_SUB(CONCAT(UTC_DATE(), ' ', UTC_TIME()), INTERVAL 1 HOUR)"
        result = cls.fetchOne(sql)
        if result is None:
            return (0, 0, 0)
        return result   # if all values are zero, result will be (0, 0, 0)
    
    @classmethod
    def hourWDAggs(cls) -> tuple:
        '''hourWDAggs(): gets hourly aggregates for Wind direction
        parameters:
            cls: this class object
        returns: tuple of 17 results
        '''
        sql = "SELECT SUM(N) AS hrN, SUM(NNW) AS hrNNW, SUM(NW) AS hrNW, SUM(WNW) AS hrWNW, SUM(W) AS hrW, SUM(WSW) AS hrWSW, SUM(SW) AS hrSW, SUM(SSW) AS hrSSW, SUM(S) AS hrS, "
        sql += "SUM(SSE) AS hrSSE, SUM(SE) AS hrSE, SUM(ESE) AS hrESE, SUM(E) AS hrE, SUM(ENE) AS hrENE, SUM(NE) AS hrNE, SUM(NNE) AS hrNNE, SUM(INV) AS hrINV FROM WDNow "
        sql += "WHERE dtWDNow > DATE_SUB(CONCAT(UTC_DATE(), ' ', UTC_TIME()), INTERVAL 1 HOUR)"
        result = cls.fetchOne(sql)
        if result is None:
            return (0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
        return result
    
    @classmethod
    def addHourRow(cls, row) -> bool:
        '''
        addHourRow(): adds a row to the HrValues table
        parameters:
            cls: this class object
            row: tuple of values in format:for entering into SQL as args
        returns: bool: True if successful, otherwise False
        '''
        global errMsg
        try:
            db = MySQLdb.connect(host="localhost", user=user_id, password=passwd, db=dbase)
            c = db.cursor()
            sql = "INSERT INTO HrValues(dtHr, RainHr, WSpeedHr, GustHr, Temp, Humdty, Pressure, Light) "
            sql += "VALUES (%s, %s, %s, %s, %s, %s, %s, %s)"
            c.execute(sql, row)
            db.commit()
            rv = True
        except MySQLdb.Error as ex:
            errMsg = str(ex)
            print (cls.errMsg)
            db.rollback()
            rv = False
        c. close()
        db.close()
        return rv

    @classmethod
    def addDayRow(cls, dtNow : datetime, risefall : int) -> bool:
        '''
        addDayRow(): adds a row to the DayValues table
        parameters:
            cls: this class object
            dtNow: datetime: time now (midnight)
            risefall: int: -1, 0, or +1 (fall, steady, rise)
        returns: bool: True if successful, False otherwise
        '''
        db = MySQLdb.connect(host="localhost", user=user_id, password=passwd, db=dbase)
        c = db.cursor()
        try:
            args = (dtNow, risefall)
            print("DT: " + str(dtNow))
            c.callproc("spHourToDay", args)
            db.commit()
            print("Day added!")
            c.close()
            db.close()
            return True
        except Exception as ex:
            cls.errMsg = str(ex)
            db.rollback()
            print("Day add error")
            c.close()
            db.close()
            return False

    @classmethod
    def getHrPressures(cls) -> list | None:
        '''
        getHrPressures(): method to get 24 (or less) hourly pressure results
        parameters:
            cls: this class object
        returns: list of tuples (hour, pressure)
        '''
        db = MySQLdb.connect(host="localhost", user=user_id, password=passwd, db=dbase)
        c = db.cursor()
        try:
            sql = "SELECT HOUR(dtHr) AS hr, Pressure FROM HrValues WHERE dtHr > %s"
            dtYest = datetime.now(timezone.utc) - timedelta(hours=24)
            args = (dtYest, )
            c.execute(sql, args)
            res = c.fetchall()
        except Exception as ex:
            cls.errMsg = str(ex)
            res = None
        c.close()
        db.close()
        if res is None:
            return None
        else:
            return list(res)
        
    @classmethod
    def getError(cls) -> str:
        return cls.errMsg

*/
