# Standard stuff

.SUFFIXES:
$(VERBOSE).SILENT:

MAKEFLAGS+= --no-builtin-rules          # Disable the built-in implicit rules.
MAKEFLAGS+= --warn-undefined-variables  # Warn when an undefined variable is referenced.

# Project config
### export CXX:=g++-11
## export CXX:=clang++

CXXFLAGS+=-std=c++17 -Wall -Wextra -Wshadow -Werror
CPPFLAGS+=-MMD #XXX -DVERBOSE
TARGET_ARCH:=
LDFLAGS:=

.PHONY: all build ctest test check format clean distclean
##########################################################

all: ctest #XXX sca_property_parser

sca_property_parser: sca_property_parser.cpp
	$(LINK.cc) -o $@ $<

test: build
	pwd
	ls -lrt s*.json
	@for file in s*.json; \
	do \
		echo "build/bin/sca_property_parser $${file} | ./json_pp.py | diff -u $${file} -"; \
		build/bin/sca_property_parser $${file} | ./json_pp.py | diff -u $${file} -; \
	done
	gcovr -r .
#XXX 		cat $${file} | ./json_pp.py; \

clean:
	${RM} sca_property_parser *.exe *.o
	find build/CMakeFiles -type f -name '*.cpp.o' -delete

distclean: clean
	rm -rf build *.d *.orig *~

build:
	pip3 install -r .requirement.txt
	cmake -B build -S . -G Ninja -D CMAKE_BUILD_TYPE=Debug
	ninja -C build

ctest: build
	ninja -C build
	cd build && ctest --rerun-failed --verbose

check: build
	run-clang-tidy.py -p build -checks='-modernize-*,-misc-no-recursion' sca_property_parser.cpp minijson_example.cpp

format:
	clang-format -i *.hpp *.cpp

-include sca_property_parser.d
