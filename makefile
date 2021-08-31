# Standard stuff

.SUFFIXES:
$(VERBOSE).SILENT:

MAKEFLAGS+= --no-builtin-rules          # Disable the built-in implicit rules.
MAKEFLAGS+= --warn-undefined-variables  # Warn when an undefined variable is referenced.

# Project config
#TODO CXX:=g++-11
CXXFLAGS+=-std=c++2a -Wall -Wextra -Wshadow
CPPFLAGS+=-MMD -DVERBOSE
TARGET_ARCH:=
LDFLAGS:=

.PHONY: all test clean check
all: minijson_parser
minijson_parser: minijson_parser.cpp
	$(LINK.cc) -o $@ $<

test: all
	-@for file in {simple,struct}*.json; \
	do \
		echo "cat $${file} | ./json_pp.py | diff -uw $${file} -"; \
		cat $${file} | ./json_pp.py; \
		cat $${file} | ./minijson_parser | ./json_pp.py; \
	done

clean:
	${RM} minijson_parser *.exe *.o *.d *.orig

check: build
	run-clang-tidy.py -p build -checks='-modernize-*,-misc-no-recursion'

-include minijson_parser.d
