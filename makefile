CXXFLAGS+=-std=c++14 -Wextra

.PHONY: all test clean
all: minijson_parser

test: all
	@-for file in s*.json; \
	do \
		echo $${file}; \
		cat $${file} | ./minijson_parser | ./json_pp.py | diff -uw $${file} -; \
	done

clean:
	${RM} minijson_parser *.exe *.o *.orig
