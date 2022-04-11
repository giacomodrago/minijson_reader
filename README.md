# minijson_reader

[![CMake](https://github.com/giacomodrago/minijson_reader/actions/workflows/cmake.yml/badge.svg)](https://github.com/giacomodrago/minijson_reader/actions/workflows/cmake.yml)

## Motivation and design

When parsing JSON messages, most C/C++ libraries employ a DOM-based approach, i.e. they work by building an in-memory representation of object, and the client can then create/read/update/delete the properties of the object as needed, and most importantly access them in whatever order.
While this is very convenient and provides maximum flexibility, there are situations in which memory allocations must or should preferably be avoided. `minijson_reader` is a callback-based parser, which can effectively parse JSON messages *without allocating a single byte of memory*, provided the input buffer can be modified.

[`minijson_writer`](https://github.com/giacomodrago/minijson_writer) is the independent counterpart for writing JSON messages.

## Dependencies

`minijson_reader` is a single header file of ~1,300 LOC with **no library dependencies**.
**C++17** support is required.

## Contexts

First of all, the client must create a **context**. A context contains the message to be parsed, plus other state the client should not be concerned about. Different context classes are currently available, corresponding to different ways of providing the input, different memory footprints, and different exception guarantees.

### `buffer_context`

`buffer_context` can be used when the input buffer can be modified. It guarantees no memory allocations are performed, and consequently no `std::bad_alloc` exceptions will ever be thrown. Its constructor takes a pointer to a ASCII or UTF-8 encoded C string (not necessarily null-terminated) and the length of the string in bytes (not in UTF-8 characters).

```cpp
char buffer[] = "{}";
minijson::buffer_context ctx(buffer, sizeof(buffer) - 1);
// ...
```

### `const_buffer_context`

Similar to a `buffer_context`, but it does not modify the input buffer. `const_buffer_context` immediately allocates a buffer on the heap having the same size of the input buffer. It can throw `std::bad_alloc` only in the constructor, as no other memory allocations are performed after the object is created.
The input buffer must stay valid for the entire lifetime of the `const_buffer_context` instance.

```
const char* buffer = "{}";
minijson::const_buffer_context ctx(buffer, strlen(buffer)); // may throw
// ...
```

### `istream_context`

With `istream_context` the input is provided as a `std::istream`. The stream doesn't have to be seekable and will be read only once, one character at a time, until EOF is reached, or an error occurs. An arbitrary number of memory allocations may be performed upon construction and when the input is parsed with`parse_object` or `parse_array`, effectively changing the interface of those functions, that can throw `std::bad_alloc` when used with `istream_context`.

```cpp
// let input be a std::istream
minijson::istream_context ctx(input);
// ...
```

### More about contexts

Contexts cannot be copied, nor moved. Even if the context classes may have public methods, the client must not rely on them, as they may change without prior notice. The client-facing interface is limited to the constructor and the destructor.

The client can implement custom context classes, although the authors of this library do not yet provide a formal definition of a`Context` concept, which has to be reverse engineered from the source code, and can change without prior notice.


## Parsing messages

### `parse_object` and `parse_array`

A JSON **object** must be parsed with `parse_object`:

```cpp
// let ctx be a context
minijson::parse_object(ctx, [&](std::string_view name, minijson::value value)
{
    // for every field...
});
```

`name` is a UTF-8 encoded string representing the name of the field.

A JSON **array** must be parsed by using `parse_array`:

```cpp
// let ctx be a context
minijson::parse_array(ctx, [&](minijson::value value)
{
    // for every element...
});
```

In both cases `value` represents the field or element value (`minijson::value` will be detailed in the following paragraph).

Both `name` and `value` can be safely copied, and all their copies will stay valid until the context is destroyed (or the underlying buffer is destroyed in case `buffer_context` is used).

Of course, in place of the lambda, you may use callbacks or function objects.

### `value`

Field and element values are accessible through instances of the `minijson::value` class.

`value` has the following public methods:

- `minijson::value_type type()`: the type of the value. Possible types are `String`, `Number`, `Boolean`, `Object`, `Array`, and `Null`.
- `std::string_view as_string()`: the value as a UTF-8 encoded string. This representation is always available except when `type()` is `Object` or `Array`, in which case an empty string is returned. The string outlives the `value` instance, but its lifetime is limited by the one of the underlying context, except for `buffer_context`, in which case it will stay valid until the buffer itself is destroyed.
- `long as_long()`: the value as a `long` integer. This representation is available when `type()` is `Number` and the number could be parsed by [`strtol`](http://en.cppreference.com/w/cpp/string/byte/strtol) without overflows, or when the type is `Boolean`, in which case  `1` or `0` are returned for `true` and `false` respectively. In all the other cases, `0` is returned.
- `bool long_available()`: whether the `long` representation is available.
- `double as_double()`: the value as a double-precision floating-point number. This representation is available when `type()` is `Number` and the number could be parsed by [`strtod`](http://en.cppreference.com/w/cpp/string/byte/strtod) without overflows or underflows, or when the type is `Boolean`, in which case `non-zero` or `0.0` are returned for `true` and `false` respectively. In all the other cases, `0.0` is returned.
- `bool double_available()`: whether the `double` representation is available.
- `bool as_bool()`: the value as a boolean. This method simply returns the value of `as_long()` cast to `bool`.

Copying a `value` does not allocate memory, and no method of the class throws.

### Parsing nested objects or arrays

When the `type()` of a `value` is `Object` or `Array`, you **must** parse the nested object or array by doing something like:

```cpp
// let ctx be a context
minijson::parse_object(ctx, [&](std::string_view name, minijson::value value)
{
    // ...
    if (name == "..." && value.type() == minijson::Object)
    {
        minijson::parse_object(ctx, [&](std::string_view name, minijson::value value)
        {
           // parse the nested object
        });
    }
});
```

### Ignoring nested objects and arrays

While all other fields and values can be simply ignored by omission, failing to parse a nested object or array **will cause a parse error** and consequently an exception to be thrown. You can properly ignore a nested object or array by calling `minijson::ignore` as follows:

```cpp
// let ctx be a context
minijson::parse_object(ctx, [&](std::string_view name, minijson::value value)
{
    // ...
    if (name == "..." && value.type() == minijson::Object)
    {
        minijson::ignore(ctx); // proper way to ignore a nested object
    }
});
```

Simply passing an empty callback *does not achieve the same result*. `minijson::ignore` will recursively parse (and ignore) all the nested elements of the nested element itself (if you are thinking about possible stack overflows, please refer to the **Errors** section of this document). `minijson::ignore` is intended for nested objects and arrays, but does no harm if used to ignore elements of any other type.

## A more compact syntax

The arguments accepted by the callback passed to `parse_object` suggest to handle objects fields by the means of a chain of `if`...`else if` blocks:

```cpp
// let ctx be a context
minijson::parse_object(ctx, [&](std::string_view name, minijson::value value)
{
   if (name == "field1") { /* do something */ }
   else if (name == "field2") { /* do something else */ }
   // ...
   else { /* unknown field, either ignore it or throw an exception */ }
});
```

Of course this works, but a **more compact syntax** is provided by the means of `minijson::dispatch`:

```cpp
// let ctx be a context
minijson::parse_object(ctx, [&](std::string_view name, minijson::value value)
{
    minijson::dispatch(name)
    <<"field1">> [&]{ /* do something */ }
    <<"field2">> [&]{ /* do something */ }
    // ...
    <<minijson::any>> [&]{ minijson::ignore(ctx); /* or throw */ };
});
```

Please note the use of `minijson::any` to match any other field that has not been matched so far.

## A fully-featured example

```cpp
char json_obj[] =
    "{ \"field1\": 42, \"array\" : [ 1, 2, 3 ], \"field2\": \"asd\", "
    "\"nested\" : { \"field1\" : 42.0, \"field2\" : true, "
    "\"ignored_field\" : 0, "
    "\"ignored_object\" : {\"a\":[0]} },"
    "\"ignored_array\" : [4, 2, {\"a\":5}, [7]] }";

struct obj_type
{
    long field1 = 0;
    std::string field2; // you can use a string_view,
                        // but in that case beware of lifetimes!
    struct
    {
        double field1 = 0;
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
    <<"field1">> [&]{ obj.field1 = v.as_long(); }
    <<"field2">> [&]{ obj.field2 = v.as_string(); }
    <<"nested">> [&]
    {
        parse_object(ctx, [&](std::string_view k, value v)
        {
            dispatch (k)
            <<"field1">> [&]{ obj.nested.field1 = v.as_double(); }
            <<"field2">> [&]{ obj.nested.field2 = v.as_bool(); }
            <<any>> [&]{ ignore(ctx); };
        });
    }
    <<"array">> [&]
    {
        parse_array(ctx, [&](value v)
        {
            obj.array.push_back(v.as_long());
        });
    }
    <<any>> [&]{ ignore(ctx); };
});
```

You probably want to check that the `type()` of each `value` is the one you expect. This has been omitted for the sake of brevity.

## Errors

`parse_object` and `parse_array` will throw a `minijson::parse_error` exception when something goes wrong.

`parse_error` provides a `reason()` method that returns a member of the `parse_error::error_reason` enum:

- `EXPECTED_OPENING_QUOTE`
- `EXPECTED_UTF16_LOW_SURROGATE`: [learn more](http://en.wikipedia.org/wiki/UTF-16#U.2B10000_to_U.2B10FFFF)
- `INVALID_ESCAPE_SEQUENCE`
- `INVALID_UTF16_CHARACTER`
- `EXPECTED_CLOSING_QUOTE`
- `INVALID_VALUE`
- `UNTERMINATED_VALUE`
- `EXPECTED_OPENING_BRACKET`
- `EXPECTED_COLON`
- `EXPECTED_COMMA_OR_CLOSING_BRACKET`
- `NESTED_OBJECT_OR_ARRAY_NOT_PARSED`: if this happens, make sure you are ignoring unnecessary nested objects or arrays in the proper way
- `EXCEEDED_NESTING_LIMIT`: this means that the nesting depth exceeded a sanity limit that is defaulted to `32` and can be overriden at compile time by defining the `MJR_NESTING_LIMIT` macro. A sanity check on the nesting depth is essential to avoid stack overflows caused by malicious inputs such as `[[[[[[[[[[[[[[[...more nesting...]]]]]]]]]]]]]]]`.
- `NULL_UTF16_CHARACTER`

`parse_error` also has a `size_t offset()` method returning the approximate offset in the input message at which the error occurred. Beware: this offset is **not** guaranteed to be accurate, it can be out-of-bounds, and can change without prior notice in future versions of the library (for example, because it is made more accurate).
