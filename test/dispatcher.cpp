// Copyright (c) Giacomo Drago <giacomo@giacomodrago.com>
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of Giacomo Drago nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY GIACOMO DRAGO "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL GIACOMO DRAGO BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "minijson_reader.hpp"

#include <gtest/gtest.h>

namespace
{

struct Order
{
    std::string_view ticker;
    unsigned int price = 0;
    unsigned int size = 0;
    bool has_nyse = false;
    bool urgent = false;
};

using namespace minijson::handlers;
using minijson::value;

std::size_t debug_field_count = 0;
const minijson::dispatcher order_dispatcher
{
    handler("ticker", [](Order& o, value v) {v.to(o.ticker);}),
    handler("price", [](Order& o, value v) {v.to(o.price);}),
    handler("size", [](Order& o, value v) {v.to(o.size);}),
    optional_handler("urgent", [](Order& o, value v) {v.to(o.urgent);}),
    ignore_handler("sender"),
    handler(
        "exchanges",
        [](Order& o, value, auto& context)
        {
            parse_array(
                context,
                [&](value v)
                {
                    if (v.as<std::string_view>() == "NYSE")
                    {
                        o.has_nyse = true;
                    }
                });
        }),
    any_handler(
        [](Order&, std::string_view name, value)
        {
            if (name.find("debug-") == 0)
            {
                ++debug_field_count;
                return true;
            }
            return false;
        }),
    ignore_any_handler {},
};

} // namespace {anonymous}

TEST(minijson_dispatcher, typical_usage)
{
    // "urgent" not present, "NYSE" among exchanges
    {
        debug_field_count = 0;
        char buffer[] = R"json(
        {
            "sender": {"source": "trader", "department": 1},
            "ticker": "ABCD",
            "price": 12,
            "size": 47,
            "exchanges": ["IEX", "NYSE"],
            "extended-debug-1": {"latency": 22},
            "debug-1": 42,
            "debug-2": -7
        })json";

        minijson::buffer_context context(buffer, sizeof(buffer));
        Order order;
        order_dispatcher.run(context, order);

        ASSERT_EQ("ABCD", order.ticker);
        ASSERT_EQ(12u, order.price);
        ASSERT_EQ(47u, order.size);
        ASSERT_TRUE(order.has_nyse);
        ASSERT_FALSE(order.urgent);
        ASSERT_EQ(2u, debug_field_count);
    }

    // "urgent" present, "NYSE" not among exchanges
    {
        debug_field_count = 0;
        char buffer[] = R"json(
        {
            "sender": {"source": "trader", "department": 1},
            "ticker": "ABCD",
            "price": 12,
            "size": 47,
            "exchanges": ["IEX"],
            "extended-debug-1": {"latency": 22},
            "debug-1": 42,
            "debug-2": -7,
            "urgent": true
        })json";

        minijson::buffer_context context(buffer, sizeof(buffer));
        Order order;
        order_dispatcher.run(context, order);

        ASSERT_EQ("ABCD", order.ticker);
        ASSERT_EQ(12u, order.price);
        ASSERT_EQ(47u, order.size);
        ASSERT_FALSE(order.has_nyse);
        ASSERT_TRUE(order.urgent);
        ASSERT_EQ(2u, debug_field_count);
    }
}

TEST(minijson_dispatcher, duplicate_fields)
{
    char buffer[] = R"json(
    {
        "sender": {"source": "trader", "department": 1},
        "ticker": "ABCD",
        "price": 12,
        "size": 47,
        "exchanges": ["IEX"],
        "extended-debug-1": {"latency": 22},
        "ticker": "EFGH",
        "sender": {"source": "trader", "department": 1},
        "debug-1": 42,
        "urgent": false,
        "debug-2": -7,
        "urgent": true
    })json";

    minijson::buffer_context context(buffer, sizeof(buffer));
    Order order;
    order_dispatcher.run(context, order);

    ASSERT_EQ("EFGH", order.ticker);
    ASSERT_EQ(12u, order.price);
    ASSERT_EQ(47u, order.size);
    ASSERT_FALSE(order.has_nyse);
    ASSERT_TRUE(order.urgent);
}

TEST(minijson_dispatcher, missing_field_exception)
{
    char buffer[] = R"json(
    {
        "sender": {"source": "trader", "department": 1},
        "ticker": "ABCD",
        "price": 12,
        "exchanges": ["IEX"],
        "extended-debug-1": {"latency": 22},
        "debug-1": 42,
        "debug-2": -7,
        "urgent": true
    })json";

    minijson::buffer_context context(buffer, sizeof(buffer));
    Order order;
    try
    {
        order_dispatcher.run(context, order);
        FAIL();
    }
    catch (const minijson::missing_field_error& e)
    {
        ASSERT_EQ("size", e.field_name_truncated());
        ASSERT_STREQ(
            "at least one required JSON field is missing",
            e.what());
    }
}

TEST(minijson_dispatcher, unhandled_field_exception)
{
    using namespace minijson::handlers;
    using minijson::value;

    static const minijson::dispatcher dispatcher
    {
        ignore_handler("foo"),
        any_handler(
            [](std::string_view n, value, auto& ctx)
            {
                if (n == "bar")
                {
                    minijson::ignore(ctx);
                    return true;
                }
                return false;
            }),
    };

    char buffer[] = R"json(
    {
        "foo": {},
        "bar": {},
        "i_have_to_get_to_56_characters_in_length_which_is_lots!X": [],
    })json";
    minijson::buffer_context context(buffer, sizeof(buffer));

    try
    {
        dispatcher.run(context);
        FAIL();
    }
    catch (const minijson::unhandled_field_error& e)
    {
        ASSERT_EQ(
            "i_have_to_get_to_56_characters_in_length_which_is_lots!",
            e.field_name_truncated());
        ASSERT_STREQ(
            "a JSON field was not handled",
            e.what());
    }
}

TEST(minijson_dispatcher, no_targets)
{
    using namespace minijson::handlers;
    using minijson::value;

    bool called = false;
    static const minijson::dispatcher dispatcher
    {
        handler("foo",
            [&](value v)
            {
                ASSERT_EQ("bar", v.as<std::string_view>());
                called = true;
            }),
    };

    char buffer[] = R"json(
    {
        "foo": "bar"
    })json";

    minijson::buffer_context context(buffer, sizeof(buffer));
    dispatcher.run(context);
    ASSERT_TRUE(called);
}

TEST(minijson_dispatcher, multiple_targets)
{
    using namespace minijson::handlers;
    using minijson::value;

    static const minijson::dispatcher dispatcher
    {
        handler("a", [&](int& a, int&, value v) {v.to(a);}),
        handler("b", [&](int&, int& b, value v) {v.to(b);}),
    };

    char buffer[] = R"json(
    {
        "a": 1,
        "b": 2
    })json";

    minijson::buffer_context context(buffer, sizeof(buffer));
    int a = 0;
    int b = 0;
    dispatcher.run(context, a, b);
    ASSERT_EQ(1, a);
    ASSERT_EQ(2, b);
}

TEST(minijson_dispatcher, dispatcher_run_inspect)
{
    char buffer[] = R"json(
    {
        "sender": {"source": "trader", "department": 1},
        "ticker": "ABCD",
        "price": 12,
        "extended-debug-2": {"delay": 1},
        "exchanges": ["IEX"],
        "extended-debug-1": {"latency": 22},
        "debug-1": 42,
        "debug-2": -7,
        "urgent": true,
        "extended-debug-3": {"usage": "open"}
    })json";
    minijson::buffer_context context(buffer, sizeof(buffer));

    Order order;
    minijson::dispatcher_run run(order_dispatcher, order);
    minijson::parse_object(context, run);

    std::size_t inspector_called_count = 0;

    // Check move construction and inspect both work
    minijson::dispatcher_run other_run(std::move(run));
    other_run.inspect(
        [&](const auto& handler, const std::size_t handle_count)
        {
            ++inspector_called_count;
            if constexpr (traits<decltype(handler)>::is_field_specific)
            {
                const bool required =
                    handler.field_name() != "sender" &&
                    handler.field_name() != "urgent";
                const bool ignored = handler.field_name() == "sender";

                ASSERT_EQ(
                    required,
                    traits<decltype(handler)>::is_required_field);
                ASSERT_EQ(
                    ignored,
                    traits<decltype(handler)>::is_ignore);

                ASSERT_EQ(
                    handler.field_name() == "size" ? 0 : 1,
                    handle_count);
            }
            else if constexpr (traits<decltype(handler)>::is_ignore)
            {
                // 3 "debug-" fields
                ASSERT_EQ(3, handle_count);
            }
            else
            {
                // 2 "extended-debug-" fields
                ASSERT_EQ(2, handle_count);
            }
        });
    static_assert(order_dispatcher.n_handlers == 8);
    ASSERT_EQ(order_dispatcher.n_handlers, inspector_called_count);
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
