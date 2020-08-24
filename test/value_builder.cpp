//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/BeastLounge
//

// Test that header file is self-contained.
#include <boost/json/value_builder.hpp>

#include <boost/json/serializer.hpp>

#include <boost/json/detail/unchecked_builder.hpp>

#include <iostream>

#include "test_suite.hpp"

namespace boost {
namespace json {

class value_builder_test
{
public:
    void
    testBuilder()
    {
    // This is from the javadoc

    value_builder vb;
    vb.reset();
    vb.begin_object();
    vb.insert_key("a");
    vb.insert_int64(1);
    vb.insert_key("b");
    vb.insert_null();
    vb.insert_key("c");
    vb.insert_string("hello");
    vb.end_object();
    assert( to_string(vb.release()) == "{\"a\":1,\"b\":null,\"c\":\"hello\"}" );


    detail::unchecked_builder ub;
    ub.begin_object();
    ub.insert_key("1", 1);
    ub.insert_int64(1);
    ub.end_object(1);
    //std::cout << to_string(ub.release()) << '\n';
    //auto v = ub.release();

    //auto a = v.at("a");
    //auto b = v.at("b");
    //auto c = v.at("c");

    //for(auto i : a.as_object())
        //std::cout << to_string(i.value());
    //assert( to_string(ub.release()) == "{\"a\":1,\"b\":null,\"c\":\"hello\"}" );
    }

    void
    run()
    {
        testBuilder();
    }
};

TEST_SUITEX(value_builder_test, "boost.json.value_builder");

} // json
} // boost
