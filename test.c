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

    size_t cur_type;

    //libexample_print(&parser);
    for (int c = fgetc(src_file); c != EOF; c = fgetc(src_file)) {
        // printf("[INFO] read byte 0x%X\n", c);
        libexample_return_t r = libexample_parse(&parser, (libexample_byte_t) c);
        switch (r) {
            case LIBEXAMPLE_ERR:
                //printf("0x%02X\n", c);
                printf("[ERROR] got error from library\n");
                libexample_print(&parser);
                fclose(src_file);
                exit(1);
            case LIBEXAMPLE_OK:
                //printf("0x%02X ", c);
                break;
            case LIBEXAMPLE_ELEMSTART:
                //printf("\n");
                cur_type = parser.type;
                printf("[INFO] ");
                for (size_t i=0; i<parser.this_depth-1; i++) printf("|");
                printf("+--%zu--%s--0x%lX--%s--%lu--\n", parser.this_depth, parser.name, parser.id[parser.this_depth], type_as_string[parser.type], parser.size[parser.this_depth]);
                //printf("0x%02X ", c);
                break;
            case LIBEXAMPLE_ELEMEND:
                //printf("\n");
                switch (cur_type) {
                    case 1: //uinteger
                        printf("[INFO] ");
                        for (size_t i=0; i<parser.this_depth; i++) printf("|");
                        printf("%lu\n", parser.value);
                        break;
                    case 4: //string
                        printf("[INFO] ");
                        for (size_t i=0; i<parser.this_depth; i++) printf("|");
                        printf("%s\n", parser.string_buffer);
                        break;
                    case 6: //binary
                        break;
                    case 3: //utf-8
                        //TODO: print the utf-8 as a string
                        break;
                    case 7: //float
                        //TODO: print the float
                        break;
                    case 0: //master
                        break;
                    default:
                        printf("[ERROR] got type %zu (%s)\n", cur_type, type_as_string[cur_type]);
                        UNIMPLEMENTED("handling LIBEXAMPLE ELEMEND");
                }
                //printf("0x%02X ", c);
                break;
        }
    }
    libexample_return_t r = libexample_eof(&parser);
    switch (r) {
        case LIBEXAMPLE_ERR:
            printf("[ERROR] got error from library\n");
            break;
        case LIBEXAMPLE_OK:
        case LIBEXAMPLE_ELEMSTART:
        case LIBEXAMPLE_ELEMEND:
            break;
    }

    fclose(src_file);
}
