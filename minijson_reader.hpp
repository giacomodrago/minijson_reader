#ifndef MINIJSON_READER_H
#define MINIJSON_READER_H

#include <array>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <istream>
#include <list>
#include <stdexcept>
#include <string_view>
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

class buffer_context_base : public context_base
{
protected:

    const char* const m_read_buffer;
    char* const m_write_buffer;
    std::size_t m_length;
    std::size_t m_read_offset = 0;
    std::size_t m_write_offset = 0;
    const char* m_current_token = m_write_buffer;

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

    void start_new_token() noexcept
    {
        m_current_token = m_write_buffer + m_write_offset;
    }

    void write(const char c) noexcept
    {
        if (m_write_offset >= m_read_offset)
        {
            // This is VERY bad.
            // If we reach this line, then either the library contains a most
            // serious bug, or the memory is hopelessly corrupted. Better to
            // fail fast and get a crash dump. If this happens and you can
            // prove it's not the client's fault, please do file a bug report.
            std::abort(); // LCOV_EXCL_LINE
        }

        m_write_buffer[m_write_offset++] = c;
    }

    const char* current_token() const noexcept
    {
        return m_current_token;
    }

    std::size_t current_token_length() const noexcept
    {
        return m_write_buffer + m_write_offset - m_current_token;
    }
}; // class buffer_context_base

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
    std::list<std::vector<char>> m_tokens;

public:

    explicit istream_context(std::istream& stream)
    : m_stream(stream)
    {
    }

    char read()
    {
        const char c = m_stream.get();

        if (m_stream)
        {
            ++m_read_offset;

            return c;
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

    void start_new_token()
    {
        m_tokens.emplace_back();
    }

    void write(const char c)
    {
        m_tokens.back().push_back(c);
    }

    // This method to retrieve the address of the current token MUST be called
    // AFTER all the calls to write() for the current current token have been
    // performed
    const char* current_token() const noexcept
    {
        const std::vector<char>& token = m_tokens.back();

        return !token.empty() ? token.data() : nullptr;
    }

    std::size_t current_token_length() const noexcept
    {
        return m_tokens.back().size();
    }
}; // class istream_context

namespace detail
{

template<typename Context>
class token_writer final
{
private:

    Context& m_context;

public:

    explicit token_writer(Context& context) noexcept
    : m_context(context)
    {
        m_context.start_new_token();
    }

    void write(const char c) noexcept(noexcept(m_context.write(c)))
    {
        m_context.write(c);
    }

    std::string_view finalize() noexcept(noexcept(m_context.write(0)))
    {
        // Get the length of the token
        const std::size_t length = m_context.current_token_length();

        // Write a null terminator. This is not strictly required, but brings
        // some extra safety at negligible cost.
        m_context.write(0);

        return {m_context.current_token(), length};
    }
}; // class token_writer

} // namespace detail

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
        EXPECTED_CLOSING_QUOTE,
        INVALID_VALUE,
        UNTERMINATED_VALUE,
        EXPECTED_OPENING_BRACKET,
        EXPECTED_COLON,
        EXPECTED_COMMA_OR_CLOSING_BRACKET,
        NESTED_OBJECT_OR_ARRAY_NOT_PARSED,
        EXCEEDED_NESTING_LIMIT,
        NULL_UTF16_CHARACTER
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
        case EXPECTED_CLOSING_QUOTE:
            return "Expected closing quote";
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
        }

        return ""; // to suppress compiler warnings -- LCOV_EXCL_LINE
    }
}; // class parse_error

namespace detail
{

// this exception is not to be propagated outside minijson
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
            // since the high code unit is not a surrogate, the low code unit
            // should be zero
            throw encoding_error();
        }

        result = high;
    }
    else
    {
        if (high > 0xDBFF) // we already know high >= 0xD800
        {
            // the high surrogate is not within the expected range
            throw encoding_error();
        }

        if (low < 0xDC00 || low > 0xDFFF)
        {
            // the low surrogate is not within the expected range
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

    if      (utf32_char <= 0x00007F)
    {
        result[0] = utf32_char;
    }
    else if (utf32_char <= 0x0007FF)
    {
        result[0] = 0xC0 | ((utf32_char & (0x1F <<  6)) >>  6);
        result[1] = 0x80 | ((utf32_char & (0x3F      ))      );
    }
    else if (utf32_char <= 0x00FFFF)
    {
        result[0] = 0xE0 | ((utf32_char & (0x0F << 12)) >> 12);
        result[1] = 0x80 | ((utf32_char & (0x3F <<  6)) >>  6);
        result[2] = 0x80 | ((utf32_char & (0x3F      ))      );
    }
    else if (utf32_char <= 0x1FFFFF)
    {
        result[0] = 0xF0 | ((utf32_char & (0x07 << 18)) >> 18);
        result[1] = 0x80 | ((utf32_char & (0x3F << 12)) >> 12);
        result[2] = 0x80 | ((utf32_char & (0x3F <<  6)) >>  6);
        result[3] = 0x80 | ((utf32_char & (0x3F      ))      );
    }
    else
    {
        // invalid code unit
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

// this exception is not to be propagated outside minijson
struct number_parse_error
{
};

inline long parse_long(const std::string_view token, const int base = 10)
{
    long result;

    const auto [parse_end_ptr, error] =
        std::from_chars(token.begin(), token.end(), result, base);
    if (parse_end_ptr != token.end() || error != std::errc())
    {
        // We could not parse the whole string as an integer or the number is
        // out of range
        throw number_parse_error();
    }

    return result;
}

inline double parse_double(const std::string_view token)
{
    // Perform a check on the characters to make sure we reject NaN, INF, and
    // whatever other special sequence JSON does not allow but std::from_chars()
    // has to support
    for (const char c : token)
    {
        if (!std::isdigit(c) &&
            c != '+' && c != '-' && c != '.' && c != 'e' && c != 'E')
        {
            throw number_parse_error();
        }
    }

    double result;

    const auto [parse_end_ptr, error] =
        std::from_chars(token.begin(), token.end(), result);
    if (parse_end_ptr != token.end() || error != std::errc())
    {
        // We could not parse the whole string as a floating point number
        // or the number is out of range
        throw number_parse_error();
    }

    return result;
}

inline constexpr std::size_t UTF16_ESCAPE_SEQ_LENGTH = 4;

inline std::uint16_t parse_utf16_escape_sequence(const char* const seq)
{
    for (std::size_t i = 0; i < UTF16_ESCAPE_SEQ_LENGTH; ++i)
    {
        if (!std::isxdigit(seq[i]))
        {
            throw encoding_error();
        }
    }

    return static_cast<std::uint16_t>(parse_long(seq, 16));
}

template<typename Context>
void write_utf8_char(
    token_writer<Context>& writer,
    const std::array<std::uint8_t, 4>& c)
{
    for (std::size_t i = 0; i < c.size(); ++i)
    {
        const char byte = c[i];
        if (i > 0 && byte == 0)
        {
            break;
        }

        writer.write(byte);
    }
}

template<typename Context>
std::string_view read_quoted_string(
    Context& context,
    const bool skip_opening_quote = false)
{
    token_writer writer(context);

    enum
    {
        OPENING_QUOTE,
        CHARACTER,
        ESCAPE_SEQUENCE,
        UTF16_SEQUENCE,
        CLOSED
    } state = (skip_opening_quote) ? CHARACTER : OPENING_QUOTE;

    bool empty = true;
    char utf16_seq[UTF16_ESCAPE_SEQ_LENGTH + 1] = { 0 };
    std::size_t utf16_seq_offset = 0;
    std::uint16_t high_surrogate = 0;

    char c;

    while (state != CLOSED && (c = context.read()) != 0)
    {
        empty = false;

        switch (state)
        {
        case OPENING_QUOTE:

            if (c != '"')
            {
                throw parse_error(context, parse_error::EXPECTED_OPENING_QUOTE);
            }
            state = CHARACTER;

            break;

        case CHARACTER:

            if (c == '\\')
            {
                state = ESCAPE_SEQUENCE;
            }
            else if (high_surrogate != 0)
            {
                throw parse_error(
                    context, parse_error::EXPECTED_UTF16_LOW_SURROGATE);
            }
            else if (c == '"')
            {
                state = CLOSED;
            }
            else
            {
                writer.write(c);
            }

            break;

        case ESCAPE_SEQUENCE:

            state = CHARACTER;

            switch (c)
            {
            case '"':
                writer.write('"');
                break;
            case '\\':
                writer.write('\\');
                break;
            case '/':
                writer.write('/');
                break;
            case 'b':
                writer.write('\b');
                break;
            case 'f':
                writer.write('\f');
                break;
            case 'n':
                writer.write('\n');
                break;
            case 'r':
                writer.write('\r');
                break;
            case 't':
                writer.write('\t');
                break;
            case 'u':
                state = UTF16_SEQUENCE;
                break;
            default:
                throw parse_error(
                    context, parse_error::INVALID_ESCAPE_SEQUENCE);
            }

            break;

        case UTF16_SEQUENCE:

            utf16_seq[utf16_seq_offset++] = c;

            if (utf16_seq_offset == sizeof(utf16_seq) - 1)
            {
                try
                {
                    const std::uint16_t code_unit =
                        parse_utf16_escape_sequence(utf16_seq);

                    if (code_unit == 0 && high_surrogate == 0)
                    {
                        throw parse_error(
                            context, parse_error::NULL_UTF16_CHARACTER);
                    }

                    if (high_surrogate != 0)
                    {
                        // We were waiting for the low surrogate
                        // (that now is code_unit)
                        write_utf8_char(
                            writer,
                            utf16_to_utf8(high_surrogate, code_unit));
                        high_surrogate = 0;
                    }
                    else if (code_unit >= 0xD800 && code_unit <= 0xDBFF)
                    {
                        high_surrogate = code_unit;
                    }
                    else
                    {
                        write_utf8_char(writer, utf16_to_utf8(code_unit, 0));
                    }
                }
                catch (const encoding_error&)
                {
                    throw parse_error(
                        context, parse_error::INVALID_UTF16_CHARACTER);
                }

                utf16_seq_offset = 0;

                state = CHARACTER;
            }

            break;

        case CLOSED: // to silence compiler warnings

            throw std::runtime_error(
                "[minijson_reader] this line should never be reached, "
                "please file a bug report"); // LCOV_EXCL_LINE
        }
    }

    if (empty && !skip_opening_quote)
    {
        throw parse_error(context, parse_error::EXPECTED_OPENING_QUOTE);
    }
    else if (state != CLOSED)
    {
        throw parse_error(context, parse_error::EXPECTED_CLOSING_QUOTE);
    }

    return writer.finalize();
}

// Reads primitive values that are not between quotes (null, bools and numbers).
// Returns the value in raw text form and its termination character.
template<typename Context>
std::pair<std::string_view, char>
read_unquoted_value(Context& context, const char first_char = 0)
{
    token_writer writer(context);

    if (first_char != 0)
    {
        writer.write(first_char);
    }

    char c;

    while (
        (c = context.read()) != 0 && c != ',' && c != '}' && c != ']' &&
        !std::isspace(c))
    {
        writer.write(c);
    }

    if (c == 0)
    {
        throw parse_error(context, parse_error::UNTERMINATED_VALUE);
    }

    return {writer.finalize(), c};
}

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

class value final
{
private:

    value_type m_type = Null;
    std::string_view m_token = "";
    long m_long_value = 0;
    bool m_long_available = false;
    double m_double_value = 0.0;
    bool m_double_available = false;

public:

    explicit value() noexcept = default;

    explicit value(const value_type type) noexcept
    : m_type(type)
    {
    }

    explicit value(
        const value_type type,
        const std::string_view token) noexcept
    : m_type(type)
    , m_token(token)
    {
    }

    explicit value(
        const value_type type,
        const std::string_view token,
        const long long_value,
        const bool long_available,
        const double double_value,
        const bool double_available) noexcept
    : m_type(type)
    , m_token(token)
    , m_long_value(long_value)
    , m_long_available(long_available)
    , m_double_value(double_value)
    , m_double_available(double_available)
    {
    }

    value_type type() const noexcept
    {
        return m_type;
    }

    std::string_view as_string() const noexcept
    {
        return m_token;
    }

    long as_long() const noexcept
    {
        return m_long_value;
    }

    bool long_available() const noexcept
    {
        return m_long_available;
    }

    bool as_bool() const noexcept
    {
        return static_cast<bool>(m_long_value);
    }

    double as_double() const noexcept
    {
        return m_double_value;
    }

    bool double_available() const noexcept
    {
        return m_double_available;
    }
}; // class value

namespace detail
{

// Parses primitive values that are not between quotes (null, bools and numbers)
template<typename Context>
value parse_unquoted_value(
    const Context& context,
    const std::string_view token)
{
    if (token == "true")
    {
        return value(Boolean, token, 1, true, 1.0, true);
    }
    else if (token == "false")
    {
        return value(Boolean, token, 0, true, 0.0, true);
    }
    else if (token == "null")
    {
        return value(Null, token);
    }
    else // numbers
    {
        long long_value = 0;
        double double_value = 0.0;
        bool long_available = false;
        bool double_available = false;

        try
        {
            long_value = parse_long(token);
            long_available = true;
            double_value = long_value;
            double_available = true;
        }
        catch (const number_parse_error&)
        {
            try
            {
                double_value = parse_double(token);
                double_available = true;
            }
            catch (const number_parse_error&)
            {
                throw parse_error(context, parse_error::INVALID_VALUE);
            }
        }

        return value(
            Number,
            token,
            long_value,
            long_available,
            double_value,
            double_available);
    }
}

// Reads a value. Returns the parsed value and its termination character.
template<typename Context>
std::pair<value, char> read_value(Context& context, const char first_char)
{
    if (first_char == '{') // object
    {
        return {value(Object), 0};
    }
    else if (first_char == '[') // array
    {
        return {value(Array), 0};
    }
    else if (first_char == '"') // quoted string
    {
        return {value(String, read_quoted_string(context, true)), 0};
    }
    else // unquoted value
    {
        const auto [token, ending_char] =
            read_unquoted_value(context, first_char);

        return {parse_unquoted_value(context, token), ending_char};
    }
}

template<typename Context>
void parse_init_helper(
    const Context& context,
    char& c,
    bool& must_read) noexcept
{
    switch (context.nested_status())
    {
    case Context::NESTED_STATUS_NONE:
        c = 0;
        must_read = true;
        break;
    case Context::NESTED_STATUS_OBJECT:
        c = '{';
        must_read = false;
        break;
    case Context::NESTED_STATUS_ARRAY:
        c = '[';
        must_read = false;
        break;
    }
}

template<typename Context>
value parse_value_helper(Context& context, char& c, bool& must_read)
{
    const std::pair<value, char> read_value_result =
        detail::read_value(context, c);

    const value& v = read_value_result.first;

    if (v.type() == Object)
    {
        context.begin_nested(Context::NESTED_STATUS_OBJECT);
    }
    else if (v.type() == Array)
    {
        context.begin_nested(Context::NESTED_STATUS_ARRAY);
    }
    else if (v.type() != String)
    {
        c = read_value_result.second;
        must_read = false;
    }

    return v;
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

    parse_init_helper(context, c, must_read);
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

        if (std::isspace(c)) // skip whitespace
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
            field_name = detail::read_quoted_string(context, true);
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
            handler(field_name, parse_value_helper(context, c, must_read));
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

        case END:

            throw std::runtime_error(
                "[minijson_reader] this line should never be reached, "
                "please file a bug report"); // LCOV_EXCL_LINE
        }

        if (c == 0)
        {
            throw std::runtime_error(
                "[minijson_reader] this line should never be reached, "
                "please file a bug report"); // LCOV_EXCL_LINE
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

    parse_init_helper(context, c, must_read);
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

        if (std::isspace(c)) // skip whitespace
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
            handler(parse_value_helper(context, c, must_read));
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

        case END:

            throw std::runtime_error(
                "[minijson_reader] this line should never be reached, "
                "please file a bug report"); // LCOV_EXCL_LINE
        }

        if (c == 0)
        {
            throw std::runtime_error(
                "[minijson_reader] this line should never be reached, "
                "please file a bug report"); // LCOV_EXCL_LINE
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

    void operator()(std::string_view, const value&) const
    {
        (*this)();
    }

    void operator()(const value&) const
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
