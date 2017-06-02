/* 
Juwhan's version of aligned array.

The aligned array must satisfy 2 properties:
1. The start of the array must be aligned.
2. Each item is properly strided according to alignment.

In pictorial description,

1.
|xxxxxxxx0000|xxxxxxxx0000| ...
 ^
 |
Start of the array aligned to alignement.

2.
|xxxxxxxx0000|xxxxxxxx0000| ...
 ^------^ : The actual data element.

|xxxxxxxx0000|xxxxxxxx0000| ...
 ^----------^ : Aligned object which takes exactly the integral multiple of alignment value.

Also, part of the main reasons why I wrote this class instead of using the standard aligned_alloc and alligned_storage is - I wanted to avoid fancy templates that might not be supported in embedded systems. In fact, I am not even sure if some systems support aligned_* functionalities.

Hence, everything is INTENTIONALLY written in simple and less powerful manner.

*/

#ifndef juwhan_aligned_circular_array_h
#define juwhan_aligned_circular_array_h

#include <atomic>
#include <string>
#include <exception>
#include <stdexcept>
#include <cstring>

#include "juwhan_std.h"

// Define cacheline size. For intel CPUs it is 64, and the most recent(at the time of writing this) ARM15 has also 64 bytes-sized cacheline. Assume other CPUs have the size of cacheline LESS than this value. I might substitute this line with pre-defined macros to select proper values, but for now, the following one line definition serves the purpose of the library. 
#define JUWHAN_CACHELINE_SIZE 64

#include "include_me.h"

#define aca_info(...)
#define aca_info_if(...)


namespace juwhan
{


// A small function to round up an integer to the nearest power of 2 value.
template<bool B, typename T = void> struct Tif {};
template<typename T> struct Tif<true, T> { using type = T; };
// T is 16 bit.
template<typename T> inline
typename Tif<sizeof(T)==2, T>::type next_power_of_2(T _value){
	// Return the value itself, if it is already a power of 2.
	if ( _value && !(_value & (_value - 1)) ) return _value;
	--_value;
  _value |= _value >> 1;
  _value |= _value >> 2;
  _value |= _value >> 4;
  _value |= _value >> 8;
  return _value+1;
}
// T is 32 bit.
template<typename T> inline
typename Tif<sizeof(T)==4, T>::type next_power_of_2(T _value){
	// Return the value itself, if it is already a power of 2.
	if ( _value && !(_value & (_value - 1)) ) return _value;
	--_value;
  _value |= _value >> 1;
  _value |= _value >> 2;
  _value |= _value >> 4;
  _value |= _value >> 8;
  _value |= _value >> 16;
  return _value+1;
}
// T is 64 bit.
template<typename T> inline
typename Tif<sizeof(T)==8, T>::type next_power_of_2(T _value){
	// Return the value itself, if it is already a power of 2.
	if ( _value && !(_value & (_value - 1)) ) return _value;
	--_value;
  _value |= _value >> 1;
  _value |= _value >> 2;
  _value |= _value >> 4;
  _value |= _value >> 8;
  _value |= _value >> 16;
  _value |= _value >> 32;
  return _value+1;
}

// A templated, recursive, and constexpr alignment size function.
// Let s be the size of the type, x be the alignment requirement. The alignment y must be 2^n, such that y>=x and y>=s.
// We need this function alongside with next_power_of_2 to evaluate the next power of 2 at compile time, i.e., next_power_of_2 is for runtime usage, while this is for compile time usage.
constexpr size_t reucursive_alignment_fit(size_t type_size, size_t minimum_alignment, size_t the_alignment)
{
	return
	( (type_size<=the_alignment) && (minimum_alignment<=the_alignment) ) ?
	the_alignment : reucursive_alignment_fit(type_size, minimum_alignment, the_alignment*2);

}
// A wrapper.
template<typename T> constexpr size_t alignment_fit(size_t alignment_requirement)
{
	return reucursive_alignment_fit(sizeof(T), alignment_requirement, 2);
}

template<typename T, size_t Align = JUWHAN_CACHELINE_SIZE> // Here, the default alignment is 64, instead of alignment_of<T>::value. The 64 is the cacheline size of intel and ARM15 CPUs, and generally the most conservative alignment value for integral types.
struct aligned_element
{
	union
	{
		char buffer[alignment_fit<T>(Align)];
		T value;
	} data;
	using value_type = T;
	aligned_element() {};
	aligned_element(T& _value) { data.value = _value; };
	aligned_element(T&& _value) { data.value = ::juwhan::move(_value); };
	aligned_element(aligned_element<T, Align>& _element) { data.value = _element.data.value; };
	aligned_element(aligned_element<T, Align>&& _element) { data.value = ::juwhan::move(_element.data.value); };
	aligned_element<T, Align>& operator=(aligned_element<T, Align>& _element) 
	{ 
		data.value = _element.data.value; 
		return *this; 
	};
	aligned_element<T, Align>& operator=(aligned_element<T, Align>&& _element) 
	{ 
		data.value = ::juwhan::move(_element.data.value); 
		return *this; 
	};
	T& value() { return data.value; }
};


// Instead of implementing a aligned array class and building a circular array class based upon it, implement directly a circular class based on aligned_element. Thus, reducing complexity. This section of code is supposed to be used inly in threadpood implementation anyway.
// One more implementation specific specialization: the array size must be a power of 2.
template<typename T, size_t Align = JUWHAN_CACHELINE_SIZE>
class aligned_circular_array
{
	char* raw_data_ptr;
	aligned_element<T, Align>* adjusted_data_ptr;
	// Initialize.
	void initialize() 
	{ 
		aca_info("Initializer of an aligned circular array has been entered. We will initialize all elements with the default constructor.");
		for(auto i=0 ; i<size ; ++i)
		{
			new (&adjusted_data_ptr[i].value()) std::atomic<T>{T()};
		}
	};
	// Destroy.
	template<typename V = T> typename void_if<is_integral<V>::value>::type destroy() 
	{
		aca_info("Destroyer for intrinsic types has entered. We'll do nothing.");
	}; 

	template<typename V = T> typename void_if<!is_integral<V>::value && !is_pointer<V>::value>::type destroy()
	{ 
		aca_info("Destroyer for derived types(class, struct, and etc) has entered. We'll explicitly call destructors for all elements.");
		for(auto i=0 ; i<size ; ++i)
		{
			auto element_value = adjusted_data_ptr[i].data.value;
			element_value.~T();
		}
	};
	template<typename V = T> typename void_if<!is_integral<V>::value && is_pointer<V>::value>::type destroy()
	{ 
		// Follow the usual semantics. Do nothing for pointers.
		aca_info("Destroyer for pointers has entered. We'll do nothing.");
		/*
		aca_info("Destroyer for pointers has been entered. We will delete all pointers leaving no dangling pointers.");
		for(auto i=0 ; i<size ; ++i)
		{
			auto element_value = adjusted_data_ptr[i].value().load(std::memory_order_relaxed);
			delete(element_value);
		}
		*/
	};
public:
	size_t utilized_bytes;
	size_t allocated_bytes;
	size_t size;
	using value_type = T;
	using element_type = aligned_element<T, Align>;
	// Accessors.
	void* raw_address() const { return reinterpret_cast<void*>(raw_data_ptr); };
	void* aligned_address() const { return reinterpret_cast<void*>(adjusted_data_ptr); };
	element_type* address() const { return adjusted_data_ptr; };
	// Delete the default constructor.
	aligned_circular_array() = delete;
	explicit aligned_circular_array(size_t _size) : size{next_power_of_2(_size)}
	{
		aca_info_if(size==0, "An attempt to initialize aligned_circular_array with size 0 has been detected."); 
		// Alignement requirement.
		constexpr auto alignment = sizeof(element_type);
		// Caclulate memory offset.
		// Procure at least the utilized byte size + alignement requirement - 1 so that we can dump the unwanted byes and start at the alignment boundary.
		/* Pictorially,
		|-------|-------|---
		^
		Alignment boundary
		  |------------------
		  ^
		  Start of the allocated memory
		  |-----|-------|---
		        ^
		        Dump the un-aligned section and start using here at the next boundary
		*/
		constexpr auto memory_offset = alignment - 1;
		// Calculate the apparent size that's visible to outside.
		utilized_bytes = alignment * size;
		// Caclulate the actual memory footage.
		allocated_bytes = utilized_bytes + memory_offset;
		// Allocate raw_data.
		try
		{
			raw_data_ptr = new char[allocated_bytes];
		}
		catch(...)
		{
			// Finish construction anyway.
			raw_data_ptr = nullptr;
			adjusted_data_ptr = nullptr;
			utilized_bytes = 0;
			allocated_bytes = 0;
			size = 0;
			// fatal("Memory acquisition in aligned_circular_array failed: while trying to grab " + to_string(allocated_bytes) + " bytes.");
			throw(std::runtime_error("Memory acquisition in aligned_circular_array failed"));
		}
		// Adjust pointer to an aligned boundary.
		adjusted_data_ptr = reinterpret_cast<element_type*>
		( 
			// Adjust to the alignment boundary by calculating mod remainder.
			// The following is a mod expression where memory_offset+1 is a power of 2.
			( reinterpret_cast<size_t>(raw_data_ptr) + memory_offset ) & ~memory_offset
		);
		// Initialize.
		initialize();
	};

	// Copy constructor.
	aligned_circular_array(const aligned_circular_array<T, Align>& other) : aligned_circular_array(other.size)
	{
		// Copy.
		memcpy(reinterpret_cast<void*>(adjusted_data_ptr), reinterpret_cast<void*>(other.adjusted_data_ptr), utilized_bytes);
	};


	// Move constructor.
	aligned_circular_array(aligned_circular_array<T, Align>&& other) :  size{other.size}, utilized_bytes{other.utilized_bytes}, allocated_bytes{other.allocated_bytes}, raw_data_ptr{other.raw_data_ptr}, adjusted_data_ptr{other.adjusted_data_ptr}
	{
		other.size = 0;
		other.utilized_bytes = 0;
		other.allocated_bytes = 0;
		other.raw_data_ptr = nullptr;
		other.adjusted_data_ptr = nullptr;
	};


	// Copy assignment.
	aligned_circular_array& operator=(const aligned_circular_array<T, Align>& other)
	{
		char* raw_data_ptr_new;
		// Allocate raw_data.
		try
		{
			raw_data_ptr_new = new char[other.allocated_bytes];
		}
		catch(...)
		{
			// fatal("Memory acquisition in aligned_circular_array failed: while trying to grab " + to_string(allocated_bytes)+" bytes in construction.");
			throw(std::runtime_error("Memory acquisition in aligned_circular_array failed"));
		}
		// Adjust pointer to an aligned boundary.
		// Alignement requirement.
		constexpr auto alignment = sizeof(element_type);
		// Caclulate memory offset.
		constexpr auto memory_offset = alignment - 1;
		// Calculate the apparent size that's visible to outside.
		auto adjusted_data_ptr_new = reinterpret_cast<element_type*>
		( 
			(
				reinterpret_cast<size_t>(raw_data_ptr_new) + memory_offset
			) & ~memory_offset
		);
		// Copy, destroy and delete.
		memcpy(reinterpret_cast<void*>(adjusted_data_ptr_new), reinterpret_cast<void*>(other.adjusted_data_ptr), other.utilized_bytes);
		destroy();
		delete[] raw_data_ptr;
		// Set to the new values.
		size = other.size;
		utilized_bytes = other.utilized_bytes;
		allocated_bytes = other.allocated_bytes;
		raw_data_ptr = raw_data_ptr_new;
		adjusted_data_ptr = adjusted_data_ptr_new;
		return *this;
	};

	// Move assignment.
	aligned_circular_array& operator=(aligned_circular_array<T, Align>&& other)
	{
		// Destroy, delete, and ::juwhan::move.
		destroy();
		delete[] raw_data_ptr;
		size = other.size;
		utilized_bytes = other.utilized_bytes;
		allocated_bytes = other.allocated_bytes;
		raw_data_ptr = other.raw_data_ptr;
		adjusted_data_ptr = other.adjusted_data_ptr;
		// Nullify the original.
		other.size = 0;
		other.utilized_bytes = 0;
		other.allocated_bytes = 0;
		other.raw_data_ptr = nullptr;
		other.adjusted_data_ptr = nullptr;
		return *this;
	};
	// Assigning nullptr deletes the data WITHOUT destroying elements.
	// Hence, if we want to preserve data that this array is pointing to, we can simply assign nullptr to the array and then delete it.
	aligned_circular_array& operator=(nullptr_t) 
	{
		aca_info("Nullifyer for an aligned circular array has been entered. The user perhaps does not want to destroy the contents pointed at by the array elements. We will simply erase the memory occupied by the array, not calling destructor for each element.");
		delete[] raw_data_ptr;
		size = 0;
		utilized_bytes = 0;
		allocated_bytes = 0;
		raw_data_ptr = nullptr;
		adjusted_data_ptr = nullptr;
		return *this;
	};
	// Get and set.
	value_type const get(const size_t idx) const
	{
		return adjusted_data_ptr[idx & (size - 1)].data.value; 
	}; 
	void put(size_t idx, const value_type& item)
	{
		adjusted_data_ptr[idx & (size - 1)].data.value = item;
	};
	// Operators.
	value_type& operator[](const size_t idx) 
	{
	 	return adjusted_data_ptr[idx & (size - 1)].data.value; 
	};
	value_type const& operator[](const size_t idx) const 
	{
		return adjusted_data_ptr[idx & (size - 1)].data.value; 
	}; 
	// bool type conversion.
	operator bool() const { return raw_data_ptr != nullptr; };
	// Destructor.
	virtual ~aligned_circular_array() 
	{ 
		aca_info("Destructor of an aligned circular array has been entered. We will clean up the array buffer if it has one.")
		if (raw_data_ptr)
		{
			aca_info("We detected that this aligned circular array indeed has data allocated. We'll destroy elements first.");
			destroy();
			aca_info("Now, we're ready and will erase the memory occupied by this aligned circular array.");
			delete[] raw_data_ptr;
		}
 
	};
};

}  // End of namespace threadpool.

#endif











