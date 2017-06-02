#ifndef juwhan_std_h
#define juwhan_std_h

#include<cstddef>
#include<cstdio>
#include<string>

// This is a RESCUE std implementation in case std is not available in the target platform.

namespace juwhan
{


using ::std::string;
using ::std::size_t;

// Reimplementation of a part of std. In case the library is not available.


template< typename T > struct remove_reference						{ using type = T; };
template< typename T > struct remove_reference<T&>				{ using type = T; };
template< typename T > struct remove_reference<T&&>				{ using type = T; };
template< typename T > struct remove_extent								{ using type = T; };
template< typename T > struct remove_extent<T[]>					{ using type = T; };

template<typename T>
constexpr T&& forward(typename remove_reference<T>::type& t) noexcept
{
  return static_cast<T&&>(t);
};
// Oddly enough, c++11/14 standard requires this to be implemented. Maybe for the case forward is used outside function scope.
template<typename T>
constexpr T&& forward(typename remove_reference<T>::type&& t) noexcept
{
  return static_cast<T&&>(t);
};
// The following is a suggested HIGHLY SOPHISTICATED version of forward by Howard E. Hinnant at http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2009/n2951.html. The implementation is AS IS published on the website and COMMENTED OUT intentionally to honor simplicity. It is just for reference.
/*template <class T, class U,
    class = typename enable_if<
         (is_lvalue_reference<T>::value ?
             is_lvalue_reference<U>::value :
             true) &&
         is_convertible<typename remove_reference<U>::type*,
                        typename remove_reference<T>::type*>::value
    >::type>
inline
T&&
forward(U&& u)
{
    return static_cast<T&&>(u);
}*/

template<typename T> 
constexpr typename remove_reference<T>::type&&
move(T&& t) noexcept
{
  using RvalRef = typename remove_reference<T>::type&&;
  return static_cast<RvalRef>(t);
};


template<typename T, T v>
struct integral_constant {
    static constexpr T value = v;
    using value_type = T;
    using type = integral_constant;
    constexpr operator value_type() const noexcept { return value; }
};
template< typename T > struct remove_const          			{ using type = T; };
template< typename T > struct remove_const<const T> 			{ using type = T; };
template< typename T > struct remove_volatile             { using type = T; };
template< typename T > struct remove_volatile<volatile T> { using type = T; };
template< typename T >
struct remove_cv {
    using type = typename remove_volatile<typename remove_const<T>::type>::type;
};

template< typename T, size_t N >
struct remove_extent<T[N]> 																{ using type = T; };
template< class T > struct remove_pointer                    { using type = T; };
template< class T > struct remove_pointer<T*>                { using type = T; };
template< class T > struct remove_pointer<T* const>          { using type = T; };
template< class T > struct remove_pointer<T* volatile>       { using type = T; };
template< class T > struct remove_pointer<T* const volatile> { using type = T; };

template< typename T >
struct add_pointer			{ using type = typename remove_reference<T>::type*; };

template< typename T>
struct add_rvalue_reference	{ using type = T&&; };

template< typename T>
struct add_lvalue_reference	{ using type = T&; };

using true_type = integral_constant<bool, true>;
using false_type = integral_constant<bool, false>;
template< typename T > struct is_pointer_helper     : false_type {};
template< typename T > struct is_pointer_helper<T*> : true_type {};
template< typename T > struct is_pointer : is_pointer_helper<typename remove_cv<T>::type> {};
template< typename T > struct is_integral_helper								: false_type {};
template<> struct is_integral_helper<bool>											: true_type {};
template<> struct is_integral_helper<char>											: true_type {};
template<> struct is_integral_helper<char16_t>									: true_type {};
template<> struct is_integral_helper<char32_t>									: true_type {};
template<> struct is_integral_helper<wchar_t>										: true_type {};
template<> struct is_integral_helper<signed char>								: true_type {};
template<> struct is_integral_helper<short int>									: true_type {};
template<> struct is_integral_helper<int>												: true_type {};
template<> struct is_integral_helper<long int>									: true_type {};
template<> struct is_integral_helper<long long int>							: true_type {};
template<> struct is_integral_helper<unsigned char>							: true_type {};
template<> struct is_integral_helper<unsigned short int>				: true_type {};
template<> struct is_integral_helper<unsigned int>							: true_type {};
template<> struct is_integral_helper<unsigned long int>					: true_type {};
template<> struct is_integral_helper<unsigned long long int>		: true_type {};
template<> struct is_integral_helper<float>											: true_type {};
template<> struct is_integral_helper<double>										: true_type {};
template< typename T > struct is_integral : is_integral_helper<typename remove_cv<T>::type> {};

// primary template
template<typename>
struct is_function : std::false_type { };
 
// specialization for regular functions
template<typename Ret, typename... Args>
struct is_function<Ret(Args...)> : true_type{};
 
// specialization for variadic functions such as std::printf
template<typename Ret, typename... Args>
struct is_function<Ret(Args......)> : true_type{};
 
// specialization for function types that have cv-qualifiers
template<typename Ret, typename... Args>
struct is_function<Ret(Args...)const> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args...)volatile> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args...)const volatile> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args......)const> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args......)volatile> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args......)const volatile> : true_type{};
 
// specialization for function types that have ref-qualifiers
template<typename Ret, typename... Args>
struct is_function<Ret(Args...) &> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args...)const &> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args...)volatile &> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args...)const volatile &> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args......) &> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args......)const &> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args......)volatile &> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args......)const volatile &> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args...) &&> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args...)const &&> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args...)volatile &&> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args...)const volatile &&> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args......) &&> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args......)const &&> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args......)volatile &&> : true_type{};
template<typename Ret, typename... Args>
struct is_function<Ret(Args......)const volatile &&> : true_type{};

template<typename T>
struct is_array : std::false_type {};
template<typename T>
struct is_array<T[]> : std::true_type {};
template<typename T, size_t N>
struct is_array<T[N]> : std::true_type {};


template<bool B, typename T, typename F> struct conditional 			{ using type = T; };
template<typename T, typename F> struct conditional<false, T, F> 	{ using type = F; };

template< typename T >
struct decay {
  using U = typename std::remove_reference<T>::type;
  using type = typename std::conditional< 
    is_array<U>::value,
    typename remove_extent<U>::type*,
    typename conditional< 
      is_function<U>::value,
      typename add_pointer<U>::type,
      typename remove_cv<U>::type
    >::type
  >::type;
};



// Oddly enough, android NDK does not support to_string. Make it.

inline string to_string(int value)
{
	char* buffer;
	// Implement here.
	try
	{
		buffer= new char[80];
	}
	catch(...)
	{
		// We failed to grab memory to hold the converted string. This probably not an exceptional case since missing a string fragment is not such a big deal.
		// Return an error string.
		return string{"to_string(int) error"};
	}
	sprintf(buffer, "%d", value);
	string return_value{buffer};
	delete[] buffer; 
	return return_value;
};

inline string to_string(long value)
{
	char* buffer;
	// Implement here.
	try
	{
		buffer= new char[80];
	}
	catch(...)
	{
		// We failed to grab memory to hold the converted string. This probably not an exceptional case since missing a string fragment is not such a big deal.
		// Return an error string.
		return string{"to_string(long) error"};
	}
	sprintf(buffer, "%ld", value);
	string return_value{buffer};
	delete[] buffer; 
	return return_value; 
};

inline string to_string(long long value)
{
	char* buffer;
	// Implement here.
	try
	{
		buffer= new char[80];
	}
	catch(...)
	{
		// We failed to grab memory to hold the converted string. This probably not an exceptional case since missing a string fragment is not such a big deal.
		// Return an error string.
		return string{"to_string(long long) error"};
	}
	sprintf(buffer, "%lld", value);
	string return_value{buffer};
	delete[] buffer; 
	return return_value; 
};

inline string to_string(unsigned value)
{
	char* buffer;
	// Implement here.
	try
	{
		buffer= new char[80];
	}
	catch(...)
	{
		// We failed to grab memory to hold the converted string. This probably not an exceptional case since missing a string fragment is not such a big deal.
		// Return an error string.
		return string{"to_string(unsigned) error"};
	}
	sprintf(buffer, "%u", value);
	string return_value{buffer};
	delete[] buffer; 
	return return_value; 
};

inline string to_string(unsigned long value)
{
	char* buffer;
	// Implement here.
	try
	{
		buffer= new char[80];
	}
	catch(...)
	{
		// We failed to grab memory to hold the converted string. This probably not an exceptional case since missing a string fragment is not such a big deal.
		// Return an error string.
		return string{"to_string(unsigned long) error"};
	}
	sprintf(buffer, "%lu", value);
	string return_value{buffer};
	delete[] buffer; 
	return return_value;
};

inline string to_string(unsigned long long value)
{
	char* buffer;
	// Implement here.
	try
	{
		buffer= new char[80];
	}
	catch(...)
	{
		// We failed to grab memory to hold the converted string. This probably not an exceptional case since missing a string fragment is not such a big deal.
		// Return an error string.
		return string{"to_string(unsigned long long) error"};
	}
	sprintf(buffer, "%llu", value);
	string return_value{buffer};
	delete[] buffer; 
	return return_value;
};

inline string to_string(float value)
{
	char* buffer;
	// Implement here.
	try
	{
		buffer= new char[80];
	}
	catch(...)
	{
		// We failed to grab memory to hold the converted string. This probably not an exceptional case since missing a string fragment is not such a big deal.
		// Return an error string.
		return string{"to_string(float) error"};
	}
	sprintf(buffer, "%g", value);
	string return_value{buffer};
	delete[] buffer; 
	return return_value;
};

inline string to_string(double value)
{
	char* buffer;
	// Implement here.
	try
	{
		buffer= new char[80];
	}
	catch(...)
	{
		// We failed to grab memory to hold the converted string. This probably not an exceptional case since missing a string fragment is not such a big deal.
		// Return an error string.
		return string{"to_string(double) error"};
	}
	sprintf(buffer, "%g", value);
	string return_value{buffer};
	delete[] buffer; 
	return return_value;
};

inline string to_string(long double value)
{
	char* buffer;
	// Implement here.
	try
	{
		buffer= new char[80];
	}
	catch(...)
	{
		// We failed to grab memory to hold the converted string. This probably not an exceptional case since missing a string fragment is not such a big deal.
		// Return an error string.
		return string{"to_string(long double) error"};
	}
	sprintf(buffer, "%Lg", value);
	string return_value{buffer};
	delete[] buffer; 
	return return_value;
}

// nullptr_t.
using nullptr_t = decltype(nullptr);


// function traits.
template<size_t N, typename H, typename... T> struct function_traits_helper {};
#define F_T_H_MACRO(N) template<typename H, typename... T> struct function_traits_helper<N, H, T...> : function_traits_helper<N+1, T...> { using arg##N##_type = H; };
F_T_H_MACRO(1); F_T_H_MACRO(2); F_T_H_MACRO(3); F_T_H_MACRO(4); F_T_H_MACRO(5); F_T_H_MACRO(6); F_T_H_MACRO(7); F_T_H_MACRO(8); F_T_H_MACRO(9); F_T_H_MACRO(10);
#undef F_T_H_MACRO
#define F_T_H_MACRO(N) template<typename H> struct function_traits_helper<N, H> { static constexpr size_t arity = N; using arg##N##_type = H; };
F_T_H_MACRO(1); F_T_H_MACRO(2); F_T_H_MACRO(3); F_T_H_MACRO(4); F_T_H_MACRO(5); F_T_H_MACRO(6); F_T_H_MACRO(7); F_T_H_MACRO(8); F_T_H_MACRO(9); F_T_H_MACRO(10);
#undef F_T_H_MACRO
template<typename R = void, typename... A> struct function_traits {};
// Real functions.
template<typename R, typename... A>
struct function_traits<R(A...)> : function_traits_helper<1, A...>
{
	using result_type = R;
};
template<typename R> struct function_traits<R()>
{
	static constexpr size_t arity = 0;
	using result_type = R;
};
// Function references.
template<typename R, typename... A>
struct function_traits<R(&)(A...)> : function_traits_helper<1, A...>
{
	using result_type = R;
};
template<typename R> struct function_traits<R(&)()>
{
	static constexpr size_t arity = 0;
	using result_type = R;
};
// Function pointers.
template<typename R, typename... A>
struct function_traits<R(*)(A...)> : function_traits_helper<1, A...>
{
	using result_type = R;
};
template<typename R> struct function_traits<R(*)()>
{
	static constexpr size_t arity = 0;
	using result_type = R;
};
// Class member functions.
template<typename R, typename C, typename... A>
struct function_traits<R(C::*)(A...)> : function_traits_helper<1, A...>
{
	using result_type = R;
};
template<typename R, typename C> struct function_traits<R(C::*)()>
{
	static constexpr size_t arity = 0;
	using result_type = R;
};
// Const class member functions.
template<typename R, typename C, typename... A>
struct function_traits<R(C::*)(A...) const> : function_traits_helper<1, A...>
{
	using result_type = R;
};
template<typename R, typename C> struct function_traits<R(C::*)() const>
{
	static constexpr size_t arity = 0;
	using result_type = R;
};
// Lambda functions.
template<typename T> struct function_traits<T> : function_traits<decltype(&T::operator())> {};

// To utilize SFINAE, if condition is true, type is void.
template<bool B> struct void_if {};
template<> struct void_if<true> { using type = void; };

template<typename T, typename U>
struct is_same : std::false_type {};
 
template<typename T>
struct is_same<T, T> : std::true_type {};

template<bool B, typename T = void> struct enable_if {};
template<typename T> struct enable_if<true, T> { using type = T; };


template <typename T>
class reference_wrapper {
	  T* _ptr;
public:
  // types
  using type = T;
  // construct/copy/destroy
  reference_wrapper(T& ref) noexcept : _ptr(::std::addressof(ref)) {};
  reference_wrapper(T&&) = delete;
  reference_wrapper(const reference_wrapper&) noexcept = default;
  // assignment
  reference_wrapper& operator=(const reference_wrapper& x) noexcept = default;
  // access
  operator T& () const noexcept { return *_ptr; };
  T& get() const noexcept { return *_ptr; };
};


template< class T >
inline typename add_rvalue_reference<T>::type declval() noexcept;

		namespace detail 
		{

template <typename F, typename... Args>
inline auto INVOKE(F&& f, Args&&... args) ->
    decltype(forward<F>(f)(forward<Args>(args)...)) {
      return forward<F>(f)(forward<Args>(args)...);
}
 
template <typename Base, typename T, typename Derived>
inline auto INVOKE(T Base::*pmd, Derived&& ref) ->
    decltype(forward<Derived>(ref).*pmd) {
      return forward<Derived>(ref).*pmd;
}
 
template <typename PMD, typename Pointer>
inline auto INVOKE(PMD&& pmd, Pointer&& ptr) ->
    decltype((*forward<Pointer>(ptr)).*forward<PMD>(pmd)) {
      return (*forward<Pointer>(ptr)).*forward<PMD>(pmd);
}
 
template <typename Base, typename T, typename Derived, typename... Args>
inline auto INVOKE(T Base::*pmf, Derived&& ref, Args&&... args) ->
    decltype((forward<Derived>(ref).*pmf)(forward<Args>(args)...)) {
      return (forward<Derived>(ref).*pmf)(forward<Args>(args)...);
}
 
template <typename PMF, typename Pointer, typename... Args>
inline auto INVOKE(PMF&& pmf, Pointer&& ptr, Args&&... args) ->
    decltype(((*forward<Pointer>(ptr)).*forward<PMF>(pmf))(forward<Args>(args)...)) {
      return ((*forward<Pointer>(ptr)).*forward<PMF>(pmf))(forward<Args>(args)...);
}


		} // End of namespace detail.
 
// Minimal C++11 implementation:
template <typename> struct result_of;
template <typename F, typename... ArgTypes>
struct result_of<F(ArgTypes...)> {
    using type = decltype(detail::INVOKE(std::declval<F>(), std::declval<ArgTypes>()...));
};
 
// Conforming C++14 implementation (is also a valid C++11 implementation):
		namespace detail 
		{

template <typename, typename = void>
struct result_of {};
template <typename F, typename...Args>
struct result_of<F(Args...),
                 decltype(void(detail::INVOKE(std::declval<F>(), std::declval<Args>()...)))> {
    using type = decltype(detail::INVOKE(std::declval<F>(), std::declval<Args>()...));
};


		} // End of namespace detail.
 
template <typename T> struct result_of : detail::result_of<T> {};

template<typename R = void, typename... A> struct function_type_deduction {};

template<typename R, typename... A> struct function_type_deduction<R(A...)>
{
	static constexpr bool is_static = true;
	static constexpr bool is_member = false;
	static constexpr bool is_functor = false;
	using full_traits = R(A...);
	using simplified_traits = R(A...);
};

template<typename R, typename C, typename... A> struct function_type_deduction<R(C::*)(A...)>
{
	static constexpr bool is_static = false;
	static constexpr bool is_member = true;
	static constexpr bool is_functor = false;
	using full_traits = R(C::*)(A...);
	using simplified_traits = R(A...);
};

template<typename R, typename C, typename... A> struct function_type_deduction<R(C::*)(A...) const>
{
	static constexpr bool is_static = false;
	static constexpr bool is_member = true;
	static constexpr bool is_functor = false;
	using full_traits = R(C::*)(A...) const;
	using simplified_traits = R(A...);
};

template<typename R, typename... A> struct function_type_deduction<R(*)(A...)>
{
	static constexpr bool is_static = true;
	static constexpr bool is_member = false;
	static constexpr bool is_functor = false;
	using full_traits = R(*)(A...);
	using simplified_traits = R(A...);
};

template<typename R, typename... A> struct function_type_deduction<R(&)(A...)>
{
	static constexpr bool is_static = true;
	static constexpr bool is_member = false;
	static constexpr bool is_functor = false;
	using full_traits = R(&)(A...);
	using simplified_traits = R(A...);
};

template<typename T> struct function_type_deduction<T> : function_type_deduction<decltype(&T::operator())> 
{
	static constexpr bool is_static = false;
	static constexpr bool is_member = false;
	static constexpr bool is_functor = true;
};


}  // End of namespace juwhan.





#endif






























