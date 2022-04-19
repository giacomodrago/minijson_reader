#include "minijson_reader.hpp"

#include <gtest/gtest.h>

#include <bitset>
#include <climits>

template<typename Context>
void test_context_helper(Context& context)
{
    context.begin_literal();
    bool loop = true;
    while (loop)
    {
        switch (context.read_offset())
        {
        case 0:
            ASSERT_EQ('h', context.read());
            context.write('H');
            break;
        case 1:
            ASSERT_EQ('e', context.read());
            context.write('e');
            break;
        case 2:
            ASSERT_EQ('l', context.read());
            context.write('l');
            break;
        case 3:
            ASSERT_EQ('l', context.read());
            context.write('l');
            break;
        case 4:
            ASSERT_EQ('o', context.read());
            context.write('o');
            break;
        case 5:
            ASSERT_EQ(' ', context.read());
            context.write(0);
            ASSERT_STREQ("Hello", context.current_literal());
            context.begin_literal();
            break;
        case 6:
            ASSERT_EQ('w', context.read());
            context.write('W');
            break;
        case 7:
            ASSERT_EQ('o', context.read());
            context.write('o');
            break;
        case 8:
            ASSERT_EQ('r', context.read());
            context.write('r');
            break;
        case 9:
            ASSERT_EQ('l', context.read());
            context.write('l');
            break;
        case 10:
            ASSERT_EQ('d', context.read());
            context.write('d');
            break;
        case 11:
            ASSERT_EQ('.', context.read());
            context.write(0);
            break;
        case 12:
            ASSERT_EQ(0, context.read());
            loop = false;
            break;
        }
    }

    ASSERT_EQ(0, context.read());
    ASSERT_EQ(12U, context.read_offset());
    ASSERT_STREQ("World", context.current_literal());

    ASSERT_EQ(
        minijson::detail::context_base::NESTED_STATUS_NONE,
        context.nested_status());

    context.begin_nested(minijson::detail::context_base::NESTED_STATUS_OBJECT);
    ASSERT_EQ(
        minijson::detail::context_base::NESTED_STATUS_OBJECT,
        context.nested_status());
    ASSERT_EQ(1U, context.nesting_level());
    context.begin_nested(minijson::detail::context_base::NESTED_STATUS_ARRAY);
    ASSERT_EQ(
        minijson::detail::context_base::NESTED_STATUS_ARRAY,
        context.nested_status());
    ASSERT_EQ(2U, context.nesting_level());
    context.end_nested();
    ASSERT_EQ(1U, context.nesting_level());
    context.end_nested();
    ASSERT_EQ(0U, context.nesting_level());

    context.reset_nested_status();
    ASSERT_EQ(
        minijson::detail::context_base::NESTED_STATUS_NONE,
        context.nested_status());
}

TEST(minijson_reader, buffer_context)
{
    char buffer[] =
        {'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '.'};
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    test_context_helper(buffer_context);

    ASSERT_STREQ("Hello", buffer);
    ASSERT_STREQ("World", buffer + 6);
    ASSERT_DEATH({buffer_context.write('x');}, "");
    buffer_context.begin_literal();
    ASSERT_EQ(buffer + sizeof(buffer), buffer_context.current_literal());
    ASSERT_DEATH({buffer_context.write('x');}, "");
}

TEST(minijson_reader, const_buffer_context)
{
    const char buffer[] = "hello world.";
    minijson::const_buffer_context const_buffer_context(
        buffer,
        sizeof(buffer) - 1);
    const char* const original_write_buffer =
        const_buffer_context.current_literal();
    test_context_helper(const_buffer_context);

    ASSERT_STREQ("hello world.", buffer); // no side effects
    ASSERT_DEATH({const_buffer_context.write('x');}, "");
    const_buffer_context.begin_literal();
    ASSERT_EQ(
        original_write_buffer + strlen(buffer),
        const_buffer_context.current_literal());
    ASSERT_DEATH({const_buffer_context.write('x');}, "");
}

TEST(minijson_reader, istream_context)
{
    std::istringstream buffer("hello world.");
    minijson::istream_context istream_context(buffer);
    test_context_helper(istream_context);
}

TEST(minijson_reader, parse_error)
{
    {
        minijson::buffer_context buffer_context(nullptr, 0);
        minijson::parse_error parse_error(
            buffer_context,
            minijson::parse_error::UNKNOWN);

        ASSERT_EQ(0U, parse_error.offset());
        ASSERT_EQ(minijson::parse_error::UNKNOWN, parse_error.reason());
        ASSERT_STREQ("Unknown parse error", parse_error.what());
    }
    {
        const char buffer[] = "hello world.";
        minijson::const_buffer_context const_buffer_context(
            buffer,
            sizeof(buffer) - 1);
        const_buffer_context.read();
        const_buffer_context.read();
        ASSERT_EQ(2U, const_buffer_context.read_offset());

        minijson::parse_error parse_error(
            const_buffer_context,
            minijson::parse_error::UNKNOWN);
        ASSERT_EQ(1U, parse_error.offset());
        ASSERT_EQ(minijson::parse_error::UNKNOWN, parse_error.reason());
        ASSERT_STREQ("Unknown parse error", parse_error.what());
    }
}

TEST(minijson_reader_detail, utf16_to_utf32)
{
    // code points 0000 to D7FF and E000 to FFFF
    ASSERT_EQ(0x000000u, minijson::detail::utf16_to_utf32(0x0000, 0x0000));
    ASSERT_EQ(0x000001u, minijson::detail::utf16_to_utf32(0x0001, 0x0000));
    ASSERT_EQ(0x00D7FEu, minijson::detail::utf16_to_utf32(0xD7FE, 0x0000));
    ASSERT_EQ(0x00D7FFu, minijson::detail::utf16_to_utf32(0xD7FF, 0x0000));
    ASSERT_EQ(0x00E000u, minijson::detail::utf16_to_utf32(0xE000, 0x0000));
    ASSERT_EQ(0x00FFFFu, minijson::detail::utf16_to_utf32(0xFFFF, 0x0000));

    // code points 010000 to 10FFFF
    ASSERT_EQ(0x010000u, minijson::detail::utf16_to_utf32(0xD800, 0xDC00));
    ASSERT_EQ(0x010001u, minijson::detail::utf16_to_utf32(0xD800, 0xDC01));
    ASSERT_EQ(0x10FFFEu, minijson::detail::utf16_to_utf32(0xDBFF, 0xDFFE));
    ASSERT_EQ(0x10FFFFu, minijson::detail::utf16_to_utf32(0xDBFF, 0xDFFF));
}

TEST(minijson_reader_detail, utf16_to_utf32_invalid)
{
    ASSERT_THROW(
        minijson::detail::utf16_to_utf32(0x0000, 0x0001),
        minijson::detail::encoding_error);
    ASSERT_THROW(
        minijson::detail::utf16_to_utf32(0xD800, 0xDBFF),
        minijson::detail::encoding_error);
    ASSERT_THROW(
        minijson::detail::utf16_to_utf32(0xD800, 0xE000),
        minijson::detail::encoding_error);
    ASSERT_THROW(
        minijson::detail::utf16_to_utf32(0xDC00, 0xDC00),
        minijson::detail::encoding_error);
}

TEST(minijson_reader_detail, utf32_to_utf8)
{
    // 1 byte
    {
        const std::array<std::uint8_t, 4> expected {0x00, 0x00, 0x00, 0x00};
        ASSERT_EQ(expected, minijson::detail::utf32_to_utf8(0x000000));
    }
    {
        const std::array<std::uint8_t, 4> expected {0x01, 0x00, 0x00, 0x00};
        ASSERT_EQ(expected, minijson::detail::utf32_to_utf8(0x000001));
    }
    {
        const std::array<std::uint8_t, 4> expected {0x7E, 0x00, 0x00, 0x00};
        ASSERT_EQ(expected, minijson::detail::utf32_to_utf8(0x00007E));
    }
    {
        const std::array<std::uint8_t, 4> expected {0x7F, 0x00, 0x00, 0x00};
        ASSERT_EQ(expected, minijson::detail::utf32_to_utf8(0x00007F));
    }

    // 2 bytes
    {
        const std::array<std::uint8_t, 4> expected {0xC2, 0x80, 0x00, 0x00};
        ASSERT_EQ(expected, minijson::detail::utf32_to_utf8(0x000080));
    }
    {
        const std::array<std::uint8_t, 4> expected {0xC2, 0x81, 0x00, 0x00};
        ASSERT_EQ(expected, minijson::detail::utf32_to_utf8(0x000081));
    }
    {
        const std::array<std::uint8_t, 4> expected {0xDF, 0xBE, 0x00, 0x00};
        ASSERT_EQ(expected, minijson::detail::utf32_to_utf8(0x0007FE));
    }
    {
        const std::array<std::uint8_t, 4> expected {0xDF, 0xBF, 0x00, 0x00};
        ASSERT_EQ(expected, minijson::detail::utf32_to_utf8(0x0007FF));
    }

    // 3 bytes
    {
        const std::array<std::uint8_t, 4> expected {0xE0, 0xA0, 0x80, 0x00};
        ASSERT_EQ(expected, minijson::detail::utf32_to_utf8(0x000800));
    }
    {
        const std::array<std::uint8_t, 4> expected {0xE0, 0xA0, 0x81, 0x00};
        ASSERT_EQ(expected, minijson::detail::utf32_to_utf8(0x000801));
    }
    {
        const std::array<std::uint8_t, 4> expected {0xEF, 0xBF, 0xBE, 0x00};
        ASSERT_EQ(expected, minijson::detail::utf32_to_utf8(0x00FFFE));
    }
    {
        const std::array<std::uint8_t, 4> expected {0xEF, 0xBF, 0xBF, 0x00};
        ASSERT_EQ(expected, minijson::detail::utf32_to_utf8(0x00FFFF));
    }

    // 4 bytes
    {
        const std::array<std::uint8_t, 4> expected {0xF0, 0x90, 0x80, 0x80};
        ASSERT_EQ(expected, minijson::detail::utf32_to_utf8(0x010000));
    }
    {
        const std::array<std::uint8_t, 4> expected {0xF0, 0x90, 0x80, 0x81};
        ASSERT_EQ(expected, minijson::detail::utf32_to_utf8(0x010001));
    }
    {
        const std::array<std::uint8_t, 4> expected {0xF7, 0xBF, 0xBF, 0xBE};
        ASSERT_EQ(expected, minijson::detail::utf32_to_utf8(0x1FFFFE));
    }
    {
        const std::array<std::uint8_t, 4> expected {0xF7, 0xBF, 0xBF, 0xBF};
        ASSERT_EQ(expected, minijson::detail::utf32_to_utf8(0x1FFFFF));
    }
}

TEST(minijson_reader_detail, utf32_to_utf8_invalid)
{
    // invalid code unit
    ASSERT_THROW(
        minijson::detail::utf32_to_utf8(0x200000),
        minijson::detail::encoding_error);
}

TEST(minijson_reader_detail, utf16_to_utf8)
{
    // Just one test case, since utf16_to_utf8 calls utf16_to_utf32
    // and utf32_to_utf8, and other cases have been covered by previous tests

    const std::array<std::uint8_t, 4> expected {0xF4, 0x8F, 0xBF, 0xBF};
    ASSERT_EQ(expected, minijson::detail::utf16_to_utf8(0xDBFF, 0xDFFF));
}

TEST(minijson_reader_detail, parse_utf16_escape_sequence)
{
    ASSERT_EQ(
        0x0000u,
        minijson::detail::parse_utf16_escape_sequence({'0', '0', '0', '0'}));
    ASSERT_EQ(
        0x1000u,
        minijson::detail::parse_utf16_escape_sequence({'1', '0', '0', '0'}));
    ASSERT_EQ(
        0x2345u,
        minijson::detail::parse_utf16_escape_sequence({'2', '3', '4', '5'}));
    ASSERT_EQ(
        0x6789u,
        minijson::detail::parse_utf16_escape_sequence({'6', '7', '8', '9'}));
    ASSERT_EQ(
        0xA6BCu,
        minijson::detail::parse_utf16_escape_sequence({'A', '6', 'B', 'C'}));
    ASSERT_EQ(
        0xabcdu,
        minijson::detail::parse_utf16_escape_sequence({'a', 'b', 'c', 'd'}));
    ASSERT_EQ(
        0xabcdu,
        minijson::detail::parse_utf16_escape_sequence({'A', 'B', 'C', 'D'}));
    ASSERT_EQ(
        0xEFefu,
        minijson::detail::parse_utf16_escape_sequence({'E', 'F', 'e', 'f'}));
    ASSERT_EQ(
        0xFFFFu,
        minijson::detail::parse_utf16_escape_sequence({'F', 'F', 'F', 'F'}));
}

TEST(minijson_reader_detail, parse_utf16_escape_sequence_invalid)
{
    ASSERT_THROW(
        minijson::detail::parse_utf16_escape_sequence({'f', 'f', 'F', 'p'}),
        minijson::detail::encoding_error);
    ASSERT_THROW(
        minijson::detail::parse_utf16_escape_sequence({'-', 'b', 'c', 'd'}),
        minijson::detail::encoding_error);
    ASSERT_THROW(
        minijson::detail::parse_utf16_escape_sequence({' ', 'a', 'b', 'c'}),
        minijson::detail::encoding_error);
    ASSERT_THROW(
        minijson::detail::parse_utf16_escape_sequence({'a', 'b', 'c', 0}),
        minijson::detail::encoding_error);
}

static void test_write_utf8_char(
    std::array<std::uint8_t, 4> c,
    const char* expected_str)
{
    char buf[] = "____";

    minijson::buffer_context buffer_context(buf, sizeof(buf));
    buffer_context.read();
    buffer_context.read();
    buffer_context.read();
    buffer_context.read();

    minijson::detail::literal_io literal_io(buffer_context);
    minijson::detail::write_utf8_char(literal_io, c);
    ASSERT_STREQ(expected_str, buf);
}

TEST(minijson_reader_detail, write_utf8_char)
{
    test_write_utf8_char(
        std::array<std::uint8_t, 4> {0x00, 0x00, 0x00, 0x00},
        "");
    test_write_utf8_char(
        std::array<std::uint8_t, 4> {0xFF, 0x00, 0x00, 0x00},
        "\xFF___");
    test_write_utf8_char(
        std::array<std::uint8_t, 4> {0xFF, 0xFE, 0x00, 0x00},
        "\xFF\xFE__");
    test_write_utf8_char(
        std::array<std::uint8_t, 4> {0xFF, 0xFE, 0xFD, 0x00},
        "\xFF\xFE\xFD_");
    test_write_utf8_char(
        std::array<std::uint8_t, 4> {0xFF, 0xFE, 0xFD, 0xFC},
        "\xFF\xFE\xFD\xFC");
}

TEST(minijson_reader_detail, parse_string_empty)
{
    char buffer[] = "\"";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    minijson::detail::parse_string(buffer_context);
    ASSERT_STREQ("", buffer_context.current_literal());
}

TEST(minijson_reader_detail, parse_string_one_char)
{
    char buffer[] = "a\"";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    minijson::detail::parse_string(buffer_context);
    ASSERT_STREQ("a", buffer_context.current_literal());
}

TEST(minijson_reader_detail, parse_string_ascii)
{
    char buffer[] = "foo\"";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    minijson::detail::parse_string(buffer_context);
    ASSERT_STREQ("foo", buffer_context.current_literal());
}

TEST(minijson_reader_detail, parse_string_utf8)
{
    char buffer[] = "你好\"";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    minijson::detail::parse_string(buffer_context);
    ASSERT_STREQ("你好", buffer_context.current_literal());
}

TEST(minijson_reader_detail, parse_string_escape_sequences)
{
    char buffer[] = "\\\"\\\\\\/\\b\\f\\n\\r\\t\"";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    minijson::detail::parse_string(buffer_context);
    ASSERT_STREQ("\"\\/\b\f\n\r\t", buffer_context.current_literal());
}

TEST(minijson_reader_detail, parse_string_escape_sequences_utf16)
{
    char buffer[] =
        "\\u0001\\u0002a\\ud7ff\\uE000\\uFffFb\\u4F60\\uD800"
        "\\uDC00\\uDBFF\\uDFFFà\"";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    minijson::detail::parse_string(buffer_context);
    ASSERT_STREQ(
        "\x01\x02" "a" "\xED\x9F\xBF\xEE\x80\x80\xEF\xBF\xBF" "b" "你"
        "\xF0\x90\x80\x80" "\xF4\x8F\xBF\xBF" "à",
        buffer_context.current_literal());
}

template<std::size_t Length>
void parse_string_invalid_helper(
    const char (&buffer)[Length],
    const minijson::parse_error::error_reason expected_reason,
    const std::size_t expected_offset,
    const char* const expected_what)
{
    SCOPED_TRACE(buffer);
    bool exception_thrown = false;

    try
    {
        minijson::const_buffer_context context(buffer, Length - 1);
        minijson::detail::parse_string(context);
    }
    catch (const minijson::parse_error& parse_error)
    {
        exception_thrown = true;
        ASSERT_EQ(expected_reason, parse_error.reason());
        ASSERT_EQ(expected_offset, parse_error.offset());
        ASSERT_STREQ(expected_what, parse_error.what());
    }

    ASSERT_TRUE(exception_thrown);
}

TEST(minijson_reader_detail, parse_string_invalid)
{
    parse_string_invalid_helper(
        "",
        minijson::parse_error::UNTERMINATED_VALUE,
        0,
        "Unterminated value");

    parse_string_invalid_helper(
        "asd",
        minijson::parse_error::UNTERMINATED_VALUE,
        2,
        "Unterminated value");

    parse_string_invalid_helper(
        "\\h\"",
        minijson::parse_error::INVALID_ESCAPE_SEQUENCE,
        1,
        "Invalid escape sequence");

    parse_string_invalid_helper(
        "\\u0rff\"",
        minijson::parse_error::INVALID_ESCAPE_SEQUENCE,
        3,
        "Invalid escape sequence");

    parse_string_invalid_helper(
        "\\uD800\\uD7FF\"",
        minijson::parse_error::INVALID_UTF16_CHARACTER,
        11,
        "Invalid UTF-16 character");

    parse_string_invalid_helper(
        "\\uD800\\u0000\"",
        minijson::parse_error::INVALID_UTF16_CHARACTER,
        11,
        "Invalid UTF-16 character");

    parse_string_invalid_helper(
        "\\uDC00\"",
        minijson::parse_error::INVALID_UTF16_CHARACTER,
        5,
        "Invalid UTF-16 character");

    parse_string_invalid_helper(
        "\\u0000\"",
        minijson::parse_error::NULL_UTF16_CHARACTER,
        5,
        "Null UTF-16 character");

    parse_string_invalid_helper(
        "\\uD800\"",
        minijson::parse_error::EXPECTED_UTF16_LOW_SURROGATE,
        6,
        "Expected UTF-16 low surrogate");

    parse_string_invalid_helper(
        "\\uD800a\"",
        minijson::parse_error::EXPECTED_UTF16_LOW_SURROGATE,
        6,
        "Expected UTF-16 low surrogate");
}

TEST(minijson_reader, value_default_constructed)
{
    const minijson::value value;
    ASSERT_EQ(minijson::Null, value.type());
    ASSERT_EQ("null", value.raw());

    ASSERT_THROW(value.as<std::string_view>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<long>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<bool>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<double>(), minijson::bad_value_cast);

    ASSERT_EQ(std::nullopt, value.as<std::optional<std::string_view>>());
    ASSERT_EQ(std::nullopt, value.as<std::optional<long>>());
    ASSERT_EQ(std::nullopt, value.as<std::optional<bool>>());
    ASSERT_EQ(std::nullopt, value.as<std::optional<double>>());
}

TEST(minijson_reader, value_example)
{
    const minijson::value value(minijson::Number, "-0.42e-42");

    ASSERT_EQ(minijson::Number, value.type());
    ASSERT_EQ("-0.42e-42", value.raw());

    ASSERT_THROW(value.as<std::string_view>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<long>(), std::range_error);
    ASSERT_THROW(value.as<bool>(), minijson::bad_value_cast);
    ASSERT_DOUBLE_EQ(-0.42e-42, value.as<double>());

    ASSERT_THROW(
        value.as<std::optional<std::string_view>>(),
        minijson::bad_value_cast);
    ASSERT_THROW(value.as<std::optional<long>>(), std::range_error);
    ASSERT_THROW(value.as<std::optional<bool>>(), minijson::bad_value_cast);
    ASSERT_DOUBLE_EQ(-0.42e-42, value.as<std::optional<double>>().value_or(-1));
}

template<typename Context>
void parse_unquoted_value_bad_helper(
    Context& context,
    const std::size_t expected_offset,
    const minijson::parse_error::error_reason reason)
{
    bool exception_thrown = false;

    try
    {
        minijson::detail::parse_unquoted_value(context, context.read());
    }
    catch (const minijson::parse_error& parse_error)
    {
        exception_thrown = true;

        ASSERT_EQ(reason, parse_error.reason());
        ASSERT_EQ(expected_offset, parse_error.offset());
    }

    ASSERT_TRUE(exception_thrown);
}

TEST(minijson_reader_detail, parse_unquoted_value_whitespace)
{
    char buffer[] = "  42";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    parse_unquoted_value_bad_helper(
        buffer_context,
        0,
        minijson::parse_error::EXPECTED_VALUE);
}

TEST(minijson_reader_detail, parse_unquoted_value_true)
{
    char buffer[] = "true  ";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

    const auto [value, termination_char] =
        minijson::detail::parse_unquoted_value(
            buffer_context,
            buffer_context.read());
    ASSERT_EQ(termination_char, ' ');

    ASSERT_EQ(minijson::Boolean, value.type());
    ASSERT_EQ("true", value.raw());

    ASSERT_THROW(value.as<std::string_view>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<long>(), minijson::bad_value_cast);
    ASSERT_TRUE(value.as<bool>());
    ASSERT_THROW(value.as<double>(), minijson::bad_value_cast);

    ASSERT_THROW(
        value.as<std::optional<std::string_view>>(),
        minijson::bad_value_cast);
    ASSERT_THROW(value.as<std::optional<long>>(), minijson::bad_value_cast);
    ASSERT_EQ(true, value.as<std::optional<bool>>());
    ASSERT_THROW(value.as<std::optional<double>>(), minijson::bad_value_cast);
}

TEST(minijson_reader_detail, parse_unquoted_value_false)
{
    char buffer[] = "false}";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

    const auto [value, termination_char] =
        minijson::detail::parse_unquoted_value(
            buffer_context,
            buffer_context.read());
    ASSERT_EQ(termination_char, '}');

    ASSERT_EQ(minijson::Boolean, value.type());
    ASSERT_EQ("false", value.raw());

    ASSERT_THROW(value.as<std::string_view>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<long>(), minijson::bad_value_cast);
    ASSERT_FALSE(value.as<bool>());
    ASSERT_THROW(value.as<double>(), minijson::bad_value_cast);

    ASSERT_THROW(
        value.as<std::optional<std::string_view>>(),
        minijson::bad_value_cast);
    ASSERT_THROW(value.as<std::optional<long>>(), minijson::bad_value_cast);
    ASSERT_EQ(false, value.as<std::optional<bool>>());
    ASSERT_THROW(value.as<std::optional<double>>(), minijson::bad_value_cast);
}

TEST(minijson_reader_detail, parse_unquoted_value_null)
{
    char buffer[] = "null}";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

    const auto [value, termination_char] =
        minijson::detail::parse_unquoted_value(
            buffer_context,
            buffer_context.read());
    ASSERT_EQ(termination_char, '}');

    ASSERT_EQ(minijson::Null, value.type());
    ASSERT_EQ("null", value.raw());

    ASSERT_THROW(value.as<std::string_view>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<long>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<bool>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<double>(), minijson::bad_value_cast);

    ASSERT_EQ(value.as<std::optional<std::string_view>>(), std::nullopt);
    ASSERT_EQ(value.as<std::optional<long>>(), std::nullopt);
    ASSERT_EQ(value.as<std::optional<bool>>(), std::nullopt);
    ASSERT_EQ(value.as<std::optional<double>>(), std::nullopt);
}

TEST(minijson_reader_detail, parse_unquoted_value_integer)
{
    // Biggest positive value that fits in an int64_t. Of course it "fits" in
    // a double but cannot be represented accurately.
    {
        char buffer[] = "9223372036854775807]";
        minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

        const auto [value, termination_char] =
            minijson::detail::parse_unquoted_value(
                buffer_context,
                buffer_context.read());
        ASSERT_EQ(termination_char, ']');

        ASSERT_EQ(minijson::Number, value.type());
        ASSERT_EQ("9223372036854775807", value.raw());

        ASSERT_THROW(value.as<std::string_view>(), minijson::bad_value_cast);
        ASSERT_EQ(9223372036854775807, value.as<int64_t>());
        ASSERT_THROW(value.as<bool>(), minijson::bad_value_cast);
        ASSERT_DOUBLE_EQ(9223372036854775807.0, value.as<double>());

        ASSERT_THROW(
            value.as<std::optional<std::string_view>>(),
            minijson::bad_value_cast);
        ASSERT_EQ(9223372036854775807, value.as<std::optional<int64_t>>());
        ASSERT_THROW(value.as<std::optional<bool>>(), minijson::bad_value_cast);
        ASSERT_DOUBLE_EQ(
            9223372036854775807.0,
            value.as<std::optional<double>>().value_or(-1));
    }

    // Biggest negative value that fits in an int64_t. Of course it "fits" in
    // a double but cannot be represented accurately.
    {
        char buffer[] = "-9223372036854775808\t";
        minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

        const auto [value, termination_char] =
            minijson::detail::parse_unquoted_value(
                buffer_context,
                buffer_context.read());
        ASSERT_EQ(termination_char, '\t');

        ASSERT_EQ(minijson::Number, value.type());
        ASSERT_EQ("-9223372036854775808", value.raw());

        // Have to use that -1 trick due to some sad gotcha of how integer
        // literals are parsed
        const int64_t val = -9223372036854775807 - 1;

        ASSERT_THROW(value.as<std::string_view>(), minijson::bad_value_cast);
        ASSERT_EQ(val, value.as<int64_t>());
        ASSERT_THROW(value.as<uint64_t>(), std::range_error);
        ASSERT_THROW(value.as<int32_t>(), std::range_error);
        ASSERT_THROW(value.as<bool>(), minijson::bad_value_cast);
        ASSERT_DOUBLE_EQ(-9223372036854775808.0, value.as<double>());

        ASSERT_THROW(
            value.as<std::optional<std::string_view>>(),
            minijson::bad_value_cast);
        ASSERT_EQ(val, value.as<std::optional<int64_t>>());
        ASSERT_THROW(value.as<std::optional<bool>>(), minijson::bad_value_cast);
        ASSERT_DOUBLE_EQ(
            -9223372036854775808.0,
            value.as<std::optional<double>>().value_or(-1));
    }
}

TEST(minijson_reader_detail, parse_unquoted_value_integer_too_large)
{
    // Smallest positive value that does not fit in an int64_t.
    // Of course it "fits" in a double but cannot be represented accurately.

    char buffer[] = "9223372036854775808,";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

    const auto [value, termination_char] =
        minijson::detail::parse_unquoted_value(
            buffer_context,
            buffer_context.read());
    ASSERT_EQ(termination_char, ',');

    ASSERT_EQ(minijson::Number, value.type());
    ASSERT_EQ("9223372036854775808", value.raw());

    ASSERT_THROW(value.as<std::string_view>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<int64_t>(), std::range_error);
    ASSERT_THROW(value.as<bool>(), minijson::bad_value_cast);
    ASSERT_DOUBLE_EQ(9223372036854775808.0, value.as<double>());

    ASSERT_THROW(
        value.as<std::optional<std::string_view>>(),
        minijson::bad_value_cast);
    ASSERT_THROW(value.as<std::optional<int64_t>>(), std::range_error);
    ASSERT_THROW(value.as<std::optional<bool>>(), minijson::bad_value_cast);
    ASSERT_DOUBLE_EQ(
        9223372036854775808.0,
        value.as<std::optional<double>>().value_or(-1));
}

TEST(minijson_reader_detail, parse_unquoted_value_double)
{
    char buffer[] = "42e+76,";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

    const auto [value, termination_char] =
        minijson::detail::parse_unquoted_value(
            buffer_context,
            buffer_context.read());
    ASSERT_EQ(termination_char, ',');

    ASSERT_EQ(minijson::Number, value.type());
    ASSERT_EQ("42e+76", value.raw());

    ASSERT_THROW(value.as<std::string_view>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<long>(), std::range_error);
    ASSERT_THROW(value.as<bool>(), minijson::bad_value_cast);
    ASSERT_DOUBLE_EQ(42e+76, value.as<double>());

    ASSERT_THROW(
        value.as<std::optional<std::string_view>>(),
        minijson::bad_value_cast);
    ASSERT_THROW(value.as<std::optional<long>>(), std::range_error);
    ASSERT_THROW(value.as<std::optional<bool>>(), minijson::bad_value_cast);
    ASSERT_DOUBLE_EQ(42e+76, value.as<std::optional<double>>().value_or(-1));
}

TEST(minijson_reader_detail, parse_unquoted_value_invalid)
{
    char buffer[] = "  asd,";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    buffer_context.read();
    buffer_context.read();

    parse_unquoted_value_bad_helper(
        buffer_context,
        2,
        minijson::parse_error::INVALID_VALUE);
}

TEST(minijson_reader_detail, parse_value_object)
{
    char buffer[] = "{...";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));

    const char first_char = buffer_context.read();

    char c = first_char;
    bool must_read = true;
    const auto value =
        minijson::detail::parse_value(buffer_context, c, must_read);
    ASSERT_EQ(c, first_char);
    ASSERT_TRUE(must_read);

    ASSERT_EQ(minijson::Object, value.type());
    ASSERT_EQ("", value.raw());

    ASSERT_THROW(value.as<std::string_view>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<long>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<bool>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<double>(), minijson::bad_value_cast);

    ASSERT_THROW(
        value.as<std::optional<std::string_view>>(),
        minijson::bad_value_cast);
    ASSERT_THROW(
        value.as<std::optional<long>>(),
        minijson::bad_value_cast);
    ASSERT_THROW(
        value.as<std::optional<bool>>(),
        minijson::bad_value_cast);
    ASSERT_THROW(
        value.as<std::optional<double>>(),
        minijson::bad_value_cast);
}

TEST(minijson_reader_detail, parse_value_array)
{
    char buffer[] = "[...";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));

    const char first_char = buffer_context.read();

    char c = first_char;
    bool must_read = true;
    const auto value =
        minijson::detail::parse_value(buffer_context, c, must_read);
    ASSERT_EQ(c, first_char);
    ASSERT_TRUE(must_read);

    ASSERT_EQ(minijson::Array, value.type());
    ASSERT_EQ("", value.raw());

    ASSERT_THROW(value.as<std::string_view>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<long>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<bool>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<double>(), minijson::bad_value_cast);

    ASSERT_THROW(
        value.as<std::optional<std::string_view>>(),
        minijson::bad_value_cast);
    ASSERT_THROW(
        value.as<std::optional<long>>(),
        minijson::bad_value_cast);
    ASSERT_THROW(
        value.as<std::optional<bool>>(),
        minijson::bad_value_cast);
    ASSERT_THROW(
        value.as<std::optional<double>>(),
        minijson::bad_value_cast);
}

TEST(minijson_reader_detail, parse_value_quoted_string)
{
    char buffer[] = "\"Hello world\"";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

    const char first_char = buffer_context.read();

    char c = first_char;
    bool must_read = true;
    const auto value =
        minijson::detail::parse_value(buffer_context, c, must_read);
    ASSERT_EQ(c, first_char);
    ASSERT_TRUE(must_read);

    ASSERT_EQ(minijson::String, value.type());
    ASSERT_EQ("Hello world", value.raw());

    ASSERT_EQ("Hello world", value.as<std::string_view>());
    ASSERT_THROW(value.as<long>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<bool>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<double>(), minijson::bad_value_cast);

    ASSERT_EQ("Hello world", value.as<std::optional<std::string_view>>());
    ASSERT_THROW(value.as<std::optional<long>>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<std::optional<bool>>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<std::optional<double>>(), minijson::bad_value_cast);
}

TEST(minijson_reader_detail, parse_value_quoted_string_empty)
{
    char buffer[] = "\"\"";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

    const char first_char = buffer_context.read();

    char c = first_char;
    bool must_read = true;
    const auto value =
        minijson::detail::parse_value(buffer_context, c, must_read);
    ASSERT_EQ(c, first_char);
    ASSERT_TRUE(must_read);

    ASSERT_EQ(minijson::String, value.type());
    ASSERT_EQ("", value.raw());

    ASSERT_EQ("", value.as<std::string_view>());
    ASSERT_THROW(value.as<long>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<bool>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<double>(), minijson::bad_value_cast);

    ASSERT_EQ("", value.as<std::optional<std::string_view>>());
    ASSERT_THROW(value.as<std::optional<long>>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<std::optional<bool>>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<std::optional<double>>(), minijson::bad_value_cast);
}

TEST(minijson_reader_detail, parse_value_unquoted)
{
    char buffer[] = "true\n";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

    const char first_char = buffer_context.read();

    char c = first_char;
    bool must_read = true;
    const auto value =
        minijson::detail::parse_value(buffer_context, c, must_read);
    ASSERT_EQ(c, '\n');
    ASSERT_FALSE(must_read);

    ASSERT_EQ(minijson::Boolean, value.type());
    ASSERT_EQ("true", value.raw());

    ASSERT_THROW(value.as<std::string_view>(), minijson::bad_value_cast);
    ASSERT_THROW(value.as<long>(), minijson::bad_value_cast);
    ASSERT_TRUE(value.as<bool>());
    ASSERT_THROW(value.as<double>(), minijson::bad_value_cast);

    ASSERT_THROW(
        value.as<std::optional<std::string_view>>(),
        minijson::bad_value_cast);
    ASSERT_THROW(value.as<std::optional<long>>(), minijson::bad_value_cast);
    ASSERT_EQ(true, value.as<std::optional<bool>>());
    ASSERT_THROW(value.as<std::optional<double>>(), minijson::bad_value_cast);

    // boolean false, null, integer and double cases have been already tested
    // with parse_unquoted_value
}

TEST(minijson_reader_detail, parse_value_unquoted_invalid)
{
    char buffer[] = ":xxx,";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);
    buffer_context.read(); // discard initial :

    bool exception_thrown = false;

    try
    {
        char c = buffer_context.read();
        bool must_read = false;
        minijson::detail::parse_value(buffer_context, c, must_read);
    }
    catch (const minijson::parse_error& parse_error)
    {
        exception_thrown = true;

        ASSERT_EQ(minijson::parse_error::INVALID_VALUE, parse_error.reason());
        ASSERT_EQ(1, parse_error.offset());
        ASSERT_STREQ("Invalid value", parse_error.what());
    }

    ASSERT_TRUE(exception_thrown);
}

TEST(minijson_reader, parse_object_empty)
{
    char buffer[] = "{}";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

    minijson::parse_object(buffer_context, [](std::string_view, minijson::value)
    {
        FAIL();
    });
}

TEST(minijson_reader, parse_object_single_field)
{
    char buffer[] = " {  \n\t\"field\" :\r \"value\"\t\n}  ";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

    bool read_field = false;
    minijson::parse_object(buffer_context, [&](
        const std::string_view field_name,
        const minijson::value field_value)
    {
        ASSERT_FALSE(read_field);
        read_field = true;
        ASSERT_EQ("field", field_name);

        ASSERT_EQ(minijson::String, field_value.type());
        ASSERT_EQ("value", field_value.as<std::string_view>());
    });
    ASSERT_TRUE(read_field);
}

struct parse_object_multiple_fields_handler
{
    std::bitset<7> flags;

    void operator()(const std::string_view n, const minijson::value v)
    {
        if (n == "string")
        {
            flags[0] = 1;
            ASSERT_EQ(minijson::String, v.type());
            ASSERT_EQ("value\"\\/\b\f\n\r\t", v.as<std::string_view>());
        }
        else if (n == "integer")
        {
            flags[1] = 1;
            ASSERT_EQ(minijson::Number, v.type());
            ASSERT_EQ(42, v.as<long>());
        }
        else if (n == "floating_point")
        {
            flags[2] = 1;
            ASSERT_EQ(minijson::Number, v.type());
            ASSERT_DOUBLE_EQ(4261826387162873618273687126387.0, v.as<double>());
        }
        else if (n == "boolean_true")
        {
            flags[3] = 1;
            ASSERT_EQ(minijson::Boolean, v.type());
            ASSERT_TRUE(v.as<bool>());
        }
        else if (n == "boolean_false")
        {
            flags[4] = 1;
            ASSERT_EQ(minijson::Boolean, v.type());
            ASSERT_FALSE(v.as<bool>());
        }
        else if (n == "")
        {
            flags[5] = 1;
            ASSERT_EQ(minijson::Null, v.type());
        }
        else if (n == "\xc3\xA0\x01\x02" "a"
                      "\xED\x9F\xBF\xEE\x80\x80\xEF\xBF\xBF"
                      "b" "你" "\xF0\x90\x80\x80" "\xF4\x8F\xBF\xBF" "à")
        {
            flags[6] = 1;
            ASSERT_EQ(minijson::String, v.type());
            ASSERT_EQ("", v.as<std::string_view>());
        }
        else
        {
            FAIL();
        }
    }
};

TEST(minijson_reader, parse_object_multiple_fields)
{
    char buffer[] =
        "{\"string\":\"value\\\"\\\\\\/\\b\\f\\n\\r\\t\",\"integer\":42,"
        "\"floating_point\":4261826387162873618273687126387,"
        "\"boolean_true\":true,\n\"boolean_false\":false,\"\":null,"
        "\"\\u00e0\\u0001\\u0002a\\ud7ff\\uE000\\uFffFb\\u4F60"
        "\\uD800\\uDC00\\uDBFF\\uDFFFà\":\"\"}";

    {
        minijson::const_buffer_context const_buffer_context(
            buffer,
            sizeof(buffer) - 1);

        parse_object_multiple_fields_handler handler;
        minijson::parse_object(const_buffer_context, handler);
        ASSERT_TRUE(handler.flags.all());
    }
    {
        std::istringstream ss(buffer);
        minijson::istream_context istream_context(ss);

        parse_object_multiple_fields_handler handler;
        minijson::parse_object(istream_context, handler);
        ASSERT_TRUE(handler.flags.all());
    }
    {
        // damage null terminator to test robustness
        buffer[sizeof(buffer) - 1] = 'x';
        minijson::buffer_context buffer_context(
            buffer,
            sizeof(buffer) - 1);

        parse_object_multiple_fields_handler handler;
        minijson::parse_object(buffer_context, handler);
        ASSERT_TRUE(handler.flags.all());
    }
}

template<typename Context>
struct parse_object_nested_handler
{
    std::bitset<3> flags;
    Context& context;

    explicit parse_object_nested_handler(Context& context)
    : context(context)
    {
    }

    void operator()(std::string_view n, minijson::value v)
    {
        if (n == "")
        {
            ASSERT_EQ(minijson::Object, v.type());

            minijson::parse_object(
                context,
                [&](std::string_view n, minijson::value v)
                {
                    ASSERT_EQ("nested2", n);
                    ASSERT_EQ(minijson::Object, v.type());
                    ASSERT_FALSE(flags[0]);

                    minijson::parse_object(
                        context, [&](std::string_view n, minijson::value v)
                        {
                            if (n == "val1")
                            {
                                ASSERT_FALSE(flags[0]);
                                flags[0] = 1;
                                ASSERT_EQ(minijson::Number, v.type());
                                ASSERT_EQ(42, v.as<uint16_t>());
                                ASSERT_EQ(42, v.as<float>());
                            }
                            else if (n == "nested3")
                            {
                                ASSERT_FALSE(flags[2]);
                                flags[2] = 1;
                                ASSERT_EQ(minijson::Array, v.type());
                                minijson::parse_array(
                                    context, [](minijson::value)
                                    {
                                        FAIL();
                                    });
                            }
                            else
                            {
                                FAIL();
                            }
                        });
                });
        }
        else if (n == "val2")
        {
            ASSERT_FALSE(flags[1]);
            flags[1] = 1;
            ASSERT_EQ(minijson::Number, v.type());
            ASSERT_DOUBLE_EQ(42.0, v.as<double>());
        }
        else
        {
            FAIL();
        }
    }
};

TEST(minijson_reader, parse_object_nested)
{
    char buffer[] =
        "{\"\":{\"nested2\":{\"val1\":42,\"nested3\":[]}},\"val2\":42.0}";

    {
        minijson::const_buffer_context const_buffer_context(
            buffer,
            sizeof(buffer) - 1);

        parse_object_nested_handler handler(const_buffer_context);
        minijson::parse_object(const_buffer_context, handler);
        ASSERT_TRUE(handler.flags.all());
    }
    {
        std::istringstream ss(buffer);
        minijson::istream_context istream_context(ss);

        parse_object_nested_handler handler(istream_context);
        minijson::parse_object(istream_context, handler);
        ASSERT_TRUE(handler.flags.all());
    }
    {
        // damage null terminator to test robustness
        buffer[sizeof(buffer) - 1] = 'x';
        minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

        parse_object_nested_handler handler(buffer_context);
        minijson::parse_object(buffer_context, handler);
        ASSERT_TRUE(handler.flags.all());
    }
}

TEST(minijson_reader, parse_array_empty)
{
    char buffer[] = "[]";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

    minijson::parse_array(buffer_context, [](minijson::value)
    {
        FAIL();
    });
}

TEST(minijson_reader, parse_array_single_elem)
{
    char buffer[] = " [  \n\t\"value\"\t\n]  ";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

    bool read_elem = false;
    minijson::parse_array(buffer_context, [&](minijson::value elem_value)
    {
        ASSERT_FALSE(read_elem);
        read_elem = true;

        ASSERT_EQ(minijson::String, elem_value.type());
        ASSERT_EQ("value", elem_value.as<std::string_view>());
    });
    ASSERT_TRUE(read_elem);
}

TEST(minijson_reader, parse_array_single_elem2)
{
    char buffer[] = "[1]";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

    bool read_elem = false;
    minijson::parse_array(buffer_context, [&](minijson::value elem_value)
    {
        ASSERT_FALSE(read_elem);
        read_elem = true;

        ASSERT_EQ(minijson:: Number, elem_value.type());
        ASSERT_EQ(1, elem_value.as<int8_t>());
        ASSERT_EQ(1, elem_value.as<float>());
        ASSERT_THROW(
            elem_value.as<std::string_view>(),
            minijson::bad_value_cast);
    });
    ASSERT_TRUE(read_elem);
}

struct parse_array_multiple_elems_handler
{
    std::size_t counter = 0;

    void operator()(const minijson::value v)
    {
        switch (counter++)
        {
        case 0:
            ASSERT_EQ(minijson::String, v.type());
            ASSERT_EQ("value", v.as<std::string_view>());
            break;
        case 1:
            ASSERT_EQ(minijson::Number, v.type());
            ASSERT_EQ(42, v.as<long>());
            break;
        case 2:
            ASSERT_EQ(minijson::Number, v.type());
            ASSERT_DOUBLE_EQ(42.0, v.as<double>());
            break;
        case 3:
            ASSERT_EQ(minijson::Boolean, v.type());
            ASSERT_TRUE(v.as<bool>());
            break;
        case 4:
            ASSERT_EQ(minijson::Boolean, v.type());
            ASSERT_FALSE(v.as<bool>());
            break;
        case 5:
            ASSERT_EQ(minijson::Null, v.type());
            break;
        case 6:
            ASSERT_EQ(minijson::String, v.type());
            ASSERT_EQ("", v.as<std::string_view>());
            break;
        default:
            FAIL();
        }
    }
};

TEST(minijson_reader, parse_array_multiple_elems)
{
    char buffer[] = "[\"value\",42,42.0,true,\nfalse,null,\"\"]";

    {
        minijson::const_buffer_context const_buffer_context(
            buffer,
            sizeof(buffer) - 1);

        parse_array_multiple_elems_handler handler;
        minijson::parse_array(const_buffer_context, handler);
        ASSERT_EQ(handler.counter, 7);
    }
    {
        std::istringstream ss(buffer);
        minijson::istream_context istream_context(ss);

        parse_array_multiple_elems_handler handler;
        minijson::parse_array(istream_context, handler);
        ASSERT_EQ(handler.counter, 7);
    }
    {
        // damage null terminator to test robustness
        buffer[sizeof(buffer) - 1] = 'x';
        minijson::buffer_context buffer_context(
            buffer,
            sizeof(buffer) - 1);

        parse_array_multiple_elems_handler handler;
        minijson::parse_array(buffer_context, handler);
        ASSERT_EQ(handler.counter, 7);
    }
}

template<typename Context>
struct parse_array_nested_handler
{
    std::size_t counter = 0;
    Context& context;

    explicit parse_array_nested_handler(Context& context)
    : context(context)
    {
    }

    void operator()(const minijson::value v)
    {
        switch (counter++)
        {
        case 0:
        {
            ASSERT_EQ(minijson::Array, v.type());
            nested_handler handler(context);
            minijson::parse_array(context, handler);
            ASSERT_TRUE(handler.read_elem);
            break;
        }
        case 1:
            ASSERT_EQ(minijson::Number, v.type());
            ASSERT_DOUBLE_EQ(42.0, v.as<double>());
            break;
        default:
            FAIL();
        }
    }

    struct nested_handler
    {
        Context& context;
        bool read_elem = false;

        explicit nested_handler(Context& context)
        : context(context)
        {
        }

        void operator()(const minijson::value v)
        {
            ASSERT_FALSE(read_elem);
            read_elem = true;

            ASSERT_EQ(minijson::Array, v.type());

            std::size_t counter_nested = 0;
            minijson::parse_array(
                context,
                [&](const minijson::value v)
                {
                    switch (counter_nested++)
                    {
                    case 0:
                        ASSERT_EQ(minijson::Number, v.type());
                        ASSERT_EQ(42, v.as<long>());
                        break;
                    case 1:
                        ASSERT_EQ(minijson::Object, v.type());
                        minijson::parse_object(context, [](auto...){FAIL();});
                        break;
                    default:
                        FAIL();
                    }
                });
            ASSERT_EQ(counter_nested, 2);
        }
    };
};

TEST(minijson_reader, parse_array_nested)
{
    char buffer[] = "[[[42,{}]],42.0]";

    {
        minijson::const_buffer_context const_buffer_context(
            buffer,
            sizeof(buffer) - 1);

        parse_array_nested_handler handler(const_buffer_context);
        minijson::parse_array(const_buffer_context, handler);
        ASSERT_EQ(handler.counter, 2);
    }
    {
        std::istringstream ss(buffer);
        minijson::istream_context istream_context(ss);

        parse_array_nested_handler handler(istream_context);
        minijson::parse_array(istream_context, handler);
        ASSERT_EQ(handler.counter, 2);
    }
    {
         // damage null terminator to test robustness
        buffer[sizeof(buffer) - 1] = 'x';
        minijson::buffer_context buffer_context(
            buffer,
            sizeof(buffer) - 1);

        parse_array_nested_handler handler(buffer_context);
        minijson::parse_array(buffer_context, handler);
        ASSERT_EQ(handler.counter, 2);
    }
}

TEST(minijson_reader, parse_object_truncated)
{
    using minijson::parse_error;

    char buffer[] = "{\"str\":\"val\",\"int\":42,\"null\":null}";

    for (std::size_t i = sizeof(buffer) - 2; i < sizeof(buffer); i--)
    {
        buffer[i] = 0;
        minijson::const_buffer_context const_buffer_context(
            buffer,
            sizeof(buffer) - 1);

        bool exception_thrown = false;
        try
        {
            minijson::parse_object(
                const_buffer_context,
                minijson::detail::ignore(const_buffer_context));
        }
        catch (const parse_error& e)
        {
            exception_thrown = true;

            switch (i)
            {
            case 0:
                ASSERT_EQ(parse_error::EXPECTED_OPENING_BRACKET, e.reason());
                ASSERT_STREQ("Expected opening bracket", e.what());
                break;
            case 1:
                ASSERT_EQ(parse_error::EXPECTED_OPENING_QUOTE, e.reason());
                break;
            case 2:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 3:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 4:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 5:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 6:
                ASSERT_EQ(parse_error::EXPECTED_COLON, e.reason());
                ASSERT_STREQ("Expected colon", e.what());
                break;
            case 7:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 8:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 9:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 10:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 11:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 12:
                ASSERT_EQ(
                    parse_error::EXPECTED_COMMA_OR_CLOSING_BRACKET,
                    e.reason());
                ASSERT_STREQ("Expected comma or closing bracket", e.what());
                break;
            case 13:
                ASSERT_EQ(parse_error::EXPECTED_OPENING_QUOTE, e.reason());
                break;
            case 14:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 15:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 16:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 17:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 18:
                ASSERT_EQ(parse_error::EXPECTED_COLON, e.reason());
                break;
            case 19:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 20:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 21:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 22:
                ASSERT_EQ(parse_error::EXPECTED_OPENING_QUOTE, e.reason());
                break;
            case 23:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 24:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 25:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 26:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 27:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 28:
                ASSERT_EQ(parse_error::EXPECTED_COLON, e.reason());
                break;
            case 29:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 30:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 31:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 32:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 33:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            default: FAIL();
            }
        }

        ASSERT_TRUE(exception_thrown);
    }
}

TEST(minijson_reader, parse_array_truncated)
{
    using minijson::parse_error;

    char buffer[] = "[\"val\",42,null]";

    for (std::size_t i = sizeof(buffer) - 2; i < sizeof(buffer); i--)
    {
        buffer[i] = 0;
        minijson::const_buffer_context const_buffer_context(
            buffer,
            sizeof(buffer) - 1);

        bool exception_thrown = false;
        try
        {
            minijson::parse_array(
                const_buffer_context,
                minijson::detail::ignore(const_buffer_context));
        }
        catch (const parse_error& e)
        {
            exception_thrown = true;

            switch (i)
            {
            case 0:
                ASSERT_EQ(parse_error::EXPECTED_OPENING_BRACKET, e.reason());
                break;
            case 1:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 2:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 3:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 4:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 5:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 6:
                ASSERT_EQ(
                    parse_error::EXPECTED_COMMA_OR_CLOSING_BRACKET,
                    e.reason());
                break;
            case 7:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 8:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 9:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 10:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 11:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 12:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 13:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            case 14:
                ASSERT_EQ(parse_error::UNTERMINATED_VALUE, e.reason());
                break;
            default:
                FAIL();
            }
        }

        ASSERT_TRUE(exception_thrown);
    }
}

template<std::size_t Length>
void parse_object_invalid_helper(
    const char (&buffer)[Length],
    const minijson::parse_error::error_reason expected_reason,
    const char* const expected_what = nullptr)
{
    SCOPED_TRACE(buffer);
    minijson::const_buffer_context const_buffer_context(
        buffer,
        sizeof(buffer) - 1);

    bool exception_thrown = false;

    try
    {
        minijson::parse_object(
            const_buffer_context,
            minijson::detail::ignore(const_buffer_context));
    }
    catch (const minijson::parse_error& e)
    {
        exception_thrown = true;
        ASSERT_EQ(expected_reason, e.reason());
        if (expected_what)
        {
            ASSERT_STREQ(expected_what, e.what());
        }
    }

    ASSERT_TRUE(exception_thrown);
}

template<std::size_t Length>
void parse_array_invalid_helper(
    const char (&buffer)[Length],
    const minijson::parse_error::error_reason expected_reason,
    const char* const expected_what = nullptr)
{
    SCOPED_TRACE(buffer);
    minijson::const_buffer_context const_buffer_context(
        buffer,
        sizeof(buffer) - 1);

    bool exception_thrown = false;

    try
    {
        minijson::parse_array(
            const_buffer_context,
            minijson::detail::ignore(const_buffer_context));
    }
    catch (const minijson::parse_error& e)
    {
        exception_thrown = true;
        ASSERT_EQ(expected_reason, e.reason());
        if (expected_what)
        {
            ASSERT_STREQ(expected_what, e.what());
        }
    }

    ASSERT_TRUE(exception_thrown);
}

TEST(minijson_reader, parse_object_invalid)
{
    parse_object_invalid_helper(
        "",
        minijson::parse_error::EXPECTED_OPENING_BRACKET);

    parse_object_invalid_helper(
        "{\"x\":8.2e+62738",
        minijson::parse_error::UNTERMINATED_VALUE);

    parse_object_invalid_helper(
        "{\"x\":",
        minijson::parse_error::UNTERMINATED_VALUE);

    parse_object_invalid_helper(
        "{\"x\":}",
        minijson::parse_error::EXPECTED_VALUE);

    parse_object_invalid_helper(
        "{\"x\": }",
        minijson::parse_error::EXPECTED_VALUE);

    parse_object_invalid_helper(
        "{\"x\":,\"foo\"}",
        minijson::parse_error::EXPECTED_VALUE);

    parse_object_invalid_helper(
        "{:\"foo\"}",
        minijson::parse_error::EXPECTED_OPENING_QUOTE);

    parse_object_invalid_helper(
        "{\"x\":\"foo\",:\"bar\"}",
        minijson::parse_error::EXPECTED_OPENING_QUOTE);

    parse_object_invalid_helper(
        "{\"x\": ,\"foo\"}",
        minijson::parse_error::EXPECTED_VALUE);

    parse_object_invalid_helper(
        "{\"x\":8.}",
        minijson::parse_error::INVALID_VALUE);

    parse_object_invalid_helper(
        "{\"x\":8..2}",
        minijson::parse_error::INVALID_VALUE);

    parse_object_invalid_helper(
        "{\"x\":8.2e}",
        minijson::parse_error::INVALID_VALUE);

    parse_object_invalid_helper(
        "{\"x\":8.2e+-7}",
        minijson::parse_error::INVALID_VALUE);

    parse_object_invalid_helper(
        "{\"x\":8.2e7e}",
        minijson::parse_error::INVALID_VALUE);

    parse_object_invalid_helper(
        "{\"x\":8.2e7+}",
        minijson::parse_error::INVALID_VALUE);

    parse_object_invalid_helper(
        "{\"x\":8.2e+}",
        minijson::parse_error::INVALID_VALUE);

    parse_object_invalid_helper(
        "{\"x\":.2}",
        minijson::parse_error::INVALID_VALUE);

    parse_object_invalid_helper(
        "{\"x\":0.e7}",
        minijson::parse_error::INVALID_VALUE);

    parse_object_invalid_helper(
        "{\"x\":01}",
        minijson::parse_error::INVALID_VALUE);

    parse_object_invalid_helper(
        "{\"x\":- 1}",
        minijson::parse_error::INVALID_VALUE);

    parse_object_invalid_helper(
        "{\"x\":+1}",
        minijson::parse_error::INVALID_VALUE);

    parse_object_invalid_helper(
        "{\"x\":3.4.5}",
        minijson::parse_error::INVALID_VALUE);

    parse_object_invalid_helper(
        "{\"x\":0x1273}",
        minijson::parse_error::INVALID_VALUE);

    parse_object_invalid_helper(
        "{\"x\":NaN}",
        minijson::parse_error::INVALID_VALUE);

    parse_object_invalid_helper(
        "{\"x\":nuxl}",
        minijson::parse_error::INVALID_VALUE);

    parse_object_invalid_helper(
        "{\"\\ufffx\":null}",
        minijson::parse_error::INVALID_ESCAPE_SEQUENCE);

    parse_object_invalid_helper(
        "{\"x\":\"\\ufffx\"}",
        minijson::parse_error::INVALID_ESCAPE_SEQUENCE);

    parse_object_invalid_helper(
        "{\"x\":\"\\ufff",
        minijson::parse_error::UNTERMINATED_VALUE);

    parse_object_invalid_helper(
        "{\"\\u\":\"\"}",
        minijson::parse_error::INVALID_ESCAPE_SEQUENCE);

    parse_object_invalid_helper(
        "{\"\\ud800\":null}",
        minijson::parse_error::EXPECTED_UTF16_LOW_SURROGATE);

    parse_object_invalid_helper(
        "{\"\\ud800:null}",
        minijson::parse_error::EXPECTED_UTF16_LOW_SURROGATE);

    parse_object_invalid_helper(
        "{\"\\udc00\":null}",
        minijson::parse_error::INVALID_UTF16_CHARACTER);

    parse_object_invalid_helper(
        "{\"\\u0000\":null}",
        minijson::parse_error::NULL_UTF16_CHARACTER);

    parse_object_invalid_helper(
        "{\"\\ud800\\uee00\":null}",
        minijson::parse_error::INVALID_UTF16_CHARACTER);

    parse_object_invalid_helper(
        "{\"\\ud800\\u0000\":null}",
        minijson::parse_error::INVALID_UTF16_CHARACTER);

    parse_object_invalid_helper(
        "{\"\\x\":null}",
        minijson::parse_error::INVALID_ESCAPE_SEQUENCE);

    parse_object_invalid_helper(
        "{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":"
        "[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":["
        "]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}",
        minijson::parse_error::EXCEEDED_NESTING_LIMIT,
        "Exceeded nesting limit (32)");
}

TEST(minijson_reader, parse_array_invalid)
{
    parse_array_invalid_helper(
        "",
        minijson::parse_error::EXPECTED_OPENING_BRACKET);

    parse_array_invalid_helper(
        "[8.2e+62738",
        minijson::parse_error::UNTERMINATED_VALUE);

    parse_array_invalid_helper(
        "[",
        minijson::parse_error::UNTERMINATED_VALUE);

    parse_array_invalid_helper(
        "[5,",
        minijson::parse_error::UNTERMINATED_VALUE);

    parse_array_invalid_helper(
        "[,]",
        minijson::parse_error::EXPECTED_VALUE);

    parse_array_invalid_helper(
        "[ ,]",
        minijson::parse_error::EXPECTED_VALUE);

    parse_array_invalid_helper(
        "[,42]",
        minijson::parse_error::EXPECTED_VALUE);

    parse_array_invalid_helper(
        "[ ,42]",
        minijson::parse_error::EXPECTED_VALUE);

    parse_array_invalid_helper(
        "[42,]",
        minijson::parse_error::EXPECTED_VALUE);

    parse_array_invalid_helper(
        "[42, ]",
        minijson::parse_error::EXPECTED_VALUE);

    parse_array_invalid_helper(
        "[e+62738]",
        minijson::parse_error::INVALID_VALUE);

    parse_array_invalid_helper(
        "[3.4.5]",
        minijson::parse_error::INVALID_VALUE);

    parse_array_invalid_helper(
        "[0x1273]",
        minijson::parse_error::INVALID_VALUE);

    parse_array_invalid_helper(
        "[NaN]",
        minijson::parse_error::INVALID_VALUE);

    parse_array_invalid_helper(
        "[nuxl]",
        minijson::parse_error::INVALID_VALUE);

    parse_array_invalid_helper(
        "[\"\\ufffx\"]",
        minijson::parse_error::INVALID_ESCAPE_SEQUENCE);

    parse_array_invalid_helper(
        "[\"\\ufff\"]",
        minijson::parse_error::INVALID_ESCAPE_SEQUENCE);

    parse_array_invalid_helper(
        "[\"\\ud800\"]",
        minijson::parse_error::EXPECTED_UTF16_LOW_SURROGATE);

    parse_array_invalid_helper(
        "[\"\\udc00\"]",
        minijson::parse_error::INVALID_UTF16_CHARACTER);

    parse_array_invalid_helper(
        "[\"\\u0000\"]",
        minijson::parse_error::NULL_UTF16_CHARACTER);

    parse_array_invalid_helper(
        "[\"\\ud800\\uee00\"]",
        minijson::parse_error::INVALID_UTF16_CHARACTER);

    parse_array_invalid_helper(
        "[\"\\ud800\\u0000\"]",
        minijson::parse_error::INVALID_UTF16_CHARACTER);

    parse_array_invalid_helper(
        "[\"\\x\"]",
        minijson::parse_error::INVALID_ESCAPE_SEQUENCE);

    parse_array_invalid_helper(
        "[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":"
        "[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{"
        "}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]",
        minijson::parse_error::EXCEEDED_NESTING_LIMIT,
        "Exceeded nesting limit (32)");
}

TEST(minijson_reader, nested_not_parsed)
{
    {
        char buffer[] = "{\"a\":[]}";
        minijson::buffer_context context(buffer, sizeof(buffer));

        try
        {
            minijson::parse_object(context, [](auto...){});
            FAIL(); // should never get here
        }
        catch (const minijson::parse_error& e)
        {
            ASSERT_EQ(
                minijson::parse_error::NESTED_OBJECT_OR_ARRAY_NOT_PARSED,
                e.reason());
            ASSERT_STREQ(
                "Nested object or array not parsed",
                e.what());
        }
    }

    {
        char buffer[] = "[{}]";
        minijson::buffer_context context(buffer, sizeof(buffer));

        try
        {
            minijson::parse_array(context, [](auto...){});
            FAIL(); // should never get here
        }
        catch (const minijson::parse_error& e)
        {
            ASSERT_EQ(
                minijson::parse_error::NESTED_OBJECT_OR_ARRAY_NOT_PARSED,
                e.reason());
            ASSERT_STREQ(
                "Nested object or array not parsed",
                e.what());
        }
    }
}

TEST(minijson_dispatch, present)
{
    bool handled[4] {};

    minijson::dispatch("test2")
        <<"test1">> [&]{handled[0] = true;}
        <<"test2">> [&]{handled[1] = true;} // should "break" here
        <<"test3">> [&]{handled[2] = true;}
        <<"test2">> [&]{handled[3] = true;};

    ASSERT_FALSE(handled[0]);
    ASSERT_TRUE(handled[1]);
    ASSERT_FALSE(handled[2]);
    ASSERT_FALSE(handled[3]);
}

TEST(minijson_dispatch, absent)
{
    bool handled[3] {};

    minijson::dispatch("x")
        <<"test1">> [&]{handled[0] = true;}
        <<"test2">> [&]{handled[1] = true;}
        <<"test3">> [&]{handled[2] = true;};

    ASSERT_FALSE(handled[0]);
    ASSERT_FALSE(handled[1]);
    ASSERT_FALSE(handled[2]);
}

TEST(minijson_dispatch, absent_with_any_handler)
{
    bool handled[4] {};

    using minijson::any;

    minijson::dispatch("x")
        <<"test1">> [&]{handled[0] = true;}
        <<"test2">> [&]{handled[1] = true;}
        <<"test3">> [&]{handled[2] = true;}
        <<any>>     [&]{handled[3] = true;};

    ASSERT_FALSE(handled[0]);
    ASSERT_FALSE(handled[1]);
    ASSERT_FALSE(handled[2]);
    ASSERT_TRUE(handled[3]);
}

TEST(minijson_dispatch, std_string)
{
    const std::string x = "x";

    bool handled = false;

    minijson::dispatch(x)
        <<x>> [&]{handled = true;};

    ASSERT_TRUE(handled);
}

TEST(minijson_dispatch, parse_object)
{
    char json_obj[] =
        R"json(
{
    "field1": 42,
    "array" : [ 1, 2, 3 ],
    "field2": "He said \"hi\"",
    "nested" :
    {
        "field1": 42.0,
        "field2" :true,
        "ignored_field" : 0,
        "ignored_object" : {"a":[0]}
    },
    "ignored_array" : [4, 2, {"a":5}, [7]]
}
)json";

    struct obj_type
    {
        long field1 = 0;
        std::string_view field2;
        struct
        {
            double field1 = std::numeric_limits<double>::quiet_NaN();
            bool field2 = false;
        } nested;
        std::vector<int> array;
    };

    obj_type obj;

    using namespace minijson;

    buffer_context ctx(json_obj, sizeof(json_obj) - 1);
    parse_object(ctx, [&](std::string_view k, value v)
    {
        dispatch (k)
        <<"field1">> [&]{obj.field1 = v.as<long>();}
        <<"field2">> [&]{obj.field2 = v.as<std::string_view>();}
        <<"nested">> [&]
        {
            parse_object(ctx, [&](std::string_view k, value v)
            {
                dispatch (k)
                <<"field1">> [&]{obj.nested.field1 = v.as<double>();}
                <<"field2">> [&]{obj.nested.field2 = v.as<bool>();}
                <<any>> [&]{ignore(ctx);};
            });
        }
        <<"array">> [&]
        {
            parse_array(
                ctx,
                [&](value v) {obj.array.push_back(v.as<long>());});
        }
        <<any>> [&]{ignore(ctx);};
    });

    ASSERT_EQ(42, obj.field1);
    ASSERT_EQ("He said \"hi\"", obj.field2);
    ASSERT_DOUBLE_EQ(42.0, obj.nested.field1);
    ASSERT_TRUE(obj.nested.field2);
    ASSERT_EQ(3U, obj.array.size());
    ASSERT_EQ(1, obj.array[0]);
    ASSERT_EQ(2, obj.array[1]);
    ASSERT_EQ(3, obj.array[2]);
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
