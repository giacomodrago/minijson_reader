/***
clausklein$ make CXXFLAGS='-std=c++17 -Wextra' minijson_example
c++ -std=c++17 -Wextra    minijson_example.cpp   -o minijson_example

clausklein$ ./minijson_example | ./json_pp.py
{
   "field1": 42,
   "array": [
      1,
      2,
      3
   ],
   "field2": "asd",
   "nested": {
      "field1": 42.0,
      "field2": true,
      "ignored_field": 0,
      "ignored_object": {
         "a": [
            0
         ]
      }
   },
   "ignored_array": [
      4,
      2,
      {
         "a": 5
      },
      [
         7
      ]
   ]
}
clausklein$
 ***/

#include "minijson_reader.hpp"

#include <cassert>
#include <exception>
#include <iostream>

static char json_obj[] = "{ \"field1\": 42, \"array\" : [ 1, 2, 3 ], \"field2\": \"asd\", "
                         "\"nested\" : { \"field1\" : 42.0, \"field2\" : true, "
                         "\"ignored_field\" : 0, "
                         "\"ignored_object\" : {\"a\":[0]} },"
                         "\"ignored_array\" : [4, 2, {\"a\":5}, [7]] }";

struct obj_type {
  long long field1 = 0L;
  std::string field2;  // you can use a const char*, but in that case beware of lifetime!
  struct {
    double field1 = 0.0;
    bool field2 = false;
  } nested;
  std::vector<long> array;
};

int main() {
  using namespace minijson;

  obj_type obj;
  //=================================
  std::cout << json_obj << std::endl;
  //=================================

  try {
    buffer_context ctx(json_obj, sizeof(json_obj) - 1);
    //=================================
    parse_object(ctx, [&](const char* k, value v) {
      dispatch(k) << "field1" >> [&] { obj.field1 = v.as_longlong(); } << "field2" >>
          [&] { obj.field2 = v.as_string(); } << "nested" >>
          [&] {
            //=================================
            parse_object(ctx,
                [&](const char* k, value v)  // recursion
                {
                  dispatch(k) << "field1" >> [&] { obj.nested.field1 = v.as_double(); } << "field2" >>
                      [&] { obj.nested.field2 = v.as_bool(); } << any >> [&] { ignore(ctx); };  // ANY other OBJECT
                });
            //=================================
          } << "array" >>
          [&] {
            //=================================
            parse_array(ctx, [&](value v) { obj.array.push_back(v.as_long()); });
            //=================================
          } << any >>
          [&] { ignore(ctx); };  // ANY other OBJECT
    });
    //=================================
  } catch (std::exception& e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return -1;
  }

  //=================================
  std::vector<long> expected = {1, 2, 3};
  assert(obj.field1 == 42LL);
  assert(obj.field2 == "asd");
  assert(obj.nested.field1 == 42.0);
  assert(obj.nested.field2 == true);
  assert(obj.array == expected);
  //=================================

  return 0;
}
