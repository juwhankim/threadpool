#include"logger_implementation.h"

#ifndef juwhan_logger_h
#define juwhan_logger_h
namespace juwhan {
    namespace logger {

        using default_log_format = log_line<time_element, head_element, file_element, line_element, thread_element, message_element>;

        extern default_log_format info_message;
        extern default_log_format debug_message;
        extern default_log_format warn_message;
        extern default_log_format fatal_message;

// Set this value to one of the above to set logging level.
        extern int log_level;

// Logging files. If user_log file is not set, default_log_file is used.
// If you want to use a specific file, execute user_log_file.open(filename, std::ofstream::out | std::ofstream::app);
        extern ofstream default_log_file;
        extern ofstream user_log_file;

// logging functions.
        extern void
        clog_f(default_log_format &log_line, const string &file_name, const string &line_number, const string &message);

        extern void
        log_f(default_log_format &log_line, const string &file_name, const string &line_number, const string &message);

    } // End of namespace logger.
} // End of namespace juwhan.	

#endif