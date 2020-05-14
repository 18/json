//
// Copyright (c) 2020 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/json
//

#ifndef BOOST_JSON_DETAIL_MONOTONIC_RESOURCE_HPP
#define BOOST_JSON_DETAIL_MONOTONIC_RESOURCE_HPP

#include <boost/json/detail/config.hpp>

#include <cstddef>

namespace boost {
namespace json {
namespace detail {

inline
unsigned char*
align_up(
    unsigned char* ptr,
    std::size_t align)
{
    // alignment shall be a power of two
    BOOST_ASSERT(!(align & (align - 1)));
    return reinterpret_cast<unsigned char*>(
        (reinterpret_cast<std::uintptr_t>(ptr) + align - 1) & ~(align - 1));
}

} // detail
} // json
} // boost

#endif
