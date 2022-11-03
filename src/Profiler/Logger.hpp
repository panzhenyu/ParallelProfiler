#pragma once

#include <ctime>
#include <string>
#include <cstring>
#include <iostream>

enum LoggerLevel {
    INFO,
    ERROR,
    NR
};

struct Logger {
    static constexpr char* level[NR] = {
        (char*)"[INFO] ",
        (char*)"[ERROR]",
    };

    Logger(LoggerLevel lv, std::ostream& out): m_out(out) {
        time_t t;
        tm *info;
        char *str;

        time(&t);
        info = localtime(&t);
        if (info && (str=asctime(info))) {
            str[std::strlen(str)-1] = '\0';
        } else {
            str = (char*)"??";
        }

        m_out << "[" << str << "] " << level[lv] << ' ';
    }

    ~Logger() {
        m_out << std::endl;
    }

    std::ostream& getStream() { return m_out; }

    std::ostream&   m_out;
};

#define LOG(level)  Logger(level, m_log).getStream()
