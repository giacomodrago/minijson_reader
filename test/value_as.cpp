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

enum class OrderType
{
    UNKNOWN,
    BUY,
    SELL,
};

namespace minijson
{

template<>
struct value_as<OrderType>
{
    OrderType operator()(const value v) const
    {
        // In production code, throw if v.type() is not String

        // In production code, this probably needs to be case insensitive
        if (v.raw() == "BUY")
        {
            return OrderType::BUY;
        }
        if (v.raw() == "SELL")
        {
            return OrderType::SELL;
        }

        // In production code, throw here
        return OrderType::UNKNOWN;
    }
};

template<>
struct value_as<std::optional<OrderType>>
{
    std::optional<OrderType> operator()(const value v) const
    {
        if (v.type() == Null)
        {
            return std::nullopt;
        }

        // In production code, throw if v.type() is not String

        // In production code, this probably needs to be case insensitive
        if (v.raw() == "UNKNOWN")
        {
            return std::nullopt;
        }
        if (v.raw() == "BUY")
        {
            return OrderType::BUY;
        }
        if (v.raw() == "SELL")
        {
            return OrderType::SELL;
        }

        // In production code, throw here
        return std::nullopt;
    }
};

template<typename T>
struct value_as<T, std::enable_if_t<std::is_floating_point_v<T>>>
{
    T operator()(const value v) const
    {
        // In production code, throw if v.type() is not Number

        // Of course this is just for test...
        if (v.raw() == "1")
        {
            return 42;
        }
        return value_as_default<T>(v);
    }
};

} // namespace minijson

TEST(minijson_reader_as_specializations, string_to_enum)
{
    {
        minijson::value v(minijson::String, "BUY");
        ASSERT_EQ(OrderType::BUY, v.as<OrderType>());
        ASSERT_EQ(OrderType::BUY, v.as<std::optional<OrderType>>());
    }
    {
        minijson::value v(minijson::Null, "null");
        ASSERT_EQ(std::nullopt, v.as<std::optional<OrderType>>());
    }
    {
        minijson::value v(minijson::String, "UNKNOWN");
        ASSERT_EQ(std::nullopt, v.as<std::optional<OrderType>>());
    }
}

TEST(minijson_reader_as_specializations, floating_point)
{
    minijson::value v(minijson::Number, "1");
    ASSERT_EQ(1, v.as<int>());

    // Yup, it does not make sense, but makes sure the specialization is
    // picked up
    ASSERT_EQ(42, v.as<double>());
    ASSERT_EQ(42, v.as<float>());

    // Make sure that optional<T> uses our specialization
    ASSERT_EQ(42, v.as<std::optional<double>>());
    ASSERT_EQ(42, v.as<std::optional<float>>());
}

TEST(minijson_reader_as_specializations, floating_point_fallback)
{
    minijson::value v(minijson::Number, "12");
    ASSERT_EQ(12, v.as<double>());
    ASSERT_EQ(12, v.as<float>());
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
