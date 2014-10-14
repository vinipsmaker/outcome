/* expected_future.hpp
Provides a non-allocating expected<T, E> based future-promise
(C) 2014 Niall Douglas http://www.nedprod.com/
File Created: Oct 2014


Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef BOOST_EXPECTED_FUTURE_HPP
#define BOOST_EXPECTED_FUTURE_HPP

#include "spinlock.hpp"
#include "expected/include/boost/expected/expected.hpp"

#include "local-bind-cpp-library/include/import.hpp"
#ifndef BOOST_EXPECTED_FUTURE_V1_STL11_IMPL
#define BOOST_EXPECTED_FUTURE_V1_STL11_IMPL std
#endif
#define BOOST_EXPECTED_FUTURE_V1_STL11_IMPL_VER v1_##BOOST_EXPECTED_FUTURE_V1_STL11_IMPL
#define BOOST_EXPECTED_FUTURE_V1 (boost), (expected_future), (BOOST_EXPECTED_FUTURE_V1_STL11_IMPL_VER, inline)
#define BOOST_EXPECTED_FUTURE_V1_NAMESPACE       BOOST_LOCAL_BIND_NAMESPACE      (BOOST_EXPECTED_FUTURE_V1)
#define BOOST_EXPECTED_FUTURE_V1_NAMESPACE_BEGIN BOOST_LOCAL_BIND_NAMESPACE_BEGIN(BOOST_EXPECTED_FUTURE_V1)
#define BOOST_EXPECTED_FUTURE_V1_NAMESPACE_END   BOOST_LOCAL_BIND_NAMESPACE_END  (BOOST_EXPECTED_FUTURE_V1)

#define BOOST_STL11_ATOMIC_MAP_NAMESPACE_BEGIN        BOOST_LOCAL_BIND_NAMESPACE_BEGIN(BOOST_EXPECTED_FUTURE_V1, (stl11, inline))
#define BOOST_STL11_ATOMIC_MAP_NAMESPACE_END          BOOST_LOCAL_BIND_NAMESPACE_END  (BOOST_EXPECTED_FUTURE_V1, (stl11, inline))
#define BOOST_STL11_FUTURE_MAP_NAMESPACE_BEGIN        BOOST_LOCAL_BIND_NAMESPACE_BEGIN(BOOST_EXPECTED_FUTURE_V1, (stl11, inline))
#define BOOST_STL11_FUTURE_MAP_NAMESPACE_END          BOOST_LOCAL_BIND_NAMESPACE_END  (BOOST_EXPECTED_FUTURE_V1, (stl11, inline))
#define BOOST_STL11_THREAD_MAP_NAMESPACE_BEGIN        BOOST_LOCAL_BIND_NAMESPACE_BEGIN(BOOST_EXPECTED_FUTURE_V1, (stl11, inline))
#define BOOST_STL11_THREAD_MAP_NAMESPACE_END          BOOST_LOCAL_BIND_NAMESPACE_END  (BOOST_EXPECTED_FUTURE_V1, (stl11, inline))
#define BOOST_STL11_CHRONO_MAP_NAMESPACE_BEGIN        BOOST_LOCAL_BIND_NAMESPACE_BEGIN(BOOST_EXPECTED_FUTURE_V1, (stl11, inline), (chrono))
#define BOOST_STL11_CHRONO_MAP_NAMESPACE_END          BOOST_LOCAL_BIND_NAMESPACE_END  (BOOST_EXPECTED_FUTURE_V1, (stl11, inline), (chrono))
#include BOOST_LOCAL_BIND_INCLUDE_STL11(BOOST_EXPECTED_FUTURE_V1_STL11_IMPL, atomic)
#include BOOST_LOCAL_BIND_INCLUDE_STL11(BOOST_EXPECTED_FUTURE_V1_STL11_IMPL, future)
#include BOOST_LOCAL_BIND_INCLUDE_STL11(BOOST_EXPECTED_FUTURE_V1_STL11_IMPL, thread)
#include BOOST_LOCAL_BIND_INCLUDE_STL11(BOOST_EXPECTED_FUTURE_V1_STL11_IMPL, chrono)

#define BOOST_SPINLOCK_MAP_NAMESPACE_BEGIN BOOST_LOCAL_BIND_NAMESPACE_BEGIN(BOOST_EXPECTED_FUTURE_V1, (spinlock, inline))
#define BOOST_SPINLOCK_MAP_NAMESPACE_END   BOOST_LOCAL_BIND_NAMESPACE_END(BOOST_EXPECTED_FUTURE_V1, (spinlock, inline))
#include "spinlock.bind.hpp"

BOOST_EXPECTED_FUTURE_V1_NAMESPACE_BEGIN

  BOOST_LOCAL_BIND_DECLARE(BOOST_EXPECTED_FUTURE_V1)
  template<bool consuming, class ContainerType, class... Continuations> class basic_future;
  template<class FutureType> class basic_promise;
  namespace detail
  {
    template<class me_type, class other_type> struct future_promise_unlocker
    {
      me_type *me;
      other_type *other;
      decltype(me_type::_lock) *me_lock;
      decltype(other_type::_lock) *other_lock;
      future_promise_unlocker(me_type *f, other_type *p) : me(f), other(p), me_lock(f ? &f->_lock : nullptr), other_lock(p ? &p->lock : nullptr) { }
      future_promise_unlocker(future_promise_unlocker &&o) : me(o.me), other(o.other), me_lock(o.me_lock), other_lock(o.other_lock) { me_lock=nullptr; other_lock=nullptr; }
      future_promise_unlocker(const future_promise_unlocker &) = delete;
      future_promise_unlocker &operator=(const future_promise_unlocker &) = delete;
      future_promise_unlocker &operator=(future_promise_unlocker &&) = delete;
      ~future_promise_unlocker()
      {
        if(me_lock)
          me_lock->unlock();
        if(other_lock)
          other_lock->unlock();
      }
    };
    template<class S, class D=typename S::other_type> future_promise_unlocker<S, D> lock_future_promise(S *me)
    {
      for(size_t count=0;;count++)
      {
        // Lock myself first
        if(me->_lock.try_lock())
        {
          if(!me->_other || me->_other->_lock.try_lock())
            return future_promise_unlocker<S, D>(me, me->_other);
          me->_lock.unlock();
        }
        if(count>10)
          this_thread::sleep_for(chrono::milliseconds(1));
        else
          this_thread::yield();        
      }      
    }
  }
  template<bool consuming, class T, class E> class basic_future<consuming, expected<T, E>> : protected expected<T, E>
  {
    friend class basic_promise<basic_future>;
  public:
    typedef basic_future<consuming, expected<T, E>> future_type;
    typedef basic_promise<future_type> promise_type;
    typedef promise_type other_type;
  private:
    static BOOST_CONSTEXPR_OR_CONST bool consumes=consuming;
    spinlock<bool> _lock, _ready; // _ready is INVERTED
    atomic<bool> _abandoned;
    promise_type *_other;
    BOOST_CONSTEXPR void _detach() BOOST_NOEXCEPT
    {
      if(_other)
      {
        _other->_other=nullptr;
        _other=nullptr;
      }
    }
    BOOST_CONSTEXPR inline void _abandon() BOOST_NOEXCEPT
    {
      _abandoned=true;
      _detach();
    }
    template<class U> BOOST_CONSTEXPR void _set(U &&v)
    {
      expected<T, E>::operator=(std::forward<U>(v));
      _ready.unlock();
    }
    BOOST_CONSTEXPR basic_future(promise_type *p, bool ready, expected<T, E> &&o) BOOST_NOEXCEPT_IF((std::is_nothrow_default_constructible<expected<T, E>>::value)) : _abandoned(false), _other(p), expected<T, E>(std::move(o)) { if(!ready) _ready.lock(); }
  public:
    BOOST_CONSTEXPR basic_future() BOOST_NOEXCEPT_IF((std::is_nothrow_default_constructible<expected<T, E>>::value)) : _abandoned(false), _other(nullptr) { _ready.lock(); }
    BOOST_CONSTEXPR basic_future(basic_future &&o) BOOST_NOEXCEPT_IF((std::is_nothrow_move_assignable<expected<T, E>>::value && std::is_nothrow_default_constructible<expected<T, E>>::value)) : basic_future() { this->operator=(std::move(o)); }
    basic_future(const basic_future &o) = delete;
    ~basic_future() BOOST_NOEXCEPT_IF((std::is_nothrow_destructible<expected<T, E>>::value))
    {
      auto lock=lock_future_promise(this);
      _detach();
    }
    BOOST_CONSTEXPR basic_future &operator=(basic_future &&o) BOOST_NOEXCEPT_IF((std::is_nothrow_move_assignable<expected<T, E>>::value))
    {
      // First lock myself and detach myself from any promise of mine
      auto lock1=lock_future_promise(this);
      _detach();
      // Lock source and his promise
      auto lock2=lock_future_promise(&o);
      _ready=std::move(o._ready);
      _abandoned=std::move(o._abandoned);
      _other=std::move(o._other);
      expected<T, E>::operator=(std::move(o));
      _other->_other=this;
      o._abandoned=false;
      o._other=nullptr;
      return *this;
    }
    basic_future &operator=(const basic_future &o) = delete;
    BOOST_CONSTEXPR basic_shared_future<T, E> share();
    BOOST_CONSTEXPR T get()
    {
      wait();
      return value();
    }
    BOOST_CONSTEXPR bool valid() const BOOST_NOEXCEPT { return _promise; }
    BOOST_CONSTEXPR bool is_consuming() const BOOST_NOEXCEPT { return consuming; }
    BOOST_CONSTEXPR bool is_ready() const BOOST_NOEXCEPT { return !_ready.get(); }
    BOOST_CONSTEXPR bool is_abandoned() const BOOST_NOEXCEPT { return _abandoned.get(); }
    BOOST_CONSTEXPR bool has_exception() const BOOST_NOEXCEPT { return ready() && !expected<T, E>::valid(); }
    BOOST_CONSTEXPR bool has_value() const BOOST_NOEXCEPT { return ready() && expected<T, E>::valid(); }
    BOOST_CONSTEXPR void wait() const
    {
      if(!valid()) throw future_error(future_errc::no_state);
      if(is_abandoned()) throw future_error(future_errc::broken_promise);
      // TODO: Use a permit object here instead of a spinlock
      if(!ready())
      {
        _ready.lock();
        _ready.unlock();
      }
    }
    template<class Rep, class Period>
    future_status wait_for(const chrono::duration<Rep,Period> &timeout_duration) const;
    template<class Clock, class Duration>
    future_status wait_until(const chrono::time_point<Clock,Duration> &timeout_time) const;
  };

  template<class T, class E> class basic_promise<basic_future<expected<T, E>>> : protected expected<T, E>
  {
    friend class basic_future<expected<T, E>>;
  public:
    typedef basic_future<expected<T, E>> future_type;
    typedef basic_promise<future_type> promise_type;
    typedef future_type other_type;
  private:
    static BOOST_CONSTEXPR_OR_CONST bool consumes=future_type::consumes;
    spinlock<bool> _lock;
    atomic<bool> _ready, _retrieved;
    future_type *_other;
    BOOST_CONSTEXPR void _abandon() BOOST_NOEXCEPT
    {
      if(_other)
      {
         if(!_other->ready())
           _other->_abandon();
         else
           _other->_detach();
      }
    }
    template<class U> BOOST_CONSTEXPR void _set(U &&v)
    {
      if(_ready)
        throw future_error(future_errc::promise_already_satisfied);
      auto lock=lock_future_promise(this);
      // If no future exists yet, set myself to the value and we'll set the future on creation
      if(!_other)
        expected<T, E>::operator=(std::forward<U>(v));
      else
        _other->_set(std::forward<U>(v));
      _ready=true;
    }
  public:
    BOOST_CONSTEXPR basic_promise() BOOST_NOEXCEPT_IF((std::is_nothrow_default_constructible<expected<T, E>>::value)) : _ready(false), _retrieved(false), _other(nullptr)
    {
      static_assert(!std::uses_allocator<basic_promise, std::allocator<T>>::value, "Non-allocating future-promise cannot make use of allocators");
      static_assert(!std::uses_allocator<basic_promise, std::allocator<E>>::value, "Non-allocating future-promise cannot make use of allocators");
    }
    BOOST_CONSTEXPR basic_promise(basic_promise &&o) BOOST_NOEXCEPT_IF((std::is_nothrow_move_assignable<expected<T, E>>::value)) : basic_promise() { this->operator=(std::move(o)); }
    basic_promise(const basic_promise &) = delete;
    ~basic_promise()
    {
      auto lock=lock_future_promise(this);
      _abandon();
    }
    BOOST_CONSTEXPR basic_promise &operator=(basic_promise &&o) BOOST_NOEXCEPT_IF((std::is_nothrow_move_assignable<expected<T, E>>::value))
    {
      // First lock myself and detach/abandon any future of mine
      auto lock1=lock_future_promise(this);
      _abandon();
      // Lock source and his future
      auto lock2=lock_future_promise(&o);
      _ready=std::move(o._ready);
      _retrieved=std::move(o._retrieved);
      _other=std::move(o._other);
      expected<T, E>::operator=(std::move(o));
      _other->_other=this;
      o._ready=false;
      o._retrieved=false;
      o._other=nullptr;
      return *this;
    }
    basic_promise &operator=(const basic_promise &) = delete;
    BOOST_CONSTEXPR void swap(basic_promise &o) BOOST_NOEXCEPT;
    BOOST_CONSTEXPR future_type get_future() BOOST_NOEXCEPT_IF((std::is_nothrow_default_constructible<future_type>::value && std::is_nothrow_move_constructible<future_type>::value))
    {
      if(consumes && _retrieved)
        throw future_error(future_errc::future_already_retrieved);
      auto lock1=lock_future_promise(this);
      if(consumes && _retrieved)
        throw future_error(future_errc::future_already_retrieved);
      future_type ret(this, _ready, _ready ? std::move(*this) : expected<T, E>());
      _other=&ret;
      return ret;
    }
    BOOST_CONSTEXPR void set_value(const T &v)
    {
      _set(v);
    }
    BOOST_CONSTEXPR void set_value(T &&v)
    {
      _set(std::move(v));
    }
    // set_value_at_thread_exit() not possible with this design
    BOOST_CONSTEXPR void set_exception(const E &v)
    {
      _set(make_unexpected(v));
    }
    BOOST_CONSTEXPR void set_exception(E &&v)
    {
      _set(make_unexpected(std::move(v)));
    }
    // set_exception_at_thread_exit() not possible with this design
  };
#if 0
  template<class T, class E> class basic_shared_future
  {
  };
  // non-constexpr promise, must vector through a virtual function to set value
  template<class T, class E> class basic_promise<T, E>
  {
  };
#endif
  template<class T, class E=exception_ptr> using future = basic_future<expected<T, E>>;
  template<class T, class E=exception_ptr> using promise = basic_promise<future<T, E>>;
  //template<class T, class E=exception_ptr> using shared_future = basic_shared_future<T, E>;

BOOST_EXPECTED_FUTURE_V1_NAMESPACE_END

namespace std
{
  template<class T, class E> void swap(BOOST_EXPECTED_FUTURE_V1_NAMESPACE::basic_promise<T, E> &a, BOOST_EXPECTED_FUTURE_V1_NAMESPACE::basic_promise<T, E> &b) BOOST_NOEXCEPT
  {
    a.swap(b);
  }
}

#endif

// Do local namespace binding
#ifdef BOOST_EXPECTED_FUTURE_V1_MAP_NAMESPACE_BEGIN

BOOST_EXPECTED_FUTURE_V1_MAP_NAMESPACE_BEGIN
template<class T, class... Args> using basic_promise = BOOST_EXPECTED_FUTURE_V1_NAMESPACE::basic_promise<T, Args...>;
template<class T, class E> class basic_shared_future = BOOST_EXPECTED_FUTURE_V1_NAMESPACE::basic_shared_future<T, E>;
template<class T, class E> class basic_future = BOOST_EXPECTED_FUTURE_V1_NAMESPACE::basic_future<T, E>;
BOOST_EXPECTED_FUTURE_V1_MAP_NAMESPACE_END

#endif