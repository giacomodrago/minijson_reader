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

With `istream_context` the input is provided as a `std::istream`. The stream doesn't have to be seekable and will be read only once, one character at a time, until the end of the JSON message is reached, EOF is reached, or a [parse error](#parse-errors) occurs. An arbitrary number of memory allocations may be performed upon construction and when the input is parsed with [`parse_object()` or `parse_array()`](#parse_object-and-parse_array), effectively changing the interface of those functions, that can throw `std::bad_alloc` when used with an `istream_context`.

```cpp
// let input be a std::istream
minijson::istream_context ctx(input);
// ...
```

### More about contexts

Contexts cannot be copied, but can be moved. Using a context that has been moved from causes undefined behavior.

Even if the context classes may have public methods, the client must not rely on them, as they may change without notice. The client-facing interface is limited to the constructor and the destructor.

The client can implement custom context classes, although the authors of this library do not yet provide a formal definition of a `Context` concept, which has to be reverse engineered from the source code, and can change without notice.

The same context cannot be used to parse more than one message, and cannot be reused after it is used for parsing a JSON message that causes a [parse error](#parse-errors): reusing contexts causes undefined behavior.


## Parsing messages

### `parse_object` and `parse_array`

A JSON **object** must be parsed with `parse_object()`:

```cpp
// let ctx be a context
minijson::parse_object(ctx, [&](std::string_view name, minijson::value value)
{
    // for every field...
});
```

`name` is a UTF-8 encoded string representing the name of the field. Its lifetime is that of the parsing [context](#contexts), except for [`buffer_context`](#buffer_context), in which case it will stay valid until the underlying input buffer is destroyed.

A JSON **array** must be parsed by using `parse_array()`:

```cpp
// let ctx be a context
minijson::parse_array(ctx, [&](minijson::value value)
{
    // for every element...
});
```

[`minijson::value`](#value) represents the object field or array element value. [Parsing nested objects or arrays](#parsing-nested-objects-or-arrays), however, requires an explicit recursive call.

Both `name` and `value` can be safely copied, and all their copies will stay valid until the [context](#contexts) is destroyed (or the underlying buffer is destroyed in case [`buffer_context`](#buffer_context) is used).

The functor passed to `parse_object()` or `parse_array()` may optionally accept a [context](#contexts) as the last parameter, in which case it will be passed the current parsing [context](#contexts), thus preventing the need to capture it inside the functor.

`parse_object()` and `parse_array()` return the number of bytes read from the input.

### `value`

Object field and array element **values** are accessible through instances of the `minijson::value` class.

`value` has the following public methods:

- **`minijson::value_type type()`**. The type of the value (`String`, `Number`, `Boolean`, `Object`, `Array` or `Null`). When the type of a `value` is `Object` or `Array`, you **must** [parse the nested object or array](#parsing-nested-objects-or-arrays) by means of an explicit recursive call into `parse_object()` or `parse_array()` respectively.
- **`template<typename T> T as()`**. The value as a `T`, where `T` is one of the following:
  - **`std::string_view`**. UTF-8 encoded string. This representation is available when `type()` is `String`; in all the other cases `minijson::bad_value_cast` is thrown. The lifetime of the returned `std::string_view` is that of the parsing [context](#contexts), except for [`buffer_context`](#buffer_context), in which case the `std::string_view` will stay valid until the underlying input buffer is destroyed.
  - **`bool`**. Only available when `type()` is `Boolean`; in all the other cases `minijson::bad_value_cast` is thrown.
  - **arithmetic types** (excluding `bool`). If `type()` is `Number`, the string representation of the value is contextually parsed by means of [`std::from_chars`](https://en.cppreference.com/w/cpp/utility/from_chars), and `std::range_error` is thrown in case the conversion fails because the value does not fit in the chosen arithmetic type. If `type()` is not `Number`, `minijson::bad_value_cast` is thrown.
  - **`std::optional<T>`**, where `T` is any of the above. The behavior is the same as for `T`, except that an empty optional is returned *if and only if* `type()` is `Null`. Exceptions caused by other failure modes are propagated.
- **`template<typename T> T& to(T& dest)`**. Shorthand for `return (dest = as<T>());`.
- **`std::string_view raw()`**. The raw contents of the value. This method is useful for debugging or wrapping this library, but in general `as()` should be preferred. If `type()` is `String`, this method behaves just like `as<std::string_view>()`; if `type()` is `Boolean`, this method returns either `true` or `false`; if `type()` is `Null`, this method returns `null`; if `type()` is `Number`, this method returns the number exactly as it appears on the JSON message; if `type()` is `Object` or `Array`, this method returns an empty `std::string_view`. The lifetime of the returned `std::string_view` is that of the parsing [context](#contexts), except for [`buffer_context`](#buffer_context), in which case the `std::string_view` will stay valid until the underlying input buffer is destroyed.

The behavior of `value::as()` [can be customized](#customizing-valueas).

### Parsing nested objects or arrays

When the `type()` of a `value` is `Object` or `Array`, you **must** parse the nested object or array by means of an explicit recursive call into `parse_object()` or `parse_array()` respectively, e.g.:

```cpp
// let ctx be a context
minijson::parse_object(ctx, [&](std::string_view name, minijson::value value)
{
    // ...
    if (name == "...")
    {
        if (value.type() != minijson::Object)
        {
            throw some_exception("we were expecting an Object here");
        }
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
    if (name == "...")
    {
        if (value.type() != minijson::Object)
        {
            throw some_exception("we were expecting an Object here");
        }
        minijson::ignore(ctx); // proper way to ignore a nested object
    }
});
```

Simply passing an empty functor *does not achieve the same result*. `minijson::ignore` will *recursively* parse (and ignore) all the nested elements of the nested element itself (there is a protection against stack overflows: please refer to [Parse errors](#parse-errors) to learn more). `minijson::ignore` is intended for nested objects and arrays, but does no harm if used to ignore elements of any other type.

### Parse errors

[`parse_object()` and `parse_array()`](#parse_object-and-parse_array) will throw a `minijson::parse_error` exception when something goes wrong.

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

### Customizing `value::as()`

You can extend the set of types `value::as()` can handle, or even override its behavior for some of the types supported by default, by specializing the `minijson::value_as` struct. For example:

```cpp
enum class OrderType
{
    BUY,
    SELL,
};

namespace minijson
{

// Add support for your enum
template<>
struct value_as<OrderType>
{
    OrderType operator()(const value v) const
    {
        if (v.type() != String)
        {
            throw std::runtime_error(
                "could not convert JSON value to OrderType");
        }

        if (boost::iequals(v.raw(), "buy"))
        {
            return OrderType::BUY;
        }
        if (boost::iequals(v.raw(), "sell"))
        {
            return OrderType::SELL;
        }
        throw std::runtime_error("could not convert JSON value to OrderType");
    }
};

// *Override* the default behavior for floating point types
template<typename T>
struct value_as<T, std::enable_if_t<std::is_floating_point_v<T>>>
{
    T operator()(const value v) const
    {
        if (v.type() != Number)
        {
            throw std::runtime_error(
                "could not convert JSON value to a floating point number");
        }

        // Unclear why anyone would want to do this (it's just an example...)
        return boost::lexical_cast<T>(v.raw());

        // Note: you can always fall back to the default behavior like so:
        // return value_as_default<T>(v);
    }
};

} // namespace minijson
```


## Dispatchers

While the arguments accepted by the functor passed to `parse_object()` allow for a great deal of control over how to process the fields, in a lot of straightforward cases they only force the client to write boilerplate such as:

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

**Dispatchers** can help factor out some of that boilerplate. This shows a typical usage example to get you started:

```cpp
struct Order
{
    // To avoid memory allocations, here you could use a fixed-size string and
    // specialize struct value_as for it.
    std::string ticker;
    unsigned int price = 0;
    unsigned int size = 0;
    bool has_nyse = false;
    bool urgent = false;
};

using namespace minijson::handlers;
using minijson::value;

// It's advisable that dispatchers be singletons: do not create one each time
// you need to parse a message, it's not needed. Do make sure that your
// functors are completely stateless and thread-safe, though.
static const minijson::dispatcher dispatcher
{
    // These fields are *REQUIRED*. If they are missing, exceptions will
    // be thrown (more on that later...).
    handler("ticker", [](Order& o, value v) {v.to(o.ticker);}),
    handler("price", [](Order& o, value v) {v.to(o.price);}),
    handler("size", [](Order& o, value v) {v.to(o.size);}),

    // This field is optional
    optional_handler("urgent", [](Order& o, value v) {v.to(o.urgent);}),

    // This field will be handled but ignored if found: this is *NOT*
    // the same as providing an empty functor to optional_handler,
    // because it does the right thing to ignore objects and arrays!
    ignore_handler("sender"),

    // We want to handle this nested array in some special way...
    handler(
        "exchanges",
        // If you provide a 3-argument functor, the third argument is the
        // parsing context
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

    // An any_handler may choose to handle anything not previously handled,
    // or just reject it
    any_handler(
        [&](Order&, std::string_view name, value v)
        {
            if (name.find("debug-") == 0)
            {
                log_debug(v.as<int>());
                // We handled this field: no subsequent handlers will be called
                return true;
            }
            // Choose not to handle this field. Subsequent handlers will be
            // given a chance to handle it.
            return false;
        }),

    // This will ignore anything not previously handled, like a final catch-all
    // that sucks all unhandled fields into a black hole. When such handler
    // is not present, unhandled fields cause exceptions to be thrown
    // (more on that later...).
    ignore_any_handler {},
};

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
dispatcher.run(context, order);
// If run() has thrown no exceptions (which it can!), order is now populated
```

The above example illustrates the essentials. The following sections delve into the finer details.

### Handlers

As seen in [this example](#dispatchers), `minijson::dispatcher` is constructed with an ordered list of handlers.

There are various types of handlers available in the `minijson::handlers` namespace:
* **`handler(std::string_view field_name, Functor functor)`**. Handles each occurrence of the **required** field named `field_name` by means of the provided `functor`, which has to accept the following arguments:
  1. The [target(s)](#targets), i.e. the object(s) being populated, if any
  2. The parsed [`value`](#value)
  3. *Optionally*, the parsing [context](#contexts)
* **`optional_handler(std::string_view field_name, Functor functor)`**. Handles each occurrence of the **optional** field named `field_name` by means of the provided `functor`, which has to accept the following arguments:
  1. The [target(s)](#targets), i.e. the object(s) being populated, if any
  2. The parsed [`value`](#value)
  3. *Optionally*, the parsing [context](#contexts)
* **`any_handler(Functor functor)`**. This handler is given the choice of handling potentially *any* field by means of the provided `functor`, which has to accept the following arguments:
  1. The [target(s)](#targets), i.e. the object(s) being populated, if any
  2. The field name as a `std::string_view` (lifetime tied to that of the parsing [context](#contexts))
  3. The parsed [`value`](#value)
  4. *Optionally*, the parsing [context](#contexts)

  and return something `bool`-convertible telling whether the field was handled or not. If the field was not handled, then the subsequent handlers will be given a chance to handle it.
* **`ignore_handler(std::string_view field_name)`**. [Ignores](#ignoring-nested-objects-and-arrays) the optional field named `field_name`.
* **`any_ignore_handler()`**. [Ignores](#ignoring-nested-objects-and-arrays) *any* field. The only reasonable use for this handler is to place it as the very last one, to silently swallow and ignore any unknown fields.

The list of handlers used to construct a dispatcher define its type (via [CTAD](https://en.cppreference.com/w/cpp/language/class_template_argument_deduction)). It follows that the full type of a dispatcher cannot be reasonably spelled out (`auto` is your friend), and dispatchers cannot be stored in collections without somehow erasing their types.

### Targets

A dispatcher runs against zero, one or more **targets**, which are the object(s) being populated with the contents of the JSON message. The targets, if any, will be passed to the [handler](#handlers) functors (when applicable) as references, before the other arguments. It does not make much sense for the targets to be `const`, but it is allowed.

In this example, a dispatcher runs against multiple targets:
```cpp
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
```

### Dispatch errors

The `run()` method of `dispatcher` calls `parse_object()`, which [can throw `parse_error` exceptions](#parse-errors).

Additionally, `run()` throws `minijson::unhandled_field_error` when no handler chooses to handle a parsed field.

Last, `run()` throws `minijson::missing_field_error` when a JSON message is parsed successfully, but does not contain a required field.

Both `unhandled_field_error` and `missing_field_error` have a `std::string_view field_name_truncated()` method returning the name of the field which caused the error, truncated at 55 characters. The lifetime of the returned `std::string_view` matches that of the exception (it is backed by a fixed-length array inside the exception object, thus avoiding dangling references as well as memory allocations).

In case multiple required fields are missing from the JSON message being parsed, it is unspecified which one is returned by `missing_field_error::field_name_truncated()`. Refer to the [next section](#advanced-dispatchers-use-with-dispatcher_run) if your application requires more control.

### Advanced dispatchers use with `dispatcher_run`

Going back to [this example](#dispatchers), the last two lines are in fact a shorthand for:

```cpp
Order order;
minijson::dispatcher_run run(dispatcher, order);
minijson::parse_object(context, run);
run.enforce_required(); // throws missing_field_error
```

Using a `minijson::dispatcher_run` instance after the underlying `dispatcher` has been destroyed causes undefined behavior. Passing the same `dispatcher_run` instance to `parse_object()` more than once causes unspecified behavior.

Holding on to a `dispatcher_run` instance gives you greater control over how errors are handled. Indeed, you can omit the call to `enforce_required()` and optionally replace it with your custom validation logic by calling `inspect()` instead:

```cpp
run.inspect(
    [](const auto& handler, const std::size_t handle_count)
    {
        // For each handler of the dispatcher (in the order they are listed),
        // this functor will be passed the handler instance (`handler`) and the
        // number of times that handler has decided to handle a field
        // (`handle_count`).
        if constexpr (handlers::traits<decltype(handler)>::is_required_field)
        {
            if (handle_count == 0)
            {
                std::cout
                    << "We're missing " << handler.field_name() << " "
                    << "but that's OK, we won't throw, maybe we can log "
                    << "something or add the field name to a collection";
            }
        }
    });
```

In the example above, `is_required_field` is a handler trait, a compile-time `bool` constant allowing you to determine what kind of handler you are dealing with. The following traits are available under `minijson::handlers::traits<Handler>`:

| Trait                 | Description                                 | `true` for the handlers                          |
| --------------------- | ------------------------------------------- | ------------------------------------------------ |
| `is_field_specific`   | Handles each occurrence of a specific field | `handler`, `optional_handler`, `ignore_handler`  |
| `is_ignore`           | Ignores fields                              | `ignore_handler`, `any_ignore_handler`           |
| `is_required_field`   | Handles a required field                    | `handler`                                        |

Handlers for which `is_field_specific` is `true` have a `field_name()` method returning a `std::string_view` the lifetime of which is tied to that of the handler, which in turn depends on the lifetime of the underlying `dispatcher`.

You can reverse engineer how handlers are implemented to roll your very own, and as long as you expose the correct interface (including the traits listed above) it should "just work", but the authors of this library do not yet provide a formal definition of a `Handler` concept, which can change without notice.
