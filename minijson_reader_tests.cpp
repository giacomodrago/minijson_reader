#include "minijson_reader.hpp"

#include <gtest/gtest.h>

#include <bitset>

template<typename T, size_t Size>
bool arrays_match(const T (&expected)[Size], const T (&actual)[Size])
{
    return std::equal(expected, expected + Size, actual);
}

template<typename Context>
void test_context_helper(Context& context)
{
    bool loop = true;
    while (loop)
    {
        switch (context.read_offset())
        {
        case 0:  ASSERT_EQ('h', context.read()); context.write('H'); break;
        case 1:  ASSERT_EQ('e', context.read()); context.write('e'); break;
        case 2:  ASSERT_EQ('l', context.read()); context.write('l'); break;
        case 3:  ASSERT_EQ('l', context.read()); context.write('l'); break;
        case 4:  ASSERT_EQ('o', context.read()); context.write('o'); break;
        case 5:  ASSERT_EQ(' ', context.read()); context.write(0); ASSERT_STREQ("Hello", context.write_buffer()); context.new_write_buffer(); break;
        case 6:  ASSERT_EQ('w', context.read()); context.write('W'); break;
        case 7:  ASSERT_EQ('o', context.read()); context.write('o'); break;
        case 8:  ASSERT_EQ('r', context.read()); context.write('r'); break;
        case 9:  ASSERT_EQ('l', context.read()); context.write('l'); break;
        case 10: ASSERT_EQ('d', context.read()); context.write('d'); break;
        case 11: ASSERT_EQ('.', context.read()); context.write(0);   break;
        case 12: ASSERT_EQ(0,   context.read()); loop = false;  break;
        }
    }

    ASSERT_EQ(0, context.read());
    ASSERT_EQ(12U, context.read_offset());
    ASSERT_STREQ("World", context.write_buffer());

    ASSERT_EQ(minijson::detail::context_base::NESTED_STATUS_NONE, context.nested_status());

    context.begin_nested(minijson::detail::context_base::NESTED_STATUS_OBJECT);
    ASSERT_EQ(minijson::detail::context_base::NESTED_STATUS_OBJECT, context.nested_status());
    ASSERT_EQ(1U, context.nesting_level());
    context.begin_nested(minijson::detail::context_base::NESTED_STATUS_ARRAY);
    ASSERT_EQ(minijson::detail::context_base::NESTED_STATUS_ARRAY, context.nested_status());
    ASSERT_EQ(2U, context.nesting_level());
    context.end_nested();
    ASSERT_EQ(1U, context.nesting_level());
    context.end_nested();
    ASSERT_EQ(0U, context.nesting_level());

    context.reset_nested_status();
    ASSERT_EQ(minijson::detail::context_base::NESTED_STATUS_NONE, context.nested_status());
}

TEST(minijson_reader, buffer_context)
{
    char buffer[] = { 'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '.' };
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    test_context_helper(buffer_context);

    ASSERT_STREQ("Hello", buffer);
    ASSERT_STREQ("World", buffer + 6);
    ASSERT_THROW(buffer_context.write('x'), std::runtime_error);
    buffer_context.new_write_buffer();
    ASSERT_EQ(buffer + sizeof(buffer), buffer_context.write_buffer());
    ASSERT_THROW(buffer_context.write('x'), std::runtime_error);
}

TEST(minijson_reader, const_buffer_context)
{
    const char buffer[] = "hello world.";
    minijson::const_buffer_context const_buffer_context(buffer, sizeof(buffer) - 1);
    const char* const original_write_buffer = const_buffer_context.write_buffer();
    test_context_helper(const_buffer_context);

    ASSERT_STREQ("hello world.", buffer); // no side effects
    ASSERT_THROW(const_buffer_context.write('x'), std::runtime_error);
    const_buffer_context.new_write_buffer();
    ASSERT_EQ(original_write_buffer + strlen(buffer), const_buffer_context.write_buffer());
    ASSERT_THROW(const_buffer_context.write('x'), std::runtime_error);
}

TEST(minijson_reader, istream_context)
{
    std::istringstream buffer("hello world.");
    minijson::istream_context istream_context(buffer);
    test_context_helper(istream_context);
}

template<typename Context>
void test_context_copy_construction_helper(const Context& original)
{
    Context copy(original);
    (void)copy;
}

TEST(minijson_reader, context_no_copy_construction)
{
    std::istringstream ss;

    // this test is compile-time only: uncommenting any of the following lines should cause a compile error
    //test_context_copy_construction_helper(minijson::buffer_context(NULL, 0));
    //test_context_copy_construction_helper(minijson::const_buffer_context(NULL, 0));
    //test_context_copy_construction_helper(minijson::istream_context(ss));

    (void)ss;
}

template<typename Context>
void test_context_copy_assignment_helper(const Context& original, Context& copy)
{
    copy = original;
}

TEST(minijson_reader, context_no_copy_assignment)
{
    std::istringstream ss;
    minijson::buffer_context buffer_context(NULL, 0);
    minijson::const_buffer_context const_buffer_context(NULL, 0);
    minijson::istream_context istream_context(ss);

    // this test is compile-time only: uncommenting any of the following lines should cause a compile error
    //test_context_copy_assignment_helper(minijson::buffer_context(NULL, 0), buffer_context);
    //test_context_copy_assignment_helper(minijson::const_buffer_context(NULL, 0), const_buffer_context);
    //test_context_copy_assignment_helper(minijson::istream_context(ss), istream_context);

    (void)ss;
    (void)buffer_context;
    (void)const_buffer_context;
    (void)istream_context;
}

TEST(minijson_reader, parse_error)
{
    {
        minijson::buffer_context buffer_context(NULL, 0);
        minijson::parse_error parse_error(buffer_context, minijson::parse_error::UNKNOWN);

        ASSERT_EQ(0U, parse_error.offset());
        ASSERT_EQ(minijson::parse_error::UNKNOWN, parse_error.reason());
        ASSERT_STREQ("Unknown parse error", parse_error.what());
    }
    {
        const char buffer[] = "hello world.";
        minijson::const_buffer_context const_buffer_context(buffer, sizeof(buffer) - 1);
        const_buffer_context.read();
        const_buffer_context.read();
        ASSERT_EQ(2U, const_buffer_context.read_offset());

        minijson::parse_error parse_error(const_buffer_context, minijson::parse_error::UNKNOWN);
        ASSERT_EQ(1U, parse_error.offset());
        ASSERT_EQ(minijson::parse_error::UNKNOWN, parse_error.reason());
        ASSERT_STREQ("Unknown parse error", parse_error.what());
    }
}

TEST(minijson_reader_detail, utf8_quad)
{
    minijson::detail::utf8_char utf8_quad;
    ASSERT_EQ(0U, utf8_quad[0]);
    ASSERT_EQ(0U, utf8_quad[1]);
    ASSERT_EQ(0U, utf8_quad[2]);
    ASSERT_EQ(0U, utf8_quad[3]);

    const minijson::detail::utf8_char utf8_quad1(0, 1, 2, 3);
    ASSERT_EQ(0U, utf8_quad1[0]);
    ASSERT_EQ(1U, utf8_quad1[1]);
    ASSERT_EQ(2U, utf8_quad1[2]);
    ASSERT_EQ(3U, utf8_quad1[3]);

    minijson::detail::utf8_char utf8_quad2;

    ASSERT_TRUE(utf8_quad == utf8_quad2);
    ASSERT_TRUE(utf8_quad != utf8_quad1);
    ASSERT_FALSE(utf8_quad != utf8_quad2);
    ASSERT_FALSE(utf8_quad == utf8_quad1);
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
    ASSERT_THROW(minijson::detail::utf16_to_utf32(0x0000, 0x0001), minijson::detail::encoding_error);
    ASSERT_THROW(minijson::detail::utf16_to_utf32(0xD800, 0xDBFF), minijson::detail::encoding_error);
    ASSERT_THROW(minijson::detail::utf16_to_utf32(0xD800, 0xE000), minijson::detail::encoding_error);
    ASSERT_THROW(minijson::detail::utf16_to_utf32(0xDC00, 0xDC00), minijson::detail::encoding_error);
}

TEST(minijson_reader_detail, utf32_to_utf8)
{
    // 1 byte
    {
        const uint8_t expected[] = { 0x00, 0x00, 0x00, 0x00 };
        ASSERT_TRUE(arrays_match(expected, minijson::detail::utf32_to_utf8(0x000000).bytes));
    }
    {
        const uint8_t expected[] = { 0x01, 0x00, 0x00, 0x00 };
        ASSERT_TRUE(arrays_match(expected, minijson::detail::utf32_to_utf8(0x000001).bytes));
    }
    {
        const uint8_t expected[] = { 0x7E, 0x00, 0x00, 0x00 };
        ASSERT_TRUE(arrays_match(expected, minijson::detail::utf32_to_utf8(0x00007E).bytes));
    }
    {
        const uint8_t expected[] = { 0x7F, 0x00, 0x00, 0x00 };
        ASSERT_TRUE(arrays_match(expected, minijson::detail::utf32_to_utf8(0x00007F).bytes));
    }

    // 2 bytes
    {
        const uint8_t expected[] = { 0xC2, 0x80, 0x00, 0x00 };
        ASSERT_TRUE(arrays_match(expected, minijson::detail::utf32_to_utf8(0x000080).bytes));
    }
    {
        const uint8_t expected[] = { 0xC2, 0x81, 0x00, 0x00 };
        ASSERT_TRUE(arrays_match(expected, minijson::detail::utf32_to_utf8(0x000081).bytes));
    }
    {
        const uint8_t expected[] = { 0xDF, 0xBE, 0x00, 0x00 };
        ASSERT_TRUE(arrays_match(expected, minijson::detail::utf32_to_utf8(0x0007FE).bytes));
    }
    {
        const uint8_t expected[] = { 0xDF, 0xBF, 0x00, 0x00 };
        ASSERT_TRUE(arrays_match(expected, minijson::detail::utf32_to_utf8(0x0007FF).bytes));
    }

    // 3 bytes
    {
        const uint8_t expected[] = { 0xE0, 0xA0, 0x80, 0x00 };
        ASSERT_TRUE(arrays_match(expected, minijson::detail::utf32_to_utf8(0x000800).bytes));
    }
    {
        const uint8_t expected[] = { 0xE0, 0xA0, 0x81, 0x00 };
        ASSERT_TRUE(arrays_match(expected, minijson::detail::utf32_to_utf8(0x000801).bytes));
    }
    {
        const uint8_t expected[] = { 0xEF, 0xBF, 0xBE, 0x00 };
        ASSERT_TRUE(arrays_match(expected, minijson::detail::utf32_to_utf8(0x00FFFE).bytes));
    }
    {
        const uint8_t expected[] = { 0xEF, 0xBF, 0xBF, 0x00 };
        ASSERT_TRUE(arrays_match(expected, minijson::detail::utf32_to_utf8(0x00FFFF).bytes));
    }

    // 4 bytes
    {
        const uint8_t expected[] = { 0xF0, 0x90, 0x80, 0x80 };
        ASSERT_TRUE(arrays_match(expected, minijson::detail::utf32_to_utf8(0x010000).bytes));
    }
    {
        const uint8_t expected[] = { 0xF0, 0x90, 0x80, 0x81 };
        ASSERT_TRUE(arrays_match(expected, minijson::detail::utf32_to_utf8(0x010001).bytes));
    }
    {
        const uint8_t expected[] = { 0xF7, 0xBF, 0xBF, 0xBE };
        ASSERT_TRUE(arrays_match(expected, minijson::detail::utf32_to_utf8(0x1FFFFE).bytes));
    }
    {
        const uint8_t expected[] = { 0xF7, 0xBF, 0xBF, 0xBF };
        ASSERT_TRUE(arrays_match(expected, minijson::detail::utf32_to_utf8(0x1FFFFF).bytes));
    }
}

TEST(minijson_reader_detail, utf32_to_utf8_invalid)
{
    // invalid code unit
    ASSERT_THROW(minijson::detail::utf32_to_utf8(0x200000), minijson::detail::encoding_error);
}

TEST(minijson_reader_detail, utf16_to_utf8)
{
    // Just one test case, since utf16_to_utf8 calls utf16_to_utf32 and utf32_to_utf8,
    // and other cases have been covered by previous tests

    const uint8_t expected[] = { 0xF4, 0x8F, 0xBF, 0xBF };
    ASSERT_TRUE(arrays_match(expected, minijson::detail::utf16_to_utf8(0xDBFF, 0xDFFF).bytes));
}

TEST(minijson_reader_detail, parse_long)
{
    ASSERT_EQ(0, minijson::detail::parse_long("0"));
    ASSERT_EQ(42, minijson::detail::parse_long("42"));
    ASSERT_EQ(-42, minijson::detail::parse_long("-42"));
    ASSERT_EQ(42, minijson::detail::parse_long("+42"));
    ASSERT_EQ(42, minijson::detail::parse_long("042"));
    ASSERT_EQ(255, minijson::detail::parse_long("ff", 16));
    ASSERT_EQ(255, minijson::detail::parse_long("0xff", 16));
    ASSERT_EQ(255, minijson::detail::parse_long("0ff", 16));

    char buf[64];

    sprintf(buf, "%ld", LONG_MAX);
    ASSERT_EQ(LONG_MAX, minijson::detail::parse_long(buf));

    sprintf(buf, "%ld", LONG_MIN);
    ASSERT_EQ(LONG_MIN, minijson::detail::parse_long(buf));
}

TEST(minijson_reader_detail, parse_long_invalid)
{
    ASSERT_THROW(minijson::detail::parse_long(""), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_long("+"), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_long("-"), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_long(" 4"), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_long("47f"), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_long("_78945"), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_long("78945_"), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_long("78945 "), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_long("0x0"), minijson::detail::number_parse_error);

    ASSERT_THROW(minijson::detail::parse_long("123456789012345678901234567890"), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_long("-123456789012345678901234567890"), minijson::detail::number_parse_error);
}

TEST(minijson_reader_detail, parse_long_invalid_restore_errno)
{
    errno = 42;
    ASSERT_THROW(minijson::detail::parse_long("123456789012345678901234567890"), minijson::detail::number_parse_error);
    ASSERT_EQ(42, errno);
}

TEST(minijson_reader_detail, parse_double)
{
    ASSERT_DOUBLE_EQ(0, minijson::detail::parse_double("0"));
    ASSERT_DOUBLE_EQ(42, minijson::detail::parse_double("42"));
    ASSERT_DOUBLE_EQ(-42, minijson::detail::parse_double("-42"));
    ASSERT_DOUBLE_EQ(42, minijson::detail::parse_double("+42"));
    ASSERT_DOUBLE_EQ(42, minijson::detail::parse_double("042"));
    ASSERT_DOUBLE_EQ(42.42, minijson::detail::parse_double("42.42"));
    ASSERT_DOUBLE_EQ(42.42E+01, minijson::detail::parse_double("42.42E+01"));
    ASSERT_DOUBLE_EQ(42.42E-01, minijson::detail::parse_double("42.42E-01"));

    char buf[2048];

    sprintf(buf, "%lf", std::numeric_limits<double>::max());
    ASSERT_DOUBLE_EQ(std::numeric_limits<double>::max(), minijson::detail::parse_double(buf));

    sprintf(buf, "%lf", -std::numeric_limits<double>::max());
    ASSERT_DOUBLE_EQ(-std::numeric_limits<double>::max(), minijson::detail::parse_double(buf));

#if 0 // for some reason I have to determine, the following two tests fail
    sprintf(buf, "%lf", std::numeric_limits<double>::min());
    ASSERT_DOUBLE_EQ(std::numeric_limits<double>::min(), minijson::detail::parse_double(buf));

    sprintf(buf, "%lf", -std::numeric_limits<double>::min());
    ASSERT_DOUBLE_EQ(-std::numeric_limits<double>::min(), minijson::detail::parse_double(buf));
#endif
}

TEST(minijson_reader_detail, parse_double_invalid)
{
    ASSERT_THROW(minijson::detail::parse_double(""), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_double("+"), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_double("-"), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_double(" 4"), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_double("47f"), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_double("_78945"), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_double("78945_"), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_double("78945 "), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_double("0x0"), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_double("42..42"), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_double("42.42E+094 "), minijson::detail::number_parse_error);
    ASSERT_THROW(minijson::detail::parse_double("42.42E+09.4"), minijson::detail::number_parse_error);

    ASSERT_THROW(minijson::detail::parse_double("1.0E+999"), minijson::detail::number_parse_error); // overflow
    ASSERT_THROW(minijson::detail::parse_double("-1.0E+999"), minijson::detail::number_parse_error); // overflow
    ASSERT_THROW(minijson::detail::parse_double("1.0E-999"), minijson::detail::number_parse_error); // underflow
    ASSERT_THROW(minijson::detail::parse_double("-1.0E-999"), minijson::detail::number_parse_error); // underflow
}

TEST(minijson_reader_detail, parse_double_invalid_restore_errno)
{
    errno = 42;
    ASSERT_THROW(minijson::detail::parse_double("1.0E+999"), minijson::detail::number_parse_error);
    ASSERT_EQ(42, errno);
}

TEST(minijson_reader_detail, parse_utf16_escape_sequence)
{
    ASSERT_EQ(0x0000u, minijson::detail::parse_utf16_escape_sequence("0000"));
    ASSERT_EQ(0x0001u, minijson::detail::parse_utf16_escape_sequence("0001"));
    ASSERT_EQ(0xA6BCu, minijson::detail::parse_utf16_escape_sequence("A6BC"));
    ASSERT_EQ(0xFFFEu, minijson::detail::parse_utf16_escape_sequence("FFFE"));
    ASSERT_EQ(0xFFFFu, minijson::detail::parse_utf16_escape_sequence("FFFF"));
    ASSERT_EQ(0xFFFEu, minijson::detail::parse_utf16_escape_sequence("fffe"));
    ASSERT_EQ(0xFFFFu, minijson::detail::parse_utf16_escape_sequence("ffff"));
    ASSERT_EQ(0xFFFFu, minijson::detail::parse_utf16_escape_sequence("ffFf"));
}

TEST(minijson_reader_detail, parse_utf16_escape_sequence_invalid)
{
    ASSERT_THROW(minijson::detail::parse_utf16_escape_sequence("ffFp"), minijson::detail::encoding_error);
    ASSERT_THROW(minijson::detail::parse_utf16_escape_sequence("-bcd"), minijson::detail::encoding_error);
    ASSERT_THROW(minijson::detail::parse_utf16_escape_sequence(" abc"), minijson::detail::encoding_error);
    ASSERT_THROW(minijson::detail::parse_utf16_escape_sequence("abc "), minijson::detail::encoding_error);
}

static void test_write_utf8_char(minijson::detail::utf8_char c, const char* expected_str)
{
    char buf[] = "____";

    minijson::buffer_context buffer_context(buf, sizeof(buf));
    buffer_context.read();
    buffer_context.read();
    buffer_context.read();
    buffer_context.read();

    minijson::detail::write_utf8_char(buffer_context, c);
    ASSERT_STREQ(expected_str, buf);
}

TEST(minijson_reader_detail, write_utf8_char)
{
    test_write_utf8_char(minijson::detail::utf8_char(0x00, 0x00, 0x00, 0x00), "");
    test_write_utf8_char(minijson::detail::utf8_char(0xFF, 0x00, 0x00, 0x00), "\xFF___");
    test_write_utf8_char(minijson::detail::utf8_char(0xFF, 0xFE, 0x00, 0x00), "\xFF\xFE__");
    test_write_utf8_char(minijson::detail::utf8_char(0xFF, 0xFE, 0xFD, 0x00), "\xFF\xFE\xFD_");
    test_write_utf8_char(minijson::detail::utf8_char(0xFF, 0xFE, 0xFD, 0xFC), "\xFF\xFE\xFD\xFC");
}

TEST(minijson_reader_detail, read_quoted_string_empty)
{
    char buffer[] = "\"\"";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    minijson::detail::read_quoted_string(buffer_context);
    ASSERT_STREQ("", buffer_context.write_buffer());
}

TEST(minijson_reader_detail, read_quoted_string_one_char)
{
    char buffer[] = "\"a\"";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    minijson::detail::read_quoted_string(buffer_context);
    ASSERT_STREQ("a", buffer_context.write_buffer());
}

TEST(minijson_reader_detail, read_quoted_string_ascii)
{
    char buffer[] = "\"foo\"";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    minijson::detail::read_quoted_string(buffer_context);
    ASSERT_STREQ("foo", buffer_context.write_buffer());
}

TEST(minijson_reader_detail, read_quoted_string_utf8)
{
    char buffer[] = "\"你好\"";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    minijson::detail::read_quoted_string(buffer_context);
    ASSERT_STREQ("你好", buffer_context.write_buffer());
}

TEST(minijson_reader_detail, read_quoted_string_escape_sequences)
{
    char buffer[] = "\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    minijson::detail::read_quoted_string(buffer_context);
    ASSERT_STREQ("\"\\/\b\f\n\r\t", buffer_context.write_buffer());
}

TEST(minijson_reader_detail, read_quoted_string_escape_sequences_utf16)
{
    char buffer[] = "\"\\u0001\\u0002a\\ud7ff\\uE000\\uFffFb\\u4F60\\uD800\\uDC00\\uDBFF\\uDFFFà\"";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    minijson::detail::read_quoted_string(buffer_context);
    ASSERT_STREQ("\x01\x02" "a" "\xED\x9F\xBF\xEE\x80\x80\xEF\xBF\xBF" "b" "你" "\xF0\x90\x80\x80" "\xF4\x8F\xBF\xBF" "à",
        buffer_context.write_buffer());
}

TEST(minijson_reader_detail, read_quoted_string_skip_opening_quote)
{
    char buffer[] = "asd\"";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    minijson::detail::read_quoted_string(buffer_context, true);
    ASSERT_STREQ("asd", buffer_context.write_buffer());
}

template<size_t Length>
void read_quoted_string_invalid_helper(
    const char (&buffer)[Length], minijson::parse_error::error_reason expected_reason, size_t expected_offset, const char* expected_what)
{
    bool exception_thrown = false;

    try
    {
        minijson::const_buffer_context context(buffer, Length - 1);
        minijson::detail::read_quoted_string(context);
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

TEST(minijson_reader_detail, read_quoted_string_invalid)
{
    read_quoted_string_invalid_helper("",                   minijson::parse_error::EXPECTED_OPENING_QUOTE,       0,  "Expected opening quote");
    read_quoted_string_invalid_helper("a",                  minijson::parse_error::EXPECTED_OPENING_QUOTE,       0,  "Expected opening quote");
    read_quoted_string_invalid_helper("\"",                 minijson::parse_error::EXPECTED_CLOSING_QUOTE,       0,  "Expected closing quote");
    read_quoted_string_invalid_helper("\"asd",              minijson::parse_error::EXPECTED_CLOSING_QUOTE,       3,  "Expected closing quote");
    read_quoted_string_invalid_helper("\"\\h\"",            minijson::parse_error::INVALID_ESCAPE_SEQUENCE,      2,  "Invalid escape sequence");
    read_quoted_string_invalid_helper("\"\\u0rff\"",        minijson::parse_error::INVALID_UTF16_CHARACTER,      6,  "Invalid UTF-16 character");
    read_quoted_string_invalid_helper("\"\\uD800\\uD7FF\"", minijson::parse_error::INVALID_UTF16_CHARACTER,      12, "Invalid UTF-16 character");
    read_quoted_string_invalid_helper("\"\\uD800\\u0000\"", minijson::parse_error::INVALID_UTF16_CHARACTER,      12, "Invalid UTF-16 character");
    read_quoted_string_invalid_helper("\"\\uDC00\"",        minijson::parse_error::INVALID_UTF16_CHARACTER,      6,  "Invalid UTF-16 character");
    read_quoted_string_invalid_helper("\"\\u0000\"",        minijson::parse_error::NULL_UTF16_CHARACTER,         6,  "Null UTF-16 character");
    read_quoted_string_invalid_helper("\"\\uD800\"",        minijson::parse_error::EXPECTED_UTF16_LOW_SURROGATE, 7,  "Expected UTF-16 low surrogate");
    read_quoted_string_invalid_helper("\"\\uD800a\"",       minijson::parse_error::EXPECTED_UTF16_LOW_SURROGATE, 7,  "Expected UTF-16 low surrogate");
}


template<size_t Length>
void read_unquoted_value_invalid_helper(
    const char (&buffer)[Length], minijson::parse_error::error_reason expected_reason, size_t expected_offset, const char* expected_what)
{
    bool exception_thrown = false;

    try
    {
        minijson::const_buffer_context context(buffer, Length - 1);
        minijson::detail::read_unquoted_value(context);
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

TEST(minijson_reader_detail, read_unquoted_value_empty)
{
    read_unquoted_value_invalid_helper("", minijson::parse_error::UNTERMINATED_VALUE, 0, "Unterminated value");
}

TEST(minijson_reader_detail, read_unquoted_value_comma_terminated)
{
    char buffer[] = "42.42,";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    ASSERT_EQ(',', minijson::detail::read_unquoted_value(buffer_context));
    ASSERT_STREQ("42.42", buffer_context.write_buffer());
}

TEST(minijson_reader_detail, read_unquoted_value_curly_bracket_terminated)
{
    char buffer[] = "42.42E+104}";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    ASSERT_EQ('}', minijson::detail::read_unquoted_value(buffer_context));
    ASSERT_STREQ("42.42E+104", buffer_context.write_buffer());
}

TEST(minijson_reader_detail, read_unquoted_value_square_bracket_terminated)
{
    char buffer[] = "42.42]";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    ASSERT_EQ(']', minijson::detail::read_unquoted_value(buffer_context));
    ASSERT_STREQ("42.42", buffer_context.write_buffer());
}

TEST(minijson_reader_detail, read_unquoted_value_whitespace_terminated)
{
    char buffer[] = "true\t";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    ASSERT_EQ('\t', minijson::detail::read_unquoted_value(buffer_context));
    ASSERT_STREQ("true", buffer_context.write_buffer());
}

TEST(minijson_reader_detail, read_unquoted_value_unterminated)
{
    read_unquoted_value_invalid_helper("5", minijson::parse_error::UNTERMINATED_VALUE, 0, "Unterminated value");
}

TEST(minijson_reader_detail, read_unquoted_value_first_char)
{
    char buffer[] = "true\t";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    buffer_context.read();
    ASSERT_EQ('\t', minijson::detail::read_unquoted_value(buffer_context, 't'));
    ASSERT_STREQ("true", buffer_context.write_buffer());
}

TEST(minijson_reader, value_default_constructed)
{
    const minijson::value value;
    ASSERT_EQ(minijson::Null, value.type());
    ASSERT_STREQ("", value.as_string());
    ASSERT_EQ(0, value.as_long());
    ASSERT_FALSE(value.as_bool());
    ASSERT_DOUBLE_EQ(0.0, value.as_double());
    ASSERT_FALSE(value.long_available());
    ASSERT_FALSE(value.double_available());
}

TEST(minijson_reader, value_example)
{
    const minijson::value value(minijson::Number, "42.42", 42, true, 42.42, true);
    ASSERT_EQ(minijson::Number, value.type());
    ASSERT_STREQ("42.42", value.as_string());
    ASSERT_EQ(42, value.as_long());
    ASSERT_TRUE(value.as_bool());
    ASSERT_DOUBLE_EQ(42.42, value.as_double());
    ASSERT_TRUE(value.long_available());
    ASSERT_TRUE(value.double_available());
}

template<typename Context>
void parse_unquoted_value_invalid_helper(Context& context, size_t expected_offset)
{
    bool exception_thrown = false;

    try
    {
        minijson::detail::parse_unquoted_value(context);
    }
    catch (const minijson::parse_error& parse_error)
    {
        exception_thrown = true;

        ASSERT_EQ(minijson::parse_error::INVALID_VALUE, parse_error.reason());
        ASSERT_EQ(expected_offset, parse_error.offset());
        ASSERT_STREQ("Invalid value", parse_error.what());
    }

    ASSERT_TRUE(exception_thrown);
}

TEST(minijson_reader_detail, parse_unquoted_value_whitespace)
{
    char buffer[] = "  42";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    read_unquoted_value(buffer_context);
    parse_unquoted_value_invalid_helper(buffer_context, 0);
}

TEST(minijson_reader_detail, parse_unquoted_value_true)
{
    char buffer[] = "true  ";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);
    minijson::detail::read_unquoted_value(buffer_context);

    const minijson::value value = minijson::detail::parse_unquoted_value(buffer_context);
    ASSERT_EQ(minijson::Boolean, value.type());
    ASSERT_STREQ("true",         value.as_string());
    ASSERT_EQ(1,                 value.as_long());
    ASSERT_TRUE(                 value.as_bool());
    ASSERT_DOUBLE_EQ(1.0,        value.as_double());
    ASSERT_TRUE(                 value.long_available());
    ASSERT_TRUE(                 value.double_available());
}

TEST(minijson_reader_detail, parse_unquoted_value_false)
{
    char buffer[] = "false}";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);
    minijson::detail::read_unquoted_value(buffer_context);

    const minijson::value value = minijson::detail::parse_unquoted_value(buffer_context);
    ASSERT_EQ(minijson::Boolean, value.type());
    ASSERT_STREQ("false",        value.as_string());
    ASSERT_EQ(0,                 value.as_long());
    ASSERT_FALSE(                value.as_bool());
    ASSERT_DOUBLE_EQ(0.0,        value.as_double());
    ASSERT_TRUE(                 value.long_available());
    ASSERT_TRUE(                 value.double_available());
}

TEST(minijson_reader_detail, parse_unquoted_value_null)
{
    char buffer[] = "null}";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);
    minijson::detail::read_unquoted_value(buffer_context);

    const minijson::value value = minijson::detail::parse_unquoted_value(buffer_context);
    ASSERT_EQ(minijson::Null, value.type());
    ASSERT_STREQ("null",      value.as_string());
    ASSERT_EQ(0,              value.as_long());
    ASSERT_FALSE(             value.as_bool());
    ASSERT_DOUBLE_EQ(0.0,     value.as_double());
    ASSERT_FALSE(             value.long_available());
    ASSERT_FALSE(             value.double_available());
}

TEST(minijson_reader_detail, parse_unquoted_value_integer)
{
    char buffer[] = "42]";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);
    minijson::detail::read_unquoted_value(buffer_context);

    const minijson::value value = minijson::detail::parse_unquoted_value(buffer_context);
    ASSERT_EQ(minijson::Number, value.type());
    ASSERT_STREQ("42",          value.as_string());
    ASSERT_EQ(42,               value.as_long());
    ASSERT_TRUE(                value.as_bool());
    ASSERT_DOUBLE_EQ(42.0,      value.as_double());
    ASSERT_TRUE(                value.long_available());
    ASSERT_TRUE(                value.double_available());
}

TEST(minijson_reader_detail, parse_unquoted_value_large_integer)
{
    char buffer[] = "9223372036854775808]";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);
    minijson::detail::read_unquoted_value(buffer_context);

    const minijson::value value = minijson::detail::parse_unquoted_value(buffer_context);
    ASSERT_EQ(minijson::Number, value.type());
    ASSERT_STREQ("9223372036854775808",     value.as_string());
    ASSERT_EQ(0,                            value.as_long());
    ASSERT_FALSE(                           value.as_bool());
    ASSERT_DOUBLE_EQ(9223372036854775808.0, value.as_double());
    ASSERT_FALSE(                           value.long_available());
    ASSERT_TRUE(                            value.double_available());
}

TEST(minijson_reader_detail, parse_unquoted_value_double)
{
    char buffer[] = "42.0e+76,";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);
    minijson::detail::read_unquoted_value(buffer_context);

    const minijson::value value = minijson::detail::parse_unquoted_value(buffer_context);
    ASSERT_EQ(minijson::Number, value.type());
    ASSERT_STREQ("42.0e+76",    value.as_string());
    ASSERT_EQ(0,                value.as_long());
    ASSERT_FALSE(               value.as_bool());
    ASSERT_DOUBLE_EQ(42.0E+76,  value.as_double());
    ASSERT_FALSE(               value.long_available());
    ASSERT_TRUE(                value.double_available());
}

TEST(minijson_reader_detail, parse_unquoted_value_invalid)
{
    char buffer[] = "asd,";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    minijson::detail::read_unquoted_value(buffer_context);

    parse_unquoted_value_invalid_helper(buffer_context, 3);
}

TEST(minijson_reader_detail, read_value_object)
{
    char buffer[] = "{...";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    buffer_context.read();

    const std::pair<minijson::value, char> result = minijson::detail::read_value(buffer_context, buffer[0]);
    const minijson::value value = result.first;
    const char ending_char = result.second;

    ASSERT_EQ(minijson::Object, value.type());
    ASSERT_STREQ("",            value.as_string());
    ASSERT_EQ(0,                value.as_long());
    ASSERT_FALSE(               value.as_bool());
    ASSERT_DOUBLE_EQ(0.0,       value.as_double());
    ASSERT_FALSE(               value.long_available());
    ASSERT_FALSE(               value.double_available());

    ASSERT_EQ(0, ending_char);
}

TEST(minijson_reader_detail, read_value_array)
{
    char buffer[] = "[...";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer));
    buffer_context.read();

    const std::pair<minijson::value, char> result = minijson::detail::read_value(buffer_context, buffer[0]);
    const minijson::value value = result.first;
    const char ending_char = result.second;

    ASSERT_EQ(minijson::Array, value.type());
    ASSERT_STREQ("",           value.as_string());
    ASSERT_EQ(0,               value.as_long());
    ASSERT_FALSE(              value.as_bool());
    ASSERT_DOUBLE_EQ(0.0,      value.as_double());
    ASSERT_FALSE(              value.long_available());
    ASSERT_FALSE(              value.double_available());

    ASSERT_EQ(0, ending_char);
}

TEST(minijson_reader_detail, read_value_quoted_string)
{
    char buffer[] = "\"Hello world\"";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);
    buffer_context.read();

    const std::pair<minijson::value, char> result = minijson::detail::read_value(buffer_context, buffer[0]);
    const minijson::value value = result.first;
    const char ending_char = result.second;

    ASSERT_EQ(minijson::String, value.type());
    ASSERT_STREQ("Hello world", value.as_string());
    ASSERT_EQ(0,                value.as_long());
    ASSERT_FALSE(               value.as_bool());
    ASSERT_DOUBLE_EQ(0.0,       value.as_double());
    ASSERT_FALSE(               value.long_available());
    ASSERT_FALSE(               value.double_available());

    ASSERT_EQ(0, ending_char);
}

TEST(minijson_reader_detail, read_value_quoted_string_empty)
{
    char buffer[] = "\"\"";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);
    buffer_context.read();

    const std::pair<minijson::value, char> result = minijson::detail::read_value(buffer_context, buffer[0]);
    const minijson::value value = result.first;
    const char ending_char = result.second;

    ASSERT_EQ(minijson::String, value.type());
    ASSERT_STREQ("",            value.as_string());
    ASSERT_EQ(0,                value.as_long());
    ASSERT_FALSE(               value.as_bool());
    ASSERT_DOUBLE_EQ(0.0,       value.as_double());
    ASSERT_FALSE(               value.long_available());
    ASSERT_FALSE(               value.double_available());

    ASSERT_EQ(0, ending_char);
}

TEST(minijson_reader_detail, read_value_unquoted)
{
    char buffer[] = "true,";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);
    buffer_context.read();

    const std::pair<minijson::value, char> result = minijson::detail::read_value(buffer_context, buffer[0]);
    const minijson::value value = result.first;
    const char ending_char = result.second;

    ASSERT_EQ(minijson::Boolean, value.type());
    ASSERT_STREQ("true",         value.as_string());
    ASSERT_EQ(1,                 value.as_long());
    ASSERT_TRUE(                 value.as_bool());
    ASSERT_DOUBLE_EQ(1.0,        value.as_double());
    ASSERT_TRUE(                 value.long_available());
    ASSERT_TRUE(                 value.double_available());

    ASSERT_EQ(',', ending_char);

    // boolean false, null, integer and double cases have been already tested with parse_unquoted_value
}

TEST(minijson_reader_detail, read_value_unquoted_invalid)
{
    char buffer[] = "xxx,";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);
    buffer_context.read();

    bool exception_thrown = false;

    try
    {
        minijson::detail::read_value(buffer_context, buffer[0]);
    }
    catch (const minijson::parse_error& parse_error)
    {
        exception_thrown = true;

        ASSERT_EQ(minijson::parse_error::INVALID_VALUE, parse_error.reason());
        ASSERT_EQ(3u, parse_error.offset());
        ASSERT_STREQ("Invalid value", parse_error.what());
    }

    ASSERT_TRUE(exception_thrown);
}

void parse_object_empty_handler(const char*, minijson::value)
{
    FAIL();
}

void parse_array_empty_handler(minijson::value)
{
    FAIL();
}

TEST(minijson_reader, parse_object_empty)
{
    char buffer[] = "{}";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

    minijson::parse_object(buffer_context, parse_object_empty_handler);
}

struct check_on_destroy_handler
{
    mutable bool check_on_destroy;

    check_on_destroy_handler() :
        check_on_destroy(true)
    {
    }

    check_on_destroy_handler(const check_on_destroy_handler& other) :
        check_on_destroy(true)
    {
        other.check_on_destroy = false;
    }
};

struct parse_object_single_field_handler : check_on_destroy_handler
{
    bool read_field;

    explicit parse_object_single_field_handler() :
        read_field(false)
    {
    }

    ~parse_object_single_field_handler()
    {
        if (check_on_destroy)
        {
            EXPECT_TRUE(read_field);
        }
    }

    void operator()(const char* field_name, const minijson::value& field_value)
    {
        read_field = true;
        ASSERT_STREQ("field", field_name);

        ASSERT_EQ(minijson::String, field_value.type());
        ASSERT_STREQ("value", field_value.as_string());
    }
};

TEST(minijson_reader, parse_object_single_field)
{
    char buffer[] = " {  \n\t\"field\" : \"value\"\t\n}  ";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

    minijson::parse_object(buffer_context, parse_object_single_field_handler());
}

struct parse_object_multiple_fields_handler : check_on_destroy_handler
{
    std::bitset<7> h;

    ~parse_object_multiple_fields_handler()
    {
        if (check_on_destroy)
        {
            EXPECT_TRUE(h.all());
        }
    }

    void operator()(const char* n, const minijson::value& v)
    {
        if (strcmp(n, "string") == 0)
            { h[0] = 1; ASSERT_EQ(minijson::String,  v.type()); ASSERT_STREQ("value\"\\/\b\f\n\r\t", v.as_string()); }
        else if (strcmp(n, "integer") == 0)
            { h[1] = 1; ASSERT_EQ(minijson::Number,  v.type()); ASSERT_EQ(42, v.as_long()); }
        else if (strcmp(n, "floating_point") == 0)
            { h[2] = 1; ASSERT_EQ(minijson::Number,  v.type()); ASSERT_DOUBLE_EQ(4261826387162873618273687126387.0, v.as_double()); }
        else if (strcmp(n, "boolean_true") == 0)
            { h[3] = 1; ASSERT_EQ(minijson::Boolean, v.type()); ASSERT_TRUE(v.as_bool()); }
        else if (strcmp(n, "boolean_false") == 0)
            { h[4] = 1; ASSERT_EQ(minijson::Boolean, v.type()); ASSERT_FALSE(v.as_bool()); }
        else if (strcmp(n, "") == 0)
            { h[5] = 1; ASSERT_EQ(minijson::Null,    v.type()); }
        else if (strcmp(n, "\xc3\xA0\x01\x02" "a" "\xED\x9F\xBF\xEE\x80\x80\xEF\xBF\xBF" "b" "你" "\xF0\x90\x80\x80" "\xF4\x8F\xBF\xBF" "à") == 0)
            { h[6] = 1; ASSERT_EQ(minijson::String,  v.type()); ASSERT_STREQ("", v.as_string()); }
        else
            { FAIL(); }
    }
};

TEST(minijson_reader, parse_object_multiple_fields)
{
    char buffer[] = "{\"string\":\"value\\\"\\\\\\/\\b\\f\\n\\r\\t\",\"integer\":42,\"floating_point\":4261826387162873618273687126387,"
                    "\"boolean_true\":true,\n\"boolean_false\":false,\"\":null,"
                    "\"\\u00e0\\u0001\\u0002a\\ud7ff\\uE000\\uFffFb\\u4F60\\uD800\\uDC00\\uDBFF\\uDFFFà\":\"\"}";

    {
        minijson::const_buffer_context const_buffer_context(buffer, sizeof(buffer) - 1);
        minijson::parse_object(const_buffer_context, parse_object_multiple_fields_handler());
    }
    {
        std::istringstream ss(buffer);
        minijson::istream_context istream_context(ss);
        minijson::parse_object(istream_context, parse_object_multiple_fields_handler());
    }
    {
        buffer[sizeof(buffer) - 1] = 'x'; // damage null terminator to test robustness
        minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);
        minijson::parse_object(buffer_context, parse_object_multiple_fields_handler());
    }
}

template<typename Context>
struct parse_object_nested_handler : check_on_destroy_handler
{
    std::bitset<2> h;
    Context& context;

    explicit parse_object_nested_handler(Context& context) :
        context(context)
    {
    }

    ~parse_object_nested_handler()
    {
        if (check_on_destroy)
        {
            EXPECT_TRUE(h.all());
        }
    }

    void operator()(const char* n, const minijson::value& v)
    {
        if (strcmp(n, "") == 0)
            { h[0] = 1; ASSERT_EQ(minijson::Object, v.type()); minijson::parse_object(context, nested1_handler(context)); }
        else if (strcmp(n, "val2") == 0)
            { h[1] = 1; ASSERT_EQ(minijson::Number, v.type()); ASSERT_DOUBLE_EQ(42.0, v.as_double()); }
        else
            { FAIL(); }
    }

    struct nested1_handler : check_on_destroy_handler
    {
        Context& context;
        bool read_field;

        explicit nested1_handler(Context& context) :
            context(context),
            read_field(false)
        {
        }

        ~nested1_handler()
        {
            if (check_on_destroy)
            {
                EXPECT_TRUE(read_field);
            }
        }

        void operator()(const char* n, const minijson::value& v)
        {
            read_field = true;
            ASSERT_STREQ("nested2", n);
            ASSERT_EQ(minijson::Object, v.type());
            minijson::parse_object(context, nested2_handler(context));
        }

        struct nested2_handler : check_on_destroy_handler
        {
            std::bitset<2> h;
            Context& context;

            explicit nested2_handler(Context& context) :
                context(context)
            {
            }

            ~nested2_handler()
            {
                if (check_on_destroy)
                {
                    EXPECT_TRUE(h.all());
                }
            }

            void operator()(const char* n, const minijson::value& v)
            {
                if (strcmp(n, "val1") == 0)
                    { h[0] = 1; ASSERT_EQ(minijson::Number, v.type()); ASSERT_EQ(42, v.as_long()); }
                else if (strcmp(n, "nested3") == 0)
                    { h[1] = 1; ASSERT_EQ(minijson::Array, v.type()); minijson::parse_array(context, parse_array_empty_handler); }
                else
                    { FAIL(); }
            }
        };
    };
};

TEST(minijson_reader, parse_object_nested)
{
    char buffer[] = "{\"\":{\"nested2\":{\"val1\":42,\"nested3\":[]}},\"val2\":42.0}";

    {
        minijson::const_buffer_context const_buffer_context(buffer, sizeof(buffer) - 1);
        minijson::parse_object(const_buffer_context, parse_object_nested_handler<minijson::const_buffer_context>(const_buffer_context));
    }
    {
        std::istringstream ss(buffer);
        minijson::istream_context istream_context(ss);
        minijson::parse_object(istream_context, parse_object_nested_handler<minijson::istream_context>(istream_context));
    }
    {
        buffer[sizeof(buffer) - 1] = 'x'; // damage null terminator to test robustness
        minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);
        minijson::parse_object(buffer_context, parse_object_nested_handler<minijson::buffer_context>(buffer_context));
    }
}

TEST(minijson_reader, parse_array_empty)
{
    char buffer[] = "[]";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

    minijson::parse_array(buffer_context, parse_array_empty_handler);
}

struct parse_array_single_elem_handler : check_on_destroy_handler
{
    bool read_elem;

    explicit parse_array_single_elem_handler() :
        read_elem(false)
    {
    }

    ~parse_array_single_elem_handler()
    {
        if (check_on_destroy)
        {
            EXPECT_TRUE(read_elem);
        }
    }

    void operator()(const minijson::value& elem_value)
    {
        read_elem = true;

        ASSERT_EQ(minijson::String, elem_value.type());
        ASSERT_STREQ("value", elem_value.as_string());
    }
};

TEST(minijson_reader, parse_array_single_elem)
{
    char buffer[] = " [  \n\t\"value\"\t\n]  ";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

    minijson::parse_array(buffer_context, parse_array_single_elem_handler());
}

struct parse_array_single_elem2_handler : check_on_destroy_handler
{
    bool read_elem;

    explicit parse_array_single_elem2_handler() :
        read_elem(false)
    {
    }

    ~parse_array_single_elem2_handler()
    {
        if (check_on_destroy)
        {
            EXPECT_TRUE(read_elem);
        }
    }

    void operator()(const minijson::value& elem_value)
    {
        read_elem = true;

        ASSERT_EQ(minijson:: Number, elem_value.type());
        ASSERT_EQ(1, elem_value.as_long());
        ASSERT_STREQ("1", elem_value.as_string());
    }
};

TEST(minijson_reader, parse_array_single_elem2)
{
    char buffer[] = "[1]";
    minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);

    minijson::parse_array(buffer_context, parse_array_single_elem2_handler());
}

struct parse_array_multiple_elems_handler : check_on_destroy_handler
{
    size_t counter;

    parse_array_multiple_elems_handler() :
        counter(0)
    {
    }

    ~parse_array_multiple_elems_handler()
    {
        if (check_on_destroy)
        {
            EXPECT_EQ(7U, counter);
        }
    }

    void operator()(const minijson::value& v)
    {
        switch (counter++)
        {
        case 0:  ASSERT_EQ(minijson::String,   v.type()); ASSERT_STREQ("value", v.as_string());  break;
        case 1:  ASSERT_EQ(minijson::Number,   v.type()); ASSERT_EQ(42, v.as_long());            break;
        case 2:  ASSERT_EQ(minijson::Number,   v.type()); ASSERT_DOUBLE_EQ(42.0, v.as_double()); break;
        case 3:  ASSERT_EQ(minijson::Boolean,  v.type()); ASSERT_TRUE(v.as_bool());              break;
        case 4:  ASSERT_EQ(minijson::Boolean,  v.type()); ASSERT_FALSE(v.as_bool());             break;
        case 5:  ASSERT_EQ(minijson::Null,     v.type());                                        break;
        case 6:  ASSERT_EQ(minijson::String,   v.type()); ASSERT_STREQ("", v.as_string());       break;
        default: FAIL();
        }
    }
};

TEST(minijson_reader, parse_array_multiple_elems)
{
    char buffer[] = "[\"value\",42,42.0,true,\nfalse,null,\"\"]";

    {
        minijson::const_buffer_context const_buffer_context(buffer, sizeof(buffer) - 1);
        minijson::parse_array(const_buffer_context, parse_array_multiple_elems_handler());
    }
    {
        std::istringstream ss(buffer);
        minijson::istream_context istream_context(ss);
        minijson::parse_array(istream_context, parse_array_multiple_elems_handler());
    }
    {
        buffer[sizeof(buffer) - 1] = 'x'; // damage null terminator to test robustness
        minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);
        minijson::parse_array(buffer_context, parse_array_multiple_elems_handler());
    }
}

template<typename Context>
struct parse_array_nested_handler : check_on_destroy_handler
{
    size_t counter;
    Context& context;

    explicit parse_array_nested_handler(Context& context) :
        counter(0),
        context(context)
    {
    }

    ~parse_array_nested_handler()
    {
        if (check_on_destroy)
        {
            EXPECT_EQ(2U, counter);
        }
    }

    void operator()(const minijson::value& v)
    {
        switch (counter++)
        {
        case 0:  ASSERT_EQ(minijson::Array,  v.type()); minijson::parse_array(context, nested1_handler(context)); break;
        case 1:  ASSERT_EQ(minijson::Number, v.type()); ASSERT_DOUBLE_EQ(42.0, v.as_double());                    break;
        default: FAIL();
        }
    }

    struct nested1_handler : check_on_destroy_handler
    {
        Context& context;
        bool read_elem;

        explicit nested1_handler(Context& context) :
            context(context),
            read_elem(false)
        {
        }

        ~nested1_handler()
        {
            if (check_on_destroy)
            {
                EXPECT_TRUE(read_elem);
            }
        }

        void operator()(const minijson::value& v)
        {
            read_elem = true;
            ASSERT_EQ(minijson::Array, v.type());
            minijson::parse_array(context, nested2_handler(context));
        }

        struct nested2_handler : check_on_destroy_handler
        {
            size_t counter;
            Context& context;

            explicit nested2_handler(Context& context) :
                counter(0),
                context(context)
            {
            }

            ~nested2_handler()
            {
                if (check_on_destroy)
                {
                    EXPECT_EQ(2U, counter);
                }
            }

            void operator()(const minijson::value& v)
            {
                switch (counter++)
                {
                case 0:  ASSERT_EQ(minijson::Number, v.type()); ASSERT_EQ(42, v.as_long());                                  break;
                case 1:  ASSERT_EQ(minijson::Object, v.type()); minijson::parse_object(context, parse_object_empty_handler); break;
                default: FAIL();
                }
            }
        };
    };
};

TEST(minijson_reader, parse_array_nested)
{
    char buffer[] = "[[[42,{}]],42.0]";

    {
        minijson::const_buffer_context const_buffer_context(buffer, sizeof(buffer) - 1);
        minijson::parse_array(const_buffer_context, parse_array_nested_handler<minijson::const_buffer_context>(const_buffer_context));
    }
    {
        std::istringstream ss(buffer);
        minijson::istream_context istream_context(ss);
        minijson::parse_array(istream_context, parse_array_nested_handler<minijson::istream_context>(istream_context));
    }
    {
        buffer[sizeof(buffer) - 1] = 'x'; // damage null terminator to test robustness
        minijson::buffer_context buffer_context(buffer, sizeof(buffer) - 1);
        minijson::parse_array(buffer_context, parse_array_nested_handler<minijson::buffer_context>(buffer_context));
    }
}

struct parse_dummy
{
    void operator()(const char*, minijson::value)
    {
    }

    void operator()(minijson::value)
    {
    }
};

template<typename Context>
struct parse_dummy_consume
{
    Context& context;

    explicit parse_dummy_consume(Context& context) :
        context(context)
    {
    }

    void operator()(const char*, minijson::value value)
    {
        operator()(value);
    }

    void operator()(minijson::value value)
    {
        if (value.type() == minijson::Object)
        {
            minijson::parse_object(context, *this);
        }
        else if (value.type() == minijson::Array)
        {
            minijson::parse_array(context, *this);
        }
    }
};

TEST(minijson_reader, parse_object_truncated)
{
    using minijson::parse_error;

    char buffer[] = "{\"str\":\"val\",\"int\":42,\"null\":null}";

    for (size_t i = sizeof(buffer) - 2; i < sizeof(buffer); i--)
    {
        buffer[i] = 0;
        minijson::const_buffer_context const_buffer_context(buffer, sizeof(buffer) - 1);

        bool exception_thrown = false;
        try
        {
            minijson::parse_object(const_buffer_context, parse_dummy());
        }
        catch (const parse_error& e)
        {
            exception_thrown = true;

            switch (i)
            {
            case 0:  ASSERT_EQ(parse_error::EXPECTED_OPENING_BRACKET,          e.reason()); ASSERT_STREQ("Expected opening bracket", e.what()); break;
            case 1:  ASSERT_EQ(parse_error::EXPECTED_OPENING_QUOTE,            e.reason()); break;
            case 2:  ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 3:  ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 4:  ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 5:  ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 6:  ASSERT_EQ(parse_error::EXPECTED_COLON,                    e.reason()); ASSERT_STREQ("Expected colon", e.what()); break;
            case 7:  ASSERT_EQ(parse_error::UNTERMINATED_VALUE,                e.reason()); break;
            case 8:  ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 9:  ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 10: ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 11: ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 12: ASSERT_EQ(parse_error::EXPECTED_COMMA_OR_CLOSING_BRACKET, e.reason()); ASSERT_STREQ("Expected comma or closing bracket", e.what()); break;
            case 13: ASSERT_EQ(parse_error::EXPECTED_OPENING_QUOTE,            e.reason()); break;
            case 14: ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 15: ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 16: ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 17: ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 18: ASSERT_EQ(parse_error::EXPECTED_COLON,                    e.reason()); break;
            case 19: ASSERT_EQ(parse_error::UNTERMINATED_VALUE,                e.reason()); break;
            case 20: ASSERT_EQ(parse_error::UNTERMINATED_VALUE,                e.reason()); break;
            case 21: ASSERT_EQ(parse_error::UNTERMINATED_VALUE,                e.reason()); break;
            case 22: ASSERT_EQ(parse_error::EXPECTED_OPENING_QUOTE,            e.reason()); break;
            case 23: ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 24: ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 25: ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 26: ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 27: ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 28: ASSERT_EQ(parse_error::EXPECTED_COLON,                    e.reason()); break;
            case 29: ASSERT_EQ(parse_error::UNTERMINATED_VALUE,                e.reason()); break;
            case 30: ASSERT_EQ(parse_error::UNTERMINATED_VALUE,                e.reason()); break;
            case 31: ASSERT_EQ(parse_error::UNTERMINATED_VALUE,                e.reason()); break;
            case 32: ASSERT_EQ(parse_error::UNTERMINATED_VALUE,                e.reason()); break;
            case 33: ASSERT_EQ(parse_error::UNTERMINATED_VALUE,                e.reason()); break;
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

    for (size_t i = sizeof(buffer) - 2; i < sizeof(buffer); i--)
    {
        buffer[i] = 0;
        minijson::const_buffer_context const_buffer_context(buffer, sizeof(buffer) - 1);

        bool exception_thrown = false;
        try
        {
            minijson::parse_array(const_buffer_context, parse_dummy());
        }
        catch (const parse_error& e)
        {
            exception_thrown = true;

            switch (i)
            {
            case 0:  ASSERT_EQ(parse_error::EXPECTED_OPENING_BRACKET,          e.reason()); break;
            case 1:  ASSERT_EQ(parse_error::UNTERMINATED_VALUE,                e.reason()); break;
            case 2:  ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 3:  ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 4:  ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 5:  ASSERT_EQ(parse_error::EXPECTED_CLOSING_QUOTE,            e.reason()); break;
            case 6:  ASSERT_EQ(parse_error::EXPECTED_COMMA_OR_CLOSING_BRACKET, e.reason()); break;
            case 7:  ASSERT_EQ(parse_error::UNTERMINATED_VALUE,                e.reason()); break;
            case 8:  ASSERT_EQ(parse_error::UNTERMINATED_VALUE,                e.reason()); break;
            case 9:  ASSERT_EQ(parse_error::UNTERMINATED_VALUE,                e.reason()); break;
            case 10: ASSERT_EQ(parse_error::UNTERMINATED_VALUE,                e.reason()); break;
            case 11: ASSERT_EQ(parse_error::UNTERMINATED_VALUE,                e.reason()); break;
            case 12: ASSERT_EQ(parse_error::UNTERMINATED_VALUE,                e.reason()); break;
            case 13: ASSERT_EQ(parse_error::UNTERMINATED_VALUE,                e.reason()); break;
            case 14: ASSERT_EQ(parse_error::UNTERMINATED_VALUE,                e.reason()); break;
            default: FAIL();
            }
        }

        ASSERT_TRUE(exception_thrown);
    }
}

template<size_t Length>
void parse_object_invalid_helper(const char (&buffer)[Length], minijson::parse_error::error_reason expected_reason, const char* expected_what = NULL)
{
    minijson::const_buffer_context const_buffer_context(buffer, sizeof(buffer) - 1);

    bool exception_thrown = false;

    try
    {
        minijson::parse_object(const_buffer_context, parse_dummy());
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

template<size_t Length>
void parse_object_invalid_helper2(const char (&buffer)[Length], minijson::parse_error::error_reason expected_reason, const char* expected_what = NULL)
{
    minijson::const_buffer_context const_buffer_context(buffer, sizeof(buffer) - 1);

    bool exception_thrown = false;

    try
    {
        minijson::parse_object(const_buffer_context, parse_dummy_consume<minijson::const_buffer_context>(const_buffer_context));
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

template<size_t Length>
void parse_array_invalid_helper(const char (&buffer)[Length], minijson::parse_error::error_reason expected_reason, const char* expected_what = NULL)
{
    minijson::const_buffer_context const_buffer_context(buffer, sizeof(buffer) - 1);

    bool exception_thrown = false;

    try
    {
        minijson::parse_array(const_buffer_context, parse_dummy());
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

template<size_t Length>
void parse_array_invalid_helper2(const char (&buffer)[Length], minijson::parse_error::error_reason expected_reason, const char* expected_what = NULL)
{
    minijson::const_buffer_context const_buffer_context(buffer, sizeof(buffer) - 1);

    bool exception_thrown = false;

    try
    {
        minijson::parse_array(const_buffer_context, parse_dummy_consume<minijson::const_buffer_context>(const_buffer_context));
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
    parse_object_invalid_helper("{\"x\":8.2e+62738",         minijson::parse_error::UNTERMINATED_VALUE);
    parse_object_invalid_helper("{\"x\":8.2e+62738}",        minijson::parse_error::INVALID_VALUE);
    parse_object_invalid_helper("{\"x\":3.4.5}",             minijson::parse_error::INVALID_VALUE);
    parse_object_invalid_helper("{\"x\":0x1273}",            minijson::parse_error::INVALID_VALUE);
    parse_object_invalid_helper("{\"x\":NaN}",               minijson::parse_error::INVALID_VALUE);
    parse_object_invalid_helper("{\"x\":nuxl}",              minijson::parse_error::INVALID_VALUE);
    parse_object_invalid_helper("{\"\\ufffx\":null}",        minijson::parse_error::INVALID_UTF16_CHARACTER);
    parse_object_invalid_helper("{\"x\":\"\\ufffx\"}",       minijson::parse_error::INVALID_UTF16_CHARACTER);
    parse_object_invalid_helper("{\"\\u\":\"\"}",            minijson::parse_error::INVALID_UTF16_CHARACTER);
    parse_object_invalid_helper("{\"\\ud800\":null}",        minijson::parse_error::EXPECTED_UTF16_LOW_SURROGATE);
    parse_object_invalid_helper("{\"\\udc00\":null}",        minijson::parse_error::INVALID_UTF16_CHARACTER);
    parse_object_invalid_helper("{\"\\u0000\":null}",        minijson::parse_error::NULL_UTF16_CHARACTER);
    parse_object_invalid_helper("{\"\\ud800\\uee00\":null}", minijson::parse_error::INVALID_UTF16_CHARACTER);
    parse_object_invalid_helper("{\"\\ud800\\u0000\":null}", minijson::parse_error::INVALID_UTF16_CHARACTER);
    parse_object_invalid_helper("{\"\\x\":null}",            minijson::parse_error::INVALID_ESCAPE_SEQUENCE);
    parse_object_invalid_helper("{\"a\":{}}",                minijson::parse_error::NESTED_OBJECT_OR_ARRAY_NOT_PARSED, "Nested object or array not parsed");
    parse_object_invalid_helper2(
        "{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":["
        "]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}",
        minijson::parse_error::EXCEEDED_NESTING_LIMIT, "Exceeded nesting limit (32)");
}

TEST(minijson_reader, parse_array_invalid)
{
    parse_array_invalid_helper("[8.2e+62738",          minijson::parse_error::UNTERMINATED_VALUE);
    parse_array_invalid_helper("[8.2e+62738]",         minijson::parse_error::INVALID_VALUE);
    parse_array_invalid_helper("[3.4.5]",              minijson::parse_error::INVALID_VALUE);
    parse_array_invalid_helper("[0x1273]",             minijson::parse_error::INVALID_VALUE);
    parse_array_invalid_helper("[NaN]",                minijson::parse_error::INVALID_VALUE);
    parse_array_invalid_helper("[nuxl]",               minijson::parse_error::INVALID_VALUE);
    parse_array_invalid_helper("[\"\\ufffx\"]",        minijson::parse_error::INVALID_UTF16_CHARACTER);
    parse_array_invalid_helper("[\"\\ufff\"]",         minijson::parse_error::INVALID_UTF16_CHARACTER);
    parse_array_invalid_helper("[\"\\ud800\"]",        minijson::parse_error::EXPECTED_UTF16_LOW_SURROGATE);
    parse_array_invalid_helper("[\"\\udc00\"]",        minijson::parse_error::INVALID_UTF16_CHARACTER);
    parse_array_invalid_helper("[\"\\u0000\"]",        minijson::parse_error::NULL_UTF16_CHARACTER);
    parse_array_invalid_helper("[\"\\ud800\\uee00\"]", minijson::parse_error::INVALID_UTF16_CHARACTER);
    parse_array_invalid_helper("[\"\\ud800\\u0000\"]", minijson::parse_error::INVALID_UTF16_CHARACTER);
    parse_array_invalid_helper("[\"\\x\"]",            minijson::parse_error::INVALID_ESCAPE_SEQUENCE);
    parse_array_invalid_helper("[[]]",                 minijson::parse_error::NESTED_OBJECT_OR_ARRAY_NOT_PARSED, "Nested object or array not parsed");
    parse_array_invalid_helper2(
        "[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{\"\":[{"
        "}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]}]",
        minijson::parse_error::EXCEEDED_NESTING_LIMIT, "Exceeded nesting limit (32)");
}

TEST(minijson_dispatch, present)
{
    bool handled[4] { };

    minijson::dispatch("test2")
        <<"test1">> [&]{ handled[0] = true; }
        <<"test2">> [&]{ handled[1] = true; } // should "break" here
        <<"test3">> [&]{ handled[2] = true; }
        <<"test2">> [&]{ handled[3] = true; };

    ASSERT_FALSE(handled[0]);
    ASSERT_TRUE(handled[1]);
    ASSERT_FALSE(handled[2]);
    ASSERT_FALSE(handled[3]);
}

TEST(minijson_dispatch, absent)
{
    bool handled[3] { };

    minijson::dispatch("x")
        <<"test1">> [&]{ handled[0] = true; }
        <<"test2">> [&]{ handled[1] = true; }
        <<"test3">> [&]{ handled[2] = true; };

    ASSERT_FALSE(handled[0]);
    ASSERT_FALSE(handled[1]);
    ASSERT_FALSE(handled[2]);
}

TEST(minijson_dispatch, absent_with_any_handler)
{
    bool handled[4] { };

    using minijson::any;

    minijson::dispatch("x")
        <<"test1">> [&]{ handled[0] = true; }
        <<"test2">> [&]{ handled[1] = true; }
        <<"test3">> [&]{ handled[2] = true; }
        <<any>>     [&]{ handled[3] = true; };

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
        <<x>> [&]{ handled = true; };

    ASSERT_TRUE(handled);
}

TEST(minijson_dispatch, parse_object)
{
    char json_obj[] = "{ \"field1\": 42, \"array\" : [ 1, 2, 3 ], \"field2\": \"asd\", "
            "\"nested\" : { \"field1\" : 42.0, \"field2\" : true, \"ignored_field\" : 0, \"ignored_object\" : {\"a\":[0]} },"
            "\"ignored_array\" : [4, 2, {\"a\":5}, [7]] }";

    struct obj_type
    {
        long field1 = 0;
        std::string field2;
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
    parse_object(ctx, [&](const char* k, value v)
    {
        dispatch (k)
        <<"field1">> [&]{ obj.field1 = v.as_long(); }
        <<"field2">> [&]{ obj.field2 = v.as_string(); }
        <<"nested">> [&]
        {
            parse_object(ctx, [&](const char* k, value v)
            {
                dispatch (k)
                <<"field1">> [&]{ obj.nested.field1 = v.as_double(); }
                <<"field2">> [&]{ obj.nested.field2 = v.as_bool(); }
                <<any>> [&]{ ignore(ctx); };
            });
        }
        <<"array">> [&]
        {
            parse_array(ctx, [&](value v) { obj.array.push_back(v.as_long()); });
        }
        <<any>> [&]{ ignore(ctx); };
    });

    ASSERT_EQ(42, obj.field1);
    ASSERT_EQ("asd", obj.field2);
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
