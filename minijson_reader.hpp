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

#ifndef MINIJSON_READER_H
#define MINIJSON_READER_H

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <forward_list>
#include <functional>
#include <istream>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#ifndef MJR_NESTING_LIMIT
#define MJR_NESTING_LIMIT 32
#endif

#define MJR_STRINGIFY(S) MJR_STRINGIFY_HELPER(S)
#define MJR_STRINGIFY_HELPER(S) #S

namespace minijson
{

namespace detail
{

// Base for all context classes
class context_base
{
public:
    context_base(const context_base&) = delete;
    context_base(context_base&&) = default;
    context_base& operator=(const context_base&) = delete;
    context_base& operator=(context_base&&) = default;

    enum context_nested_status
    {
        NESTED_STATUS_NONE,
        NESTED_STATUS_OBJECT,
        NESTED_STATUS_ARRAY
    };

    context_nested_status nested_status() const noexcept
    {
        return m_nested_status;
    }

    void begin_nested(const context_nested_status nested_status) noexcept
    {
        m_nested_status = nested_status;
        ++m_nesting_level;
    }

    void reset_nested_status() noexcept
    {
        m_nested_status = NESTED_STATUS_NONE;
    }

    void end_nested() noexcept
    {
        if (m_nesting_level > 0)
        {
            --m_nesting_level;
        }
    }

    std::size_t nesting_level() const noexcept
    {
        return m_nesting_level;
    }

protected:
    explicit context_base() noexcept = default;

private:
    context_nested_status m_nested_status = NESTED_STATUS_NONE;
    std::size_t m_nesting_level = 0;
}; // class context_base

// Base for context classes backed by a buffer
class buffer_context_base : public context_base
{
public:
    buffer_context_base(const buffer_context_base&) = delete;

    buffer_context_base(buffer_context_base&& other) noexcept
    : context_base(std::move(other))
    , m_read_buffer(other.m_read_buffer)
    , m_write_buffer(other.m_write_buffer)
    , m_length(other.m_length)
    , m_read_offset(other.m_read_offset)
    , m_write_offset(other.m_write_offset)
    , m_current_literal(other.m_current_literal)
    {
        other.m_read_buffer = nullptr;
        other.m_write_buffer = nullptr;
        other.m_length = 0;
        other.m_read_offset = 0;
        other.m_write_offset = 0;
        other.m_current_literal = nullptr;
    }

    buffer_context_base& operator=(const buffer_context_base&) = delete;

    buffer_context_base& operator=(buffer_context_base&& other) noexcept
    {
        if (this != &other)
        {
            auto moved(std::move(other));
            swap(moved);
        }
        return *this;
    }

    char read() noexcept
    {
        if (m_read_offset >= m_length)
        {
            return 0;
        }

        return m_read_buffer[m_read_offset++];
    }

    std::size_t read_offset() const noexcept
    {
        return m_read_offset;
    }

    void begin_literal() noexcept
    {
        m_current_literal = m_write_buffer + m_write_offset;
    }

    void write(const char c) noexcept
    {
        // LCOV_EXCL_START
        if (m_write_offset >= m_read_offset)
        {
            // This is VERY bad.
            // If we reach this line, then either the library contains a most
            // serious bug, or the memory is hopelessly corrupted. Better to
            // fail fast and get a crash dump. If this happens and you can
            // prove it's not the client's fault, please do file a bug report.
            std::abort();
        }
        // LCOV_EXCL_STOP

        m_write_buffer[m_write_offset++] = c;
    }

    const char* current_literal() const noexcept
    {
        return m_current_literal;
    }

    std::size_t current_literal_length() const noexcept
    {
        return m_write_buffer + m_write_offset - m_current_literal;
    }

    void swap(buffer_context_base& other) noexcept
    {
        using std::swap;
        swap(
            static_cast<context_base&>(*this),
            static_cast<context_base&>(other));
        swap(m_read_buffer, other.m_read_buffer);
        swap(m_write_buffer, other.m_write_buffer);
        swap(m_length, other.m_length);
        swap(m_read_offset, other.m_read_offset);
        swap(m_write_offset, other.m_write_offset);
        swap(m_current_literal, other.m_current_literal);
    }

    friend void swap(
        buffer_context_base& lhs,
        buffer_context_base& rhs) noexcept
    {
        lhs.swap(rhs);
    }

protected:
    explicit buffer_context_base(
        const char* const read_buffer,
        char* const write_buffer,
        const std::size_t length) noexcept
    : m_read_buffer(read_buffer)
    , m_write_buffer(write_buffer)
    , m_length(length)
    {
    }

    char* write_buffer() noexcept
    {
        return m_write_buffer;
    }

private:
    const char* m_read_buffer;
    char* m_write_buffer;
    std::size_t m_length;
    std::size_t m_read_offset = 0;
    std::size_t m_write_offset = 0;
    const char* m_current_literal = m_write_buffer;
}; // class buffer_context_base

// Utility class used throughout the library to read JSON literals (strings,
// bools, nulls, numbers) from a context and write them back into the context
// after applying the necessary transformations (e.g. escape sequences).
template<typename Context>
class literal_io final
{
public:
    explicit literal_io(Context& context) noexcept
    : m_context(context)
    {
        m_context.begin_literal();
    }

    Context& context() noexcept
    {
        return m_context;
    }

    char read() noexcept(noexcept(m_context.read()))
    {
        return m_context.read();
    }

    void write(const char c) noexcept(noexcept(m_context.write(c)))
    {
        m_context.write(c);
    }

    std::string_view finalize() noexcept(noexcept(m_context.write(0)))
    {
        // Get the length of the literal
        const std::size_t length = m_context.current_literal_length();

        // Write a null terminator. This is not strictly required, but brings
        // some extra safety at negligible cost.
        m_context.write(0);

        return {m_context.current_literal(), length};
    }

private:
    Context& m_context;
}; // class literal_io

} // namespace detail

class buffer_context final : public detail::buffer_context_base
{
public:
    explicit buffer_context(
        char* const buffer,
        const std::size_t length) noexcept
    : detail::buffer_context_base(buffer, buffer, length)
    {
    }

    buffer_context(const buffer_context&) = delete;
    buffer_context(buffer_context&&) = default;
    buffer_context& operator=(const buffer_context&) = delete;
    buffer_context& operator=(buffer_context&&) = default;
}; // class buffer_context

class const_buffer_context final : public detail::buffer_context_base
{
public:
    explicit const_buffer_context(
        const char* const buffer,
        const std::size_t length)
    : detail::buffer_context_base(buffer, new char[length], length)
    // don't worry about leaks, buffer_context_base can't throw
    {
    }

    const_buffer_context(const const_buffer_context&) = delete;
    const_buffer_context(const_buffer_context&&) = default;
    const_buffer_context& operator=(const const_buffer_context&) = delete;
    const_buffer_context& operator=(const_buffer_context&&) = default;

    ~const_buffer_context() noexcept
    {
        delete[] write_buffer();
    }
}; // class const_buffer_context

class istream_context final : public detail::context_base
{
public:
    explicit istream_context(std::istream& stream)
    : m_stream(&stream)
    {
    }

    istream_context(const istream_context&) = delete;
    istream_context(istream_context&&) = default;
    istream_context& operator=(const istream_context&) = delete;
    istream_context& operator=(istream_context&&) = default;

    char read()
    {
        const int c = m_stream->get();

        if (*m_stream)
        {
            ++m_read_offset;

            return static_cast<char>(c);
        }
        else
        {
            return 0;
        }
    }

    std::size_t read_offset() const noexcept
    {
        return m_read_offset;
    }

    void begin_literal()
    {
        m_literals.emplace_front();
    }

    void write(const char c)
    {
        m_literals.front().push_back(c);
    }

    // This method to retrieve the address of the current literal MUST be called
    // AFTER all the calls to write() for the current current literal have been
    // performed
    const char* current_literal() const noexcept
    {
        const std::vector<char>& literal = m_literals.front();

        return !literal.empty() ? literal.data() : nullptr;
    }

    std::size_t current_literal_length() const noexcept
    {
        return m_literals.front().size();
    }

private:
    std::istream* m_stream;
    std::size_t m_read_offset = 0;
    std::forward_list<std::vector<char>> m_literals;
}; // class istream_context

class parse_error final : public std::exception
{
public:
    enum error_reason
    {
        UNKNOWN,
        EXPECTED_OPENING_QUOTE,
        EXPECTED_UTF16_LOW_SURROGATE,
        INVALID_ESCAPE_SEQUENCE,
        INVALID_UTF16_CHARACTER,
        INVALID_VALUE,
        UNTERMINATED_VALUE,
        EXPECTED_OPENING_BRACKET,
        EXPECTED_COLON,
        EXPECTED_COMMA_OR_CLOSING_BRACKET,
        NESTED_OBJECT_OR_ARRAY_NOT_PARSED,
        EXCEEDED_NESTING_LIMIT,
        NULL_UTF16_CHARACTER,
        EXPECTED_VALUE,
        UNESCAPED_CONTROL_CHARACTER,
    };

    template<typename Context>
    explicit parse_error(
        const Context& context,
        const error_reason reason) noexcept
    : m_offset(get_offset(context))
    , m_reason(reason)
    {
    }

    std::size_t offset() const noexcept
    {
        return m_offset;
    }

    error_reason reason() const noexcept
    {
        return m_reason;
    }

    const char* what() const noexcept override
    {
        switch (m_reason)
        {
        case UNKNOWN:
            return "Unknown parse error";
        case EXPECTED_OPENING_QUOTE:
            return "Expected opening quote";
        case EXPECTED_UTF16_LOW_SURROGATE:
            return "Expected UTF-16 low surrogate";
        case INVALID_ESCAPE_SEQUENCE:
            return "Invalid escape sequence";
        case INVALID_UTF16_CHARACTER:
            return "Invalid UTF-16 character";
        case INVALID_VALUE:
            return "Invalid value";
        case UNTERMINATED_VALUE:
            return "Unterminated value";
        case EXPECTED_OPENING_BRACKET:
            return "Expected opening bracket";
        case EXPECTED_COLON:
            return "Expected colon";
        case EXPECTED_COMMA_OR_CLOSING_BRACKET:
            return "Expected comma or closing bracket";
        case NESTED_OBJECT_OR_ARRAY_NOT_PARSED:
            return "Nested object or array not parsed";
        case EXCEEDED_NESTING_LIMIT:
            return "Exceeded nesting limit ("
                MJR_STRINGIFY(MJR_NESTING_LIMIT) ")";
        case NULL_UTF16_CHARACTER:
            return "Null UTF-16 character";
        case EXPECTED_VALUE:
            return "Expected a value";
        case UNESCAPED_CONTROL_CHARACTER:
            return "Unescaped control character";
        }

        return ""; // to suppress compiler warnings -- LCOV_EXCL_LINE
    }

private:
    template<typename Context>
    static std::size_t get_offset(const Context& context) noexcept
    {
        const std::size_t read_offset = context.read_offset();

        return (read_offset != 0) ? (read_offset - 1) : 0;
    }

    std::size_t m_offset;
    error_reason m_reason;
}; // class parse_error

// LCOV_EXCL_START
inline std::ostream& operator<<(
    std::ostream& out,
    const parse_error::error_reason reason)
{
    switch (reason)
    {
    case parse_error::UNKNOWN:
        return out << "UNKNOWN";
    case parse_error::EXPECTED_OPENING_QUOTE:
        return out << "EXPECTED_OPENING_QUOTE";
    case parse_error::EXPECTED_UTF16_LOW_SURROGATE:
        return out << "EXPECTED_UTF16_LOW_SURROGATE";
    case parse_error::INVALID_ESCAPE_SEQUENCE:
        return out << "INVALID_ESCAPE_SEQUENCE";
    case parse_error::INVALID_UTF16_CHARACTER:
        return out << "INVALID_UTF16_CHARACTER";
    case parse_error::INVALID_VALUE:
        return out << "INVALID_VALUE";
    case parse_error::UNTERMINATED_VALUE:
        return out << "UNTERMINATED_VALUE";
    case parse_error::EXPECTED_OPENING_BRACKET:
        return out << "EXPECTED_OPENING_BRACKET";
    case parse_error::EXPECTED_COLON:
        return out << "EXPECTED_COLON";
    case parse_error::EXPECTED_COMMA_OR_CLOSING_BRACKET:
        return out << "EXPECTED_COMMA_OR_CLOSING_BRACKET";
    case parse_error::NESTED_OBJECT_OR_ARRAY_NOT_PARSED:
        return out << "NESTED_OBJECT_OR_ARRAY_NOT_PARSED";
    case parse_error::EXCEEDED_NESTING_LIMIT:
        return out << "EXCEEDED_NESTING_LIMIT";
    case parse_error::NULL_UTF16_CHARACTER:
        return out << "NULL_UTF16_CHARACTER";
    case parse_error::EXPECTED_VALUE:
        return out << "EXPECTED_VALUE";
    case parse_error::UNESCAPED_CONTROL_CHARACTER:
        return out << "UNESCAPED_CONTROL_CHARACTER";
    }

    return out << "UNKNOWN";
}
// LCOV_EXCL_STOP

namespace detail
{

// Tells whether a character is acceptable JSON whitespace to separate tokens
inline bool is_whitespace(const char c)
{
    switch (c)
    {
    case ' ':
    case '\n':
    case '\r':
    case '\t':
        return true;
    default:
        return false;
    }
}

// Tells whether a character can be used to terminate a value not enclosed in
// quotes (i.e. Null, Boolean and Number)
inline bool is_value_termination(const char c)
{
    switch (c)
    {
    case ',':
    case '}':
    case ']':
        return true;
    default:
        return is_whitespace(c);
    }
}

// There is an std::isdigit() but it's weird (takes an int among other things)
inline bool is_digit(const char c)
{
    switch (c)
    {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return true;
    default:
        return false;
    }
}

// There is an std::isxdigit() but it's weird (takes an int among other things)
inline bool is_hex_digit(const char c)
{
    switch (c)
    {
    case 'a':
    case 'A':
    case 'b':
    case 'B':
    case 'c':
    case 'C':
    case 'd':
    case 'D':
    case 'e':
    case 'E':
    case 'f':
    case 'F':
        return true;
    default:
        return is_digit(c);
    }
}

// This exception is thrown internally by the functions dealing with UTF-16
// escape sequences and is not propagated outside of the library
struct encoding_error
{
};

inline std::uint32_t utf16_to_utf32(std::uint16_t high, std::uint16_t low)
{
    std::uint32_t result;

    if (high <= 0xD7FF || high >= 0xE000)
    {
        if (low != 0)
        {
            // Since the high code unit is not a surrogate, the low code unit
            // should be zero
            throw encoding_error();
        }

        result = high;
    }
    else
    {
        if (high > 0xDBFF) // we already know high >= 0xD800
        {
            // The high surrogate is not within the expected range
            throw encoding_error();
        }

        if (low < 0xDC00 || low > 0xDFFF)
        {
            // The low surrogate is not within the expected range
            throw encoding_error();
        }

        high -= 0xD800;
        low -= 0xDC00;
        result = 0x010000 + ((high << 10) | low);
    }

    return result;
}

inline std::array<std::uint8_t, 4> utf32_to_utf8(const std::uint32_t utf32_char)
{
    std::array<std::uint8_t, 4> result {};

    // All the static_casts below are to please VS2022
    if (utf32_char <= 0x00007F)
    {
        std::get<0>(result) = static_cast<std::uint8_t>(utf32_char);
    }
    else if (utf32_char <= 0x0007FF)
    {
        std::get<0>(result) =
            static_cast<std::uint8_t>(
                0xC0 | ((utf32_char & (0x1F << 6)) >> 6));
        std::get<1>(result) =
            static_cast<std::uint8_t>(
                0x80 | (utf32_char & 0x3F));
    }
    else if (utf32_char <= 0x00FFFF)
    {
        std::get<0>(result) =
            static_cast<std::uint8_t>(
                0xE0 | ((utf32_char & (0x0F << 12)) >> 12));
        std::get<1>(result) =
            static_cast<std::uint8_t>(
                0x80 | ((utf32_char & (0x3F << 6)) >> 6));
        std::get<2>(result) =
            static_cast<std::uint8_t>(
                0x80 | (utf32_char & 0x3F));
    }
    else if (utf32_char <= 0x1FFFFF)
    {
        std::get<0>(result) =
            static_cast<std::uint8_t>(
                0xF0 | ((utf32_char & (0x07 << 18)) >> 18));
        std::get<1>(result) =
            static_cast<std::uint8_t>(
                0x80 | ((utf32_char & (0x3F << 12)) >> 12));
        std::get<2>(result) =
            static_cast<std::uint8_t>(
                0x80 | ((utf32_char & (0x3F << 6)) >> 6));
        std::get<3>(result) =
            static_cast<std::uint8_t>(
                0x80 | (utf32_char & 0x3F));
    }
    else
    {
        // Invalid code unit
        throw encoding_error();
    }

    return result;
}

inline std::array<std::uint8_t, 4> utf16_to_utf8(
    const std::uint16_t high,
    const std::uint16_t low)
{
    return utf32_to_utf8(utf16_to_utf32(high, low));
}

inline std::uint8_t parse_hex_digit(const char c)
{
    switch (c)
    {
    case '0':
        return 0x0;
    case '1':
        return 0x1;
    case '2':
        return 0x2;
    case '3':
        return 0x3;
    case '4':
        return 0x4;
    case '5':
        return 0x5;
    case '6':
        return 0x6;
    case '7':
        return 0x7;
    case '8':
        return 0x8;
    case '9':
        return 0x9;
    case 'a':
    case 'A':
        return 0xa;
    case 'b':
    case 'B':
        return 0xb;
    case 'c':
    case 'C':
        return 0xc;
    case 'd':
    case 'D':
        return 0xd;
    case 'e':
    case 'E':
        return 0xe;
    case 'f':
    case 'F':
        return 0xf;
    default:
        throw encoding_error();
    }
}

inline std::uint16_t parse_utf16_escape_sequence(
    const std::array<char, 4>& sequence)
{
    std::uint16_t result = 0;

    for (const char c : sequence)
    {
        result <<= 4;
        result |= parse_hex_digit(c);
    }

    return result;
}

template<typename Context>
void write_utf8_char(
    literal_io<Context>& literal_io,
    const std::array<std::uint8_t, 4>& c)
{
    literal_io.write(std::get<0>(c));

    for (std::size_t i = 1; i < c.size() && c[i]; ++i)
    {
        literal_io.write(c[i]);
    }
}

// Parses a string enclosed in quotes, dealing with escape sequences.
// Assumes the opening quote has already been parsed.
template<typename Context>
std::string_view parse_string(Context& context)
{
    literal_io literal_io(context);

    enum
    {
        CHARACTER,
        ESCAPE_SEQUENCE,
        UTF16_SEQUENCE,
        CLOSED
    } state = CHARACTER;

    std::array<char, 4> utf16_seq {};
    std::size_t utf16_seq_offset = 0;
    std::uint16_t high_surrogate = 0;

    char c;

    while (state != CLOSED && (c = literal_io.read()) != 0)
    {
        switch (state)
        {
        case CHARACTER:
            if (c == '\\')
            {
                state = ESCAPE_SEQUENCE;
            }
            else if (high_surrogate != 0)
            {
                throw parse_error(
                    context,
                    parse_error::EXPECTED_UTF16_LOW_SURROGATE);
            }
            else if (c == '"')
            {
                state = CLOSED;
            }
            else if (c >= 0x1 && c <= 0x1F)
            {
                throw parse_error(
                    context,
                    parse_error::UNESCAPED_CONTROL_CHARACTER);
            }
            else
            {
                literal_io.write(c);
            }
            break;

        case ESCAPE_SEQUENCE:
            state = CHARACTER;

            switch (c)
            {
            case '"':
                literal_io.write('"');
                break;
            case '\\':
                literal_io.write('\\');
                break;
            case '/':
                literal_io.write('/');
                break;
            case 'b':
                literal_io.write('\b');
                break;
            case 'f':
                literal_io.write('\f');
                break;
            case 'n':
                literal_io.write('\n');
                break;
            case 'r':
                literal_io.write('\r');
                break;
            case 't':
                literal_io.write('\t');
                break;
            case 'u':
                state = UTF16_SEQUENCE;
                break;
            default:
                throw parse_error(
                    context,
                    parse_error::INVALID_ESCAPE_SEQUENCE);
            }
            break;

        case UTF16_SEQUENCE:
            if (!is_hex_digit(c))
            {
                throw parse_error(
                    context,
                    parse_error::INVALID_ESCAPE_SEQUENCE);
            }

            utf16_seq[utf16_seq_offset++] = c;

            if (utf16_seq_offset == utf16_seq.size())
            {
                try
                {
                    const std::uint16_t code_unit =
                        parse_utf16_escape_sequence(utf16_seq);

                    if (code_unit == 0 && high_surrogate == 0)
                    {
                        throw parse_error(
                            context,
                            parse_error::NULL_UTF16_CHARACTER);
                    }

                    if (high_surrogate != 0)
                    {
                        // We were waiting for the low surrogate
                        // (that now is code_unit)
                        write_utf8_char(
                            literal_io,
                            utf16_to_utf8(high_surrogate, code_unit));
                        high_surrogate = 0;
                    }
                    else if (code_unit >= 0xD800 && code_unit <= 0xDBFF)
                    {
                        high_surrogate = code_unit;
                    }
                    else
                    {
                        write_utf8_char(
                            literal_io,
                            utf16_to_utf8(code_unit, 0));
                    }
                }
                catch (const encoding_error&)
                {
                    throw parse_error(
                        context,
                        parse_error::INVALID_UTF16_CHARACTER);
                }

                utf16_seq_offset = 0;

                state = CHARACTER;
            }
            break;

        // LCOV_EXCL_START
        case CLOSED:
            throw std::runtime_error(
                "[minijson_reader] this line should never be reached, "
                "please file a bug report");
        // LCOV_EXCL_STOP
        }
    }

    if (state != CLOSED)
    {
        throw parse_error(context, parse_error::UNTERMINATED_VALUE);
    }

    return literal_io.finalize();
}

// Type trait to check if T is a specialization of std::optional
template<typename T> struct is_std_optional
    : std::false_type {};
template<typename U> struct is_std_optional<std::optional<U>>
    : std::true_type {};

// Standard trick to prevent a static_assert() from always going off
// when it is inside a final "else" of a sequence of "if constexpr" blocks
template<typename> inline constexpr bool type_dependent_false = false;

} // namespace detail

enum value_type
{
    String,
    Number,
    Boolean,
    Object,
    Array,
    Null
};

class bad_value_cast final : public std::invalid_argument
{
public:
    using std::invalid_argument::invalid_argument;
};

template<typename T, typename Enable = void>
struct value_as;

class value final
{
public:
    explicit value() noexcept = default;

    explicit value(
        const value_type type,
        const std::string_view raw_value = "") noexcept
    : m_type(type)
    , m_raw_value(raw_value)
    {
    }

    value_type type() const noexcept
    {
        return m_type;
    }

    std::string_view raw() const
    {
        return m_raw_value;
    }

    template<typename T>
    T as() const
    {
        return value_as<T>()(*this);
    }

    template<typename T>
    T& to(T& dest) const
    {
        dest = as<T>();
        return dest;
    }

private:
    value_type m_type = Null;
    std::string_view m_raw_value = "null";
}; // class value

// Fallback behavior of value::as<T>() when no user-provided value_as
// specialization for T exists.
// This function is also meant to be called directly by the user in case they
// need to fall back to the default behavior inside their value_as
// specialization for T, for whatever reason.
template<typename T>
T value_as_default(const value v)
{
    if constexpr (detail::is_std_optional<T>())
    {
        using U = typename T::value_type;
        static_assert(
            !detail::is_std_optional<U>(),
            "it appears that T is std::optional<std::optional<...>> which is "
            "unlikely to be what you meant to do");

        if (v.type() == Null)
        {
            return std::nullopt;
        }

        return v.as<U>();
    }
    else // not an optional
    {
        switch (v.type())
        {
        case Null:
            throw bad_value_cast(
                "cannot call value::as<T>() on values of type Null: "
                "consider checking value::type() first, or use "
                "value::as<std::optional<T>>()");

        case Object:
            throw bad_value_cast(
                "cannot call value::as<T>() on values of type Object: "
                "you have to call parse_object() on the same context");

        case Array:
            throw bad_value_cast(
                "cannot call value::as<T>() on values of type Array: "
                "you have to call parse_array() on the same context");

        case String:
        case Boolean:
        case Number:
            break;
        }

        const std::string_view raw = v.raw();

        if constexpr (std::is_same_v<T, std::string_view>)
        {
            if (v.type() != String)
            {
                throw bad_value_cast(
                    "value::as<T>(): value type is not String");
            }

            return raw;
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            if (v.type() != Boolean)
            {
                throw bad_value_cast(
                    "value::as<T>(): value type is not Boolean");
            }

            // If this value comes from parse_object() or parse_array(),
            // as it should, we know that raw is either "true" or "false".
            // However, we do a paranoia check for emptiness.
            return !raw.empty() && raw[0] == 't';
        }
        else if constexpr (std::is_arithmetic_v<T>)
        {
            if (v.type() != Number)
            {
                throw bad_value_cast(
                    "value::as<T>(): value type is not Number");
            }

            T result {}; // value initialize to silence compiler warnings
            const auto begin = raw.data();
            const auto end = raw.data() + raw.size();
            const auto [parse_end, error] = std::from_chars(begin, end, result);
            if (parse_end != end || error != std::errc())
            {
                throw std::range_error(
                    "value::as<T>() could not parse the number");
            }
            return result;
        }
        else // unsupported type
        {
            // We need the predicate of static_assert() to depend on T,
            // otherwise the assert always goes off.
            static_assert(
                detail::type_dependent_false<T>,
                "value::as<T>(): T is not one of the supported types "
                "(std::string_view, bool, arithmetic types, plus all of the "
                "above wrapped in std::optional)");
        }
    }
}

template<typename T, typename Enable>
struct value_as final
{
    T operator()(const value v) const
    {
        return value_as_default<T>(v);
    }
};

namespace detail
{

// Convenience function to consume a verbatim sequence of characters
// in a value not enclosed in quotes (in practice, Null and Boolean).
// Returns the value termination character (e.g. ',').
template<typename Context, std::size_t Size>
char consume(
    literal_io<Context>& literal_io,
    const std::array<char, Size>& sequence)
{
    for (const char expected : sequence)
    {
        const char read = literal_io.read();
        if (read == 0)
        {
            throw parse_error(
                literal_io.context(),
                parse_error::UNTERMINATED_VALUE);
        }
        if (read != expected)
        {
            throw parse_error(
                literal_io.context(),
                parse_error::INVALID_VALUE);
        }
        literal_io.write(read);
    }

    const char read = literal_io.read();
    if (read == 0)
    {
        throw parse_error(
            literal_io.context(),
            parse_error::UNTERMINATED_VALUE);
    }
    if (!is_value_termination(read))
    {
        throw parse_error(
            literal_io.context(),
            parse_error::INVALID_VALUE);
    }
    return read;
}

// Parses primitive values that are not enclosed in quotes
// (i.e. Null, Boolean and Number).
// Returns the value and its termination character (e.g. ',').
template<typename Context>
std::tuple<value, char>
parse_unquoted_value(Context& context, const char first_char)
{
    literal_io literal_io(context);

    char c = first_char;

    // Cover "null", "true" and "false" cases
    switch (c)
    {
    case 'n': // "null"
        literal_io.write(c);
        c = consume(literal_io, std::array {'u', 'l', 'l'});
        return {value(Null, literal_io.finalize()), c};

    case 't': // "true"
        literal_io.write(c);
        c = consume(literal_io, std::array {'r', 'u', 'e'});
        return {value(Boolean, literal_io.finalize()), c};

    case 'f': // "false"
        literal_io.write(c);
        c = consume(literal_io, std::array {'a', 'l', 's', 'e'});
        return {value(Boolean, literal_io.finalize()), c};
    }

    // We are in the Number case.
    // Let's check that the number looks OK according to the JSON
    // specification, but let's not convert it yet
    // (that happens in value::as<T>() only as required).

    if (is_value_termination(c))
    {
        throw parse_error(context, parse_error::EXPECTED_VALUE);
    }

    enum
    {
        SIGN_OR_FIRST_DIGIT,
        FIRST_DIGIT,
        AFTER_LEADING_ZERO,
        INTEGRAL_PART,
        FRACTIONAL_PART_FIRST_DIGIT,
        FRACTIONAL_PART,
        EXPONENT_SIGN_OR_FIRST_DIGIT,
        EXPONENT_FIRST_DIGIT,
        EXPONENT,
    } state = SIGN_OR_FIRST_DIGIT;

    while (true)
    {
        if (c == 0)
        {
            throw parse_error(context, parse_error::UNTERMINATED_VALUE);
        }
        if (is_value_termination(c))
        {
            break;
        }

        switch (state)
        {
        case SIGN_OR_FIRST_DIGIT:
            if (c == '-') // leading plus sign not allowed
            {
                state = FIRST_DIGIT;
                break;
            }
            [[fallthrough]];
        case FIRST_DIGIT:
            if (c == '0')
            {
                // If zero is the first digit, then it must be the ONLY digit
                // of the integral part
                state = AFTER_LEADING_ZERO;
                break;
            }
            if (is_digit(c))
            {
                state = INTEGRAL_PART;
                break;
            }
            throw parse_error(context, parse_error::INVALID_VALUE);

        case INTEGRAL_PART:
            if (is_digit(c))
            {
                break;
            }
            [[fallthrough]];
        case AFTER_LEADING_ZERO:
            if (c == '.')
            {
                state = FRACTIONAL_PART_FIRST_DIGIT;
                break;
            }
            if (c == 'e' || c == 'E')
            {
                state = EXPONENT_SIGN_OR_FIRST_DIGIT;
                break;
            }
            throw parse_error(context, parse_error::INVALID_VALUE);

        case FRACTIONAL_PART:
            if (c == 'e' || c == 'E')
            {
                state = EXPONENT_SIGN_OR_FIRST_DIGIT;
                break;
            }
            [[fallthrough]];
        case FRACTIONAL_PART_FIRST_DIGIT:
            if (is_digit(c))
            {
                state = FRACTIONAL_PART;
                break;
            }
            throw parse_error(context, parse_error::INVALID_VALUE);

        case EXPONENT_SIGN_OR_FIRST_DIGIT:
            if (c == '+' || c == '-')
            {
                state = EXPONENT_FIRST_DIGIT;
                break;
            }
            [[fallthrough]];
        case EXPONENT_FIRST_DIGIT:
        case EXPONENT:
            if (is_digit(c))
            {
                state = EXPONENT;
                break;
            }
            throw parse_error(context, parse_error::INVALID_VALUE);
        }

        literal_io.write(c);
        c = literal_io.read();
    }

    switch (state)
    {
    case AFTER_LEADING_ZERO:
    case INTEGRAL_PART:
    case FRACTIONAL_PART:
    case EXPONENT:
        break;
    default:
        throw parse_error(context, parse_error::INVALID_VALUE);
    }

    return {value(Number, literal_io.finalize()), c};
}

// Helper function of parse_object() and parse_array() dealing with the opening
// bracket/brace of arrays and objects in presence of nesting
template<typename Context>
void parse_init(
    const Context& context,
    char& c,
    bool& must_read) noexcept
{
    switch (context.nested_status())
    {
    case Context::NESTED_STATUS_NONE:
        must_read = true;
        break;

    case Context::NESTED_STATUS_OBJECT:
        c = '{';
        // Since we are parsing a nested object, we already read an opening
        // brace. The main loop does not need to read a character from the
        // input.
        must_read = false;
        break;

    case Context::NESTED_STATUS_ARRAY:
        // Since we are parsing a nested array, we already read an opening
        // bracket. The main loop does not need to read a character from the
        // input.
        c = '[';
        must_read = false;
        break;
    }
}

// Helper function of parse_object() and parse_array() parsing JSON values.
// In case the value is a nested Object or Array, returns a placeholder value.
template<typename Context>
value parse_value(Context& context, char& c, bool& must_read)
{
    switch (c)
    {
    case '{':
        context.begin_nested(Context::NESTED_STATUS_OBJECT);
        return value(Object);

    case '[':
        context.begin_nested(Context::NESTED_STATUS_ARRAY);
        return value(Array);

    case '"':
        return value(String, parse_string(context));

    default: // Boolean, Null or Number
        value v;
        std::tie(v, c) = parse_unquoted_value(context, c);
        // c contains the character after the value, no need to read again
        // in the main loop
        must_read = false;
        return v;
    }
}

} // namespace detail

template<typename Context, typename Handler>
std::size_t parse_object(Context& context, Handler&& handler)
{
    const std::size_t read_offset = context.read_offset();

    const std::size_t nesting_level = context.nesting_level();
    if (nesting_level > MJR_NESTING_LIMIT)
    {
        throw parse_error(context, parse_error::EXCEEDED_NESTING_LIMIT);
    }

    char c = 0;
    bool must_read = false;

    detail::parse_init(context, c, must_read);
    context.reset_nested_status();

    enum
    {
        OPENING_BRACKET,
        FIELD_NAME_OR_CLOSING_BRACKET, // in case the object is empty
        FIELD_NAME,
        COLON,
        FIELD_VALUE,
        COMMA_OR_CLOSING_BRACKET,
        END
    } state = OPENING_BRACKET;

    std::string_view field_name = "";

    while (state != END)
    {
        if (context.nesting_level() != nesting_level)
        {
            throw parse_error(
                context, parse_error::NESTED_OBJECT_OR_ARRAY_NOT_PARSED);
        }

        if (must_read)
        {
            c = context.read();
        }

        must_read = true;

        if (detail::is_whitespace(c))
        {
            continue;
        }

        switch (state)
        {
        case OPENING_BRACKET:
            if (c != '{')
            {
                throw parse_error(
                    context, parse_error::EXPECTED_OPENING_BRACKET);
            }
            state = FIELD_NAME_OR_CLOSING_BRACKET;
            break;

        case FIELD_NAME_OR_CLOSING_BRACKET:
            if (c == '}')
            {
                state = END;
                break;
            }
            [[fallthrough]];

        case FIELD_NAME:
            if (c != '"')
            {
                throw parse_error(context, parse_error::EXPECTED_OPENING_QUOTE);
            }
            field_name = detail::parse_string(context);
            state = COLON;
            break;

        case COLON:
            if (c != ':')
            {
                throw parse_error(context, parse_error::EXPECTED_COLON);
            }
            state = FIELD_VALUE;
            break;

        case FIELD_VALUE:
            {
                const value v = detail::parse_value(context, c, must_read);

                // Try calling the handler with the context as the last argument
                if constexpr (
                    std::is_invocable_v<
                        decltype(handler),
                        decltype(field_name),
                        decltype(v),
                        decltype(context)>)
                {
                    std::invoke(handler, field_name, v, context);
                }
                else
                {
                    // Now try again without the context. Generate a compile
                    // error if it does not work.
                    std::invoke(handler, field_name, v);
                }
            }
            state = COMMA_OR_CLOSING_BRACKET;
            break;

        case COMMA_OR_CLOSING_BRACKET:
            if (c == ',')
            {
                state = FIELD_NAME;
            }
            else if (c == '}')
            {
                state = END;
            }
            else
            {
                throw parse_error(
                    context, parse_error::EXPECTED_COMMA_OR_CLOSING_BRACKET);
            }
            break;

        // LCOV_EXCL_START
        case END:
            throw std::runtime_error(
                "[minijson_reader] this line should never be reached, "
                "please file a bug report");
        // LCOV_EXCL_STOP
        }

        if (c == 0)
        {
            throw std::runtime_error( // LCOV_EXCL_LINE
                "[minijson_reader] this line should never be reached, "
                "please file a bug report");
        }
    }

    context.end_nested();

    return context.read_offset() - read_offset;
}

template<typename Context, typename Handler>
std::size_t parse_array(Context& context, Handler&& handler)
{
    const std::size_t read_offset = context.read_offset();

    const std::size_t nesting_level = context.nesting_level();
    if (nesting_level > MJR_NESTING_LIMIT)
    {
        throw parse_error(context, parse_error::EXCEEDED_NESTING_LIMIT);
    }

    char c = 0;
    bool must_read = false;

    detail::parse_init(context, c, must_read);
    context.reset_nested_status();

    enum
    {
        OPENING_BRACKET,
        VALUE_OR_CLOSING_BRACKET, // in case the array is empty
        VALUE,
        COMMA_OR_CLOSING_BRACKET,
        END
    } state = OPENING_BRACKET;

    while (state != END)
    {
        if (context.nesting_level() != nesting_level)
        {
            throw parse_error(
                context, parse_error::NESTED_OBJECT_OR_ARRAY_NOT_PARSED);
        }

        if (must_read)
        {
            c = context.read();
        }

        must_read = true;

        if (detail::is_whitespace(c))
        {
            continue;
        }

        switch (state)
        {
        case OPENING_BRACKET:
            if (c != '[')
            {
                throw parse_error(
                    context, parse_error::EXPECTED_OPENING_BRACKET);
            }
            state = VALUE_OR_CLOSING_BRACKET;
            break;

        case VALUE_OR_CLOSING_BRACKET:
            if (c == ']')
            {
                state = END;
                break;
            }
            [[fallthrough]];

        case VALUE:
            {
                const value v = detail::parse_value(context, c, must_read);

                // Try calling the handler with the context as the last argument
                if constexpr (
                    std::is_invocable_v<
                        decltype(handler),
                        decltype(v),
                        decltype(context)>)
                {
                    std::invoke(handler, v, context);
                }
                else
                {
                    // Now try again without the context. Generate a compile
                    // error if it does not work.
                    std::invoke(handler, v);
                }
            }
            state = COMMA_OR_CLOSING_BRACKET;
            break;

        case COMMA_OR_CLOSING_BRACKET:
            if (c == ',')
            {
                state = VALUE;
            }
            else if (c == ']')
            {
                state = END;
            }
            else
            {
                throw parse_error(
                    context, parse_error::EXPECTED_COMMA_OR_CLOSING_BRACKET);
            }
            break;

        // LCOV_EXCL_START
        case END:
            throw std::runtime_error(
                "[minijson_reader] this line should never be reached, "
                "please file a bug report");
        // LCOV_EXCL_STOP
        }

        if (c == 0)
        {
            throw std::runtime_error( // LCOV_EXCL_LINE
                "[minijson_reader] this line should never be reached, "
                "please file a bug report");
        }
    }

    context.end_nested();

    return context.read_offset() - read_offset;
}

namespace detail
{

template<typename Context>
class ignore final
{
public:
    explicit ignore(Context& context) noexcept
    : m_context(context)
    {
    }

    ignore(const ignore&) = delete;
    ignore(ignore&&) = delete;
    ignore& operator=(const ignore&) = delete;
    ignore& operator=(ignore&&) = delete;

    void operator()(std::string_view, value) const
    {
        (*this)();
    }

    void operator()(value) const
    {
        (*this)();
    }

    void operator()() const
    {
        switch (m_context.nested_status())
        {
        case Context::NESTED_STATUS_NONE:
            break;
        case Context::NESTED_STATUS_OBJECT:
            parse_object(m_context, *this);
            break;
        case Context::NESTED_STATUS_ARRAY:
            parse_array(m_context, *this);
            break;
        }
    }

private:
    Context& m_context;
}; // class ignore

// Base for unhandled_field_error and missing_field_error
class dispatcher_error_base : public std::exception
{
public:
    explicit dispatcher_error_base(const std::string_view field_name) noexcept
    : m_field_name_truncated_length(
        std::min(MAX_FIELD_NAME_LENGTH, field_name.size()))
    {
        std::copy_n(
            field_name.begin(),
            m_field_name_truncated_length,
            m_field_name_truncated.begin());
    }

    std::string_view field_name_truncated() const noexcept
    {
        return {m_field_name_truncated.data(), m_field_name_truncated_length};
    }

    inline static constexpr std::size_t MAX_FIELD_NAME_LENGTH = 55;

private:
    // The temptation of storing a the field name here as a std::string_view
    // is almost irresistible, but it would tie the validity of this exception
    // to that of the dispatcher or, even worse, the parsing context
    std::size_t m_field_name_truncated_length = 0;
    std::array<char, MAX_FIELD_NAME_LENGTH + 1> m_field_name_truncated {};
}; // class dispatcher_error_base

} // namespace detail

template<typename Context>
void ignore(Context& context)
{
    detail::ignore<Context> ignore(context);
    ignore();
}

namespace handlers
{

namespace detail
{

// Tags to declare handler traits
struct handler_tag {};
struct field_specific_handler_tag {};
struct required_field_handler_tag {};
struct ignore_handler_tag {};

// Base class for field-specific handlers
template<typename Functor, typename... Tag>
class field_specific_handler_base
    : public handler_tag
    , public field_specific_handler_tag
    , public Tag...
{
public:
    explicit field_specific_handler_base(
        const std::string_view field_name,
        Functor functor)
    : m_field_name(field_name)
    , m_functor(std::move(functor))
    {
    }

    std::string_view field_name() const noexcept
    {
        return m_field_name;
    }

    const Functor& functor() const noexcept
    {
        return m_functor;
    }

    template<typename Context, typename... Target>
    bool operator()(
        const std::string_view parsed_field_name,
        const value value,
        Context& context,
        Target&... target) const
    {
        if (parsed_field_name != m_field_name)
        {
            return false;
        }

        // Try calling the functor with the context as the last argument
        if constexpr (
            std::is_invocable_v<
                decltype(m_functor),
                decltype(target)...,
                decltype(value),
                decltype(context)>)
        {
            std::invoke(m_functor, target..., value, context);
        }
        else
        {
            // Now try again without the context. Generate a compile error if
            // it does not work.
            std::invoke(m_functor, target..., value);
        }

        return true;
    }

private:
    std::string_view m_field_name;
    Functor m_functor;
}; // class field_specific_handler_base

// Base class for non-field-specific handlers
template<typename Functor, typename... Tag>
class any_handler_base : public handler_tag, public Tag...
{
public:
    any_handler_base(Functor functor)
    : m_functor(std::move(functor))
    {
    }

    const Functor& functor() const noexcept
    {
        return m_functor;
    }

    template<typename Context, typename... Target>
    bool operator()(
        const std::string_view parsed_field_name,
        const value value,
        Context& context,
        Target&... target) const
    {
        // Try calling the functor with the context as the last argument
        if constexpr (
            std::is_invocable_v<
                decltype(m_functor),
                decltype(target)...,
                decltype(parsed_field_name),
                decltype(value),
                decltype(context)>)
        {
            return std::invoke(
                m_functor,
                target...,
                parsed_field_name,
                value,
                context);
        }
        else
        {
            // Now try again without the context. Generate a compile error if
            // it does not work.
            return std::invoke(m_functor, target..., parsed_field_name, value);
        }
    }

private:
    Functor m_functor;
}; // class any_handler_base

// Functor for all handlers which ignore the field
struct ignore_functor final
{
    template<typename... Args>
    bool operator()(Args&&... args) const
    {
        // The context is the last argument we are being passed, extract it
        const auto args_tuple =
            std::forward_as_tuple(std::forward<Args>(args)...);
        auto& context = std::get<sizeof...(args) - 1>(args_tuple);

        // Call ignore to discard the current value
        ignore(context);

        return true;
    }
}; // struct ignore_functor

} // namespace handlers::detail

template<typename Functor>
class handler final
    : public detail::field_specific_handler_base<
        Functor,
        detail::required_field_handler_tag>
{
public:
    handler(const std::string_view field_name, Functor functor)
    : detail::field_specific_handler_base<
        Functor,
        detail::required_field_handler_tag>(
            field_name,
            std::move(functor))
    {
    }
}; // class handler

template<typename Functor>
class optional_handler final
    : public detail::field_specific_handler_base<Functor>
{
public:
    optional_handler(const std::string_view field_name, Functor functor)
    : detail::field_specific_handler_base<Functor>(
        field_name,
        std::move(functor))
    {
    }
}; // class optional_handler

template<typename Functor>
class any_handler final : public detail::any_handler_base<Functor>
{
public:
    any_handler(Functor functor)
    : detail::any_handler_base<Functor>(std::move(functor))
    {
    }
}; // class any_handler

class ignore_handler final
    : public detail::field_specific_handler_base<
        detail::ignore_functor,
        detail::ignore_handler_tag>
{
public:
    ignore_handler(const std::string_view field_name)
    : detail::field_specific_handler_base<
        detail::ignore_functor,
        detail::ignore_handler_tag>(
            field_name,
            detail::ignore_functor())
    {
    }
}; // class ignore_handler

class ignore_any_handler final
    : public detail::any_handler_base<
        detail::ignore_functor,
        detail::ignore_handler_tag>
{
public:
    ignore_any_handler()
    : detail::any_handler_base<
        detail::ignore_functor,
        detail::ignore_handler_tag>(
            detail::ignore_functor())
    {
    }
}; // class ignore_any_handler

template<typename Handler, typename Enable = void>
struct traits;

template<typename Handler>
struct traits<
    Handler,
    std::enable_if_t<
        std::is_base_of_v<
            detail::handler_tag,
            std::remove_reference_t<Handler>>>> final
{
    static inline constexpr bool is_field_specific =
        std::is_base_of_v<
            detail::field_specific_handler_tag,
            std::remove_reference_t<Handler>>;

    static inline constexpr bool is_required_field =
        std::is_base_of_v<
            detail::required_field_handler_tag,
            std::remove_reference_t<Handler>>;

    static inline constexpr bool is_ignore =
        std::is_base_of_v<
            detail::ignore_handler_tag,
            std::remove_reference_t<Handler>>;
};

} // namespace handlers

class unhandled_field_error final : public detail::dispatcher_error_base
{
public:
    using detail::dispatcher_error_base::dispatcher_error_base;

    virtual const char* what() const noexcept override
    {
        return "a JSON field was not handled";
    }
}; // class unhandled_field_error

class missing_field_error final : public detail::dispatcher_error_base
{
public:
    using detail::dispatcher_error_base::dispatcher_error_base;

    virtual const char* what() const noexcept override
    {
        return "at least one required JSON field is missing";
    }
}; // class missing_field_error

template<typename Dispatcher, typename... Target>
class dispatcher_run final
{
public:
    dispatcher_run(Dispatcher& dispatcher, Target&... target) noexcept
    : m_dispatcher(dispatcher)
    , m_targets(target...)
    {
    }

    dispatcher_run(const dispatcher_run&) = delete;
    dispatcher_run(dispatcher_run&&) = default;
    dispatcher_run& operator=(const dispatcher_run&) = delete;
    dispatcher_run& operator=(dispatcher_run&&) = delete;

    template<typename Context>
    void operator()(
        const std::string_view parsed_field_name,
        const value value,
        Context& context)
    {
        // AFAIK, in pre-C++20 we are forced to resort to a helper function
        // rather than use a template lambda which would be more readable
        call_helper(
            parsed_field_name,
            value,
            context,
            std::make_index_sequence<Dispatcher::n_handlers>());
    }

    template<typename Inspector>
    void inspect(Inspector&& inspector) const
    {
        // AFAIK, in pre-C++20 we are forced to resort to a helper function
        // rather than use a template lambda which would be more readable
        inspect_helper(
            inspector,
            std::make_index_sequence<Dispatcher::n_handlers>());
    }

    void enforce_required() const
    {
        inspect(
            [](const auto& handler, const std::size_t handle_count)
            {
                if constexpr (
                    handlers::traits<decltype(handler)>::is_required_field)
                {
                    if (handle_count == 0)
                    {
                        throw missing_field_error(handler.field_name());
                    }
                }
            });
    }

private:
    // Helper of operator()() to get a sequence of indices
    template<typename Context, std::size_t... I>
    void call_helper(
        const std::string_view parsed_field_name,
        const value value,
        Context& context,
        std::index_sequence<I...>)
    {
        if (!(... || offer_to_handler<I>(parsed_field_name, value, context)))
        {
            throw unhandled_field_error(parsed_field_name);
        }
    }

    template<typename Inspector, std::size_t... I>
    void inspect_helper(Inspector& inspector, std::index_sequence<I...>) const
    {
        (..., std::invoke(
            inspector,
            std::get<I>(m_dispatcher.handlers()),
            std::get<I>(m_handle_count)));
    }

    // Offer this field to the I-th handler of the dispatcher.
    // If the handler chooses to handle it, then increment the I-th
    // m_handle_count entry and return true. Otherwise, return false.
    template<std::size_t I, typename Context>
    bool offer_to_handler(
        const std::string_view parsed_field_name,
        const value value,
        Context& context)
    {
        std::size_t& handle_count = std::get<I>(m_handle_count);
        auto& handler = std::get<I>(m_dispatcher.handlers());

        return std::apply(
            [&](auto&... target)
            {
                if (handler(parsed_field_name, value, context, target...))
                {
                    ++handle_count;
                    return true;
                }
                return false;
            },
            m_targets);
    }

    Dispatcher& m_dispatcher;
    std::tuple<Target&...> m_targets;

    // Tracks how many times the i-th handler decided to handle a field
    std::array<std::size_t, Dispatcher::n_handlers> m_handle_count {};
}; // class dispatcher_run

template<typename... Handler>
class dispatcher final
{
public:
    dispatcher(Handler... handler)
    : m_handlers {std::move(handler)...}
    {
    }

    template<typename Context, typename... Target>
    void run(Context& context, Target&... target) const
    {
        dispatcher_run run(*this, target...);
        minijson::parse_object(context, run);
        run.enforce_required();
    }

    const std::tuple<Handler...>& handlers() const noexcept
    {
        return m_handlers;
    }

    inline static constexpr std::size_t n_handlers = sizeof...(Handler);

private:
    std::tuple<Handler...> m_handlers;
}; // class dispatcher

} // namespace minijson

#endif // MINIJSON_READER_H

#undef MJR_STRINGIFY
#undef MJR_STRINGIFY_HELPER
