#ifndef juwhan_logger_helper_h
#define juwhan_logger_helper_h

// Macro definitions to log levels.
#define log_info_level 1
#define log_debug_level 2
#define log_warn_level 3
#define log_fatal_level 4

// Macros to enable and disable logging at each level.
// Also, these helper macros provide easy to used helper functions for logging.

// Compile time log removal.
#ifndef LOG_LEVEL_M
#define LOG_LEVEL_M 1
#endif

#if LOG_LEVEL_M == 1
#define info(m) juwhan::logger::log_f(juwhan::logger::info_message, __FILE__, std::to_string(__LINE__), (m));
#define info_if(cond, m) if(cond) juwhan::logger::log_f(juwhan::logger::info_message, __FILE__, std::to_string(__LINE__), (m));
#define cinfo(m) juwhan::logger::clog_f(juwhan::logger::info_message, __FILE__, std::to_string(__LINE__), (m));
#define cinfo_if(cond, m) if(cond) juwhan::logger::clog_f(juwhan::logger::info_message, __FILE__, std::to_string(__LINE__), (m));
#else
#define info(...)
#define info_if(...)
#define cinfo(...)
#define cinfo_if(...)
#endif

#if LOG_LEVEL_M <= 2
#define debug(m) juwhan::logger::log_f(juwhan::logger::debug_message, __FILE__, std::to_string(__LINE__), (m));
#define debug_if(cond, m) if(cond) juwhan::logger::log_f(juwhan::logger::debug_message, __FILE__, std::to_string(__LINE__), (m));
#define cdebug(m) juwhan::logger::clog_f(juwhan::logger::debug_message, __FILE__, std::to_string(__LINE__), (m));
#define cdebug_if(cond, m) if(cond) juwhan::logger::clog_f(juwhan::logger::debug_message, __FILE__, std::to_string(__LINE__), (m));
#else
#define debug(...)
#define debug_if(...)
#define cdebug(...)
#define cdebug_if(...)
#endif

#if LOG_LEVEL <= 3
#define warn(m) juwhan::logger::log_f(juwhan::logger::warn_message, __FILE__, std::to_string(__LINE__), (m));
#define warn_if(cond, m) if(cond) juwhan::logger::log_f(juwhan::logger::warn_message, __FILE__, std::to_string(__LINE__), (m));
#define cwarn(m) juwhan::logger::clog_f(juwhan::logger::warn_message, __FILE__, std::to_string(__LINE__), (m));
#define cwarn_if(cond, m) if(cond) juwhan::logger::clog_f(juwhan::logger::warn_message, __FILE__, std::to_string(__LINE__), (m));
#else
#define warn(...)
#define warn_if(...)
#define cwarn(...)
#define cwarn_if(...)
#endif

#if LOG_LEVEL <= 4
#define fatal(m) juwhan::logger::log_f(juwhan::logger::fatal_message, __FILE__, std::to_string(__LINE__), (m));
#define fatal_if(cond, m) if(cond) juwhan::logger::log_f(juwhan::logger::fatal_message, __FILE__, std::to_string(__LINE__), (m));
#define cfatal(m) juwhan::logger::clog_f(juwhan::logger::fatal_message, __FILE__, std::to_string(__LINE__), (m));
#define cfatal_if(cond, m) if(cond) juwhan::logger::clog_f(juwhan::logger::fatal_message, __FILE__, std::to_string(__LINE__), (m));
#else
#define fatal(...)
#define fatal_if(...)
#define cfatal(...)
#define cfatal_if(...)
#endif

#endif