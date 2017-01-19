#include "minijson_reader.hpp"

#include <vector>
#include <iostream>
#include <iomanip>
#include <exception>

struct obj_type
{
    obj_type(const char *_name) : name(_name) {};
    std::string name;
    std::string type;
    std::string value;
    std::vector<obj_type> childs;   //NOTE: recursive data type
};

//
// TODO write ostream operator ...
//
void write_quoted_string(std::ostream& stream, const char* str, const char *endl = "")
{
    stream << std::hex << std::right << std::setfill('0');
    stream << '"';

    while (*str != '\0')    //NOTE: NUL terminated string!
    {
        switch (*str)
        {
        case '"':
            stream << "\\\"";
            break;

        case '\\':
            stream << "\\\\";
            break;

        case '\n':
            stream << "\\n";
            break;

        case '\r':
            stream << "\\r";
            break;

        case '\t':
            stream << "\\t";
            break;

        default:
            //XXX this seems OK too!  if (std::iscntrl(*str))
            //FIXME ASCII control characters (NUL is not supported)
            if ((*str > 0 && *str < 32) || *str == 127)
            {
                stream << "\\u";
                stream.flush();
                stream << std::setw(4) << static_cast<unsigned>(*str);
            }
            else
            {
                stream << *str;
            }
            break;
        }
        str++;
    }

    stream << '"';
    stream.flush();
    stream << std::dec << endl;
}


// minijson callback interfaces:
void parse_object_empty_handler(const char* name, minijson::value);
void parse_array_empty_handler(minijson::value);

template<typename Context>
struct parse_object_nested_handler;

template<typename Context>
struct parse_array_nested_handler
{
    Context& context;
    size_t counter;

    explicit parse_array_nested_handler(Context& context) :
        context(context), counter(0) {}

    ~parse_array_nested_handler() {}

    void operator()(const minijson::value& v)
    {
        // write ',' only if needed
        if (counter) std::cout << ", ";

        if (minijson::Array == v.type()) {
            std::cout << "[" << std::endl;
            minijson::parse_array(context, parse_array_nested_handler(context));    // recursion
            std::cout << "]" << std::endl;
        }
        else if (minijson::Object == v.type())
        {
            std::cout << "{" << std::endl;
            minijson::parse_object(context, parse_object_nested_handler<Context>(context));
            std::cout << "}" << std::endl;
        }
        else if (minijson::String == v.type())
        {
            //XXX std::cout << "\t\"" << v.as_string() << "\"" << std::endl;
            write_quoted_string(std::cout, v.as_string(), "\n");
        }
        else if (minijson::Boolean == v.type()) { std::cout << "\t" << std::boolalpha << v.as_bool() << std::endl; }
        else { throw std::runtime_error("unexpeced type at parse_array_nested_handler()");  }

        ++counter;
    }
};


template<typename Context>
struct parse_object_nested_handler
{
    Context& context;
    size_t counter;

    explicit parse_object_nested_handler(Context& context) :
        context(context), counter(0) {}

    ~parse_object_nested_handler() {}

    void operator()(const char* name, const minijson::value& v)
    {
        // write ',' only if needed!
        if (counter) std::cout << ", ";

        if (minijson::Object == v.type())
        {
            std::cout << "\t\"" << name << "\" : {" << std::endl;
            minijson::parse_object(context, parse_object_nested_handler(context));  // recursion
            std::cout << "} " << std::endl;
        }
        else if (minijson::Array == v.type())
        {
            std::cout << "\t\"" << name << "\" : [" << std::endl;
            minijson::parse_array(context, parse_array_nested_handler<Context>(context));
            std::cout << "] " << std::endl;
        }
        else if (minijson::String == v.type())
        {
            std::cout << "\t\"" << name << "\" : "; //XXX \"" << v.as_string() << "\"" << std::endl;
            write_quoted_string(std::cout, v.as_string(), "\n");
        }
        else if (minijson::Boolean == v.type()) { std::cout << "\t\"" << name << "\" : " << std::boolalpha << v.as_bool() << std::endl; }
        else { throw std::runtime_error("unexpeced type at parse_object_nested_handler()");  }

        ++counter;
    }
};


int main()
{
    using namespace minijson;

    //TODO fill obj_type obj("");

    try
    {
        //=================================
        istream_context ctx(std::cin);
        std::cout << "{" << std::endl;
        parse_object(ctx, parse_object_nested_handler<minijson::istream_context>(ctx));
        std::cout << "}" << std::endl;
        //=================================
    }
    catch (std::exception &e)
    {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}

/***

$ cat simpleproperty.json | ./minijson_parser | /cygdrive/f/python/json_pp.py
{
   "name": "property",
   "type": "float",
   "value": "3.14"
}


$ cat simplesequence.json | ./minijson_parser | /cygdrive/f/python/json_pp.py
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


$ cat structproperty.json | ./minijson_parser | /cygdrive/f/python/json_pp.py
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


$ cat structsequence.json | ./minijson_parser | /cygdrive/f/python/json_pp.py
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
