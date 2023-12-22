#include<iostream>
#include<string>
#include<sstream>
#include<chrono>
#include<vector>
#include<iomanip>
#include<thread>
#include<fstream>

#ifndef juwhan_logger_implementation_h
#define juwhan_logger_implementation_h
namespace juwhan {
    namespace logger {

        using namespace std;


// A trivial class to define delimiters.
        class delimit {
            string left_delim;
            string right_delim;
        public:
            delimit(const string &left_in = {}, const string &right_in = {}) : left_delim{left_in},
                                                                               right_delim{right_in} {};

            string operator()(const string &input_string) const {
                return left_delim + input_string + right_delim;
            }
        };


// ANSI color enum classes.
// The enum values actually mean something. The values are ANSI codes. Also, the usual escape sequence is \33[val1;val2;...mPhrase_to_be_colorized\33[0m.
// Note that color_value=0 is reserved for no color specified.
// Refer to http://ascii-table.com/ansi-escape-sequences.php
        enum class ANSI_foreground_color : unsigned int {
            black = 30, red = 31, green = 32, yellow = 33, blue = 34, magenta = 35, cyan = 36, white = 37
        };
        enum class ANSI_background_color : unsigned int {
            black = 40, red = 41, green = 42, yellow = 43, blue = 44, magenta = 45, cyan = 46, white = 47
        };


// Selected ANSI formatting text attributes for log message formatting.
// Refer to http://ascii-table.com/ansi-escape-sequences.php
        enum class ANSI_text_attribute : unsigned int {
            bold = 1,
            underline = 4,
            blink = 5
        };


        class colorize {
            unsigned int foreground_color;
            unsigned int background_color;
            delimit delim;
            vector<unsigned int> text_attributes;

            template<typename T, typename... Types>
            void recursive_init(const T value, const Types... args) {
                // Peel off the first argument, and check if it is a proper one.
                static_assert(is_same<ANSI_foreground_color, T>::value
                              || is_same<ANSI_background_color, T>::value
                              || is_same<ANSI_text_attribute, T>::value,
                              "Input type T to recursive_init<typename T...> must be either ANSI_foreground_color, ANSI_background_color, ANSI_text_attribute, or delimit.");
                // Now, initialize according to the type.
                if (is_same<ANSI_foreground_color, T>::value) {
                    foreground_color = static_cast<unsigned int>(value);
                    recursive_init(args...); // Recurse.
                } else if (is_same<ANSI_background_color, T>::value) {
                    background_color = static_cast<unsigned int>(value);
                    recursive_init(args...); // Recurse.
                } else if (is_same<ANSI_text_attribute, T>::value) {
                    text_attributes.push_back(static_cast<unsigned int>(value));
                    recursive_init(args...); // Recurse.
                }
            }

            template<typename... Types>
            void recursive_init(const delimit value, const Types... args) {
                // We don't need to check the type of value since it is already specialized to delimit.
                delim = value;
                recursive_init(args...);
            }

            template<typename T>
            void recursive_init(const T value) {
                // Peel off the last and only argument, and check if it is a proper one.
                static_assert(is_same<ANSI_foreground_color, T>::value
                              || is_same<ANSI_background_color, T>::value
                              || is_same<ANSI_text_attribute, T>::value,
                              "Input type T to colorize<typename T...> must be either ANSI_foreground_color, ANSI_background_color, or ANSI_text_attribute.");
                // Now, initialize according to the type.
                if (is_same<ANSI_foreground_color, T>::value) {
                    foreground_color = static_cast<unsigned int>(value);
                    // Stop recursing.
                } else if (is_same<ANSI_background_color, T>::value) {
                    background_color = static_cast<unsigned int>(value);
                    // Stop recursing.
                } else if (is_same<ANSI_text_attribute, T>::value) {
                    text_attributes.push_back(static_cast<unsigned int>(value));
                    // Stop recursing.
                }
            }

            void recursive_init(const delimit value) {
                // We don't need to check the type of value since it is already specialized to delimit.
                delim = value;
                // Stop recursing.
            }

        public:
            // Default constructor is default.
            colorize() = default;

            // construct and adapt to various formatting needs by using variadic templates.
            template<typename T, typename... Types>
            colorize(const T value, const Types... args)
                    : foreground_color{0}, background_color{0}, text_attributes{}, delim{} {
                recursive_init(value, args...);
            }

            // Provide ability to configure during runtime.
            template<typename T, typename... Types>
            void configure(const T value, const Types... args) {
                recursive_init(value, args...);
            }

            template<typename T, typename... Types>
            void reset(const T value, const Types... args) {
                foreground_color = 0;
                background_color = 0;
                text_attributes.clear();
                recursive_init(value, args...);
            }

            // Output diagnostic information.
            virtual string info() const {
                ostringstream stream;
                stream << "This colorization object has the foreground color code: " << foreground_color
                       << ", the background color code: " << background_color;
                if (!text_attributes.empty()) {
                    stream << ", and the formatting codes: {" << *text_attributes.begin();
                    for (auto iter = text_attributes.begin() + 1; iter != text_attributes.end(); iter++)
                        stream << ", " << *iter;
                    stream << "}";
                }
                stream << "." << endl;
                return delim(stream.str());
            }

            // colorizing.
            string operator()(const string &input_string) const {
                // If no formatting is specified, just return the input_string.
                if (!foreground_color && !background_color && text_attributes.empty())
                    return delim(input_string);
                // This is the string stream to be manipulated.
                ostringstream stream;
                // Make the opening sequence.
                stream << "\33[";
                if (foreground_color) {
                    stream << foreground_color;
                    if (background_color) {
                        stream << ";" << background_color;
                    }
                    if (!text_attributes.empty()) for (auto attr: text_attributes) stream << ";" << attr;
                } else {
                    if (background_color) {
                        stream << background_color;
                        if (!text_attributes.empty()) for (auto attr: text_attributes) stream << ";" << attr;
                    } else {
                        if (!text_attributes.empty()) {
                            stream << *text_attributes.begin();
                            for (auto attr = text_attributes.begin() + 1; attr != text_attributes.end(); attr++)
                                stream << ";" << *attr;
                        }
                    }
                }
                // Add "m" to end the opening sequence.
                stream << "m";
                // Add the input string.
                stream << delim(input_string);
                // Add the closing sequence to return back to original formatting.
                stream << "\33[0m";
                // Return.
                return stream.str();
            }
        };


// Base class for formatting elements.
// There are two kinds of elements, one with message body and the other without one.
// The reason behind sperating them and making two base classes is because they are used differently.
// For example, time element is generated and inserted without any additional message, hence overloading the string conversion function suffices, and operator() doesn't make sense.
// On the other hand, file name must be provided by user at usage point, hence additional message is required.
        class formatting_element_without_message : public colorize {
        public:
            // Inherit constructors from colorize.
            using colorize::colorize;

            string operator()(const string &input_string) const = delete;

            virtual operator string() const = 0;
        };


// Operator overloading for formatting_element_without_message.
        ostream &operator<<(ostream &, const formatting_element_without_message &);


        class formatting_element_with_message : public colorize {
        public:
            // Inherit constructors from colorize.
            using colorize::colorize;

            // Inherit constructors from colorize.
            virtual string operator()(const string &input_string) const = 0;
        };




//////////////////////////////////////////////////////////////////////////////////////////////////////////




// Time element.
        class time_element : public formatting_element_without_message {
            const char *time_format = "%c %Z";
        public:
            // Inherit constructors from formatting_element.
            using formatting_element_without_message::formatting_element_without_message;

            // Inherit configure from colorize.
            operator string() const {
                time_t t = time(nullptr);
                tm tm = *localtime(&t);
                // Since return type of std::put_time is unspecified by the standard, we need to put it into a string stream and return the string part of it.
                ostringstream stream;
                stream << put_time(&tm, time_format);
                return colorize::operator()(stream.str());
            }
        };


// Thread element.
        class thread_element : public formatting_element_without_message {
        public:
            // Inherit constructors from formatting_element.
            using formatting_element_without_message::formatting_element_without_message;

            // Inherit configure from colorize.
            operator string() const {
                ostringstream stream;
                stream << this_thread::get_id();
                return colorize::operator()(stream.str());
            }
        };


        struct file_string {
            string text;

            file_string(const string &input_string = {}) : text(input_string) {};

            operator string() const { return text; };
        };


// File element.
        class file_element : public formatting_element_with_message {
        public:
            using text_type = file_string;
            // Inherit constructors from formatting_element.
            using formatting_element_with_message::formatting_element_with_message;

            // Inherit configure from colorize.
            string operator()(const string &input_string) const {
                return colorize::operator()(input_string);
            }
        };


        struct line_string {
            string text;

            line_string(const string &input_string = {}) : text(input_string) {};

            operator string() const { return text; };
        };


// Line element.
        class line_element : public formatting_element_with_message {
        public:
            using text_type = line_string;
            // Inherit constructors from formatting_element.
            using formatting_element_with_message::formatting_element_with_message;

            // Inherit configure from colorize.
            string operator()(const string &input_string) const {
                return colorize::operator()(input_string);
            }
        };


        struct function_string {
            string text;

            function_string(const string &input_string = {}) : text(input_string) {};

            operator string() const { return text; };
        };


// Function element.
        class function_element : public formatting_element_with_message {
        public:
            using text_type = function_string;
            // Inherit constructors from formatting_element.
            using formatting_element_with_message::formatting_element_with_message;
            // Inherit configure from colorize.
            using colorize::configure;

            string operator()(const string &input_string) const {
                return colorize::operator()(input_string);
            }
        };


        struct message_string {
            string text;

            message_string(const string &input_string = {}) : text(input_string) {};

            operator string() const { return text; };
        };


// Message element.
        class message_element : public formatting_element_with_message {
        public:
            using text_type = message_string;
            // Inherit constructors from formatting_element.
            using formatting_element_with_message::formatting_element_with_message;

            // Inherit configure from colorize.
            string operator()(const string &input_string) const {
                return colorize::operator()(input_string);
            }
        };



//////////////////////////////////////////////////////////////////////////////////////////////////////////



// A small variadic type selection utility
        template<unsigned int N, typename Head, typename... Tail>
        struct select : select<N - 1, Tail...> {
        };

        template<typename Head, typename... Tail>
        struct select<0, Head, Tail...> {
            using type = Head;
        };

// A small variadic type location utility.
        template<typename Target, typename Head, typename... Tail>
        struct locate : locate<Target, Tail...> {
            static constexpr unsigned int value = locate<Target, Tail...>::value + 1;
        };

        template<typename Target, typename... Tail>
        struct locate<Target, Target, Tail...> {
            static constexpr unsigned int value = 0;
        };




//////////////////////////////////////////////////////////////////////////////////////////////////////////



        template<typename Head, typename... Tail>
        class log_formatter : public log_formatter<Tail...> {
            void is_valid() {
                // Check if Head is either a formatting_element_with_message or a formatting_element_with_message.
                static_assert(is_base_of<formatting_element_with_message, Head>::value
                              || is_base_of<formatting_element_without_message, Head>::value,
                              "Bad types to log_formatter");
            }

            // Helper function to distinguish between with and without a message classes.
            template<typename... T>
            string compose_with_head(
                    const formatting_element_without_message & // This is the function selector
                    , const T &... messages) const {
                // Now, the element is of type without_message.
                return string(element);
            }

            template<typename... T>
            string compose_with_head(
                    const formatting_element_with_message & // This is the function selector
                    , const T &... messages) const {
                // We need to find the location of the corresponding string parameter for this element in the pack.
                constexpr unsigned int str_loc = locate<typename Head::text_type, T...>::value;
                // Now, get it.
                tuple<T...> tpl{messages...};
                auto fetched_message = std::get<str_loc>(tpl);
                return element(fetched_message);
            }

        public:
            using base = log_formatter<Tail...>;
            Head element;

            // Constructors.
            log_formatter() : base{}, element{} { is_valid(); };

            // Template constructor. I do this to enable partial specification at initialization.
            // e.g., og_formatter<time_element, thread_element, file_element> lf2{thread_element{ANSI_foreground_color::red,delimit{"[","]"}}};
            // Order of elemental initialization must be kept, although some can be omitted.
            template<typename Candidate, typename... The_rest>
            log_formatter(const Candidate c,
                          const The_rest... tr) // This function did not hit the Head type, so keep calling the base constructor until a hit is made.
                    : base{c, tr...}, element{} { is_valid(); };

            template<typename... The_rest>
            log_formatter<The_rest...>(Head h, The_rest... tr) // We hit the correct type. Peel it off from the pack.
                    : base{tr...}, element{h} { is_valid(); };

            template<typename Candidate>
            log_formatter(const Candidate c) // No hit, proceed to next.
                    : base{c}, element{} { is_valid(); };

            log_formatter(const Head h) // Final.
                    : element{h} { is_valid(); };

            // Compose a log message.
            template<typename... Ms>
            string compose(const Ms &... ms) const {
                return compose_with_head(element, ms...) + base::compose(ms...);
            }

            // element accessor, according to matching type.
            template<typename Target>
            typename enable_if<is_same<Target, decltype(element)>::value, Target>::type &
            get() {
                // cout << "Corret, returning..." << endl;
                return element;
            }

            template<typename Target>
            typename enable_if<!is_same<Target, decltype(element)>::value, Target>::type &
            get() {
                // cout << "Incorret, current type is " << typeid(element).name() << " recursing..." << endl;
                return base::template get<Target>();
            }

            // A utility helper to get and configure elements at once.
            template<typename Target, typename... Arg>
            void configure(Arg... arg) {
                get<Target>().configure(arg...);
            }

            // A utility helper to get and reset elements at once.
            template<typename Target, typename... Arg>
            void reset(Arg... arg) {
                get<Target>().reset(arg...);
            }
        };


        template<typename Head>
        class log_formatter<Head> {
            void is_valid() {
                // Check if Head is either a formatting_element_with_message or a formatting_element_with_message.
                static_assert(is_base_of<formatting_element_with_message, Head>::value
                              || is_base_of<formatting_element_without_message, Head>::value,
                              "Bad types to log_formatter");
            }

            // Helper function to distinguish between with and without a message classes.
            template<typename... T>
            string compose_with_head(
                    const formatting_element_without_message & // This is the function selector
                    , const T &... messages) const {
                // Now, the element is of type without_message.
                return string(element);
            }

            template<typename... T>
            string compose_with_head(
                    const formatting_element_with_message & // This is the function selector
                    , const T &... messages) const {
                // We need to find the location of the corresponding string parameter for this element in the pack.
                constexpr unsigned int str_loc = locate<typename Head::text_type, T...>::value;
                // Now, get it.
                tuple<T...> tpl{messages...};
                auto fetched_message = std::get<str_loc>(tpl);
                return element(fetched_message);
            }

        public:
            Head element;

            // Constructors.
            log_formatter() : element{} { is_valid(); };

            log_formatter(const Head h) // Final.
                    : element{h} { is_valid(); };

            // Compose a log message.
            template<typename M, typename... Ms>
            string compose(const M &m, const Ms &... ms) const {
                return compose_with_head(element, m, ms...);
            }

            // element accessor, according to matching type.
            template<typename Target>
            typename enable_if<is_same<Target, decltype(element)>::value, Target>::type &
            get() {
                // cout << "Corret, returning..." << endl;
                return element;
            }

            template<typename Target>
            typename enable_if<!is_same<Target, decltype(element)>::value, Target>::type &
            get() {
                static_assert(true,
                              "Something bad happened. Either you tried to find a non-existing element in log_formatter or simply it's a bug.");
            }

            // A utility helper to get and configure elements at once.
            template<typename Target, typename... Arg>
            void configure(Arg... arg) {
                get<Target>().configure(arg...);
            }

            // A utility helper to get and reset elements at once.
            template<typename Target, typename... Arg>
            void reset(Arg... arg) {
                get<Target>().reset(arg...);
            }
        };


//////////////////////////////////////////////////////////////////////////////////////////////////////////


// To store the kind of log line.
        class head_element : public formatting_element_without_message {
            string head;
        public:
            using base = formatting_element_without_message;

            head_element() = delete;

            template<typename... T>
            head_element(const string &name, T... args) : head{name}, base{args...} {};

            // Inherit configure from colorize.
            operator string() const {
                return colorize::operator()(head);
            }
        };

// Now, to the practical application.
        template<typename Head, typename... Tail>
        class log_line : public log_formatter<Tail...> {
        public:
            int priority;
        private:
            void is_valid() {
                // Check if Head is either a formatting_element_with_message or a formatting_element_with_message.
                static_assert(is_base_of<formatting_element_with_message, Head>::value
                              || is_base_of<formatting_element_without_message, Head>::value,
                              "Bad types to log_formatter");
            }

            // Helper function to distinguish between with and without a message classes.
            template<typename... T>
            string compose_with_head(
                    const formatting_element_without_message & // This is the function selector
                    , const T &... messages) const {
                // Now, the element is of type without_message.
                return string(element);
            }

            template<typename... T>
            string compose_with_head(
                    const formatting_element_with_message & // This is the function selector
                    , const T &... messages) const {
                // We need to find the location of the corresponding string parameter for this element in the pack.
                constexpr unsigned int str_loc = locate<typename Head::text_type, T...>::value;
                // Now, get it.
                tuple<T...> tpl{messages...};
                auto fetched_message = std::get<str_loc>(tpl);
                return element(fetched_message);
            }

        public:
            using base = log_formatter<Tail...>;
            Head element;

            // Constructors.
            log_line() = delete;

            // Template constructor. I do this to enable partial specification at initialization.
            // e.g., og_formatter<time_element, thread_element, file_element> lf2{thread_element{ANSI_foreground_color::red,delimit{"[","]"}}};
            // Order of elemental initialization must be kept, although some can be omitted.
            template<typename Candidate, typename... The_rest>
            log_line(const int i, const Candidate c,
                     const The_rest... tr) // This function did not hit the Head type, so keep calling the base constructor until a hit is made.
                    : priority{i}, base{c, tr...}, element{} { is_valid(); };

            template<typename... The_rest>
            log_line<The_rest...>(const int i, Head h,
                                  The_rest... tr) // We hit the correct type. Peel it off from the pack.
                    : priority{i}, base{tr...}, element{h} { is_valid(); };

            template<typename Candidate>
            log_line(const int i, const Candidate c) // No hit, proceed to next.
                    : priority{i}, base{c}, element{} { is_valid(); };

            log_line(const int i, const Head h) // Final.
                    : priority{i}, element{h} { is_valid(); };

            // Compose a log message.
            template<typename... Ms>
            string compose(const Ms &... ms) const {
                return compose_with_head(element, ms...) + base::compose(ms...);
            }

            // element accessor, according to matching type.
            template<typename Target>
            typename enable_if<is_same<Target, decltype(element)>::value, Target>::type &
            get() {
                // cout << "Corret, returning..." << endl;
                return element;
            }

            template<typename Target>
            typename enable_if<!is_same<Target, decltype(element)>::value, Target>::type &
            get() {
                // cout << "Incorret, current type is " << typeid(element).name() << " recursing..." << endl;
                return base::template get<Target>();
            }

            // A utility helper to get and configure elements at once.
            template<typename Target, typename... Arg>
            void configure(Arg... arg) {
                get<Target>().configure(arg...);
            }

            // A utility helper to get and reset elements at once.
            template<typename Target, typename... Arg>
            void reset(Arg... arg) {
                get<Target>().reset(arg...);
            }

            // Operator ().
            template<typename... Ms>
            string operator()(const Ms &... ms) const {
                return compose(ms...);
            }
        };


        template<typename Head>
        class log_line<Head> {
        public:
            int priority;
        private:
            void is_valid() {
                // Check if Head is either a formatting_element_with_message or a formatting_element_with_message.
                static_assert(is_base_of<formatting_element_with_message, Head>::value
                              || is_base_of<formatting_element_without_message, Head>::value,
                              "Bad types to log_formatter");
            }

            // Helper function to distinguish between with and without a message classes.
            template<typename... T>
            string compose_with_head(
                    const formatting_element_without_message & // This is the function selector
                    , const T &... messages) const {
                // Now, the element is of type without_message.
                return string(element);
            }

            template<typename... T>
            string compose_with_head(
                    const formatting_element_with_message & // This is the function selector
                    , const T &... messages) const {
                // We need to find the location of the corresponding string parameter for this element in the pack.
                constexpr unsigned int str_loc = locate<typename Head::text_type, T...>::value;
                // Now, get it.
                tuple<T...> tpl{messages...};
                auto fetched_message = std::get<str_loc>(tpl);
                return element(fetched_message);
            }

        public:
            Head element;

            // Constructors.
            log_line() = delete;

            log_line(const int i, const Head h) // Final.
                    : priority{i}, element{h} { is_valid(); };

            // Compose a log message.
            template<typename M, typename... Ms>
            string compose(const M &m, const Ms &... ms) const {
                return compose_with_head(element, m, ms...);
            }

            // element accessor, according to matching type.
            template<typename Target>
            typename enable_if<is_same<Target, decltype(element)>::value, Target>::type &
            get() {
                // cout << "Corret, returning..." << endl;
                return element;
            }

            template<typename Target>
            typename enable_if<!is_same<Target, decltype(element)>::value, Target>::type &
            get() {
                static_assert(true,
                              "Something bad happened. Either you tried to find a non-existing element in log_formatter or simply it's a bug.");
            }

            // A utility helper to get and configure elements at once.
            template<typename Target, typename... Arg>
            void configure(Arg... arg) {
                get<Target>().configure(arg...);
            }

            // A utility helper to get and reset elements at once.
            template<typename Target, typename... Arg>
            void reset(Arg... arg) {
                get<Target>().reset(arg...);
            }

            // Operator ().
            template<typename... Ms>
            string operator()(const Ms &... ms) const {
                return compose(ms...);
            }
        };

    } // End of namespace logger.
} // End of namespace juwhan.
#endif