CXXFLAGS+=-std=c++2a -Wall -Wextra

.PHONY: all test clean check
all: minijson_parser

test: all
	-@for file in *.json; \
	do \
		echo "cat $${file} | ./json_pp.py | diff -u $${file} -"; \
		cat $${file} | ./json_pp.py | diff -uw $${file} -; \
	done
#TODO 		cat $${file} | ./minijson_parser | ./json_pp.py | diff -uw $${file} -; \

clean:
	${RM} minijson_parser *.exe *.o *.orig

check: build
	run-clang-tidy.py -p build -checks='-modernize-*,-misc-no-recursion'
