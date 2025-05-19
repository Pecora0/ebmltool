#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>

#include "devutils.h"
#include "thirdparty/yxml.h"

#define SCHEMA_FILE_NAME "example.xml"
#define TARGET_LIBRARY_NAME "libexample"

#define XML_PARSE_BUFSIZE 4069
char xml_parse_buffer[XML_PARSE_BUFSIZE];

//=====================================================================
/********************************************************************
 * I took this from https://github.com/tsoding/la/blob/master/lag.c *
 * Thank you to Tsoding!                                            *
 ********************************************************************/
#define SHORT_STRING_LENGTH 64
typedef struct {
    char cstr[SHORT_STRING_LENGTH];
} Short_String;

#if defined(__GNUC__) || defined(__clang__)
#define CHECK_PRINTF_FMT(a, b) __attribute__ ((format (printf, a, b)))
#else
#define CHECK_PRINTF_FMT(...)
#endif

CHECK_PRINTF_FMT(1, 2) Short_String shortf(const char *fmt, ...)
{
    Short_String result = {0};

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(result.cstr, sizeof(result.cstr), fmt, args);
    assert(n >= 0);
    assert((size_t) n + 1 <= sizeof(result.cstr));
    va_end(args);

    return result;
}
//=========================================================================

Short_String append(Short_String str1, char *str2) {
    return shortf("%s%s", str1.cstr, str2);
}

Short_String capitalize(Short_String str) {
    for (size_t i=0; i<SHORT_STRING_LENGTH; i++) {
        if (str.cstr[i] == '\0') {
            return str;
        } else if ('a' <= str.cstr[i] && str.cstr[i] <= 'z') {
            str.cstr[i] = str.cstr[i] + ('A' - 'a');
        }
    }
    UNREACHABLE("short string has no zero termination");
}

typedef struct {
    Short_String name;
    Short_String path;
    Short_String id;
    Short_String type;
    Short_String range;
} Pre_EBML_Element;

void init_pre_element(Pre_EBML_Element *elem) {
    elem->name.cstr[0]  = '\0';
    elem->path.cstr[0]  = '\0';
    elem->id.cstr[0]    = '\0';
    elem->type.cstr[0]  = '\0';
    elem->range.cstr[0] = '\0';
}

void print_pre_element(Pre_EBML_Element elem) {
    printf("[INFO]     name  = '%s'\n", elem.name.cstr);
    printf("[INFO]     path  = '%s'\n", elem.path.cstr);
    printf("[INFO]     id    = '%s'\n", elem.id.cstr);
    printf("[INFO]     type  = '%s'\n", elem.type.cstr);
    printf("[INFO]     range = '%s'\n", elem.range.cstr);
}

typedef enum {
    MASTER,
    UINTEGER,
    UTF_8,
    STRING,
    DATE,
    BINARY,
} EBML_Type;

typedef enum {
    RANGE_NONE,
    RANGE_UPPER_BOUND,
    RANGE_LOWER_BOUND,
    RANGE_UPLOW_BOUND,
    RANGE_EXACT,
    RANGE_EXCLUDED,
} Range_Kind;

typedef struct {
    Range_Kind kind;
    uint64_t lo;
    bool lo_in;
    uint64_t hi;
    bool hi_in;
} EBML_Range;

typedef struct {
    Short_String name;
    Short_String path;
    uint64_t id;
    EBML_Type type;
    EBML_Range range;
} EBML_Element;

#define MAX_ELEMENT_COUNT 32
EBML_Element element_list[MAX_ELEMENT_COUNT];
size_t element_count = 0;

EBML_Range parse_range(Short_String str) {
    uint64_t acc = 0;
    for (size_t i=0; i<SHORT_STRING_LENGTH; i++) {
        char c = str.cstr[i];
        if (c == '\0') {
            EBML_Range result;
            if (i == 0) {
                result.kind = RANGE_NONE;
            } else {
                result.kind = RANGE_EXACT;
                result.lo = acc;
                result.hi = acc;
            }
            return result;
        } else if ('0' <= c && c <= '9') {
            acc = 10*acc + (c - '0');
        } else {
            printf("[ERROR] got character %c\n", c);
            UNIMPLEMENTED("parse_range");
        }
    }
    UNREACHABLE("Short_String is not zero terminated");
}

bool has_upper_bound(EBML_Range r) {
    return r.kind == RANGE_UPPER_BOUND || r.kind == RANGE_UPLOW_BOUND || r.kind == RANGE_EXACT;
}

size_t upper_bound(EBML_Range r) {
    switch (r.kind) {
        case RANGE_NONE:
        case RANGE_LOWER_BOUND:
        case RANGE_EXCLUDED:
            UNREACHABLE("range has no upper bound");
        case RANGE_UPPER_BOUND:
        case RANGE_UPLOW_BOUND:
            if (!r.hi_in) {
                UNIMPLEMENTED("exclusive bound");
            }
            return r.hi;
        case RANGE_EXACT:
            assert(r.hi == r.lo);
            return r.hi;
    }
    UNREACHABLE("no Range_Kind matched");
}

EBML_Element process_element(Pre_EBML_Element elem) {
    EBML_Element result = {
        .name = elem.name,
        .path = elem.path,
    };
    result.id = strtol(elem.id.cstr, NULL, 16);
    if (errno > 0) {
        printf("[ERROR] Could not parse id '%s': %s\n", elem.id.cstr, strerror(errno));
        UNIMPLEMENTED("proper id representation");
    }
    if (strcmp(elem.type.cstr, "master") == 0) {
        result.type = MASTER;
    } else if (strcmp(elem.type.cstr, "uinteger") == 0) {
        result.type = UINTEGER;
    } else if (strcmp(elem.type.cstr, "utf-8") == 0) {
        result.type = UTF_8;
    } else if (strcmp(elem.type.cstr, "string") == 0) {
        result.type = STRING;
    } else if (strcmp(elem.type.cstr, "date") == 0) {
        result.type = DATE;
    } else if (strcmp(elem.type.cstr, "binary") == 0) {
        result.type = BINARY;
    } else {
        printf("[ERROR] Unknown type '%s' in element '%s'\n", elem.type.cstr, elem.name.cstr);
        exit(1);
    }
    result.range = parse_range(elem.range);
    return result;
}

void append_element(EBML_Element elem) {
    if (element_count >= MAX_ELEMENT_COUNT) {
        UNIMPLEMENTED("element_list is full");
    }
    element_list[element_count] = elem;
    element_count++;
}

int main(void) {
    FILE *schema_file = fopen(SCHEMA_FILE_NAME, "r");
    if (schema_file == NULL) {
        printf("[ERROR] Could not open file '%s': %s\n", SCHEMA_FILE_NAME, strerror(errno));
    }

    yxml_t parser;
    yxml_init(&parser, xml_parse_buffer, XML_PARSE_BUFSIZE);
    Pre_EBML_Element new;
    bool in_element = false;
    for (int c = fgetc(schema_file); c != EOF; c = fgetc(schema_file)) {
        yxml_ret_t r = yxml_parse(&parser, c);
        switch (r) {
            case YXML_EEOF:  
            case YXML_EREF:  
            case YXML_ECLOSE:
            case YXML_ESTACK:
            case YXML_ESYN:  
                UNIMPLEMENTED("parse error handling");
            case YXML_OK:
                break;
            case YXML_ELEMSTART:
                if (strcmp(parser.elem, "element") == 0) {
                    in_element = true;
                    init_pre_element(&new);
                }
                break;
            case YXML_CONTENT:  
                break;
            case YXML_ELEMEND:
                if (in_element) {
                    // printf("[INFO] found element:\n");
                    // print_pre_element(new);
                    append_element(process_element(new));
                    in_element = false;
                }
                break;
            case YXML_ATTRSTART:
                break;
            case YXML_ATTRVAL:
                if (in_element) {
                    if (strcmp(parser.attr, "name") == 0) {
                        new.name = append(new.name, parser.data);
                    } else if (strcmp(parser.attr, "path") == 0) {
                        new.path = append(new.path, parser.data);
                    } else if (strcmp(parser.attr, "id") == 0) {
                        new.id = append(new.id, parser.data);
                    } else if (strcmp(parser.attr, "type") == 0) {
                        new.type = append(new.type, parser.data);
                    } else if (strcmp(parser.attr, "range") == 0) {
                        new.range = append(new.range, parser.data);
                    }
                }
                break;
            case YXML_ATTREND:
                break;
            case YXML_PISTART:  
                UNIMPLEMENTED("YXML_PISTART");
                break;
            case YXML_PICONTENT:
                UNIMPLEMENTED("YXML_PICONTENT");
                break;
            case YXML_PIEND:    
                UNIMPLEMENTED("YXML_PIEND");
                break;
        }
    }
    yxml_ret_t r = yxml_eof(&parser);
    if (r < 0) {
        UNIMPLEMENTED("parse error handling");
    }
    fclose(schema_file);

    Short_String target_file_name = shortf("build/%s.h", TARGET_LIBRARY_NAME);
    Short_String include_guard = capitalize(shortf("%s_H", TARGET_LIBRARY_NAME));
    Short_String implementation_guard = capitalize(shortf("%s_IMPLEMENTATION", TARGET_LIBRARY_NAME));
    // EBML_Range max_id_length_range = {
    //     .kind = RANGE_LOWER_BOUND,
    //     .lo = 4,
    //     .lo_in = true,
    // };
    // for (size_t i=0; i<element_count; i++) {
    //     if (element_list[i].id == 0x42F2) {
    //         printf("[INFO] found redefinition of element MaxIdLength\n");
    //         max_id_length_range = element_list[i].range;
    //     }
    // }
    // EBML_Range max_size_length_range = {
    //     .kind = RANGE_EXCLUDED,
    //     .lo = 0,
    //     .hi = 0,
    // };
    // for (size_t i=0; i<element_count; i++) {
    //     if (element_list[i].id == 0x42F3) {
    //         printf("[INFO] found redefinition of element MaxSizeLength\n");
    //         max_size_length_range = element_list[i].range;
    //     }
    // }

    FILE *target_file = fopen(target_file_name.cstr, "w");
    if (target_file == NULL) {
        printf("[ERROR] Could not open file '%s': %s\n", target_file_name.cstr, strerror(errno));
    }

    fprintf(target_file, "#ifndef %s\n",   include_guard.cstr);
    fprintf(target_file, "#define %s\n",   include_guard.cstr);
    fprintf(target_file, "\n");

    // header code
    Short_String byte_type_name   = shortf("%s_byte_t", TARGET_LIBRARY_NAME);
    Short_String parser_type_name = shortf("%s_parser_t", TARGET_LIBRARY_NAME);
    Short_String return_type_name = shortf("%s_return_t", TARGET_LIBRARY_NAME);
    fprintf(target_file, "typedef unsigned char %s;\n", byte_type_name.cstr);
    fprintf(target_file, "\n");
    fprintf(target_file, "typedef enum {\n");
    fprintf(target_file, "} %s;\n", return_type_name.cstr);
    fprintf(target_file, "\n");
    fprintf(target_file, "typedef struct {\n");
    fprintf(target_file, "    Parser_State state;\n");
    fprintf(target_file, "} %s;\n", parser_type_name.cstr);
    fprintf(target_file, "\n");
    fprintf(target_file, "void %s_init(%s *p);\n", TARGET_LIBRARY_NAME, parser_type_name.cstr);
    fprintf(target_file, "%s %s_parse(%s *p, %s b);\n", return_type_name.cstr, TARGET_LIBRARY_NAME, parser_type_name.cstr, byte_type_name.cstr);
    fprintf(target_file, "void %s_eof(%s *p);\n", TARGET_LIBRARY_NAME, parser_type_name.cstr);

    fprintf(target_file, "\n");
    fprintf(target_file, "#endif // %s\n", include_guard.cstr);

    fprintf(target_file, "\n");
    fprintf(target_file, "#ifdef %s\n",    implementation_guard.cstr);
    // implementation goes here
    fprintf(target_file, "#endif // %s\n", implementation_guard.cstr);

    fclose(target_file);
}
