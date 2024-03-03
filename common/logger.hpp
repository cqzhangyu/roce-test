#pragma once

#include <cmath>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <mutex>

#define CO_RED                "\033[1;31m"
#define CO_BLACK              "\033[1;30m"
#define CO_RED                "\033[1;31m"
#define CO_GREEN              "\033[1;32m"
#define CO_YELLOW             "\033[1;33m"
#define CO_BLUE               "\033[1;34m"
#define CO_PURPLE             "\033[1;35m"
#define CO_SKYBLUE            "\033[1;36m"
#define CO_WHITE              "\033[1;37m"
#define CO_RESET              "\033[0m"
#define BG_BLACK              "\033[40m"
#define BG_RED                "\033[41m"
#define BG_GREEN              "\033[42m"
#define BG_YELLOW             "\033[43m"
#define BG_BLUE               "\033[44m"
#define BG_PURPLE             "\033[45m"
#define BG_SKYBLUE            "\033[46m"
#define BG_WHITE              "\033[47m"
#define BG_RESET              "\033[0m"

using namespace std;

class Logger {
private:
    recursive_mutex mtx;
public:
    ofstream of;
    enum Level {
        NO,
        ERRO,
        WARN,
        INFO,
        PERF,
        DEBUG,
    };
    uint32_t log_mask;
    Level log_level;

    Logger() {
        log_level = INFO;
        log_mask = -1;
    }
    ~Logger() {
        if (of.is_open())
            of.close();
    }

    void open(const string &fname) {
        unique_lock<recursive_mutex> lck(mtx);

        of.open(fname, ios::out);
        if (of.fail()) {
            cout << "Failed to open " << fname << " as log file." << endl;
        }
    }
    
    Logger(const char *fname) {
        log_level = INFO;
        log_mask = -1;
        open(string(fname));
    }

    void set_level(const string &level) {
        unique_lock<recursive_mutex> lck(mtx);

        if (level == "NO" || level == "no")
            log_level = NO;
        else if (level == "ERRO" || level == "erro")
            log_level = ERRO;
        else if (level == "WARN" || level == "warn")
            log_level = WARN;
        else if (level == "INFO" || level == "info")
            log_level = INFO;
        else if (level == "PERF" || level == "perf")
            log_level = PERF;
        else if (level == "DEBUG" || level == "debug")
            log_level = DEBUG;
        else
            cout << "Unknown log level : " << level << "\n";
    }

    void flush() {
        unique_lock<recursive_mutex> lck(mtx);

        cout.flush();
        of.flush();
    }

    Logger & operator << (const double &a) {
        unique_lock<recursive_mutex> lck(mtx);

        cout << a;
        if (fabs(a) < 0.001) {
            if (fabs(a) < 1e-9)
                of << 0;
            else
                of << fixed << showpoint << setprecision(6) << a;
        }
        else
            of << a;
        return *this;
    }

    template<typename T>
    Logger & operator << (const T &a) {
        unique_lock<recursive_mutex> lck(mtx);

        cout << a;
        of << a;
        return *this;
    }

    Logger & operator << (ostream& (*fp)(ostream&)) {
        unique_lock<recursive_mutex> lck(mtx);

        cout << fp;
        of << fp;
        return *this;
    }
    
    template<typename T>
    Logger & operator () (const T &arg1) {
        unique_lock<recursive_mutex> lck(mtx);

        return (*this) << arg1 << "\n";
    }

    
    template<typename T, typename... Args>
    Logger & operator () (const T &arg1, const Args &...args) {
        unique_lock<recursive_mutex> lck(mtx);
        
        return ((*this) << arg1)(args...);
    }

    template<typename... Args>
    Logger & info(const Args &...args) {
        if (INFO <= log_level && ((log_mask >> INFO) & 1))
            return (*this)("[" CO_GREEN "INFO" CO_RESET "] ", args...);
        return *this;
    }
    
    template<typename... Args>
    Logger & perf(const Args &...args) {
        if (PERF <= log_level && ((log_mask >> PERF) & 1))
            return (*this)("[" CO_SKYBLUE "PERF" CO_RESET "] ", args...);
        return *this;
    }
    
    template<typename... Args>
    Logger & warn(const Args &...args) {
        if (WARN <= log_level && ((log_mask >> WARN) & 1))
            return (*this)("[" CO_YELLOW "WARN" CO_RESET "] ", args...);
        return *this;
    }
    
    template<typename... Args>
    Logger & erro(const Args &...args) {
        if (ERRO <= log_level && ((log_mask >> ERRO) & 1))
            return (*this)("[" CO_RED "ERRO" CO_RESET "] ", args...);
        return *this;
    }

    template<typename... Args>
    Logger & debug(const Args &...args) {
        if (DEBUG <= log_level && ((log_mask >> DEBUG) & 1))
            return (*this)("[" CO_PURPLE "DEBUG" CO_RESET "] ", args...);
        return *this;
    }
};

extern Logger logger;

#define loginfo(args...)    logger.info(args)
#define logperf(args...)    logger.perf(args)
#define logwarn(args...)    logger.warn(args)
#define logerro(args...)    logger.erro(args)
#define logdebug(args...)   logger.debug(args)

#define logassert(expr, args...) if (expr) logerro(args, " at ", __FILE__, ":", __LINE__);
