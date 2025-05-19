all: build/tool

build/tool: tool.c build/yxml.o devutils.h
	gcc -Wall -Wextra -Werror -o build/tool tool.c -L./build -l :yxml.o

build/yxml.o: thirdparty/yxml.h thirdparty/yxml.c
	gcc -c -Wall -Ithirdparty/ -o build/yxml.o thirdparty/yxml.c

run: build/tool example.xml
	./build/tool
