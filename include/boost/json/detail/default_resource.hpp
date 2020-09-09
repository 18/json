//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/json
//

#ifndef BOOST_JSON_DEFAULT_RESOURCE_HPP
#define BOOST_JSON_DEFAULT_RESOURCE_HPP

#include <boost/json/detail/config.hpp>
#include <new>

BOOST_JSON_NS_BEGIN
namespace detail {

// Prevents t from being destroyed
template<typename T>
union no_destroy
{
    constexpr no_destroy() : t()
    {
    }

    ~no_destroy()
    {
    }

    T t;
};

/** A simple memory resource that uses operator new and delete.
*/
class
    BOOST_SYMBOL_VISIBLE
    BOOST_JSON_CLASS_DECL
    default_resource final
    : public memory_resource
{
#ifndef BOOST_JSON_NO_DESTROY
    BOOST_JSON_DECL
    static no_destroy<default_resource> instance_;

public:
    static
    std::uintptr_t
    singleton() noexcept
    {
        return reinterpret_cast<
            std::uintptr_t>(&instance_.t);
    }
#else
public:
    static
    std::uintptr_t
    singleton() noexcept
    {
        BOOST_JSON_NO_DESTROY
        static default_resource resource_;
        return reinterpret_cast<
            std::uintptr_t>(&resource_);
    }
#endif
    
    ~default_resource();

    void*
    do_allocate(
        std::size_t n,
        std::size_t) override;

    void
    do_deallocate(
        void* p,
        std::size_t,
        std::size_t) override;

    bool
    do_is_equal(
        memory_resource const& mr) const noexcept override;
};

} // detail
BOOST_JSON_NS_END

#endif
