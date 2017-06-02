#ifndef juwhan_thread_h
#define juwhan_thread_h

#include <iostream>
#include <string>
#include <cstring>
#include <pthread.h>
#include <unistd.h>
#include <system_error>
#include <sched.h>
#include <cstdlib>

#include "parameter_pack.h"

/* 
A short story behind writing this library:
Strictly speaking, writing this is practically re-inventing the wheel, i.e., stupid.
Even so, I am doing this because
1. On MacOSX thread local storage doesn't work, and I am pretty sure it won't on Android either.
2. So I tried pthread_get/setspecific. Failed miserably with ABI error.
3. It turns out that pthread_get/setspecific is INCOMPATIBLE with thread.
4. Atomics seem to work in pthread.
5. I concluded that pthread is more stringent than std:thread.
6. I still want the convenience thread provide.
7. Hence, write a wrapper around pthread that works like thread.
*/

namespace juwhan
{

using namespace std;

template<size_t S> struct thread_id_int_t_helper {};
template<> struct thread_id_int_t_helper<2> {using type = uint16_t; };
template<> struct thread_id_int_t_helper<4> {using type = uint32_t; };
template<> struct thread_id_int_t_helper<8> {using type = uint64_t; };
struct thread_id_int_t : thread_id_int_t_helper<sizeof(pthread_t)> {};

// F returns void.
template<typename F, typename... A> inline void* launcher_helper(void* packed)
{
	// Unpack the pack.
	auto unpacked = unpack_parameters<F, A...>(packed);
	// Execute.
	(*unpacked)();
	// Free memory.
	free(packed);
	pthread_exit(nullptr);
}

class thread
{
	pthread_t thread_id;
	bool thread_active;
	bool thread_joinable;
	void* parameter_pack_alias;
public:
	using native_handle_type = pthread_t;
	// id type.
	class id 
	{
		thread_id_int_t::type value_;
	public:
		using value_type = thread_id_int_t::type;
		value_type& value() { return value_; };
	};
	// Required constructors.
	thread() noexcept : thread_id{}, thread_active{false}, thread_joinable{false} {};
	thread(thread&& other) noexcept : thread_id{other.thread_id}, thread_active{other.thread_active}, thread_joinable{other.thread_joinable} 
	{
		other.thread_id = 0;
		other.thread_active = false;
		other.thread_joinable = false;
	};
	template<typename F, typename... A> explicit thread(F&& func, A&&... args)
	: thread_id{0}, thread_active{false}, thread_joinable{false}
	{
		// system_error if the thread could not be started. The exception may represent the error condition errc::resource_unavailable_try_again or another implementation-specific error condition.
		// Make a parameter pack.
		parameter_pack_alias = pack_parameters(forward<F>(func), forward<A>(args)...); // This is supposed to be a void*.
		// Create a thread.
		auto return_code = pthread_create(&thread_id, NULL, &launcher_helper<F, A...>, parameter_pack_alias);
		if (return_code)
		{
			// Clear out.
			thread_id = 0;
			throw system_error{error_code{}, "Thread creation failure."};
		}
		thread_active = true;
		thread_joinable = true;
	};
	thread(const thread&) = delete;
	// Required move assignment.
	thread& operator=(thread&& other) noexcept
	{
		thread_id = other.thread_id;
		thread_active = other.thread_active;
		thread_joinable = other.thread_joinable;
		other.thread_id = 0;
		other.thread_active = false;
		other.thread_joinable = false;
		return *this;
	};
	// Copy assignment is deleted.
	thread& operator=(thread&) = delete;
	// Required observers.
	bool joinable() const noexcept
	{
		return thread_joinable;
	};
	id get_id() noexcept
	{
		// Memcopy thread_id into id's value.
		id _id{};
		memcpy(&_id.value(), &thread_id, sizeof(pthread_t));
		return _id;
	};
	native_handle_type native_handle() noexcept { return thread_id; };
	static unsigned hardware_concurrency() noexcept { return sysconf(_SC_NPROCESSORS_ONLN); };
	// Required operations.
	void join()
	{ 
		// Throws system_error.
		if (!thread_active) throw system_error{error_code{}, "Thread join has been requested on an inactive thread."};
		if (!thread_joinable) throw system_error{error_code{}, "Thread join has been requested on an unjoinale thread."};
		pthread_join(thread_id, nullptr);
		thread_joinable = false;
		thread_active = false;
		thread_id = 0;
	};
	void detach()
	{
		// system_error if joinable() == false or an error occurs.
		if (!thread_active) throw system_error{error_code{}, "Thread detach has been requested on an inactive thread."};
		if (!thread_joinable) throw system_error{error_code{}, "Thread detach has been requested on an unjoinable thread."};
		pthread_detach(thread_id);
		thread_joinable = false;
	};
	void swap(thread& other) noexcept 
	{
		thread tmp{};
		tmp.thread_id = thread_id;
		tmp.thread_active = thread_active;
		tmp.thread_joinable = thread_joinable;
		thread_id = other.thread_id;
		thread_active = other.thread_active;
		thread_joinable = other.thread_joinable;
		other.thread_id = tmp.thread_id;
		other.thread_active = tmp.thread_active;
		other.thread_joinable = tmp.thread_joinable;
	};
	// Required destructor.
	~thread() {};
};
inline bool operator==(thread::id lhs, thread::id rhs) noexcept	{ return lhs.value()==rhs.value(); };
inline bool operator!=(thread::id lhs, thread::id rhs) noexcept	{ return lhs.value()!=rhs.value(); };
inline bool operator<(thread::id lhs, thread::id rhs) 	noexcept	{ return lhs.value()<rhs.value(); };
inline bool operator<=(thread::id lhs, thread::id rhs) noexcept	{ return lhs.value()<=rhs.value(); };
inline bool operator>(thread::id lhs, thread::id rhs) 	noexcept	{ return lhs.value()>rhs.value(); };
inline bool operator>=(thread::id lhs, thread::id rhs) noexcept	{ return lhs.value()>=rhs.value(); };
// Non-member swap.
// void swap(thread& lhs, thread& rhs) {};
// id output.
inline string convert2hex_char(thread_id_int_t::type number)
{
	switch (number)
	{
		case 0: return "0";
		case 1: return "1";
		case 2: return "2";
		case 3: return "3";
		case 4: return "4";
		case 5: return "5";
		case 6: return "6";
		case 7: return "7";
		case 8: return "8";
		case 9: return "9";
		case 10: return "a";
		case 11: return "b";
		case 12: return "c";
		case 13: return "d";
		case 14: return "e";
		case 15: return "f";
	}
	// Something went wrong.
	return "";
}
inline string convert2hex_string(thread::id::value_type number)
{
	string hex_string = "0x";
	auto byte_size = sizeof(thread_id_int_t::type);
	auto half_byte_size = byte_size*2;
	thread::id::value_type hex_mask = (16-1) << (half_byte_size*4-4);
	for(auto i=0; i<half_byte_size-1; ++i)
	{
		auto half_byte_number = (number & hex_mask) >> (4*(half_byte_size-i-1));
		hex_string += convert2hex_char(half_byte_number);
		hex_mask = hex_mask >> 4;
	}
	auto half_byte_number = number & hex_mask;
	hex_string += convert2hex_char(half_byte_number);
	return hex_string;
}
template<typename CharT, typename Traits> inline basic_ostream<CharT, Traits>&
operator<<(basic_ostream<CharT, Traits>& ost, thread::id id)
{
	return ost << convert2hex_string(id.value());
};
// Hash.
template<typename Key> struct hash;
template<> struct hash<thread::id>
{
	thread::id::value_type value;
	using argument_type = thread::id;
	using result_type = size_t;
	hash(thread& _thread) : value{_thread.get_id().value()} {};
	hash() = default;
	hash(const hash&) = default;
	hash(hash&&) = default;
	~hash() = default;
	size_t operator()() { return static_cast<size_t>(value); };
};

		// From here, this_thread namespace.
		namespace this_thread
		{


inline void yield()
{
	auto result = sched_yield();
	if (result) throw system_error{error_code{}, "An unknown error occurred while trying to yield a thread."};
}

inline thread::id get_id()
{
	pthread_t thread_id = pthread_self();
	// Now, convert this value into an id.
	thread::id _id;
	memcpy(&_id.value(), &thread_id, sizeof(pthread_t));
	return _id;
}

// Future implementation.
/*template< class Rep, class Period >
void sleep_for( const chrono::duration<Rep, Period>& sleep_duration );

template< class Clock, class Duration >
void sleep_until( const chrono::time_point<Clock,Duration>& sleep_time );*/

		}  // End of namespace this_thread.

}  // End of namespace juwhan.



#endif


















