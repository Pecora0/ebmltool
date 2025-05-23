all: build/tool build/test

build/tool: tool.c build/yxml.o devutils.h
	gcc -Wall -Wextra -Werror -o build/tool tool.c -L./build -l :yxml.o

build/yxml.o: thirdparty/yxml.h thirdparty/yxml.c
	gcc -c -Wall -Ithirdparty/ -o build/yxml.o thirdparty/yxml.c

runtool: build/tool example.xml
	./build/tool

build/libexample.h: build/tool
	./build/tool

build/test: test.c build/libexample.h
	gcc -Wall -Wextra -Werror -o build/test test.c

runtest: build/test
	./build/test Touhou-BadApple.mkv
