#ifndef MINIJSON_READER_H
#define MINIJSON_READER_H

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <istream>
#include <list>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <ostream>
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

    enum context_nested_status
    {
        NESTED_STATUS_NONE,
        NESTED_STATUS_OBJECT,
        NESTED_STATUS_ARRAY
    };

private:

    context_nested_status m_nested_status = NESTED_STATUS_NONE;
    std::size_t m_nesting_level = 0;

public:

    explicit context_base() noexcept = default;
    context_base(const context_base&) = delete;
    context_base(context_base&&) = delete;
    context_base& operator=(const context_base&) = delete;
    context_base& operator=(context_base&&) = delete;

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
}; // class context_base

// Base for context classes backed by a buffer
class buffer_context_base : public context_base
{
protected:

    const char* const m_read_buffer;
    char* const m_write_buffer;
    std::size_t m_length;
    std::size_t m_read_offset = 0;
    std::size_t m_write_offset = 0;
    const char* m_current_literal = m_write_buffer;

    explicit buffer_context_base(
        const char* const read_buffer,
        char* const write_buffer,
        const std::size_t length) noexcept
    : m_read_buffer(read_buffer)
    , m_write_buffer(write_buffer)
    , m_length(length)
    {
    }

public:

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
}; // class buffer_context_base

// Utility class used throughout the library to read JSON literals (strings,
// bools, nulls, numbers) from a context and write them back into the context
// after applying the necessary transformations (e.g. escape sequences).
template<typename Context>
class literal_io final
{
private:

    Context& m_context;

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

    ~const_buffer_context() noexcept
    {
        delete[] m_write_buffer;
    }
}; // class const_buffer_context

class istream_context final : public detail::context_base
{
private:

    std::istream& m_stream;
    std::size_t m_read_offset = 0;
    std::list<std::vector<char>> m_literals;

public:

    explicit istream_context(std::istream& stream)
    : m_stream(stream)
    {
    }

    char read()
    {
        const int c = m_stream.get();

        if (m_stream)
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
        m_literals.emplace_back();
    }

    void write(const char c)
    {
        m_literals.back().push_back(c);
    }

    // This method to retrieve the address of the current literal MUST be called
    // AFTER all the calls to write() for the current current literal have been
    // performed
    const char* current_literal() const noexcept
    {
        const std::vector<char>& literal = m_literals.back();

        return !literal.empty() ? literal.data() : nullptr;
    }

    std::size_t current_literal_length() const noexcept
    {
        return m_literals.back().size();
    }
}; // class istream_context

class parse_error : public std::exception
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
    };

private:

    std::size_t m_offset;
    error_reason m_reason;

    template<typename Context>
    static std::size_t get_offset(const Context& context) noexcept
    {
        const std::size_t read_offset = context.read_offset();

        return (read_offset != 0) ? (read_offset - 1) : 0;
    }

public:

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
        }

        return ""; // to suppress compiler warnings -- LCOV_EXCL_LINE
    }
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
    }

    return false;
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
    }

    return false;
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

// forward declaration
template<typename T>
struct as;

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

class bad_value_cast : public std::invalid_argument
{
public:

    using std::invalid_argument::invalid_argument;
};

class value final
{
    template<typename T> friend struct detail::as;

private:

    value_type m_type = Null;
    std::string_view m_raw_value = "null";

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
        return detail::as<T>()(*this);
    }
}; // class value

namespace detail
{

// Trick to prevent static_assert() from always going off (see as_impl() below)
template<typename>
inline constexpr bool type_dependent_false = false;

template<typename T>
T as_impl(const value_type type, const std::string_view raw_value)
{
    // Here we can assume that type is not Null: that was already checked
    // by as<T> or its partial specialization as<std::optional<T>>

    if (type == Object)
    {
        throw bad_value_cast(
            "cannot call value::as<T>() on values of type Object: "
            "you have to call parse_object() on the same context");
    }

    if (type == Array)
    {
        throw bad_value_cast(
            "cannot call value::as<T>() on values of type Array: "
            "you have to call parse_array() on the same context");
    }

    if constexpr (std::is_same_v<T, std::string_view>)
    {
        if (type != String)
        {
            throw bad_value_cast("value::as<T>(): value type is not String");
        }

        return raw_value;
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        if (type != Boolean)
        {
            throw bad_value_cast("value::as<T>(): value type is not Boolean");
        }

        // If this value comes from parse_object() or parse_array(),
        // as it should, we know that raw_value is either "true" or "false".
        // However, we do a paranoia check for emptiness.
        return !raw_value.empty() && raw_value[0] == 't';
    }
    else if constexpr (std::is_arithmetic_v<T>)
    {
        if (type != Number)
        {
            throw bad_value_cast("value::as<T>(): value type is not Number");
        }

        T result {}; // value initialize to silence compiler warnings
        const auto begin = raw_value.data();
        const auto end = raw_value.data() + raw_value.size();
        const auto [parse_end, error] = std::from_chars(begin, end, result);
        if (parse_end != end || error != std::errc())
        {
            throw std::range_error("value::as<T>() could not parse the number");
        }
        return result;
    }
    else // if constexpr
    {
        // We need the predicate of static_assert() to depend on T, otherwise
        // the assert always goes off
        static_assert(
            type_dependent_false<T>,
            "value::as<T>(): T is not one of the supported types "
            "(std::string_view, bool, arithmetic types, plus all of the "
            "above wrapped in std::optional)");
    }
}

template<typename T>
struct as
{
    T operator()(const value v) const
    {
        if (v.m_type == Null)
        {
            throw bad_value_cast(
                "cannot call value::as<T>() on values of type Null: "
                "consider checking value::type() first, or use "
                "value::as<std::optional<T>>()");
        }

        return as_impl<T>(v.m_type, v.m_raw_value);
    }
}; // struct as

template<typename T>
struct as<std::optional<T>>
{
    std::optional<T> operator()(const value v) const
    {
        if (v.m_type == Null)
        {
            return std::nullopt;
        }

        return as_impl<T>(v.m_type, v.m_raw_value);
    }
}; // struct as<std::optional<T>>

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
void parse_object(Context& context, Handler&& handler)
{
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
            handler(field_name, detail::parse_value(context, c, must_read));
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
}

template<typename Context, typename Handler>
void parse_array(Context& context, Handler&& handler)
{
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
            handler(detail::parse_value(context, c, must_read));
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
}

namespace detail
{

class dispatch_rule; // forward declaration
class dispatch_rule_any; // forward declaration

struct dispatch_rule_any_tag
{
};

} // namespace detail

class dispatch
{
    friend class detail::dispatch_rule;
    friend class detail::dispatch_rule_any;

private:

    std::string_view m_field_name;
    bool m_handled = false;

public:

    explicit dispatch(const std::string_view field_name) noexcept
    : m_field_name(field_name)
    {
    }

    dispatch(const dispatch&) = delete;
    dispatch(dispatch&&) = delete;
    dispatch& operator=(const dispatch&) = delete;
    dispatch& operator=(dispatch&&) = delete;

    detail::dispatch_rule operator<<(std::string_view field_name) noexcept;

    detail::dispatch_rule_any
    operator<<(detail::dispatch_rule_any_tag) noexcept;
}; // class dispatch

namespace detail
{

class dispatch_rule
{
private:

    dispatch& m_dispatch;
    std::string_view m_field_name;

public:

    explicit dispatch_rule(
        dispatch& dispatch,
        const std::string_view field_name) noexcept
    : m_dispatch(dispatch)
    , m_field_name(field_name)
    {
    }

    dispatch_rule(const dispatch_rule&) = delete;
    dispatch_rule(dispatch_rule&&) noexcept = default;
    dispatch_rule& operator=(const dispatch_rule&) = delete;
    dispatch_rule& operator=(dispatch_rule&&) = delete;

    template<typename Handler>
    dispatch& operator>>(Handler&& handler) const
    {
        if (!m_dispatch.m_handled && m_dispatch.m_field_name == m_field_name)
        {
            handler();
            m_dispatch.m_handled = true;
        }

        return m_dispatch;
    }
}; // class dispatch_rule

class dispatch_rule_any
{
private:

    dispatch& m_dispatch;

public:

    explicit dispatch_rule_any(dispatch& dispatch) noexcept
    : m_dispatch(dispatch)
    {
    }

    dispatch_rule_any(const dispatch_rule_any&) = delete;
    dispatch_rule_any(dispatch_rule_any&&) noexcept = default;
    dispatch_rule_any& operator=(const dispatch_rule_any&) = delete;
    dispatch_rule_any& operator=(dispatch_rule_any&&) = delete;

    template<typename Handler>
    void operator>>(Handler&& handler) const
    {
        if (!m_dispatch.m_handled)
        {
            handler();
            m_dispatch.m_handled = true;
        }
    }
}; // class dispatch_rule_any

template<typename Context>
class ignore
{
private:

    Context& m_context;

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
}; // class ignore

} // namespace detail

inline detail::dispatch_rule
dispatch::operator<<(const std::string_view field_name) noexcept
{
    return detail::dispatch_rule(*this, field_name);
}

inline detail::dispatch_rule_any
dispatch::operator<<(detail::dispatch_rule_any_tag) noexcept
{
    return detail::dispatch_rule_any(*this);
}

inline constexpr const detail::dispatch_rule_any_tag any;

template<typename Context>
void ignore(Context& context)
{
    detail::ignore<Context> ignore(context);
    ignore();
}

} // namespace minijson

#endif // MINIJSON_READER_H

#undef MJR_STRINGIFY
#undef MJR_STRINGIFY_HELPER
