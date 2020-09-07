//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/json
//

#ifndef BOOST_JSON_DETAIL_IMPL_DEFAULT_RESOURCE_IPP
#define BOOST_JSON_DETAIL_IMPL_DEFAULT_RESOURCE_IPP

#include <boost/json/detail/default_resource.hpp>

BOOST_JSON_NS_BEGIN
namespace detail {

// this is here so that ~memory_resource
// is emitted in the library instead of
// the user's TU.
default_resource::
~default_resource() = default;


#ifdef BOOST_JSON_NO_DESTROY
std::uintptr_t
default_resource::
singleton() noexcept
{
    BOOST_JSON_NO_DESTROY
    static detail::default_resource resource_;
    return reinterpret_cast<
        std::uintptr_t>(&resource_);
}
#else
union no_destroy
{
    constexpr
    no_destroy() noexcept
        : resource_() 
    {
    }

    ~no_destroy() noexcept
    {
    }

    detail::default_resource resource_;
};

template<class T>
struct instance
{
    static no_destroy resource_;
};

template<class T>
no_destroy instance<T>::resource_;

std::uintptr_t
default_resource::
singleton() noexcept
{
    return reinterpret_cast<std::uintptr_t>(
        &instance<void>::resource_.resource_);
}
#endif

void*
default_resource::
do_allocate(
    std::size_t n,
    std::size_t)
{
    return ::operator new(n);
}

void
default_resource::
do_deallocate(
    void* p,
    std::size_t,
    std::size_t)
{
    ::operator delete(p);
}

bool
default_resource::
do_is_equal(
    memory_resource const& mr) const noexcept
{
    return this == &mr;
}

} // detail
BOOST_JSON_NS_END

#endif
