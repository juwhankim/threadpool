#ifndef juwhan_threadlocal_h
#define juwhan_threadlocal_h

#include <cstdlib>
#include <system_error>
#include <iostream>
#include "juwhan_std.h"

namespace juwhan
{


template<typename T>
class threadlocal
{
  pthread_key_t key;
public:
  threadlocal() : key{}
  {
    if (pthread_key_create(&key, NULL))
    {
      throw ::std::system_error{::std::error_code{}, "Key generation for thread specific storage failed."};
    }
  };
  threadlocal(const threadlocal& other) : key{other.key} {};
  threadlocal& operator=(const threadlocal& other)
  {
  	key = other.key;
    return *this;
  }
  threadlocal& operator=(T& value_)
  {
    T* value = reinterpret_cast<T*>(pthread_getspecific(key));
    if (value)
    {
      *value = value_;
    }
    else
    {
      value = reinterpret_cast<T*>(malloc(sizeof(T)));
      if (!value) throw ::std::system_error{::std::error_code{}, "Memory acquisition for thread specific storage failed."};
      *value = value_; 
      pthread_setspecific(key, value);
    }
    return *this;
  };
  threadlocal& operator=(T&& value_)
  {
    T* value = reinterpret_cast<T*>(pthread_getspecific(key));
    if (value)
    {
      *value = move(value_);
    }
    else
    {
      value = reinterpret_cast<T*>(malloc(sizeof(T)));
      if (!value) throw ::std::system_error{::std::error_code{}, "Memory acquisition for thread specific storage failed."};
      *value = move(value_); 
      pthread_setspecific(key, value);
    }
    return *this;
  };
  operator T&() const
  {
    T* value = reinterpret_cast<T*>(pthread_getspecific(key));
    return *value;
  };
  T& get() const
  {
  	T* value = reinterpret_cast<T*>(pthread_getspecific(key));
    return *value;
  };
  void set(T& value_)
  {
    T* value = reinterpret_cast<T*>(pthread_getspecific(key));
    if (value)
    {
      *value = value_;
    }
    else
    {
      value = reinterpret_cast<T*>(malloc(sizeof(T)));
      if (!value) throw ::std::system_error{::std::error_code{}, "Memory acquisition for thread specific storage failed."};
      *value = value_; 
      pthread_setspecific(key, value);
    }
  };
  void set(T&& value_)
  {
    T* value = reinterpret_cast<T*>(pthread_getspecific(key));
    if (value)
    {
      *value = move(value_);
    }
    else
    {
      value = reinterpret_cast<T*>(malloc(sizeof(T)));
      if (!value) throw ::std::system_error{::std::error_code{}, "Memory acquisition for thread specific storage failed."};
      *value = move(value_); 
      pthread_setspecific(key, value);
    }
  };
  // Each thread must take care of the storage it allocated through destroying.
  void destroy()
  {
    T* value = reinterpret_cast<T*>(pthread_getspecific(key));
    if (value)
    {
      // Call destructor first.
      value->T::~T();
      // Free memory.
      free(value);
    }
  };
  // If calling a destructor is not desirable, just release the memory.
  void release()
  {
    T* value = reinterpret_cast<T*>(pthread_getspecific(key));
    if (value)
    {
      // Free memory.
      free(value);
    }
  };
  ~threadlocal()
  {
    if (pthread_key_delete(key)) throw ::std::system_error{::std::error_code{}, "Deletion of a key associated with a threadlocal class has failed."};
  }
};

// Specialization where T is a reference type.
template<typename T>
class threadlocal<T&>
{
  using type = T;
  pthread_key_t key;
public:
  threadlocal() : key{}
  {
    if (pthread_key_create(&key, NULL))
    {
      throw ::std::system_error{::std::error_code{}, "Key generation for thread specific storage failed."};
    }
  };
  threadlocal(const threadlocal& other) : key{other.key} {};
  threadlocal& operator=(const threadlocal& other)
  {
    key = other.key;
  }
  threadlocal& operator=(T& value_)
  {
    pthread_setspecific(key,&value_);
    return *this;
  };
  threadlocal& operator=(T&& value_) = delete; // rvalues cannot be assigned to references.
  operator T&() const
  {
    T* value = reinterpret_cast<T*>(pthread_getspecific(key));
    return *value;
  };
  T& get() const noexcept
  {
    T* value = reinterpret_cast<T*>(pthread_getspecific(key));
    return *value;
  };
  void set(T& value_) noexcept
  {
    pthread_setspecific(key,&value_);
  };
  // Each thread must take care of the storage it allocated through destroying.
  void destroy()
  {
    T* value = reinterpret_cast<T*>(pthread_getspecific(key));
    if (value)
    {
      // Call destructor first.
      value->T::~T();
    }
  };
  // If calling a destructor is not desirable, just release the memory.
  void release()
  {
    // Do nothing. This is a reference and no memory is allocated.
  };
  ~threadlocal()
  {
    if (pthread_key_delete(key)) throw ::std::system_error{::std::error_code{}, "Deletion of a key associated with a threadlocal class has failed."};
  }
};

// Specialization where T is a pointer type.
template<typename T>
class threadlocal<T*>
{
  using type = T;
  pthread_key_t key;
public:
  threadlocal() : key{}
  {
    if (pthread_key_create(&key, NULL))
    {
      throw ::std::system_error{::std::error_code{}, "Key generation for thread specific storage failed."};
    }
  };
  threadlocal(const threadlocal& other) : key{other.key} {};
  threadlocal& operator=(const threadlocal& other)
  {
    key = other.key;
  }
  threadlocal& operator=(const T* value_)
  {
    pthread_setspecific(key,value_);
    return *this;
  };
  operator T*() const
  {
    T* value = reinterpret_cast<T*>(pthread_getspecific(key));
    return value;
  };
  T& operator *() { T* value = reinterpret_cast<T*>(pthread_getspecific(key)); return *value; };
  T* operator ->() { T* value = reinterpret_cast<T*>(pthread_getspecific(key)); return value; };
  T* get() const noexcept
  {
    T* value = reinterpret_cast<T*>(pthread_getspecific(key));
    return value;
  };
  void set(T* value_) noexcept
  {
    pthread_setspecific(key,value_);
  };
  // Each thread must take care of the storage it allocated through destroying.
  void destroy()
  {
    T* value = reinterpret_cast<T*>(pthread_getspecific(key));
    if (value)
    {
      // Call destructor first.
      value->T::~T();
    }
  };
  // If calling a destructor is not desirable, just release the memory.
  void release()
  {
    // Do nothing. This is a reference and no memory is allocated.
  };
  ~threadlocal()
  {
    if (pthread_key_delete(key)) throw ::std::system_error{::std::error_code{}, "Deletion of a key associated with a threadlocal class has failed."};
  }
};






}  // End of namespace juwhan.



#endif