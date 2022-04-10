#ifndef MINIJSON_READER_H
#define MINIJSON_READER_H

#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <istream>
#include <list>
#include <stdexcept>
#include <string>
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

    context_nested_status m_nested_status;
    std::size_t m_nesting_level;

public:

    context_base() :
        m_nested_status(NESTED_STATUS_NONE),
        m_nesting_level(0)
    {
    }

    context_base(const context_base&) = delete;
    context_base(context_base&&) = delete;
    context_base& operator=(const context_base&) = delete;
    context_base& operator=(context_base&&) = delete;

    char nested_status() const
    {
        return m_nested_status;
    }

    void begin_nested(context_nested_status nested_status)
    {
        m_nested_status = nested_status;
        m_nesting_level++;
    }

    void reset_nested_status()
    {
        m_nested_status = NESTED_STATUS_NONE;
    }

    void end_nested()
    {
        if (m_nesting_level > 0)
        {
            m_nesting_level--;
        }
    }

    std::size_t nesting_level() const
    {
        return m_nesting_level;
    }
}; // class context_base

class buffer_context_base : public context_base
{
protected:

    const char* m_read_buffer;
    char* m_write_buffer;
    std::size_t m_length;
    std::size_t m_read_offset;
    std::size_t m_write_offset;
    const char* m_current_write_buffer;

    explicit buffer_context_base(const char* read_buffer, char* write_buffer, std::size_t length) :
        m_read_buffer(read_buffer),
        m_write_buffer(write_buffer),
        m_length(length),
        m_read_offset(0),
        m_write_offset(0),
        m_current_write_buffer(NULL)
    {
        new_write_buffer();
    }

public:

    char read()
    {
        if (m_read_offset >= m_length)
        {
            return 0;
        }

        return m_read_buffer[m_read_offset++];
    }

    std::size_t read_offset() const
    {
        return m_read_offset;
    }

    void new_write_buffer()
    {
        m_current_write_buffer = m_write_buffer + m_write_offset;
    }

    void write(char c)
    {
        if (m_write_offset >= m_read_offset)
        {
            throw std::runtime_error("Invalid write call, please file a bug report");
        }

        m_write_buffer[m_write_offset++] = c;
    }

    const char* write_buffer() const
    {
        return m_current_write_buffer;
    }
}; // class buffer_context_base

} // namespace detail

class buffer_context final : public detail::buffer_context_base
{
public:

    explicit buffer_context(char* buffer, std::size_t length) :
        detail::buffer_context_base(buffer, buffer, length)
    {
    }
}; // class buffer_context

class const_buffer_context final : public detail::buffer_context_base
{
public:

    explicit const_buffer_context(const char* buffer, std::size_t length) :
        detail::buffer_context_base(buffer, new char[length], length) // don't worry about leaks, buffer_context_base can't throw
    {
    }

    ~const_buffer_context()
    {
        delete[] m_write_buffer;
    }
}; // class const_buffer_context

class istream_context final : public detail::context_base
{
private:

    std::istream& m_stream;
    std::size_t m_read_offset;
    std::list<std::vector<char>> m_write_buffers;

public:

    explicit istream_context(std::istream& stream) :
        m_stream(stream),
        m_read_offset(0)
    {
        new_write_buffer();
    }

    char read()
    {
        const char c = m_stream.get();

        if (m_stream)
        {
            m_read_offset++;

            return c;
        }
        else
        {
            return 0;
        }
    }

    std::size_t read_offset() const
    {
        return m_read_offset;
    }

    void new_write_buffer()
    {
        m_write_buffers.push_back(std::vector<char>());
    }

    void write(char c)
    {
        m_write_buffers.back().push_back(c);
    }

    // This method to retrieve the address of the write buffer MUST be called
    // AFTER all the calls to write() for the current write buffer have been performed
    const char* write_buffer() const
    {
        return !m_write_buffers.back().empty() ? &m_write_buffers.back()[0] : NULL;
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
    static std::size_t get_offset(const Context& context)
    {
        const std::size_t read_offset = context.read_offset();

        return (read_offset != 0) ? (read_offset - 1) : 0;
    }

public:

    template<typename Context>
    explicit parse_error(const Context& context, error_reason reason) :
        m_offset(get_offset(context)),
        m_reason(reason)
    {
    }

    std::size_t offset() const
    {
        return m_offset;
    }

    error_reason reason() const
    {
        return m_reason;
    }

    const char* what() const throw()
    {
        switch (m_reason)
        {
        case UNKNOWN:                           return "Unknown parse error";
        case EXPECTED_OPENING_QUOTE:            return "Expected opening quote";
        case EXPECTED_UTF16_LOW_SURROGATE:      return "Expected UTF-16 low surrogate";
        case INVALID_ESCAPE_SEQUENCE:           return "Invalid escape sequence";
        case INVALID_UTF16_CHARACTER:           return "Invalid UTF-16 character";
        case EXPECTED_CLOSING_QUOTE:            return "Expected closing quote";
        case INVALID_VALUE:                     return "Invalid value";
        case UNTERMINATED_VALUE:                return "Unterminated value";
        case EXPECTED_OPENING_BRACKET:          return "Expected opening bracket";
        case EXPECTED_COLON:                    return "Expected colon";
        case EXPECTED_COMMA_OR_CLOSING_BRACKET: return "Expected comma or closing bracket";
        case NESTED_OBJECT_OR_ARRAY_NOT_PARSED: return "Nested object or array not parsed";
        case EXCEEDED_NESTING_LIMIT:            return "Exceeded nesting limit (" MJR_STRINGIFY(MJR_NESTING_LIMIT) ")";
        case NULL_UTF16_CHARACTER:              return "Null UTF-16 character";
        }

        return ""; // to suppress compiler warnings -- LCOV_EXCL_LINE
    }
}; // class parse_error

namespace detail
{

struct utf8_char
{
    std::uint8_t bytes[4];

    utf8_char()
    {
        // wanted use value-initialization, but couldn't because of a weird VS2013 warning
        std::fill_n(bytes, sizeof(bytes), 0);
    }

    explicit utf8_char(std::uint8_t b0, std::uint8_t b1, std::uint8_t b2, std::uint8_t b3)
    {
        bytes[0] = b0;
        bytes[1] = b1;
        bytes[2] = b2;
        bytes[3] = b3;
    }

    std::uint8_t& operator[](std::size_t i)
    {
        return bytes[i];
    }

    const std::uint8_t& operator[](std::size_t i) const
    {
        return bytes[i];
    }

    bool operator==(const utf8_char& other) const
    {
        return std::equal(bytes, bytes + sizeof(bytes), other.bytes);
    }

    bool operator!=(const utf8_char& other) const
    {
        return !operator==(other);
    }
}; // struct utf8_char

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
            // since the high code unit is not a surrogate, the low code unit should be zero
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

inline utf8_char utf32_to_utf8(std::uint32_t utf32_char)
{
    utf8_char result;

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

inline utf8_char utf16_to_utf8(std::uint16_t high, std::uint16_t low)
{
    return utf32_to_utf8(utf16_to_utf32(high, low));
}

// this exception is not to be propagated outside minijson
struct number_parse_error
{
};

inline long parse_long(const char* str, int base = 10)
{
    if ((str == NULL) || (*str == 0) || std::isspace(str[0])) // we don't accept empty strings or strings with leading spaces
    {
        throw number_parse_error();
    }

    int saved_errno = errno; // save errno
    errno = 0; // reset errno

    char* endptr;
    const long result = std::strtol(str, &endptr, base);

    std::swap(saved_errno, errno); // restore errno

    if (*endptr != 0) // we didn't consume the whole string
    {
        throw number_parse_error();
    }
    else if ((saved_errno == ERANGE) && ((result == LONG_MIN) || (result == LONG_MAX))) // overflow
    {
        throw number_parse_error();
    }

    return result;
}

inline double parse_double(const char* str)
{
    if ((str == NULL) || (*str == 0)) // we don't accept empty strings
    {
        throw number_parse_error();
    }

    // we perform this check to reject hex numbers (supported in C++11) and string with leading spaces
    for (const char* c = str; *c != 0; c++)
    {
        if (!(std::isdigit(*c) || (*c == '+') || (*c == '-') || (*c == '.') || (*c == 'e') || (*c == 'E')))
        {
            throw number_parse_error();
        }
    }

    int saved_errno = errno; // save errno
    errno = 0; // reset errno

    char* endptr;
    const double result = std::strtod(str, &endptr);

    std::swap(saved_errno, errno); // restore errno

    if (*endptr != 0) // we didn't consume the whole string
    {
        throw number_parse_error();
    }
    else if (saved_errno == ERANGE) // underflow or overflow
    {
        throw number_parse_error();
    }

    return result;
}

static const std::size_t UTF16_ESCAPE_SEQ_LENGTH = 4;

inline std::uint16_t parse_utf16_escape_sequence(const char* seq)
{
    for (std::size_t i = 0; i < UTF16_ESCAPE_SEQ_LENGTH; i++)
    {
        if (!std::isxdigit(seq[i]))
        {
            throw encoding_error();
        }
    }

    return static_cast<std::uint16_t>(parse_long(seq, 16));
}

template<typename Context>
void write_utf8_char(Context& context, const utf8_char& c)
{
    for (std::size_t i = 0; i < sizeof(c.bytes); i++)
    {
        const char byte = c[i];
        if ((i > 0) && (byte == 0))
        {
            break;
        }

        context.write(byte);
    }
}

template<typename Context>
void read_quoted_string(Context& context, bool skip_opening_quote = false)
{
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

    while ((state != CLOSED) && ((c = context.read()) != 0))
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
                throw parse_error(context, parse_error::EXPECTED_UTF16_LOW_SURROGATE);
            }
            else if (c == '"')
            {
                state = CLOSED;
            }
            else
            {
                context.write(c);
            }

            break;

        case ESCAPE_SEQUENCE:

            state = CHARACTER;

            switch (c)
            {
            case '"': context.write('"'); break;
            case '\\': context.write('\\'); break;
            case '/': context.write('/'); break;
            case 'b': context.write('\b'); break;
            case 'f': context.write('\f'); break;
            case 'n': context.write('\n'); break;
            case 'r': context.write('\r'); break;
            case 't': context.write('\t'); break;
            case 'u': state = UTF16_SEQUENCE; break;
            default: throw parse_error(context, parse_error::INVALID_ESCAPE_SEQUENCE);
            }

            break;

        case UTF16_SEQUENCE:

            utf16_seq[utf16_seq_offset++] = c;

            if (utf16_seq_offset == sizeof(utf16_seq) - 1)
            {
                try
                {
                    const std::uint16_t code_unit = parse_utf16_escape_sequence(utf16_seq);

                    if (code_unit == 0 && high_surrogate == 0)
                    {
                        throw parse_error(context, parse_error::NULL_UTF16_CHARACTER);
                    }

                    if (high_surrogate != 0)
                    {
                        // we were waiting for the low surrogate (that now is code_unit)
                        write_utf8_char(context, utf16_to_utf8(high_surrogate, code_unit));
                        high_surrogate = 0;
                    }
                    else if (code_unit >= 0xD800 && code_unit <= 0xDBFF)
                    {
                        high_surrogate = code_unit;
                    }
                    else
                    {
                        write_utf8_char(context, utf16_to_utf8(code_unit, 0));
                    }
                }
                catch (const encoding_error&)
                {
                    throw parse_error(context, parse_error::INVALID_UTF16_CHARACTER);
                }

                utf16_seq_offset = 0;

                state = CHARACTER;
            }

            break;

        case CLOSED: // to silence compiler warnings

            throw std::runtime_error("This line should never be reached, please file a bug report"); // LCOV_EXCL_LINE
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

    context.write(0);
}

// reads any value that is not a string (or an object/array)
template<typename Context>
char read_unquoted_value(Context& context, char first_char = 0)
{
    if (first_char != 0)
    {
        context.write(first_char);
    }

    char c;

    while (((c = context.read()) != 0) && (c != ',') && (c != '}') && (c != ']') && !std::isspace(c))
    {
        context.write(c);
    }

    if (c == 0)
    {
        throw parse_error(context, parse_error::UNTERMINATED_VALUE);
    }

    context.write(0);

    return c; // return the termination character (or it will be lost forever)
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

    value_type m_type;
    bool m_long_available;
    bool m_double_available;
    const char* m_buffer;
    long m_long_value;
    double m_double_value;

public:

    explicit value(value_type type = Null,
                   const char* buffer = "",
                   long long_value = 0,
                   bool long_available = false,
                   double double_value = 0.0,
                   bool double_available = false) :
        m_type(type),
        m_long_available(long_available),
        m_double_available(double_available),
        m_buffer(buffer),
        m_long_value(long_value),
        m_double_value(double_value)
    {
    }

    value_type type() const
    {
        return m_type;
    }

    const char* as_string() const
    {
        return m_buffer;
    }

    long as_long() const
    {
        return m_long_value;
    }

    bool long_available() const
    {
        return m_long_available;
    }

    bool as_bool() const
    {
        return (m_long_value) ? true : false; // to avoid VS2013 warnings
    }

    double as_double() const
    {
        return m_double_value;
    }

    bool double_available() const
    {
        return m_double_available;
    }
}; // class value

namespace detail
{

template<typename Context>
value parse_unquoted_value(const Context& context)
{
    const char* const buffer = context.write_buffer();

    if (std::strcmp(buffer, "true") == 0)
    {
        return value(Boolean, buffer, 1, true, 1.0, true);
    }
    else if (std::strcmp(buffer, "false") == 0)
    {
        return value(Boolean, buffer, 0, true, 0.0, true);
    }
    else if (std::strcmp(buffer, "null") == 0)
    {
        return value(Null, buffer, 0, false, 0.0, false);
    }
    else
    {
        long long_value = 0;
        double double_value = 0.0;
        bool long_available = false;
        bool double_available = false;

        try
        {
            long_value = parse_long(buffer);
            long_available = true;
            double_value = long_value;
            double_available = true;
        }
        catch (const number_parse_error&)
        {
            try
            {
                double_value = parse_double(buffer);
                double_available = true;
            }
            catch (const number_parse_error&)
            {
                throw parse_error(context, parse_error::INVALID_VALUE);
            }
        }

        return value(Number, buffer, long_value, long_available, double_value, double_available);
    }
}

template<typename Context>
std::pair<value, char> read_value(Context& context, char first_char)
{
    if (first_char == '{')
    {
        return std::make_pair(value(Object), 0);
    }
    else if (first_char == '[')
    {
        return std::make_pair(value(Array), 0);
    }
    else if (first_char == '"') // quoted string
    {
        context.new_write_buffer();
        read_quoted_string(context, true);

        return std::make_pair(value(String, context.write_buffer()), 0);
    }
    else // unquoted value
    {
        context.new_write_buffer();
        const char ending_char = read_unquoted_value(context, first_char);

        return std::make_pair(parse_unquoted_value(context), ending_char);
    }
}

template<typename Context>
void parse_init_helper(const Context& context, char& c, bool& must_read)
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
    const std::pair<value, char> read_value_result = detail::read_value(context, c);
    const value v = read_value_result.first;

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

    const char* field_name = "";

    while (state != END)
    {
        if (context.nesting_level() != nesting_level)
        {
            throw parse_error(context, parse_error::NESTED_OBJECT_OR_ARRAY_NOT_PARSED);
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
                throw parse_error(context, parse_error::EXPECTED_OPENING_BRACKET);
            }
            state = FIELD_NAME_OR_CLOSING_BRACKET;
            break;

        case FIELD_NAME_OR_CLOSING_BRACKET:
            if (c == '}')
            {
                state = END;
                break;
            }
            // intentional fall-through

        case FIELD_NAME:
            if (c != '"')
            {
                throw parse_error(context, parse_error::EXPECTED_OPENING_QUOTE);
            }
            context.new_write_buffer();
            detail::read_quoted_string(context, true);
            field_name = context.write_buffer();
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
                throw parse_error(context, parse_error::EXPECTED_COMMA_OR_CLOSING_BRACKET);
            }
            break;

        case END:

            throw std::runtime_error("This line should never be reached, please file a bug report"); // LCOV_EXCL_LINE
        }

        if (c == 0)
        {
            throw std::runtime_error("This line should never be reached, please file a bug report"); // LCOV_EXCL_LINE
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
            throw parse_error(context, parse_error::NESTED_OBJECT_OR_ARRAY_NOT_PARSED);
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
                throw parse_error(context, parse_error::EXPECTED_OPENING_BRACKET);
            }
            state = VALUE_OR_CLOSING_BRACKET;
            break;

        case VALUE_OR_CLOSING_BRACKET:
            if (c == ']')
            {
                state = END;
                break;
            }
            // intentional fall-through

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
                throw parse_error(context, parse_error::EXPECTED_COMMA_OR_CLOSING_BRACKET);
            }
            break;

        case END:

            throw std::runtime_error("This line should never be reached, please file a bug report"); // LCOV_EXCL_LINE
        }

        if (c == 0)
        {
            throw std::runtime_error("This line should never be reached, please file a bug report"); // LCOV_EXCL_LINE
        }
    }

    context.end_nested();
}

namespace detail
{

class dispatch_rule; // forward declaration

} // namespace detail

class dispatch
{
    friend class detail::dispatch_rule;

private:

    const char* m_field_name;
    bool m_handled;

public:

    explicit dispatch(const char* field_name) :
        m_field_name(field_name),
        m_handled(false)
    {
    }

    explicit dispatch(const std::string& field_name) :
        m_field_name(field_name.c_str()),
        m_handled(false)
    {
    }

    dispatch(const dispatch&) = delete;
    dispatch(dispatch&&) = delete;
    dispatch& operator=(const dispatch&) = delete;
    dispatch& operator=(dispatch&&) = delete;

    detail::dispatch_rule operator<<(const char* field_name);
    detail::dispatch_rule operator<<(const std::string& field_name);
}; // class dispatch

namespace detail
{

class dispatch_rule
{
private:

    dispatch& m_dispatch;
    const char* m_field_name;

public:

    explicit dispatch_rule(dispatch& parent_dispatch, const char* field_name) :
        m_dispatch(parent_dispatch),
        m_field_name(field_name)
    {
    }

    dispatch_rule(const dispatch_rule&) = delete;
    dispatch_rule(dispatch_rule&&) = default;
    dispatch_rule& operator=(const dispatch_rule&) = delete;
    dispatch_rule& operator=(dispatch_rule&&) = delete;

    template<typename Handler>
    dispatch& operator>>(Handler&& handler) const
    {
        if (!m_dispatch.m_handled && ((m_field_name == NULL) || (std::strcmp(m_dispatch.m_field_name, m_field_name) == 0)))
        {
            handler();
            m_dispatch.m_handled = true;
        }

        return m_dispatch;
    }
}; // class dispatch_rule

template<typename Context>
class ignore
{
private:

    Context& m_context;

public:

    explicit ignore(Context& context) :
        m_context(context)
    {
    }

    ignore(const ignore&) = delete;
    ignore(ignore&&) = delete;
    ignore& operator=(const ignore&) = delete;
    ignore& operator=(ignore&&) = delete;

    void operator()(const char*, value)
    {
        operator()();
    }

    void operator()(value)
    {
        operator()();
    }

    void operator()()
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

inline detail::dispatch_rule dispatch::operator<<(const char* field_name)
{
    return detail::dispatch_rule(*this, field_name);
}

inline detail::dispatch_rule dispatch::operator<<(const std::string& field_name)
{
    return operator<<(field_name.c_str());
}

static const char* const any = NULL;

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
