all: build/tool build/test build/libexample.h unittest

clean:
	rm -r build

run: build/test
	./build/test Touhou-BadApple.mkv

FLAGS = -Wall -Wextra -Werror

build/tool: tool.c build/yxml.o devutils.h
	mkdir -p build
	cc $(FLAGS) -o build/tool tool.c -L./build -l :yxml.o

build/yxml.o: thirdparty/yxml.h thirdparty/yxml.c
	mkdir -p build
	cc -c -Wall -Ithirdparty/ -o build/yxml.o thirdparty/yxml.c

build/unit_test: unit_test.c tool.c
	mkdir -p build
	cc $(FLAGS) -o build/unit_test unit_test.c

unittest: build/unit_test
	./build/unit_test

build/libexample.h: build/tool
	mkdir -p build
	./build/tool

build/test: test.c build/libexample.h
	mkdir -p build
	cc $(FLAGS) -o build/test test.c
