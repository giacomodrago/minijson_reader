#include "minijson_reader.hpp"

#include <exception>
// TODO #include <iomanip>  // boolalpha
#include <iostream>
#include <vector>

#ifdef VERBOSE
#  define TRACE(str) (std::cerr << str << std::endl)
#  define TRACEFUNC TRACE(__PRETTY_FUNCTION__)
#else
#  define TRACE(str)
#  define TRACEFUNC
#endif

struct obj_type {
  obj_type(const char* _name = "") : name(_name){};
  std::string name;
  std::string type;
  std::string value;
  std::vector<obj_type> childs;  // NOTE: recursive data type
};

void write_structsequence(std::ostream& stream, const obj_type& property);

std::ostream& operator<<(std::ostream& rOut, const obj_type& property) {
  TRACEFUNC;

  rOut << "{ \"name\" : \"" << property.name;    // XXX start object -> '{'
  rOut << "\", \"type\" : \"" << property.type;  // NOTE: as string! CK
  rOut << "\", \"value\" : ";
  if (!property.value.empty()) { minijson::write_quoted_string(rOut, property.value.c_str(), "\n"); }

  const std::string& typeCode = property.type;
  bool isSimple = property.childs.empty();
  bool isSimpleSequence = false;
  bool isStructSequence = false;

  if (!isSimple) {
    bool isFirst = true;
    if (typeCode == "structsequence") {
      rOut << "[ [" << std::endl;
      isStructSequence = true;
    } else {
      if (typeCode != "struct") {
        rOut << "[";  // NOTE: same line! CK
        isSimpleSequence = true;
      } else {
        rOut << "[" << std::endl;
      }
    }

    const std::vector<obj_type>& children = property.childs;
    std::vector<obj_type>::const_iterator it = children.begin();
    for (; it != children.end(); ++it) {
      if (isSimpleSequence) {
        rOut << (isFirst ? "" : ", ") << (*it).value;  // NOTE: only valid for sequence of simple! CK
      } else if (isStructSequence) {
        rOut << (isFirst ? "" : "], [") << std::endl;
        write_structsequence(rOut, *it);
      } else {
        rOut << (isFirst ? "" : ", ") << *it;  // NOTE: recursion
      }

      isFirst = false;
    }

    if (isStructSequence) {
      rOut << "] ]";
    } else {
      rOut << "]";
    }
  }

  return rOut << "}";  // XXX end object -> '}'
}

void write_structsequence(std::ostream& stream, const obj_type& property) {
  TRACEFUNC;

  bool isFirst = true;
  const std::vector<obj_type>& children = property.childs;
  std::vector<obj_type>::const_iterator it = children.begin();
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
  Context& context;
  size_t counter;
  obj_type& myobj;

  explicit parse_array_nested_handler(Context& context, obj_type& obj) : context(context), counter(0), myobj(obj) {}

  ~parse_array_nested_handler() {}

  void operator()(const minijson::value& v) {
    TRACEFUNC;

    // write ',' only if needed
    if (counter) {
      // TRACE std::cout << ", ";
    }

    if (minijson::Array == v.type()) {
      // TRACE std::cout << "[" << std::endl;
      if (myobj.type == "structsequence") {
        TRACE(myobj.type);

        obj_type child;
        minijson::parse_array(context, parse_array_nested_handler(context, child));  // NOTE: recursion
        myobj.childs.push_back(child);
      } else {
        minijson::parse_array(context, parse_array_nested_handler(context, myobj));  // NOTE: recursion
      }
      // TRACE std::cout << "]" << std::endl;
    } else if (minijson::Object == v.type()) {
      // TRACE std::cout << "{" << std::endl;
      obj_type child;
      minijson::parse_object(context, parse_object_nested_handler<Context>(context, child));
      myobj.childs.push_back(child);
      // TRACE std::cout << "}" << std::endl;
    } else if (minijson::String == v.type()) {
      // TRACE minijson::write_quoted_string(std::cout, v.as_string(), "\n");
      obj_type child;
      child.value = v.as_string();
      myobj.childs.push_back(child);
    }
    // TODO else if (minijson::Boolean == v.type()) { std::cout << "\t" <<
    // std::boolalpha << v.as_bool() << std::endl; }
    else {
      throw minijson::parse_error(context, minijson::parse_error::INVALID_VALUE);
      // XXX throw std::runtime_error("unexpeced type at
      // parse_array_nested_handler()");
    }

    ++counter;
  }
};

template <typename Context>
struct parse_object_nested_handler {
  Context& context;
  size_t counter;
  obj_type& myobj;

  explicit parse_object_nested_handler(Context& context, obj_type& obj) : context(context), counter(0), myobj(obj) {}

  ~parse_object_nested_handler() {}

  void operator()(const char* name, const minijson::value& v) {
    TRACEFUNC;

    // write ',' only if needed!
    if (counter) {
      // TRACE std::cout << ", ";
    }

    if (minijson::Object == v.type()) {
      // TRACE std::cout << "\t\"" << name << "\" : {" << std::endl;
      obj_type child(name);
      minijson::parse_object(context, parse_object_nested_handler(context, child));  // NOTE: recursion
      myobj.childs.push_back(child);
      // TRACE std::cout << "} " << std::endl;
    } else if (minijson::Array == v.type()) {
      // TRACE std::cout << "\t\"" << name << "\" : [" << std::endl;
      minijson::parse_array(context, parse_array_nested_handler<Context>(context, myobj));
      // TRACE std::cout << "] " << std::endl;
    } else if (minijson::String == v.type()) {
      // TRACE std::cout << "\t\"" << name << "\" : ";
      // TRACE minijson::write_quoted_string(std::cout, v.as_string(), "\n");
      if (name == std::string("name")) {
        myobj.name = v.as_string();
      } else if (name == std::string("type")) {
        myobj.type = v.as_string();
      } else if (name == std::string("value")) {
        myobj.value = v.as_string();
      } else {
        throw minijson::parse_error(context, minijson::parse_error::INVALID_VALUE);
      }

    }
    // TODO else if (minijson::Boolean == v.type()) { std::cout << "\t\"" <<
    // name << "\" : " << std::boolalpha << v.as_bool() << std::endl; }
    else {
      throw std::runtime_error("unexpeced type at parse_object_nested_handler()");
    }

    ++counter;
  }
};

int main() {
  using namespace minijson;

  obj_type obj("");

  try {
    //=================================
    istream_context ctx(std::cin);
    // TRACE std::cout << "{" << std::endl;
    parse_object(ctx, parse_object_nested_handler<minijson::istream_context>(ctx, obj));
    // TRACE std::cout << "}" << std::endl;
    //=================================
  } catch (std::exception& e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return -1;
  }

  std::cout << obj << std::endl;

  return 0;
}

/***

$ cat simpleproperty.json | ./minijson_parser | json_pp.py
{
   "name": "property",
   "type": "float",
   "value": "3.14"
}


$ cat simplesequence.json | ./minijson_parser | json_pp.py
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


$ cat structproperty.json | ./minijson_parser | json_pp.py
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
         "type": "long",
         "value": "42"
      }
   ]
}


$ cat structsequence.json | ./minijson_parser | json_pp.py
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
            "value": "42"
         },
         {
            "name": "entry3",
            "type": "string",
            "value":
"\tUnicodeString:\"@\u20ac\u00b2\u00b3\u00df\u00c4\u00d6\u00dc\u00a7$%@#\"\r\n"
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
            "value": "123"
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
