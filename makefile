CXXFLAGS+=-std=c++2a -Wall -Wextra

.PHONY: all test clean
all: minijson_parser

test: all
	@-for file in s*.json; \
	do \
		echo $${file}; \
		cat $${file} | ./minijson_parser | ./json_pp.py | diff -uw $${file} -; \
	done
#		cat $${file} | json_pp | diff -u $${file} -; \

clean:
	${RM} minijson_parser *.exe *.o *.orig
