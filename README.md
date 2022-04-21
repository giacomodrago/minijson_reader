# minijson_reader

[![CMake](https://github.com/giacomodrago/minijson_reader/actions/workflows/cmake.yml/badge.svg)](https://github.com/giacomodrago/minijson_reader/actions/workflows/cmake.yml)

## Motivation and design

When parsing JSON messages, most C/C++ libraries employ a DOM-based approach, i.e. they work by building an in-memory representation of object, and the client can then create/read/update/delete the properties of the object as needed, and most importantly access them in whatever order.
While this is very convenient and provides maximum flexibility, there are situations in which memory allocations must or should preferably be avoided. `minijson_reader` is a callback-based parser, which can effectively parse JSON messages *without allocating a single byte of memory on the heap*, provided the input buffer can be modified.

[`minijson_writer`](https://github.com/giacomodrago/minijson_writer) is the independent counterpart for writing JSON messages.

## Dependencies

`minijson_reader` is a single header file of ~1,500 LOC with **no library dependencies**.
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

Similar to a [`buffer_context`](#buffer_context), but it does not modify the input buffer. `const_buffer_context` immediately allocates a buffer on the heap having the same size of the input buffer. It can throw `std::bad_alloc` only in the constructor, as no other memory allocations are performed after the object is created.
The input buffer must stay valid for the entire lifetime of the `const_buffer_context` instance.

```cpp
const char* buffer = "{}";
minijson::const_buffer_context ctx(buffer, strlen(buffer)); // may throw
// ...
```

### `istream_context`

With `istream_context` the input is provided as a `std::istream`. The stream doesn't have to be seekable and will be read only once, one character at a time, until EOF is reached, or an error occurs. An arbitrary number of memory allocations may be performed upon construction and when the input is parsed with [`parse_object` or `parse_array`](#parse_object-and-parse_array), effectively changing the interface of those functions, that can throw `std::bad_alloc` when used with an `istream_context`.

```cpp
// let input be a std::istream
minijson::istream_context ctx(input);
// ...
```

### More about contexts

Contexts cannot be copied, nor moved. Even if the context classes may have public methods, the client must not rely on them, as they may change without prior notice. The client-facing interface is limited to the constructor and the destructor.

The client can implement custom context classes, although the authors of this library do not yet provide a formal definition of a `Context` concept, which has to be reverse engineered from the source code, and can change without prior notice.


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

`name` is a UTF-8 encoded string representing the name of the field. Its lifetime is that of the parsing [context](#contexts), except for [`buffer_context`](#buffer_context), in which case it will stay valid until the underlying input buffer is destroyed.

A JSON **array** must be parsed by using `parse_array`:

```cpp
// let ctx be a context
minijson::parse_array(ctx, [&](minijson::value value)
{
    // for every element...
});
```

In both cases [`minijson::value`](#value) represents the field or element value.

Both `name` and `value` can be safely copied, and all their copies will stay valid until the [context](#contexts) is destroyed (or the underlying buffer is destroyed in case [`buffer_context`](#buffer_context) is used).

`parse_object` and `parse_array` return the number of bytes read from the input.

### `value`

Field and element values are accessible through instances of the `minijson::value` class.

`value` has the following public methods:

- **`minijson::value_type type()`**. The type of the value (`String`, `Number`, `Boolean`, `Object`, `Array` or `Null`).
- **`template<typename T> T as()`**. The value as a `T`, where `T` is one of the following:
  - **`std::string_view`**. UTF-8 encoded string. This representation is available when `type()` is `String`; in all the other cases `minijson::bad_value_cast` is thrown. The lifetime of the returned `std::string_view` is that of the parsing [context](#contexts), except for [`buffer_context`](#buffer_context), in which case the `std::string_view` will stay valid until the underlying input buffer is destroyed.
  - **`bool`**. Only available when `type()` is `Boolean`; in all the other cases `minijson::bad_value_cast` is thrown.
  - **arithmetic types** (excluding `bool`). If `type()` is `Number`, the string representation of the value is contextually parsed by means of [`std::from_chars`](https://en.cppreference.com/w/cpp/utility/from_chars), and `std::range_error` is thrown in case the conversion fails because the value does not fit in the chosen arithmetic type. If `type()` is not `Number`, `minijson::bad_value_cast` is thrown.
  - **`std::optional<T>`**, where `T` is any of the above. The behavior is the same as for `T`, except that an empty optional is returned *if and only if* `type()` is `Null`. Exceptions caused by other failure modes are propagated.
- **`std::string_view raw()`**. The raw contents of the value. This method is useful for debugging or wrapping this library, but in general `as()` should be preferred. If `type()` is `String`, this method behaves just like `as<std::string_view>()`; if `type()` is `Boolean`, this method returns either `true` or `false`; if `type()` is `Null`, this method returns `null`; if `type()` is `Number`, this method returns the number exactly as it appears on the JSON message; if `type()` is `Object` or `Array`, this method returns an empty `std::string_view`. The lifetime of the returned `std::string_view` is that of the parsing [context](#contexts), except for [`buffer_context`](#buffer_context), in which case the `std::string_view` will stay valid until the underlying input buffer is destroyed.

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

Simply passing an empty callback *does not achieve the same result*. `minijson::ignore` will *recursively* parse (and ignore) all the nested elements of the nested element itself (if you are concerned about possible stack overflows, please refer to [Errors](#errors)). `minijson::ignore` is intended for nested objects and arrays, but does no harm if used to ignore elements of any other type.


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
char json_obj[] = R"json(
{
    "field1": 42,
    "array": [1, 2, 3],
    "field2": "He said \"hi\"",
    "nested":
    {
        "field1": 42.0,
        "field2": true,
        "ignored_field": 0,
        "ignored_object": {"a": [0]}
    },
    "ignored_array": [4, 2, {"a": 5}, [7]]
}
)json";

struct obj_type
{
    long field1 = 0;
    std::string_view field2; // be mindful of lifetime!
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
```

## Errors

[`parse_object` and `parse_array`](#parse_object-and-parse_array) will throw a `minijson::parse_error` exception when something goes wrong.

`parse_error` provides a `reason()` method that returns a member of the `parse_error::error_reason` enum:
- `EXPECTED_OPENING_QUOTE`
- `EXPECTED_UTF16_LOW_SURROGATE`: [learn more](http://en.wikipedia.org/wiki/UTF-16#U.2B10000_to_U.2B10FFFF)
- `INVALID_ESCAPE_SEQUENCE`
- `NULL_UTF16_CHARACTER`
- `INVALID_UTF16_CHARACTER`
- `INVALID_VALUE`
- `EXPECTED_VALUE`
- `UNTERMINATED_VALUE`
- `EXPECTED_OPENING_BRACKET`
- `EXPECTED_COLON`
- `EXPECTED_COMMA_OR_CLOSING_BRACKET`
- `NESTED_OBJECT_OR_ARRAY_NOT_PARSED`: if this happens, make sure you are [ignoring unnecessary nested objects or arrays](#ignoring-nested-objects-and-arrays) in the proper way
- `EXCEEDED_NESTING_LIMIT`: this means that the nesting depth exceeded a sanity limit that is defaulted to `32` and can be overriden at compile time by defining the `MJR_NESTING_LIMIT` macro. A sanity check on the nesting depth is essential to avoid stack overflows caused by malicious inputs such as `[[[[[[[[[[[[[[[...more nesting...]]]]]]]]]]]]]]]`.

`parse_error` also has a `size_t offset()` method returning the approximate offset in the input message at which the error occurred. Beware: this offset is **not** guaranteed to be accurate, it can be out-of-bounds, and can change without prior notice in future versions of the library (for example, because it is made more accurate).

`value::as()` may also throw exceptions as specified in [the relevant section](#value).
