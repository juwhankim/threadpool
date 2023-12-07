#include"logger_implementation.h"
#include<mutex>

namespace juwhan {
    namespace logger {

// Implementation of selected functions in logger_implementation.cpp.
        ostream &operator<<(ostream &stream, const formatting_element_without_message &fe) {
            return stream << string(fe);
        }

        using default_log_format = log_line<time_element, head_element, file_element, line_element, thread_element, message_element>;

        time_element default_time_format{ANSI_foreground_color::blue, delimit{"(", ") "}};
        file_element default_file_format{ANSI_foreground_color::yellow, delimit{" ["}};
        line_element default_line_format{ANSI_foreground_color::yellow, delimit{":", "]"}};
        thread_element default_thread_format{ANSI_foreground_color::cyan, delimit{" (", ")"}};
        message_element default_message_format{delimit{": \"", "\""}, ANSI_text_attribute::bold};

        default_log_format
                info_message{1
// Start formatting.
                , default_time_format, head_element{"INFO", ANSI_foreground_color::green, delimit{"<", ">"},
                                                    ANSI_text_attribute::bold}, default_file_format,
                             default_line_format, default_thread_format, default_message_format
        };

        default_log_format
                debug_message{2
// Start formatting.
                , default_time_format, head_element{"DEBUG", ANSI_foreground_color::yellow, delimit{"<", ">"},
                                                    ANSI_text_attribute::bold}, default_file_format,
                              default_line_format, default_thread_format, default_message_format
        };

        default_log_format
                warn_message{3
// Start formatting.
                , default_time_format, head_element{"WARNING", ANSI_foreground_color::magenta, delimit{"<", ">"},
                                                    ANSI_text_attribute::bold, ANSI_text_attribute::blink},
                             default_file_format, default_line_format, default_thread_format, default_message_format
        };

        default_log_format
                fatal_message{4
// Start formatting.
                , default_time_format, head_element{"FATAL", ANSI_foreground_color::red, delimit{"<", ">"},
                                                    ANSI_text_attribute::bold, ANSI_text_attribute::blink},
                              default_file_format, default_line_format, default_thread_format, default_message_format
        };

// Set logging level in runtime.
        int log_level = 1;


// Unfortunately, std::ostream is NOT thread safe. So we need to protect the critical part with mutex.
        std::mutex log_mutex;

// Console logging. These are functions that are never meant for direct usage. Wrap these around with a macro to omit out at compile time.
        void clog_f(default_log_format &log_line, const string &file_name, const string &line_number,
                    const string &message) {
            // Guard against racing.
            std::lock_guard <std::mutex> lock(log_mutex);
            if (log_line.priority >= log_level)
                cout << log_line(file_string{file_name}, line_string{line_number}, message_string{message}) << endl;
        }


// File logging. These are functions that are never meant for direct usage. Wrap these around with a macro to omit out at compile time.

        ofstream default_log_file{""};
        ofstream user_log_file{""};

        void
        log_f(default_log_format &log_line, const string &file_name, const string &line_number, const string &message) {
            // Guard against racing.
            std::lock_guard <std::mutex> lock(log_mutex);
            if (log_line.priority >= log_level) {
                if (user_log_file) {
                    user_log_file << log_line(file_string{file_name}, line_string{line_number}, message_string{message})
                                  << endl;
                } else if (default_log_file) {
                    default_log_file
                            << log_line(file_string{file_name}, line_string{line_number}, message_string{message})
                            << endl;
                } else {
                    // None of the file is open, open.
                    default_log_file.open("default.log", ofstream::out | ofstream::app);
                    default_log_file
                            << log_line(file_string{file_name}, line_string{line_number}, message_string{message})
                            << endl;
                }
            }
        }

    }
}




















