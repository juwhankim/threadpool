// Based on Don Clugston's fastest possible C++ delegates.


#ifndef juwhan_fast_function_h
#define juwhan_fast_function_h
 
#include <cstring>
#include <type_traits>
#include <cassert>
#include <cstddef>
#include <memory>
#include <new>
#include <utility>
 
namespace juwhan
{
	namespace internal
	{

class any_class;
using ptr_to_any_class = any_class*;
using ptr_to_void_void_member_func = void(any_class::*)();
// A function pointer type to any methods of any_class.
template<typename TReturn = void, typename... TArgs> 
using ptr_to_any_member_func = TReturn(any_class::*)(TArgs...);
// A function pointer type to any global static functions
template<typename TReturn = void, typename... TArgs> 
using ptr_to_any_static_func = TReturn(*)(TArgs...);

constexpr ::std::size_t size_of_a_ptr_to_void_void_member_func{sizeof(ptr_to_void_void_member_func)};

template<class TOut, class TIn> 
union horrible_union 
{ 
	TOut out; 
	TIn in; 
};

template<class TOut, class TIn> 
inline TOut horrible_cast(TIn mIn) noexcept 
{ 
	horrible_union<TOut, TIn> u;
	static_assert(sizeof(TIn) == sizeof(u) && sizeof(TIn) == sizeof(TOut), "Cannot use horrible_cast<>");
	u.in = mIn; 
	return u.out;
}

template<class TOut, class TIn> 
inline TOut unsafe_horrible_cast(TIn mIn) noexcept 
{ 
	horrible_union<TOut, TIn> u; 
	u.in = mIn; return u.out; 
}

template<::std::size_t TN> 
struct simplify_member_function
{
	template<class TThis, class TFunc> 
	inline static ptr_to_any_class convert(const TThis*, TFunc, ptr_to_void_void_member_func&) noexcept
	{
		static_assert(TN - 100, "Unsupported member function pointer on this compiler");
		return 0;
	}
};

// Template specialization of simplify_member_function where TN = size_of_a_ptr_to_void_void_member_func.
template<> 
struct simplify_member_function<size_of_a_ptr_to_void_void_member_func>
{
	template<class TThis, class TFunc> 
	inline static ptr_to_any_class convert(const TThis* mThis, TFunc mFunc, ptr_to_void_void_member_func& mFuncOut) noexcept
	{
		mFuncOut = reinterpret_cast<ptr_to_void_void_member_func>(mFunc);
		return reinterpret_cast<ptr_to_any_class>(const_cast<TThis*>(mThis));
	}
};

template<typename TReturn, typename... TArgs> 
struct closure_t
{
private:
	using ptr_to_member_func = ptr_to_any_member_func<TReturn, TArgs...>;
	using ptr_to_static_func = ptr_to_any_static_func<TReturn, TArgs...>;
	ptr_to_any_class this_ptr{nullptr};
	ptr_to_void_void_member_func func_ptr{nullptr};
public:
	template<class TThis, class TFunc> inline void bind(TThis* mThis, TFunc mFunc) noexcept 
	{ 
		this_ptr = simplify_member_function<sizeof(mFunc)>::convert(mThis, mFunc, func_ptr); 
	}
	template<class TThis, class TInvoker> inline void bind(TThis* mThis, TInvoker mInvoker, ptr_to_static_func mFunc) noexcept
	{
		if(mFunc == nullptr) func_ptr = nullptr; else bind(mThis, mInvoker);
		this_ptr = horrible_cast<ptr_to_any_class>(mFunc);
	}

	inline bool operator==(::std::nullptr_t) const noexcept		
	{ 
		return this_ptr == nullptr && func_ptr == nullptr; 
	}
	inline bool operator==(const closure_t& mRhs) const noexcept	
	{ 
		return this_ptr == mRhs.this_ptr && func_ptr == mRhs.func_ptr; 
	}
	inline bool operator==(ptr_to_static_func mPtr) const noexcept	
	{ 
		return mPtr == nullptr ? *this == nullptr : mPtr == reinterpret_cast<ptr_to_static_func>(get_static_func_ptr()); 
	}
	inline bool operator!=(::std::nullptr_t) const noexcept		
	{ 
		return !operator==(nullptr); 
	}
	inline bool operator!=(const closure_t& mRhs) const noexcept { return !operator==(mRhs); }
	inline bool operator!=(ptr_to_static_func mPtr) const noexcept { return !operator==(mPtr); }
	inline bool operator<(const closure_t& mRhs) const
	{ 
		return this_ptr != mRhs.this_ptr ?
		this_ptr < mRhs.this_ptr 
		: ::std::memcmp(&func_ptr, &mRhs.func_ptr, sizeof(func_ptr)) < 0; 
	}
	inline bool operator>(const closure_t& mRhs) const { return !operator<(mRhs); }

	inline ::std::size_t getHash() const noexcept	
	{ 
		return reinterpret_cast<::std::size_t>(this_ptr) ^ internal::unsafe_horrible_cast<::std::size_t>(func_ptr); 
	}
	inline ptr_to_any_class get_this_ptr() const noexcept	{ return this_ptr; }
	inline ptr_to_member_func get_member_func_ptr() const noexcept	{ return reinterpret_cast<ptr_to_member_func>(func_ptr); }
	inline ptr_to_static_func get_static_func_ptr() const noexcept { return horrible_cast<ptr_to_static_func>(this); }
};

template<typename TReturn, typename... TArgs> 
class fast_function_implementation
{
private:
	using ptr_to_static_func = ptr_to_any_static_func<TReturn, TArgs...>;
	closure_t<TReturn, TArgs...> closure;
	inline TReturn invokeStaticFunc(TArgs... mArgs) const 
	{ 
		return (*(closure.get_static_func_ptr()))(::std::forward<TArgs>(mArgs)...); 
	}

protected:
	template<class TThis, class TFunc> 
	inline void bind(TThis* mThis, TFunc mFunc) noexcept 
	{ 
		closure.bind(mThis, mFunc); 
	}
	template<class TFunc> 
	inline void bind(TFunc mFunc) noexcept 
	{ 
		closure.bind(this, &fast_function_implementation::invokeStaticFunc, mFunc); 
	}

public:
	inline fast_function_implementation() noexcept = default;
	inline fast_function_implementation(::std::nullptr_t) noexcept {}
	inline fast_function_implementation(ptr_to_static_func mFunc) noexcept { bind(mFunc); }
	template<typename X, typename Y> inline fast_function_implementation(X* mThis, Y mFunc) noexcept { bind(mThis, mFunc); }

	inline fast_function_implementation& operator=(ptr_to_static_func mFunc) noexcept	{ bind(mFunc); }
	inline TReturn operator()(TArgs... mArgs) const	
	{ 
		return (closure.get_this_ptr()->*(closure.get_member_func_ptr()))(::std::forward<TArgs>(mArgs)...); 
	}

	inline bool operator==(::std::nullptr_t) const noexcept	{ return closure == nullptr; }
	inline bool operator==(const fast_function_implementation& mImpl) const noexcept { return closure == mImpl.closure; }
	inline bool operator==(ptr_to_static_func mFuncPtr) const noexcept { return closure == mFuncPtr; }
	inline bool operator!=(::std::nullptr_t) const noexcept { return !operator==(nullptr); }
	inline bool operator!=(const fast_function_implementation& mImpl) const noexcept { return !operator==(mImpl); }
	inline bool operator!=(ptr_to_static_func mFuncPtr) const noexcept { return !operator==(mFuncPtr); }
	inline bool operator<(const fast_function_implementation& mImpl) const { return closure < mImpl.closure; }
	inline bool operator>(const fast_function_implementation& mImpl) const { return !operator<(mImpl); }
};

			}  // End of namespace internal.

template<typename T> struct member_func_to_static_func;

template<typename TReturn, typename TThis, typename... TArgs>
struct member_func_to_static_func<TReturn(TThis::*)(TArgs...) const> 
{ 
	using Type = TReturn(*)(TArgs...); 
};

// #define ENABLE_IF_CONV_TO_FUN_PTR(x) typename ::std::enable_if<::std::is_constructible<typename member_func_to_static_func<decltype(&::std::decay<x>::type::operator())>::Type, x>::value>::type* = nullptr
// #define ENABLE_IF_NOT_CONV_TO_FUN_PTR(x) typename ::std::enable_if<!::std::is_constructible<typename member_func_to_static_func<decltype(&::std::decay<x>::type::operator())>::Type, x>::value>::type* = nullptr
#define ENABLE_IF_NOT_SAME(x, y)	typename = typename ::std::enable_if<!::std::is_same<x, typename ::std::decay<y>::type>{}>::type

template<typename T> class fast_function;

template<typename TReturn, typename... TArgs>
class fast_function<TReturn(TArgs...)> : public internal::fast_function_implementation<TReturn, TArgs...>
{
private:
	using base_type = internal::fast_function_implementation<TReturn, TArgs...>;
	// ::std::shared_ptr<void> storage;
	template<typename T> inline static void funcDeleter(void* mPtr) { static_cast<T*>(mPtr)->~T(); operator delete(mPtr); }

public:
	using base_type::base_type;

	inline fast_function() noexcept = default;

	template<typename TFunc, ENABLE_IF_NOT_SAME(fast_function, TFunc)> 
	// inline fast_function(TFunc&& mFunc, ENABLE_IF_CONV_TO_FUN_PTR(TFunc))
	inline fast_function(TFunc&& mFunc)
	{
		using FuncType = typename ::std::decay<TFunc>::type;
		this->bind(&mFunc, &FuncType::operator());
	}

	// template<typename TFunc, ENABLE_IF_NOT_SAME(fast_function, TFunc)> 
	// inline fast_function(TFunc&& mFunc, ENABLE_IF_NOT_CONV_TO_FUN_PTR(TFunc))
	// : storage(operator new(sizeof(TFunc)), funcDeleter<typename ::std::decay<TFunc>::type>)
	// {
	// 	::std::cout << "Class2\n";
	// 	using FuncType = typename ::std::decay<TFunc>::type;
	// 	new (storage.get()) FuncType(::std::forward<TFunc>(mFunc));
	// 	this->bind(storage.get(), &FuncType::operator());
	// }
};

#undef ENABLE_IF_CONV_TO_FUN_PTR
#undef ENABLE_IF_NOT_CONV_TO_FUN_PTR
#undef ENABLE_IF_SAME_TYPE


}   // End of namespace juwhan.

#endif







