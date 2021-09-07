#include "minijson_reader.hpp"

#include <exception>
#include <fstream>
#include <iomanip>  // boolalpha, quoted, ...
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#ifdef VERBOSE
#  define TRACE(str) (std::cerr << str << std::endl)
#  define TRACEFUNC TRACE(__PRETTY_FUNCTION__)
#else
#  define TRACE(str)
#  define TRACEFUNC
#endif

// NOTE: without uint8_t (octet), use base64 coded octet-stream! CK
typedef std::variant<bool, long long, double, std::string> value_type;

// for SCA property concept i.e see
// http://download.ist.adlinktech.com/docs/SpectraCX/modeling/properties/definingSCAProperties/definescaprop.html
//
struct obj_type {
  obj_type() = default;
  obj_type(std::string _name) : name(std::move(_name)){};

  std::string name;
  std::string type;  // TODO: create enum for simple, sequence, struct, and structsequence SCA property types! CK
  value_type value;
  std::vector<value_type> array;  // NOTE: for simplesequence
  std::vector<obj_type> childs;   // NOTE: recursive data type
};

void write_structsequence(std::ostream& stream, const obj_type& property);

// helper constant for the visitor
template <class>
inline constexpr bool always_false_v = false;

// TODO: use minijson_writer()! CK
//
// Note: we print normalized (one line)
//
std::ostream& operator<<(std::ostream& rOut, const obj_type& property) {
  TRACEFUNC;

  rOut << "{\"name\": \"" << property.name;     // NOTE: start object -> '{'
  rOut << "\", \"type\": \"" << property.type;  // NOTE: as string! CK
  rOut << "\", \"value\": ";

  const std::string& typeCode = property.type;
  bool isSimple = property.childs.empty() && property.array.empty();
  bool isSimpleSequence = false;
  bool isStructSequence = false;

  if (isSimple) {
    if (std::holds_alternative<bool>(property.value)) {
      rOut << std::boolalpha << std::get<bool>(property.value);
    } else if (std::holds_alternative<long long>(property.value)) {
      rOut << std::get<long long>(property.value);
    } else if (std::holds_alternative<double>(property.value)) {
      rOut << std::scientific << std::setprecision(15) << std::get<double>(property.value);
    } else if (std::holds_alternative<std::string>(property.value)) {
      minijson::write_quoted_string(rOut, std::get<std::string>(property.value));
    }
  } else {
    bool isFirst = true;
    if (typeCode == "structsequence") {
      rOut << "[[";  // NOTE: same line! CK
      isStructSequence = true;
    } else if (typeCode == "struct") {
      rOut << "[";  // NOTE: same line! CK
    } else {
      rOut << "[";  // NOTE: same line! CK
      isSimpleSequence = true;
    }

    if (isSimpleSequence) {
      for (const auto& item : property.array) {
        rOut << (isFirst ? "" : ", ");

        std::visit(
            [&](auto&& arg) {
              using T = std::decay_t<decltype(arg)>;
              if constexpr (std::is_same_v<T, bool>) {
                rOut << std::boolalpha << arg;
              } else if constexpr (std::is_same_v<T, long long>) {
                rOut << arg;
              } else if constexpr (std::is_same_v<T, double>) {
                rOut << std::scientific << std::setprecision(15) << arg;
              } else if constexpr (std::is_same_v<T, std::string>) {
                minijson::write_quoted_string(rOut, arg);
              } else {
                static_assert(always_false_v<T>, "non-exhaustive visitor!");
              }
            },
            item);

        isFirst = false;
      }
    } else {
      const std::vector<obj_type>& children = property.childs;
      auto it = children.begin();
      for (; it != children.end(); ++it) {
        if (isStructSequence) {
          rOut << (isFirst ? "" : "], [");  // NOTE: same line! CK
          write_structsequence(rOut, *it);
        } else {
          rOut << (isFirst ? "" : ", ") << *it;  // NOTE: recursion! CK
        }

        isFirst = false;
      }
    }

    if (isStructSequence) {
      rOut << "]]";
    } else {
      rOut << "]";
    }
  }  // if !isSimple

  return rOut << "}";  // NOTE: end object -> '}'
}

void write_structsequence(std::ostream& stream, const obj_type& property) {
  TRACEFUNC;

  bool isFirst = true;
  const std::vector<obj_type>& children = property.childs;
  auto it = children.begin();
  for (; it != children.end(); ++it) {
    stream << (isFirst ? "" : ", ") << *it;  // print struct item in list
    isFirst = false;
  }
}

// minijson callback interfaces:
void parse_object_empty_handler(const char* name, minijson::value);
void parse_array_empty_handler(minijson::value);

template <typename Context>
struct parse_object_nested_handler;

template <typename Context>
struct parse_array_nested_handler {
  Context& ctx;
  size_t counter;
  obj_type& myobj;

  explicit parse_array_nested_handler(Context& context, obj_type& obj) : ctx(context), counter(0), myobj(obj) {}

  ~parse_array_nested_handler() {}

  void operator()(const minijson::value& v) {
    TRACEFUNC;

#ifdef VERBOSE
    // write ',' only if needed
    if (counter) { std::cerr << ", "; }
#endif

    if (minijson::Array == v.type()) {
      TRACE("[");
      if (myobj.type == "structsequence") {
        TRACE(myobj.type);
        obj_type child;
        minijson::parse_array(ctx, parse_array_nested_handler(ctx, child));  // NOTE: recursion
        myobj.childs.push_back(child);
      } else {
        throw std::runtime_error("unexpeced type at parse_array_nested_handler()");
      }
      TRACE("]");
    } else if (minijson::Object == v.type()) {
      TRACE("{");
      obj_type child;
      minijson::parse_object(ctx, parse_object_nested_handler<Context>(ctx, child));
      myobj.childs.push_back(child);
      TRACE("}");
    } else {
      throw minijson::parse_error(ctx, minijson::parse_error::INVALID_VALUE);
    }

    ++counter;
  }
};

template <typename Context>
struct parse_object_nested_handler {
  Context& ctx;
  size_t counter;
  obj_type& myobj;

  explicit parse_object_nested_handler(Context& context, obj_type& obj) : ctx(context), counter(0), myobj(obj) {}

  ~parse_object_nested_handler() {}

  void operator()(const char* name, const minijson::value& v) {
    TRACEFUNC;

#ifdef VERBOSE
    // write ',' only if needed!
    if (counter) { std::cerr << ", "; }
#endif

    if (minijson::Object == v.type()) {
      TRACE("\t" << std::quoted(name) << ": {");
      obj_type child(name);
      minijson::parse_object(ctx, parse_object_nested_handler(ctx, child));  // NOTE: recursion
      myobj.childs.push_back(child);
      TRACE("}");
    } else if (minijson::Array == v.type()) {
      TRACE("\t" << std::quoted(name) << ": [");
      if (myobj.type == "boolsequence") {
        minijson::parse_array(ctx, [&](minijson::value value) { myobj.array.emplace_back(value.as_bool()); });
      } else if (myobj.type == "longsequence") {
        minijson::parse_array(ctx, [&](minijson::value value) { myobj.array.emplace_back(value.as_longlong()); });
      } else if (myobj.type == "doublesequence") {
        minijson::parse_array(ctx, [&](minijson::value value) { myobj.array.emplace_back(value.as_double()); });
      } else if (myobj.type == "stringsequence") {
        minijson::parse_array(ctx, [&](minijson::value value) { myobj.array.emplace_back(value.as_string()); });
      } else {
        minijson::parse_array(ctx, parse_array_nested_handler<Context>(ctx, myobj));
      }
      TRACE("]");
    } else if (minijson::String == v.type()) {
      TRACE("\t" << std::quoted(name) << ": ");
      // TRACE minijson::write_quoted_string(std::cerr, v.as_string());
      if (name == std::string("name")) {
        myobj.name = v.as_string();
      } else if (name == std::string("type")) {
        myobj.type = v.as_string();
      } else if (name == std::string("value")) {
        myobj.value = v.as_string();
      } else {
        throw minijson::parse_error(ctx, minijson::parse_error::INVALID_VALUE);
      }
    } else if (minijson::Boolean == v.type()) {
      myobj.value = v.as_bool();
    } else if (minijson::Number == v.type()) {
      myobj.value = v.as_longlong();
    } else if (minijson::Double == v.type()) {
      myobj.value = v.as_double();
    } else {
      throw minijson::parse_error(ctx, minijson::parse_error::INVALID_VALUE);
    }

    ++counter;
  }
};

int main(int argc, char* argv[]) {
  using namespace minijson;

  obj_type obj;
  if (argc > 1) {
    std::string filename(argv[1]);

    try {
      //=================================
      std::ifstream istream(filename);
      istream_context ctx(istream);
      TRACE("{");
      parse_object(ctx, parse_object_nested_handler<minijson::istream_context>(ctx, obj));
      TRACE("}");
      //=================================
    } catch (std::exception& e) {
      std::cerr << "EXCEPTION: " << e.what() << std::endl;
      return -1;
    }

    std::cout << obj << std::endl;
  } else {
    std::cerr << *argv << " filename [, ...]" << std::endl;
    return -1;
  }

  return 0;
}

// clang-format OFF
/***

$ ./minijson_parser simplebinarydata.json | ./json_pp.py
{
   "name": "binary data",
   "type": "application/octet-stream",
   "value":
"TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4="
}

$ ./minijson_parser simpleproperty.json | ./json_pp.py
{
   "name": "property",
   "type": "double",
   "value": 3.141592653589793
}


$ ./minijson_parser simpleBoolSequence.json | ./json_pp.py
{
   "name": "list",
   "type": "boolsequence",
   "value": [
      true,
      false,
      false,
      true
   ]
}


$ ./minijson_parser structproperty.json | ./json_pp.py
{
   "name": "structproperty",
   "type": "struct",
   "value": [
      {
         "name": "entry1",
         "type": "bool",
         "value": true
      },
      {
         "name": "entry2",
         "type": "double",
         "value": 1.123456789012345
      }
   ]
}


$ ./minijson_parser structsequence.json | ./json_pp.py
{
   "name": "structSequenceProperty",
   "type": "structsequence",
   "value": [
      [
         {
            "name": "entry1",
            "type": "bool",
            "value": true
         },
         {
            "name": "entry2",
            "type": "long",
            "value": 42
         },
         {
            "name": "entry3",
            "type": "string",
            "value": "\tUnicodeString:\"@\u20ac\u00b2\u00b3\u00df\u00c4\u00d6\u00dc\u00a7$%@#\"\r\n"
         }
      ],
      [
         {
            "name": "entry1",
            "type": "bool",
            "value": false
         },
         {
            "name": "entry2",
            "type": "long",
            "value": 123
         },
         {
            "name": "entry3",
            "type": "string",
            "value": "\tTest\t String\t with\t WS\r\n\""
         }
      ]
   ]
}

 ***/
// clang-format ON
