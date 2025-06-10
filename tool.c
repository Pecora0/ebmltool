#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>

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
#define SHORT_STRING_LENGTH 128
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

bool equal(Short_String str1, Short_String str2) {
    return strncmp(str1.cstr, str2.cstr, SHORT_STRING_LENGTH) == 0;
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
    INTEGER,
    UTF_8,
    STRING,
    DATE,
    BINARY,
    FLOAT,
    EBML_TYPE_COUNT,
} EBML_Type;

const char *ebml_type_spelling[] = {
    [MASTER]   = "master",
    [UINTEGER] = "uinteger",
    [INTEGER]  = "integer",
    [UTF_8]    = "utf-8",
    [STRING]   = "string",
    [DATE]     = "date",
    [BINARY]   = "binary",
    [FLOAT]    = "float",
};
static_assert(EBML_TYPE_COUNT == sizeof(ebml_type_spelling)/sizeof(ebml_type_spelling[0]));

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
    EBML_Type type; // must be one of UINTEGER, INTEGER, FLOAT, DATE
    uint64_t lo_uint;
    double lo_float;
    bool lo_in;
    uint64_t hi_uint;
    double hi_float;
    bool hi_in;
} EBML_Range;

#define MAX_PATH_DEPTH 8

typedef struct {
    size_t depth;
    Short_String names[MAX_PATH_DEPTH];
    bool recursive[MAX_PATH_DEPTH];
    bool global[MAX_PATH_DEPTH];
    size_t min[MAX_PATH_DEPTH];
    size_t max[MAX_PATH_DEPTH];
} EBML_Path;

typedef struct {
    Short_String name;
    EBML_Path path;
    uint64_t id;
    EBML_Type type;
    EBML_Range range;
} EBML_Element;

Pre_EBML_Element global_elements[] = {
    {
        .name = {"CRC-32"},
        .path = {"\\(1-\\)CRC-32"},
        .id   = {"0xBF"},
        .type = {"binary"},
    },
    {
        .name =  {"Void"},
        .path =  {"\\(-\\)Void"},
        .id   =  {"0xEC"},
        .type =  {"binary"},
    },
};

Pre_EBML_Element default_header[] = {
    {
        .name = {"EBML"},
        .path = {"\\EBML"},
        .id   = {"0x1A45DFA3"},
        .type = {"master"},
    },
    {
        .name  = {"EBMLVersion"},
        .path  = {"\\EBML\\EBMLVersion"},
        .id    = {"0x4286"},
        .type  = {"uinteger"},
        .range = {"not 0"},
    },
    {
        .name  = {"EBMLReadVersion"},
        .path  = {"\\EBML\\EBMLReadVersion"},
        .id    = {"0x42F7"},
        .range = {"1"},
        .type  = {"uinteger"},
    },
    {
        .name  = {"EBMLMaxIDLength"},
        .path  = {"\\EBML\\EBMLMaxIDLength"},
        .id    = {"0x42F2"},
        .range = {">=4"},
        .type  = {"uinteger"},
    },
    {
        .name  = {"EBMLMaxSizeLength"},
        .path  = {"\\EBML\\EBMLMaxSizeLength"},
        .id    = {"0x42F3"},
        .range = {"not 0"},
        .type  = {"uinteger"},
    },
    {
        .name = {"DocType"},
        .path = {"\\EBML\\DocType"},
        .id   = {"0x4282"},
        .type = {"string"},
    },
    {
        .name  = {"DocTypeVersion"},
        .path  = {"\\EBML\\DocTypeVersion"},
        .id    = {"0x4287"},
        .range = {"not 0"},
        .type  = {"uinteger"},
    },
    {
        .name  = {"DocTypeReadVersion"},
        .path  = {"\\EBML\\DocTypeReadVersion"},
        .id    = {"0x4285"},
        .range = {"not 0"},
        .type  = {"uinteger"},
    },
/*
    {

   name:  DocTypeExtension

   path:  "\EBML\DocTypeExtension"

   id:  0x4281

   description:  A DocTypeExtension adds extra Elements to the main
      DocType+DocTypeVersion tuple it's attached to.  An EBML Reader MAY
      know these extra Elements and how to use them.  A DocTypeExtension
      MAY be used to iterate between experimental Elements before they
      are integrated into a regular DocTypeVersion.  Reading one
      DocTypeExtension version of a DocType+DocTypeVersion tuple doesn't
      imply one should be able to read upper versions of this
      DocTypeExtension.

    {

   name:  DocTypeExtensionName

   path:  "\EBML\DocTypeExtension\DocTypeExtensionName"

   id:  0x4283

   length:  >0

   type:  String

   description:  The name of the DocTypeExtension to differentiate it
      from other DocTypeExtensions of the same DocType+DocTypeVersion
      tuple.  A DocTypeExtensionName value MUST be unique within the
      EBML Header.

    {

   name:  DocTypeExtensionVersion

   path:  "\EBML\DocTypeExtension\DocTypeExtensionVersion"

   id:  0x4284

   range:  not 0

   type:  Unsigned Integer

   description:  The version of the DocTypeExtension.  Different
      DocTypeExtensionVersion values of the same DocType +
      DocTypeVersion + DocTypeExtensionName tuple MAY contain completely
      different sets of extra Elements.  An EBML Reader MAY support
      multiple versions of the same tuple, only one version of the
      tuple, or not support the tuple at all.
// */
};

#define MAX_ELEMENT_COUNT 512
EBML_Element element_list[MAX_ELEMENT_COUNT];
size_t element_count = 0;

EBML_Range parse_range_exact(Short_String str) {
    EBML_Range result = {
        .kind = RANGE_EXACT
    };
    bool number_found = false;
    size_t i=0;
    while (i<SHORT_STRING_LENGTH) {
        char c = str.cstr[i];
        if (c == '\0') {
            if (!number_found) return (EBML_Range) {.kind = RANGE_NONE};
            return result;
        } else if (c == ' ') {
            i++;
        } else {
            if (number_found) {
                // there was a previous number and now we encounter something different
                // so we return an error
                return (EBML_Range) {.kind = RANGE_NONE};
            }
            number_found = true;
            char *endptr_floot;
            double floot = strtod(str.cstr + i, &endptr_floot);
            char *endptr_uint;
            long uint = strtol(str.cstr + i, &endptr_uint, 10);
            if (endptr_floot == str.cstr + i && endptr_floot == str.cstr + i) {
                // strtod and strtol did not parse anything
                return (EBML_Range) {.kind = RANGE_NONE};
            } else if (endptr_floot > endptr_uint) {
                result.type = FLOAT;
                result.lo_float = floot;
                result.hi_float = floot;
                i = endptr_floot - str.cstr;
            } else if (endptr_floot == endptr_uint) {
                assert(uint >= 0);
                result.type = UINTEGER;
                result.lo_uint = uint;
                result.hi_uint = uint;
                i = endptr_uint - str.cstr;
            } else {
                UNIMPLEMENTED("parse_range_exact");
            }
        }
    }
    UNREACHABLE("short string not zero terminated");
}

EBML_Range parse_range_not(Short_String str) {
    EBML_Range result = {
        .kind = RANGE_EXCLUDED,
    };
    bool not_found = false;
    bool number_found = false;
    size_t i=0;
    while (i<SHORT_STRING_LENGTH) {
        char c = str.cstr[i];
        if (c == '\0') {
            if (!(not_found && number_found)) return (EBML_Range) {.kind = RANGE_NONE};
            return result;
        } else if (c == ' ') {
            i++;
        } else if (c == 'n') {
            if (strncmp(str.cstr + i, "not", 3) == 0) {
                not_found = true;
                i += 3;
            } else {
                return (EBML_Range) {.kind = RANGE_NONE};
            }
        } else {
            if (!not_found) return (EBML_Range) {.kind = RANGE_NONE};
            number_found = true;
            char *endptr_floot;
            double floot = strtod(str.cstr + i, &endptr_floot);
            char *endptr_uint;
            long uint = strtol(str.cstr + i, &endptr_uint, 10);
            if (endptr_floot == str.cstr + i && endptr_floot == str.cstr + i) {
                // strtod and strtol did not parse anything
                return (EBML_Range) {.kind = RANGE_NONE};
            } else if (endptr_floot > endptr_uint) {
                result.type = FLOAT;
                result.lo_float = floot;
                result.hi_float = floot;
                i = endptr_floot - str.cstr;
            } else if (endptr_floot == endptr_uint) {
                assert(uint >= 0);
                result.type = UINTEGER;
                result.lo_uint = uint;
                result.hi_uint = uint;
                i = endptr_uint - str.cstr;
            } else {
                UNIMPLEMENTED("parse_range_exact");
            }
        }
    }
    UNREACHABLE("short string not zero terminated");
}

EBML_Range parse_range_comma(Short_String str) {
    EBML_Range result = {
        .kind = RANGE_UPLOW_BOUND
    };
    bool rel_found = false;
    bool number_found = false;
    bool comma_found = false;
    size_t i=0;
    while (i<SHORT_STRING_LENGTH) {
        char c = str.cstr[i];
        if (c == '\0') {
            if (!rel_found || !number_found || !comma_found) return (EBML_Range) {.kind = RANGE_NONE};
            return result;
        } else if (c == ' ') {
            i++;
        } else if (c == '>') {
            if (rel_found || number_found || comma_found) return (EBML_Range) {.kind = RANGE_NONE};
            rel_found = true;
            if (strncmp(str.cstr + i, ">=", 2) == 0) {
                result.lo_in = true;
                i += 2;
            } else {
                result.lo_in = false;
                i += 1;
            }
        } else if (c == '<') {
            if (rel_found || number_found || !comma_found) return (EBML_Range) {.kind = RANGE_NONE};
            rel_found = true;
            if (strncmp(str.cstr + i, "<=", 2) == 0) {
                result.hi_in = true;
                i += 2;
            } else {
                result.hi_in = false;
                i += 1;
            }
        } else if (c == ',') {
            if (!rel_found || !number_found || comma_found) return (EBML_Range) {.kind = RANGE_NONE};
            rel_found = false;
            number_found = false;
            comma_found = true;
            i++;
        } else {
            if (!rel_found || number_found) return (EBML_Range) {.kind = RANGE_NONE};
            number_found = true;
            if (!comma_found) {
                //we try to parse the lower bound value
                char *endptr_floot;
                double floot = strtod(str.cstr + i, &endptr_floot);
                char *endptr_uint;
                long uint = strtol(str.cstr + i, &endptr_uint, 10);
                if (endptr_floot == str.cstr + i && endptr_floot == str.cstr + i) {
                    // strtod and strtol did not parse anything
                    return (EBML_Range) {.kind = RANGE_NONE};
                } else if (endptr_floot > endptr_uint) {
                    result.type = FLOAT;
                    result.lo_float = floot;
                    i = endptr_floot - str.cstr;
                } else if (endptr_floot == endptr_uint) {
                    assert(uint >= 0);
                    result.type = UINTEGER;
                    result.lo_uint = uint;
                    i = endptr_uint - str.cstr;
                } else {
                    UNIMPLEMENTED("parse_range_comma");
                }
            } else {
                //we try to parse the upper bound value
                switch (result.type) {
                    case UINTEGER:
                        UNIMPLEMENTED("parse_range_comma: upper bound value UINTEGER");
                    case FLOAT:
                        char *endptr_floot;
                        double floot = strtof(str.cstr + i, &endptr_floot);
                        if (endptr_floot == str.cstr + i) {
                            // strtol did not parse anything
                            return (EBML_Range) {.kind = RANGE_NONE};
                        } else {
                            result.hi_float = floot;
                            i = endptr_floot - str.cstr;
                        }
                        break;
                    default:
                        UNREACHABLE("invalid type value for range");
                }
            }
        }
    }
    UNREACHABLE("short string not zero terminated");
}

EBML_Range parse_range(Short_String str) {
    EBML_Range exact = parse_range_exact(str);
    if (exact.kind != RANGE_NONE) return exact;
    EBML_Range not = parse_range_not(str);
    if (not.kind != RANGE_NONE) return not;
    EBML_Range comma = parse_range_comma(str);
    if (comma.kind != RANGE_NONE) return comma;
    EBML_Range op_number = {
        .kind = RANGE_NONE,
        .type = UINTEGER,
        .lo_uint = 0,
        .hi_uint = UINT64_MAX,
    };
    EBML_Range hyphen_range = {
        .kind = RANGE_NONE,
        .type = UINTEGER,
        .lo_in = false,
        .hi_in = false,
    };
    size_t i = 0;
    while (i < SHORT_STRING_LENGTH) {
        char c = str.cstr[i];
        if (c == '\0') {
            if (i == 0) {
                EBML_Range result = {
                    .kind = RANGE_NONE,
                };
                return result;
            }
            if (hyphen_range.kind != RANGE_NONE) {
                return hyphen_range;
            }
            if (op_number.kind != RANGE_NONE) {
                return op_number;
            }
            printf("[ERROR] got string '%s'\n", str.cstr);
            UNIMPLEMENTED("parse_range: end of string");
        } else if (c == ' ') {
            i++;
        } else if (c == 'n') {
            if (strncmp(str.cstr + i, "not", 3) == 0) {
                op_number.kind    = RANGE_NONE;
                hyphen_range.kind = RANGE_NONE;
                i += 3;
            } else {
                printf("[ERROR] got character '%c' in string '%s'\n", c, str.cstr);
                UNIMPLEMENTED("parse_range: contains character 'n'");
            }
        } else if (c == ',') {
            op_number.kind    = RANGE_NONE;
            hyphen_range.kind = RANGE_NONE;
            i++;
        } else if (c == '-') {
            hyphen_range.kind = RANGE_UPLOW_BOUND;
            i++;
        } else if (c == '>') {
            if (strncmp(str.cstr + i, ">=", 2) == 0) {
                op_number.kind    = RANGE_LOWER_BOUND;
                hyphen_range.kind = RANGE_NONE;
                op_number.lo_in = true;
                i += 2;
            } else {
                op_number.kind    = RANGE_LOWER_BOUND;
                hyphen_range.kind = RANGE_NONE;
                op_number.lo_in = false;
                i += 1;
            }
        } else if (c == '<') {
            if (strncmp(str.cstr + i, "<=", 2) == 0) {
                UNIMPLEMENTED("parse_range: contains operator '<='");
            } else {
                UNIMPLEMENTED("parse_range: contains operator '<'");
            }
        } else if ('0' <= c && c <= '9') {
            char *endptr_floot;
            double floot = strtod(str.cstr + i, &endptr_floot);
            char *endptr_uint;
            long uint = strtol(str.cstr + i, &endptr_uint, 10);
            if (endptr_floot == str.cstr + i) {
                UNIMPLEMENTED("no conversion to double performed");
            } else if (endptr_floot > endptr_uint) {
                op_number.lo_float = floot;
                op_number.hi_float = floot;
                op_number.type = FLOAT;
                if (hyphen_range.lo_in) {
                    hyphen_range.hi_float = floot;
                    hyphen_range.hi_in = true;
                } else {
                    hyphen_range.lo_float = floot;
                    hyphen_range.lo_in = true;
                }
                hyphen_range.type = FLOAT;
                i = endptr_floot - str.cstr;
            } else if (endptr_uint == str.cstr + i) {
                UNIMPLEMENTED("no conversion to long performed");
            } else {
                assert(uint >= 0);
                op_number.lo_uint = uint;
                op_number.hi_uint = uint;
                if (hyphen_range.lo_in) {
                    hyphen_range.hi_uint = uint;
                    hyphen_range.hi_in = true;
                } else {
                    hyphen_range.lo_uint = uint;
                    hyphen_range.lo_in = true;
                }
                i = endptr_uint - str.cstr;
            }
        } else {
            printf("[ERROR] got character '%c' in string '%s'\n", c, str.cstr);
            UNIMPLEMENTED("parse_range: unknown character");
        }
    }
    UNREACHABLE("parse_range: string not zero terminated");
}

bool is_valid_name(char *c) {
    if (!isalnum(*c)) return false;
    for (;*c != '\0'; c++) {
        if (!isalnum(*c) && *c != '-' && *c != '.') return false;
    }
    return true;
}

EBML_Path parse_path(Short_String str) {
    EBML_Path result = {
        .depth = 0,
    };
    char *path_delim = "\\";
    for (char *path_elem = strtok(str.cstr, path_delim); path_elem != NULL; path_elem = strtok(NULL, path_delim)) {
        assert(result.depth < MAX_PATH_DEPTH);
        if (path_elem[0] == ')') {
            // the path_elem in the previous iteration was a GlobalPlaceholder
            path_elem++;
        }

        if (path_elem[0] == '+') {
            result.names[result.depth] = shortf("%s", path_elem + 1);
            result.recursive[result.depth] = true;
            result.global[result.depth] = false;
            result.depth++;
        } else if (path_elem[0] == '(') {
            result.global[result.depth] = true;
            path_elem++;
            if (path_elem[0] == '-') {
                result.min[result.depth] = 0;
                path_elem++;
            } else {
                assert('0' <= path_elem[0] && path_elem[0] <= '9');
                result.min[result.depth] = strtol(path_elem, &path_elem, 10);
                assert(path_elem[0] == '-');
                path_elem++;
            }
            if ('0' <= path_elem[0] && path_elem[0] <= '9') {
                result.max[result.depth] = strtol(path_elem, &path_elem, 10);
            } else {
                assert(path_elem[0] == '\0');
                result.max[result.depth] = SIZE_MAX;
            }
            result.depth++;
        } else if (is_valid_name(path_elem)) {
            result.names[result.depth] = shortf("%s", path_elem + 0);
            result.recursive[result.depth] = false;
            result.global[result.depth] = false;
            result.depth++;
        } else {
            printf("[ERROR] not a valid path element '%s'\n", path_elem);
            UNIMPLEMENTED("proper path parsing");
        }
    }
    return result;
}

void path_print(EBML_Path path) {
    printf("[INFO] ");
    if (path.depth == 0) {
        printf("\\");
    }
    for (size_t i=0; i<path.depth; i++) {
        if (path.global[i]) {
            printf("\\(%zu-%zu\\)", path.min[i], path.max[i]);
        } else {
            printf("\\");
            if (path.recursive[i]) printf("+");
            printf("%s", path.names[i].cstr);
        }
    }
    printf("\n");
}

bool is_parent_of(EBML_Path parent, EBML_Path child) {
    if (child.depth != parent.depth + 1) return false;
    for (size_t i=0; i<parent.depth; i++) {
        if (!equal(parent.names[i], child.names[i])) return false;
    }
    return true;
}

bool has_upper_bound(EBML_Range r) {
    return r.kind == RANGE_UPPER_BOUND || r.kind == RANGE_UPLOW_BOUND || r.kind == RANGE_EXACT;
}

size_t upper_bound(EBML_Range r) {
    assert(r.type == UINTEGER);
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
            return r.hi_uint;
        case RANGE_EXACT:
            assert(r.hi_uint == r.lo_uint);
            return r.hi_uint;
    }
    UNREACHABLE("no Range_Kind matched");
}

EBML_Type parse_type(Short_String type) {
    for (EBML_Type i=0; i<EBML_TYPE_COUNT; i++) {
        if (strcmp(type.cstr, ebml_type_spelling[i]) == 0) return i;
    }
    printf("[ERROR] Unknown type '%s'\n", type.cstr);
    exit(1);
}

EBML_Element process_element(Pre_EBML_Element elem) {
    EBML_Element result = {
        .name = elem.name,
        .path = parse_path(elem.path),
    };
    result.id = strtol(elem.id.cstr, NULL, 16);
    if (errno > 0) {
        printf("[ERROR] Could not parse id '%s': %s\n", elem.id.cstr, strerror(errno));
        UNIMPLEMENTED("proper id representation");
    }
    result.type = parse_type(elem.type);
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

void insert_element(EBML_Element elem) {
    for (size_t i=0; i<element_count; i++) {
        if (elem.id == element_list[i].id) {
            printf("[INFO] redefining element '%s'\n", element_list[i].name.cstr);
            element_list[i] = elem;
            return;
        }
    }
    append_element(elem);
}

#define line() fprintf(target_file, "\n")

CHECK_PRINTF_FMT(3, 4) void print_line(FILE *stream, int depth, char *format, ...) {
    fprintf(stream, "%*s", depth*4, "");

    va_list args;
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);

    fprintf(stream, "\n");
}

#define MAX_STACK_SIZE 8
#define STRING_BUFFER_SIZE 1024
#define PREFIX      TARGET_LIBRARY_NAME
#define PREFIX_CAPS capitalize(shortf("%s", PREFIX))

typedef enum {
    API_TYPE_VOID,
    API_TYPE_RETURN,
    API_TYPE_BYTE,
    API_TYPE_PARSER,
    API_TYPE_TYPE,
    API_TYPE_COUNT,
} Api_Type;

const char *api_type_name[] = {
    [API_TYPE_VOID]   = "void",
    [API_TYPE_RETURN] = PREFIX "_return_t",
    [API_TYPE_BYTE]   = PREFIX "_byte_t",
    [API_TYPE_PARSER] = PREFIX "_parser_t",
    [API_TYPE_TYPE]   = "size_t",
};
static_assert(sizeof(api_type_name)/sizeof(api_type_name[0]) == API_TYPE_COUNT);

typedef enum {
    API_RETURN_VALUE_ERROR,
    API_RETURN_VALUE_OK,
    API_RETURN_VALUE_START,
    API_RETURN_VALUE_END,
    API_RETURN_VALUE_COUNT,
} Api_Return_Value;

const char *api_return_value_suffix[] = {
    [API_RETURN_VALUE_ERROR] = "ERR",
    [API_RETURN_VALUE_OK]    = "OK",
    [API_RETURN_VALUE_START] = "ELEMSTART",
    [API_RETURN_VALUE_END]   = "ELEMEND",
};
static_assert(sizeof(api_return_value_suffix)/sizeof(api_return_value_suffix[0]) == API_RETURN_VALUE_COUNT);

int api_return_value_number[] = {
    [API_RETURN_VALUE_ERROR] = -1,
    [API_RETURN_VALUE_OK]    = 0,
    [API_RETURN_VALUE_START] = 1,
    [API_RETURN_VALUE_END]   = 2,
};
static_assert(sizeof(api_return_value_number)/sizeof(api_return_value_number[0]) == API_RETURN_VALUE_COUNT);

Short_String api_return_value_name(Api_Return_Value v) {
    return shortf("%s_%s", PREFIX_CAPS.cstr, api_return_value_suffix[v]);
}

void define_parser_type(FILE *f) {
    print_line(f, 0, "typedef struct {");
    // fields meant for internal usage, the library user should not be concerned about them
    print_line(f, 1,     "size_t offset;");
    print_line(f, 1,     "size_t depth;");
    print_line(f, 1,     "size_t id_offset[%d];", MAX_STACK_SIZE);
    print_line(f, 1,     "size_t size_offset[%d];", MAX_STACK_SIZE);
    print_line(f, 1,     "size_t body_offset[%d];", MAX_STACK_SIZE);
    print_line(f, 1,     "uint64_t id[%d];", MAX_STACK_SIZE);
    print_line(f, 1,     "uint64_t size[%d];", MAX_STACK_SIZE);
    // fields meant for the user to extract information
    print_line(f, 1,     "size_t this_depth;");
    print_line(f, 1,     "char *name;");
    print_line(f, 1,     "%s type;", api_type_name[API_TYPE_TYPE]);
    print_line(f, 1,     "uint64_t value;");
    print_line(f, 1,     "size_t string_length;");
    print_line(f, 1,     "char string_buffer[%d];", STRING_BUFFER_SIZE);
    print_line(f, 0, "} %s;", api_type_name[API_TYPE_PARSER]);
}

void define_api_type(FILE *f, Api_Type t) {
    switch (t) {
        case API_TYPE_TYPE:
        case API_TYPE_VOID:
            return;
        case API_TYPE_RETURN:
            print_line(f, 0, "typedef enum {");
            for (size_t i=0; i<API_RETURN_VALUE_COUNT; i++) {
                print_line(f, 1, "%s = %d,", api_return_value_name(i).cstr, api_return_value_number[i]);
            }
            print_line(f, 0, "} %s;", api_type_name[API_TYPE_RETURN]);
            return;
        case API_TYPE_BYTE:
            print_line(f, 0, "typedef unsigned char %s;", api_type_name[API_TYPE_BYTE]);
            return;
        case API_TYPE_PARSER:
            define_parser_type(f);
            return;
        case API_TYPE_COUNT:
            UNREACHABLE("API_TYPE_COUNT is not a valid Api_Type");
    }
    UNREACHABLE("no valid Api_Type");
}

typedef enum {
    API_FUNC_INIT,
    API_FUNC_PARSE,
    API_FUNC_EOF,
    API_FUNC_PRINT,
    API_FUNC_COUNT,
} Api_Func;

const char *api_func_suffix[] = {
    [API_FUNC_INIT]  = "init",
    [API_FUNC_PARSE] = "parse",
    [API_FUNC_EOF]   = "eof",
    [API_FUNC_PRINT] = "print",
};
static_assert(sizeof(api_func_suffix)/sizeof(api_func_suffix[0]) == API_FUNC_COUNT);

Api_Type api_func_return[] = {
    [API_FUNC_INIT]  = API_TYPE_VOID,
    [API_FUNC_PARSE] = API_TYPE_RETURN,
    [API_FUNC_EOF]   = API_TYPE_RETURN,
    [API_FUNC_PRINT] = API_TYPE_VOID,
};
static_assert(sizeof(api_func_return)/sizeof(api_func_return[0]) == API_FUNC_COUNT);

Short_String api_func_name(Api_Func f) {
    return shortf("%s_%s", PREFIX, api_func_suffix[f]);
}

Short_String api_func_params(Api_Func f) {
    switch (f) {
        case API_FUNC_PARSE:
            return shortf("%s *p, %s b", api_type_name[API_TYPE_PARSER], api_type_name[API_TYPE_BYTE]);
        case API_FUNC_INIT:
        case API_FUNC_EOF:
        case API_FUNC_PRINT:
            return shortf("%s *p", api_type_name[API_TYPE_PARSER]);
        case API_FUNC_COUNT:
            UNREACHABLE("API_FUNC_COUNT is not a valid Api_Func");
    }
    UNREACHABLE("no valid Api_Func");
}

Short_String api_func_signature(Api_Func f) {
    return shortf("%s %s(%s)", api_type_name[api_func_return[f]], api_func_name(f).cstr, api_func_params(f).cstr);
}

void implement_vint_length(FILE *f) {
    print_line(f, 0, "size_t vint_length(%s b) {", api_type_name[API_TYPE_BYTE]);
    print_line(f, 1,     "if (b == 0) UNIMPLEMENTED(\"zero byte in vint_length\");");
    print_line(f, 1,     "size_t acc = 1;");
    print_line(f, 1,     "for (%s mark = 0x80; (mark & b) == 0; mark>>=1) acc++;", api_type_name[API_TYPE_BYTE]);
    print_line(f, 1,     "return acc;");
    print_line(f, 0, "}");
}

void implement_drop_first_active_bit(FILE *f) {
    print_line(f, 0, "%s drop_first_active_bit(%s x) {", api_type_name[API_TYPE_BYTE], api_type_name[API_TYPE_BYTE]);
    print_line(f, 0, "    %s mask = ~0;", api_type_name[API_TYPE_BYTE]);
    print_line(f, 0, "    while (mask >= x) mask >>= 1;");
    print_line(f, 0, "    return mask & x;");
    print_line(f, 0, "}");
}

void implement_type(FILE *f) {
    print_line(f, 0, "int type(%s *p) {", api_type_name[API_TYPE_PARSER]);
    print_line(f, 0, "    if (p->depth == 0) return %d;", MASTER);
    print_line(f, 0, "    uint64_t id = p->id[p->depth];");
    print_line(f, 0, "    switch (id) {");
    for (size_t i=0; i<element_count; i++) {
        print_line(f, 2, "case 0x%lX: return %d;", element_list[i].id, element_list[i].type);
    }
    print_line(f, 0, "    }");
    print_line(f, 0, "    return -1;");
    print_line(f, 0, "}");
}

void implement_name(FILE *f) {
    print_line(f, 0, "char *name(%s *p) {", api_type_name[API_TYPE_PARSER]);
    print_line(f, 0, "    assert(p->depth > 0);");
    print_line(f, 0, "    uint64_t id = p->id[p->depth];");
    print_line(f, 0, "    switch (id) {");
    for (size_t i=0; i<element_count; i++) {
        print_line(f, 2, "case 0x%lX: return \"%s\";", element_list[i].id, element_list[i].name.cstr);
    }
    print_line(f, 0, "    }");
    print_line(f, 0, "    printf(\"[ERROR] got id 0x%%lX\\n\", id);");
    print_line(f, 0, "    UNREACHABLE(\"name: unknown id\");");
    print_line(f, 0, "}");
}

void implement_init_func(FILE *f) {
    print_line(f, 0, "%s {\n", api_func_signature(API_FUNC_INIT).cstr);
    print_line(f, 1,     "p->offset = -1;");
    print_line(f, 1,     "p->depth = 0;");
    print_line(f, 1,     "for (size_t i=0; i<%d; i++) {", MAX_STACK_SIZE);
    print_line(f, 1,     "    p->id_offset[i] = -1;");
    print_line(f, 1,     "    p->size_offset[i] = -1;");
    print_line(f, 1,     "    p->body_offset[i] = -1;");
    print_line(f, 1,     "}");
    print_line(f, 1,     "p->body_offset[0] = 0;");
    print_line(f, 1,     "p->value = 0;");
    print_line(f, 1,     "p->string_length = 0;");
    print_line(f, 0, "}\n");
}

void implement_incdepth_func(FILE *f) {
    print_line(f, 0, "void incdepth(%s *p) {", api_type_name[API_TYPE_PARSER]);
    print_line(f, 0, "    assert(p->depth < %d);", MAX_STACK_SIZE);
    print_line(f, 0, "    p->depth++;");
    print_line(f, 0, "    p->id_offset[p->depth]   = -1;");
    print_line(f, 0, "    p->size_offset[p->depth] = -1;");
    print_line(f, 0, "    p->body_offset[p->depth] = -1;");
    print_line(f, 0, "}");
}

void implement_decdepth_func(FILE *f) {
    print_line(f, 0, "void decdepth(%s *p) {", api_type_name[API_TYPE_PARSER]);
    print_line(f, 0, "    assert(p->depth > 0);");
    print_line(f, 0, "    p->depth--;");
    print_line(f, 0, "}");
}

void implement_parse_func(FILE *f) {
    print_line(f, 0, "%s {\n", api_func_signature(API_FUNC_PARSE).cstr);
    print_line(f, 0, "    p->offset++;");
    print_line(f, 0, "    if (p->depth == 0) {");
    print_line(f, 0, "        incdepth(p);");
    print_line(f, 0, "        p->id_offset[p->depth]   = p->offset;");
    print_line(f, 0, "        p->size_offset[p->depth] = p->offset + vint_length(b);");
    print_line(f, 0, "        p->id[p->depth] = b;");
    print_line(f, 0, "    }");
    print_line(f, 0, "    assert(p->depth > 0);");
    print_line(f, 0, "    assert(p->id_offset[p->depth] < p->size_offset[p->depth]);");
    print_line(f, 0, "    assert(p->size_offset[p->depth] < p->body_offset[p->depth]);");
    print_line(f, 0, "    if (p->offset <= p->id_offset[p->depth]) {");
    print_line(f, 0, "        p->id_offset[p->depth] = p->offset;");
    print_line(f, 0, "        p->size_offset[p->depth] = p->offset + vint_length(b);");
    print_line(f, 0, "        p->id[p->depth] = b;");
    print_line(f, 0, "    } else if (p->offset < p->size_offset[p->depth]) {");
    print_line(f, 0, "        p->id[p->depth] = (p->id[p->depth] << 8) + b;");
    print_line(f, 0, "    } else if (p->offset == p->size_offset[p->depth]) {");
    print_line(f, 0, "        p->body_offset[p->depth] = p->offset + vint_length(b);");
    print_line(f, 0, "        p->size[p->depth] = drop_first_active_bit(b);");
    print_line(f, 0, "    } else if (p->offset < p->body_offset[p->depth]) {");
    print_line(f, 0, "        p->size[p->depth] = (p->size[p->depth] << 8) + b;");
    print_line(f, 0, "    } else if (p->offset == p->body_offset[p->depth] + p->size[p->depth]) {");
    print_line(f, 0, "        while (p->offset == p->body_offset[p->depth] + p->size[p->depth]) decdepth(p);");
    print_line(f, 0, "        if (p->offset > p->body_offset[p->depth] + p->size[p->depth]) {");
    //TODO: weird bug leads here that occurs only sometimes, possibly an uninitialized field?
    print_line(f, 0, "            printf(\"[ERROR] depth:       %%zu\\n\", p->depth);");
    print_line(f, 0, "            printf(\"[ERROR] offset:      %%zu\\n\", p->offset);");
    print_line(f, 0, "            printf(\"[ERROR] body_offset: %%zu\\n\", p->body_offset[p->depth]);");
    print_line(f, 0, "            printf(\"[ERROR] size:        %%zu\\n\", p->size[p->depth]);");
    print_line(f, 0, "            UNIMPLEMENTED(\"jumping out of nesting\");");
    print_line(f, 0, "        }");
    print_line(f, 0, "        incdepth(p);");
    print_line(f, 0, "        p->id_offset[p->depth] = p->offset;");
    print_line(f, 0, "        p->size_offset[p->depth] = p->offset + vint_length(b);");
    print_line(f, 0, "        p->id[p->depth] = b;");
    print_line(f, 0, "        return %s;", api_return_value_name(API_RETURN_VALUE_END).cstr);
    print_line(f, 0, "    } else if (p->offset == p->body_offset[p->depth]) {");
    print_line(f, 0, "        int t = type(p);"); 
    print_line(f, 0, "        if (t>=0) {");
    print_line(f, 0, "            p->type = t;"); 
    print_line(f, 0, "        } else {");
    print_line(f, 0, "            return %s;", api_return_value_name(API_RETURN_VALUE_ERROR).cstr);
    print_line(f, 0, "        }");
    print_line(f, 0, "        p->name = name(p);");
    print_line(f, 0, "        p->this_depth = p->depth;");
    print_line(f, 0, "        switch (p->type) {");
    print_line(f, 0, "            case %d:", MASTER);
    print_line(f, 0, "                if (p->size[p->depth] == 0) {");
    print_line(f, 0, "                    UNIMPLEMENTED(\"zero size master element\");");
    print_line(f, 0, "                }");
    print_line(f, 0, "                incdepth(p);");
    print_line(f, 0, "                p->id_offset[p->depth]   = p->offset;");
    print_line(f, 0, "                p->size_offset[p->depth] = p->offset + vint_length(b);");
    print_line(f, 0, "                p->id[p->depth] = b;");
    print_line(f, 0, "                break;");
    print_line(f, 0, "            case %d:", UINTEGER);
    print_line(f, 0, "                assert(p->size[p->depth] <= 8);");
    print_line(f, 0, "                p->value = b;");
    print_line(f, 0, "                break;");
    print_line(f, 0, "            case %d:", STRING);
    print_line(f, 2, "                assert(p->size[p->depth] <= %d);", STRING_BUFFER_SIZE);
    print_line(f, 0, "                p->string_buffer[0] = b;");
    print_line(f, 2, "                p->string_length = 1;");
    print_line(f, 0, "                p->string_buffer[1] = '\\0';");
    print_line(f, 0, "                break;");
    print_line(f, 0, "            case %d:", BINARY);
    print_line(f, 0, "                break;;");
    print_line(f, 0, "            case %d:", UTF_8);
    print_line(f, 0, "                break;");
    print_line(f, 0, "            case %d:", FLOAT);
    // TODO: collect the float into a value in the parser struct
    print_line(f, 0, "                break;");
    print_line(f, 0, "            default:"); 
    print_line(f, 0, "                printf(\"[ERROR] got type %%zu (%%s)\\n\", p->type, type_as_string[p->type]);");
    print_line(f, 0, "                UNREACHABLE(\"first of body: unknown type\");");
    print_line(f, 0, "        }");
    print_line(f, 0, "        return %s;", api_return_value_name(API_RETURN_VALUE_START).cstr);
    print_line(f, 0, "    } else if (p->offset < p->body_offset[p->depth] + p->size[p->depth]) {");
    print_line(f, 0, "        switch (p->type) {");
    print_line(f, 0, "            case %d:", UINTEGER);
    print_line(f, 0, "                p->value = (p->value << 8) + b;");
    print_line(f, 0, "                break;");
    print_line(f, 0, "            case %d:", STRING);
    print_line(f, 0, "                p->string_buffer[p->string_length] = b;");
    print_line(f, 0, "                p->string_length++;");
    print_line(f, 0, "                p->string_buffer[p->string_length] = '\\0';");
    print_line(f, 0, "                break;");
    print_line(f, 0, "            case %d:", BINARY);
    print_line(f, 0, "                break;");
    print_line(f, 0, "            case %d:", UTF_8);
    print_line(f, 0, "                break;");
    print_line(f, 0, "            case %d:", FLOAT);
    print_line(f, 0, "                break;");
    print_line(f, 0, "            default:");
    print_line(f, 0, "                printf(\"[ERROR] got type %%zu (%%s)\\n\", p->type, type_as_string[p->type]);");
    print_line(f, 0, "                UNREACHABLE(\"inside of body: unknown type\");");
    print_line(f, 0, "        }");
    print_line(f, 0, "    } else {");
    print_line(f, 0, "        UNIMPLEMENTED(\"offset is bigger than body_offset+size\");");
    print_line(f, 0, "    }");
    print_line(f, 0, "    return %s;", api_return_value_name(API_RETURN_VALUE_OK).cstr);
    print_line(f, 0, "}");
}

void implement_eof_func(FILE *f) {
    print_line(f, 0, "%s {", api_func_signature(API_FUNC_EOF).cstr);
    print_line(f, 0, "    UNUSED(p);");
    print_line(f, 0, "    return %s;", api_return_value_name(API_RETURN_VALUE_OK).cstr);
    print_line(f, 0, "}");
}

void implement_print_func(FILE *f) {
    print_line(f, 0, "%s {", api_func_signature(API_FUNC_PRINT).cstr);
    print_line(f, 0, "    printf(\"[INFO] Parser\\n\");");
    print_line(f, 0, "    printf(\"[INFO]   offset = %%zu\\n\", p->offset);");
    print_line(f, 0, "    printf(\"[INFO]   depth = %%zu\\n\", p->depth);");
    print_line(f, 0, "    printf(\"[INFO]   id_offset   = [\");");
    print_line(f, 0, "    printf(\"%%zu\", p->id_offset[0]);");
    print_line(f, 0, "    for (size_t i=1; i<=p->depth; i++) {");
    print_line(f, 0, "        printf(\", %%zu\", p->id_offset[i]);");
    print_line(f, 0, "    }");
    print_line(f, 0, "    printf(\"]\\n\");");
    print_line(f, 0, "    printf(\"[INFO]   size_offset = [\");");
    print_line(f, 0, "    printf(\"%%zu\", p->size_offset[0]);");
    print_line(f, 0, "    for (size_t i=1; i<=p->depth; i++) {");
    print_line(f, 0, "        printf(\", %%zu\", p->size_offset[i]);");
    print_line(f, 0, "    }");
    print_line(f, 0, "    printf(\"]\\n\");");
    print_line(f, 0, "    printf(\"[INFO]   body_offset = [\");");
    print_line(f, 0, "    printf(\"%%zu\", p->body_offset[0]);");
    print_line(f, 0, "    for (size_t i=1; i<=p->depth; i++) {");
    print_line(f, 0, "        printf(\", %%zu\", p->body_offset[i]);");
    print_line(f, 0, "    }");
    print_line(f, 0, "    printf(\"]\\n\");");
    print_line(f, 0, "    printf(\"[INFO]   id = [\");");
    print_line(f, 0, "    printf(\"0x%%lX\", p->id[0]);");
    print_line(f, 0, "    for (size_t i=1; i<=p->depth; i++) {");
    print_line(f, 0, "        printf(\", 0x%%lX\", p->id[i]);");
    print_line(f, 0, "    }");
    print_line(f, 0, "    printf(\"]\\n\");");
    print_line(f, 0, "    printf(\"[INFO]   size = [\");");
    print_line(f, 0, "    printf(\"0x%%lX\", p->size[0]);");
    print_line(f, 0, "    for (size_t i=1; i<=p->depth; i++) {");
    print_line(f, 0, "        printf(\", 0x%%lX\", p->size[i]);");
    print_line(f, 0, "    }");
    print_line(f, 0, "    printf(\"]\\n\");");
    print_line(f, 0, "    printf(\"[INFO]   value = %%lu\\n\", p->value);");
    print_line(f, 0, "    p->string_buffer[p->string_length] = '\\0';");
    print_line(f, 0, "    printf(\"[INFO]   string = '%%s'\\n\", p->string_buffer);");
    print_line(f, 0, "}");
}

#ifndef UNIT_TESTING
int main(void) {
    for (size_t i=0; i<sizeof(default_header)/sizeof(default_header[0]); i++) {
        append_element(process_element(default_header[i]));
    }
    for (size_t i=0; i<sizeof(global_elements)/sizeof(global_elements[0]); i++) {
        append_element(process_element(global_elements[i]));
    }
    FILE *schema_file = fopen(SCHEMA_FILE_NAME, "r");
    if (schema_file == NULL) {
        printf("[ERROR] Could not open file '%s': %s\n", SCHEMA_FILE_NAME, strerror(errno));
        exit(1);
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
                    insert_element(process_element(new));
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

    printf("[INFO] the following paths exist in the schema:\n");
    for (size_t i=0; i<element_count; i++) {
        path_print(element_list[i].path);
    }

    Short_String target_file_name        = shortf("build/%s.h", TARGET_LIBRARY_NAME);
    Short_String include_guard           = capitalize(shortf("%s_H", TARGET_LIBRARY_NAME));
    Short_String implementation_guard    = capitalize(shortf("%s_IMPLEMENTATION", TARGET_LIBRARY_NAME));

    FILE *target_file = fopen(target_file_name.cstr, "w");
    if (target_file == NULL) {
        printf("[ERROR] Could not open file '%s': %s\n", target_file_name.cstr, strerror(errno));
        exit(1);
    }

    print_line(target_file, 0, "#ifndef %s",   include_guard.cstr);
    print_line(target_file, 0, "#define %s",   include_guard.cstr);
    line();

    // header code ==================================
    
    // includes
    print_line(target_file, 0, "#include <assert.h>");
    print_line(target_file, 0, "#include <stdint.h>");
    print_line(target_file, 0, "#include <stdbool.h>");
    line();

    // type definitions
    for (size_t i=0; i<API_TYPE_COUNT; i++) {
        define_api_type(target_file, i);
        line();
    }

    // global variables
    print_line(target_file, 0, "char *type_as_string[] = {");
    for (EBML_Type i=0; i<EBML_TYPE_COUNT; i++) {
        print_line(target_file, 1, "[%d] = \"%s\",", i, ebml_type_spelling[i]);
    }
    print_line(target_file, 0, "};");
    line();

    // function declarations
    for (size_t i=0; i<API_FUNC_COUNT; i++) {
        print_line(target_file, 0, "%s;", api_func_signature(i).cstr);
    }

    line();
    print_line(target_file, 0, "#endif // %s", include_guard.cstr);
    line();
    // ==============================================

    // implementation code ==========================
    print_line(target_file, 0, "#ifdef %s",    implementation_guard.cstr);
    line();

    implement_vint_length(target_file);
    line();
    implement_drop_first_active_bit(target_file);
    line();
    implement_type(target_file);
    line();
    implement_name(target_file);
    line();
    implement_incdepth_func(target_file);
    line();
    implement_decdepth_func(target_file);
    line();
    implement_init_func(target_file);
    line();
    implement_parse_func(target_file);
    line();
    implement_eof_func(target_file);
    line();
    implement_print_func(target_file);

    line();
    print_line(target_file, 0, "#endif // %s", implementation_guard.cstr);
    // ==============================================

    fclose(target_file);
}
#endif //UNIT_TESTING
