#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>

#include "devutils.h"
#include "thirdparty/yxml.h"

#define SCHEMA_FILE_NAME "example.xml"
#define TARGET_FILE_NAME "build/example-lib.h"

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

typedef struct {
    Short_String name;
    Short_String path;
    Short_String id;
    Short_String type;
} Pre_EBML_Element;

typedef enum {
    MASTER,
    UINTEGER,
    UTF_8,
    STRING,
    DATE,
    BINARY,
} EBML_Type;

typedef struct {
    Short_String name;
    Short_String path;
    uint64_t id;
    EBML_Type type;
} EBML_Element;

#define MAX_ELEMENT_COUNT 32
EBML_Element element_list[MAX_ELEMENT_COUNT];
size_t element_count = 0;

void init_element(Pre_EBML_Element *elem) {
    elem->name.cstr[0] = '\0';
    elem->path.cstr[0] = '\0';
    elem->id.cstr[0]   = '\0';
    elem->type.cstr[0] = '\0';
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
                    init_element(&new);
                }
                break;
            case YXML_CONTENT:  
                break;
            case YXML_ELEMEND:
                if (in_element) {
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

    // fclose(schema_file);
    // FILE *target_file = fopen(TARGET_FILE_NAME, "w");
    // if (target_file == NULL) {
    //     printf("[ERROR] Could not open file '%s': %s\n", TARGET_FILE_NAME, strerror(errno));
    // }
    // fclose(target_file);

    for (size_t i=0; i<element_count; i++) {
        printf("%zu\t%s\t%s\t0x%lX\n", i, element_list[i].name.cstr, element_list[i].path.cstr, element_list[i].id);
    }
}
