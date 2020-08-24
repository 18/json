//
// Copyright (c) 2020 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/json
//

#ifndef BOOST_JSON_IMPL_UNCHECKED_BUILDER_IPP
#define BOOST_JSON_IMPL_UNCHECKED_BUILDER_IPP

#include <boost/json/detail/unchecked_builder.hpp>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace boost {
namespace json {


//----------------------------------------------------------

/*
... denotes 0 or more
<> denotes empty storage

Value Stack Layout:

    (value)...

State Stack Layout:
        
    key:
        (chars)...
        size_t

    string:
        (chars)...

    state:
        state
*/




} // json
} // boost

#endif
