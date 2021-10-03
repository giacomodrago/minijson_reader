# Standard stuff

.SUFFIXES:
$(VERBOSE).SILENT:

MAKEFLAGS+= --no-builtin-rules          # Disable the built-in implicit rules.
MAKEFLAGS+= --warn-undefined-variables  # Warn when an undefined variable is referenced.

# Project config
### export CXX:=g++-11
### export CXX:=clang++

CXXFLAGS+=-std=c++20 -Wall -Wextra -Wshadow -Werror
CPPFLAGS+=-MMD #XXX -DVERBOSE
TARGET_ARCH:=
LDFLAGS:=

.PHONY: all build ctest test check format clean distclean
##########################################################

all: ctest #XXX sca_property_parser

sca_property_parser: sca_property_parser.cpp
	$(LINK.cc) -o $@ $<

test: sca_property_parser
	pwd
	ls -lrt s*.json
	@for file in s*.json; \
	do \
		echo "./sca_property_parser $${file} | ./json_pp.py | diff -u $${file} -"; \
		./sca_property_parser $${file} | ./json_pp.py | diff -u $${file} -; \
	done
#XXX 		cat $${file} | ./json_pp.py; \

clean:
	${RM} sca_property_parser *.exe *.o
	-find build/CMakeFiles -type f -name '*.cpp.o' -delete

distclean: clean
	${RM} -r build build-* *.d *.orig *~ .*~ .init

.init: .requirement.txt
	pip3 install -r .requirement.txt
	touch $@

build: .init
	cmake -B build -S . -G Ninja -D CMAKE_BUILD_TYPE=Debug
	cmake --build build

ctest: build
	cmake --build build
	cd build && ctest -C Debug --rerun-failed --verbose
	gcovr -r .

check: build
	run-clang-tidy.py -p build -checks='-modernize-use-trailing-return-type,-misc-no-recursion' sca_property_parser.cpp minijson_example.cpp

format:
	clang-format -i *.hpp *.cpp
	cmake-format -i CMakeLists.txt cmake/*.cmake

-include sca_property_parser.d
