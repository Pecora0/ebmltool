#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "devutils.h"

#define LIBEXAMPLE_IMPLEMENTATION
#include "build/libexample.h"

int main(int argc, char **argv) {
    char *src_file_name;
    if (argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        exit(0);
    } else {
        src_file_name = argv[1];
    }

    FILE *src_file = fopen(src_file_name, "rb");
    if (src_file == NULL) {
        printf("[ERROR] Could not open file '%s': %s\n", src_file_name, strerror(errno));
        exit(1);
    }

    libexample_parser_t parser;
    libexample_init(&parser);

    //libexample_print(&parser);
    for (int c = fgetc(src_file); c != EOF; c = fgetc(src_file)) {
        // printf("[INFO] read byte 0x%X\n", c);
        libexample_return_t r = libexample_parse(&parser, (libexample_byte_t) c);
        switch (r) {
            case LIBEXAMPLE_OK:
                break;
            case LIBEXAMPLE_ELEMSTART:
                printf("[INFO] got element '%s' (0x%lX) of type '%s' and size %ld\n", parser.name, parser.id[parser.depth], type_as_string[parser.type], parser.size[parser.depth]);
                break;
            case LIBEXAMPLE_ELEMEND:
                break;
        }
    }
    libexample_return_t r = libexample_eof(&parser);
    switch (r) {
        case LIBEXAMPLE_OK:
        case LIBEXAMPLE_ELEMSTART:
        case LIBEXAMPLE_ELEMEND:
            break;
    }

    fclose(src_file);
}
