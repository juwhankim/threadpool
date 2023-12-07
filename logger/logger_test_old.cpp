#include<iostream>
#include<string>
#include<sstream>
#include<chrono>
#include<iomanip>
#include<ctime>
#include"logger.h"

#define LOGER_LEVEL_M 1

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


    // using namespace juwhan::logger;
    // log_level=2;
    /*cinfo("This is a sample information.");
    cdebug("This is a sample information.");
    cwarn("This is a sample information.");
    cfatal("This is a sample information.");*/

    /*cinfo("This is a sample information.");
    cdebug("This is a sample information.");
    cwarn("This is a sample information.");
    cfatal("This is a sample information.");*/


    /*cinfo_if(true,"This is a sample information.");
    cdebug_if(false,"This is a sample information.");
    cwarn_if(false,"This is a sample information.");
    cfatal_if(true,"This is a sample information.");*/

    /*clog_f(info_message, to_string(__LINE__), __FILE__, "This is a sample information.");
    clog_f(debug_message, to_string(__LINE__), __FILE__, "This is a sample debugging information.");
    clog_f(warn_message, to_string(__LINE__), __FILE__, "This is a sample warning message.");
    clog_f(fatal_message, to_string(__LINE__), __FILE__, "This is a sample fatal error.");

    logging_level = debug_level;
    cout << "Logging level set to debug." << endl;


    clog_f(info_message, to_string(__LINE__), __FILE__, "This is a sample information.");
    clog_f(debug_message, to_string(__LINE__), __FILE__, "This is a sample debugging information.");
    clog_f(warn_message, to_string(__LINE__), __FILE__, "This is a sample warning message.");
    clog_f(fatal_message, to_string(__LINE__), __FILE__, "This is a sample fatal error.");*/


    /*// Configure a log_formatter.
    log_formatter<time_element, file_element, message_element> lf{file_element{ANSI_foreground_color::yellow, ANSI_background_color::red, ANSI_text_attribute::blink, delimit{"[","]"}}, message_element{delimit{">>>message: "}}};

    // Compose.
    cout << lf.compose(line_string(to_string(__LINE__)), file_string(__FILE__), message_string("This is a sample message.")) << endl;

    // Get element.
    lf.get<time_element>();
    lf.get<file_element>();
    lf.get<message_element>();

    // Get and configure
    lf.get<time_element>().configure(ANSI_foreground_color::magenta, ANSI_text_attribute::bold, delimit{" ---<",">--- "});
    cout << lf.compose(line_string(to_string(__LINE__)), file_string(__FILE__), message_string("This is a sample message.")) << endl;

    // Simple configure an element.
    lf.configure<file_element>(ANSI_foreground_color::white, delimit{" in file: \"", "\" "});
    cout << lf.compose(line_string(to_string(__LINE__)), file_string(__FILE__), message_string("This is a sample message.")) << endl;

    // Simple reset an element.
    lf.reset<file_element>(ANSI_foreground_color::white, delimit{" in file: \"", "\" "});
    cout << lf.compose(line_string(to_string(__LINE__)), file_string(__FILE__), message_string("This is a sample message.")) << endl;
*/
    /*
    // Colorize.
    colorize cl{ANSI_foreground_color::magenta, ANSI_background_color::red, ANSI_text_attribute::bold, ANSI_text_attribute::blink, ANSI_text_attribute::underline};
    cout << cl("Magenta foreground, red background, bold, blinking and underlined") << endl;
    cout << colorize{ANSI_background_color::red}("Background color red")<<endl;
    cout << colorize{ANSI_foreground_color::red}("Foreground color red")<<endl;
    cout << colorize{ANSI_background_color::red, ANSI_text_attribute::bold}("Background red, bold on") << "\n";
    cout << colorize{ANSI_foreground_color::blue, ANSI_text_attribute::bold}("foreground blue, bold on") << "\n";
    cout << colorize{ANSI_background_color::red, ANSI_text_attribute::bold, ANSI_text_attribute::underline}("Background red, bold on, underlined") << "\n";
    cout << colorize{ANSI_text_attribute::bold, ANSI_text_attribute::underline}("Bold on, underlined") << "\n";

    // Time formatting.
     time_t t = time(nullptr);
   tm tm = *localtime(&t);
   cout << put_time(&tm, "%c %Z") << '\n';

   // Delimit.
   delimit dl{"[", "]"};
   cout << dl("Delimited")<<endl;
   cout << dl(cl("Colorized and delimited")) << endl;
   cout << cl(dl("Delimited and colorized")) << endl;

   // Colorize and delimit.
   colorize cdl{ANSI_foreground_color::magenta, ANSI_background_color::red, ANSI_text_attribute::bold, ANSI_text_attribute::blink, ANSI_text_attribute::underline, delimit("---[","]---")};
       cout << cdl("Delimited and colorized with one call.") << endl;

       // Time element.
       time_element te{ANSI_foreground_color::magenta, ANSI_background_color::red, ANSI_text_attribute::bold, ANSI_text_attribute::blink, ANSI_text_attribute::underline, delimit("---[","]---")};
       cout << te << endl;

       // Thread element.
       thread_element th{ANSI_foreground_color::yellow, ANSI_background_color::red, ANSI_text_attribute::bold, ANSI_text_attribute::blink, ANSI_text_attribute::underline, delimit("<thread id: ",">")};
       cout << th << endl;

       // File element.
       file_element fi{ANSI_foreground_color::yellow, ANSI_background_color::red, ANSI_text_attribute::bold, ANSI_text_attribute::underline, delimit("[file name: ","]")};
       cout << fi(__FILE__) << endl;

       // line element.
       line_element li{delimit("line number is: ")};
       cout << li(to_string(__LINE__)) << endl;

       // function element.
       function_element fu{delimit("function name is: ")};
       cout << fu(__FUNCTION__) << endl;

       // select.
       select<0, int, float, double, char>::type select_test0;
       cout << "The type of select<0, int, float, double, char>::type select_test; command is " << typeid(select_test0).name() << endl;
       select<1, int, float, double, char>::type select_test1;
       cout << "The type of select<1, int, float, double, char>::type select_test; command is " << typeid(select_test1).name() << endl;
       select<2, int, float, double, char>::type select_test2;
       cout << "The type of select<2, int, float, double, char>::type select_test; command is " << typeid(select_test2).name() << endl;
       select<3, int, float, double, char>::type select_test3;
       cout << "The type of select<3, int, float, double, char>::type select_test; command is " << typeid(select_test3).name() << endl;

       // Locate.
       cout << "Location of int in <int, float, double, char> is " << locate<int, int, float, double, char>::value << endl;
       cout << "Location of float in <int, float, double, char> is " << locate<float, int, float, double, char>::value << endl;
       cout << "Location of double in <int, float, double, char> is " << locate<double, int, float, double, char>::value << endl;
       cout << "Location of char in <int, float, double, char> is " << locate<char, int, float, double, char>::value << endl;
       // Generate error. Make sure the following generates an error.
       // cout << "Location of long in <long, float, double, char> is " << locate<long, int, float, double, char>::value << endl;

       // log_formatter.
       // Full initialization.
       log_formatter<time_element, thread_element> lf1{time_element{}, thread_element{}};
       // Partial default initialization.
       log_formatter<time_element, thread_element, file_element> lf2{thread_element{ANSI_foreground_color::red,delimit{"[","]"}}};
       // Generate assertion.
       //log_formatter<time_element, thread_element, int> lf3{};

       // text_type test.
       cout << typeid(message_element::text_type).name() << endl;
       cout << typeid(file_element::text_type).name() << endl;
       cout << typeid(line_element::text_type).name() << endl;
       cout << typeid(function_element::text_type).name() << endl;
       */



}



















