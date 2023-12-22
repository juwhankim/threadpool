#include<iostream>
#include<string>
#include<sstream>
#include<chrono>
#include<iomanip>
#include<ctime>
#include"logger.h"

// Change the following macro value to remove unnexessary logging functions at compile time.
#define LOG_LEVEL_M 2

#include"logger_helper.h"

int main() {

    cinfo("This is a test info message, outputted to console");
    cdebug("This is a test debug message, outputted to console");
    cwarn("This is a test warn message, outputted to console");
    cfatal("This is a test fatal message, outputted to console");

    info("This is a test info message, outputted to default.log");
    debug("This is a test debug message, outputted to default.log");
    warn("This is a test warn message, outputted to default.log");
    fatal("This is a test fatal message, outputted to default.log");

    juwhan::logger::user_log_file.open("user.log", std::ofstream::out | std::ofstream::app);
    info("This is a test info message, outputted to user.log");
    debug("This is a test debug message, outputted to user.log");
    warn("This is a test warn message, outputted to user.log");
    fatal("This is a test fatal message, outputted to user.log");

    // Change logging level at runtime.
    juwhan::logger::log_level = log_warn_level;
    std::cout << "Now, logging level is set to log_warn_level." << std::endl;

    cinfo("This is a test info message, outputted to console");
    cdebug("This is a test debug message, outputted to console");
    cwarn("This is a test warn message, outputted to console");
    cfatal("This is a test fatal message, outputted to console");

    info("This is a test info message, outputted to user.log");
    debug("This is a test debug message, outputted to user.log");
    warn("This is a test warn message, outputted to user.log");
    fatal("This is a test fatal message, outputted to user.log");

    // Testing conditional logging.
    // Set back the runtime logging level to info.
    juwhan::logger::log_level = log_info_level;
    cinfo_if(true, "This should NOT print because, even though the condition is true, the LOG_LEVEL_M is set to 2");
    cdebug_if(true, "This should print, logging level is satisfied and the condition is true.");
    cwarn_if(false, "This should NOT print, logging level is satisfied but the condition is false.");
    cfatal_if(1 > 2, "This should NOT print, logging level is satisfied but the condition is false.");

}