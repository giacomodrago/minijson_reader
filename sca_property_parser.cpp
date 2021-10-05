#include "minijson_reader.hpp"
#include "minijson_writer.hpp"

#include <boost/pfr.hpp>
#include <magic_enum.hpp>

struct my_struct {  // no ostream operator defined!
  int i;
  char c;
  double d;
};

void test_pfr() {
  my_struct s{100, 'H', 3.141593};
  std::cerr << "my_struct has " << boost::pfr::tuple_size<my_struct>::value << " fields: " << boost::pfr::io(s)
            << std::endl;
}

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

// helper constant for the visitor
template <class>
inline constexpr bool always_false_v = false;

// for SCA property concept i.e see
// http://download.ist.adlinktech.com/docs/SpectraCX/modeling/properties/definingSCAProperties/definescaprop.html
//
namespace SCA {
enum PropertyTypes {
  eNone,
  eBool,
  eLong,
  eDouble,
  eString,
  eOctetStream,  // NOTE: base64 encoded string! CK
  eBoolSequence,
  eLongSequence,
  eDoubleSequence,
  eStringSequence,
  eStruct,
  eStructSequence
};
}

namespace {

//
// NOTE: without uint8_t (octet), use base64 coded octet-stream! CK
//
using value_type = std::variant<bool, long long, double, std::string>;

struct obj_type {
  obj_type() = default;
  explicit obj_type(std::string _name) : name(std::move(_name)){};

  std::string name;
  SCA::PropertyTypes type{SCA::eNone};
  value_type value{};
  std::vector<value_type> array;  // NOTE: packed data, only for eSimpleSequence's use! CK
  std::vector<obj_type> childs;   // NOTE: recursive data type for eStruct, and eStructSequence
};

void write_structsequence(std::ostream& stream, const obj_type& property);

//
// Note: this streams a normalized form (all at one line)! CK
//
std::ostream& operator<<(std::ostream& rOut, const obj_type& property) {
  TRACEFUNC;

  minijson::detail::adjust_stream_settings(rOut);

  rOut << "{\"name\": " << std::quoted(property.name);  // NOTE: start object -> '{'
  rOut << ", \"type\": " << std::quoted(magic_enum::enum_name(property.type));
  rOut << ", \"value\": ";

  const auto typeCode = property.type;
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
    if (typeCode == SCA::eStructSequence) {
      rOut << "[[";  // NOTE: same line! CK
      isStructSequence = true;
    } else if (typeCode == SCA::eStruct) {
      rOut << "[";  // NOTE: same line! CK
    } else {
      rOut << "[";  // NOTE: same line! CK
      isSimpleSequence = true;
    }

    if (isSimpleSequence) {
      for (const auto& item : property.array) {
        rOut << (isFirst ? "" : ", ");

        // ===========================================
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
        // ===========================================

        isFirst = false;
      }
    } else {
      for (const auto& child : property.childs) {
        if (isStructSequence) {
          rOut << (isFirst ? "" : "], [");  // NOTE: same line! CK
          write_structsequence(rOut, child);
        } else {
          rOut << (isFirst ? "" : ", ") << child;  // NOTE: recursion! CK
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

  // print struct items as list
  for (const auto& child : property.childs) {
    stream << (isFirst ? "" : ", ") << child;  // NOTE: recursion! CK
    isFirst = false;
  }
}

// minijson callback interfaces:
template <typename Context>
struct parse_object_nested_handler;

template <typename Context>
struct parse_array_nested_handler {  // NOLINT(hicpp-special-member-functions)
  Context& ctx;
  size_t counter;
  obj_type& myobj;

  explicit parse_array_nested_handler(Context& context, obj_type& obj) : ctx(context), counter(0), myobj(obj) {}

  ~parse_array_nested_handler() = default;

  void operator()(const minijson::value& v) {
    TRACEFUNC;

#ifdef VERBOSE
    // write ',' only if needed
    if (counter) { std::cerr << ", "; }
#endif

    if (minijson::Array == v.type()) {
      TRACE("[");
      if (myobj.type == SCA::eStructSequence) {
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
struct parse_object_nested_handler {  // NOLINT(hicpp-special-member-functions)
  Context& ctx;
  size_t counter;
  obj_type& myobj;

  explicit parse_object_nested_handler(Context& context, obj_type& obj) : ctx(context), counter(0), myobj(obj) {}

  ~parse_object_nested_handler() = default;

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
      if (myobj.type == SCA::eBoolSequence) {
        minijson::parse_array(ctx, [&](const minijson::value& value) { myobj.array.emplace_back(value.as_bool()); });
      } else if (myobj.type == SCA::eLongSequence) {
        minijson::parse_array(ctx, [&](const minijson::value& value) { myobj.array.emplace_back(value.as_longlong()); });
      } else if (myobj.type == SCA::eDoubleSequence) {
        minijson::parse_array(ctx, [&](const minijson::value& value) { myobj.array.emplace_back(value.as_double()); });
      } else if (myobj.type == SCA::eStringSequence) {
        minijson::parse_array(ctx, [&](const minijson::value& value) { myobj.array.emplace_back(value.as_string()); });
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
        auto type = magic_enum::enum_cast<SCA::PropertyTypes>(v.as_string().c_str());
        if (type.has_value()) {
          myobj.type = type.value();
        } else {
          throw minijson::parse_error(ctx, minijson::parse_error::INVALID_VALUE);
        }
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

}  // namespace

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

    test_pfr();
    // FIXME std::cerr << "obj_type has " << boost::pfr::tuple_size<obj_type>::value << " fields: " << boost::pfr::io(obj)
    // << "\n";
  } else {
    std::cerr << *argv << " filename [, ...]" << std::endl;
    return -1;
  }

  return 0;
}
