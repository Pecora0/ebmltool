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

EBML_Element default_header[] = {
    {
        .name = {"EBML"},
        .path = {"\\EBML"},
        .id   = 0x1A45DFA3,
        .type = MASTER,
    },
    {
        .name  = {"EBMLVersion"},
        .path  = {"\\EBML\\EBMLVersion"},
        .id    = 0x4286,
        .type  = UINTEGER,
        .range = {
            .kind = RANGE_EXCLUDED,
            .lo = 0,
            .hi = 0,
            },
    },

    {
        .name  = {"EBMLReadVersion"},
        .path  = {"\\EBML\\EBMLReadVersion"},
        .id    = 0x42F7,
        .range = {
            .kind = RANGE_EXACT,
            .lo = 1,
            .hi = 1,
            },
        .type  = UINTEGER,
    },
/*
    {

   name:  EBMLMaxIDLength

   path:  "\EBML\EBMLMaxIDLength"

   id:  0x42F2

   range:  >=4

   default:  4

   type:  Unsigned Integer

   description:  The EBMLMaxIDLength Element stores the maximum
      permitted length in octets of the Element IDs to be found within
      the EBML Body.  An EBMLMaxIDLength Element value of four is
      RECOMMENDED, though larger values are allowed.

    {

   name:  EBMLMaxSizeLength

   path:  "\EBML\EBMLMaxSizeLength"

   id:  0x42F3

   range:  not 0

   default:  8

   type:  Unsigned Integer

   description:  The EBMLMaxSizeLength Element stores the maximum
      permitted length in octets of the expressions of all Element Data
      Sizes to be found within the EBML Body.  The EBMLMaxSizeLength
      Element documents an upper bound for the "length" of all Element
      Data Size expressions within the EBML Body and not an upper bound
      for the "value" of all Element Data Size expressions within the
      EBML Body.  EBML Elements that have an Element Data Size
      expression that is larger in octets than what is expressed by
      EBMLMaxSizeLength Element are invalid.

    {

   name:  DocType

   path:  "\EBML\DocType"

   id:  0x4282

   length:  >0

   type:  String

   description:  A string that describes and identifies the content of
      the EBML Body that follows this EBML Header.

    {

   name:  DocTypeVersion

   path:  "\EBML\DocTypeVersion"

   id:  0x4287

   range:  not 0

   default:  1

   type:  Unsigned Integer

   description:  The version of DocType interpreter used to create the
      EBML Document.

    {

   name:  DocTypeReadVersion

   path:  "\EBML\DocTypeReadVersion"

   id:  0x4285

   range:  not 0

   default:  1

   type:  Unsigned Integer

   description:  The minimum DocType version an EBML Reader has to
      support to read this EBML Document.  The value of the
      DocTypeReadVersion Element MUST be less than or equal to the value
      of the DocTypeVersion Element.

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
void print_line(FILE *stream, int depth, char *format, ...) {
    fprintf(stream, "%*s", depth*4, "");

    va_list args;
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);

    fprintf(stream, "\n");
}

const char *states[] = {
    "START",
    "ID",
    "SIZE",
    "DATA",
};
const size_t state_count = sizeof(states) / sizeof(char *);
const size_t state_start = 0;

Short_String build_state_nr(size_t elemnr, size_t i, Short_String pre) {
    assert(elemnr < element_count);
    assert(i < state_count);
    return shortf("%s_E%zu_%s", pre.cstr, elemnr, states[i]);
}

int main(void) {
    for (size_t i=0; i<sizeof(default_header)/sizeof(default_header[0]); i++) {
        append_element(default_header[i]);
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

    Short_String target_file_name     = shortf("build/%s.h", TARGET_LIBRARY_NAME);
    Short_String include_guard        = capitalize(shortf("%s_H", TARGET_LIBRARY_NAME));
    Short_String implementation_guard = capitalize(shortf("%s_IMPLEMENTATION", TARGET_LIBRARY_NAME));
    Short_String prefix               = shortf(TARGET_LIBRARY_NAME);
    Short_String prefix_caps          = capitalize(prefix);
    Short_String byte_type_name       = shortf("%s_byte_t", prefix.cstr);
    Short_String parser_type_name     = shortf("%s_parser_t", prefix.cstr);
    Short_String return_type_name     = shortf("%s_return_t", prefix.cstr);
    Short_String state_type_name      = shortf("%s_state", prefix.cstr);
    Short_String init_func_name       = shortf("%s_init", prefix.cstr);
    Short_String init_func_signature  = shortf("void %s(%s *p)", init_func_name.cstr, parser_type_name.cstr);
    Short_String parse_func_name      = shortf("%s_parse", prefix.cstr);
    Short_String parse_func_signature = shortf("%s %s(%s *p, %s b)", return_type_name.cstr, parse_func_name.cstr, parser_type_name.cstr, byte_type_name.cstr);
    Short_String eof_func_name        = shortf("%s_eof", prefix.cstr);
    Short_String eof_func_signature   = shortf("%s %s(%s *p)", return_type_name.cstr, eof_func_name.cstr, parser_type_name.cstr);
    Short_String print_func_name      = shortf("%s_print", prefix.cstr);
    Short_String print_func_signature = shortf("void %s(%s *p)", print_func_name.cstr, parser_type_name.cstr);

    FILE *target_file = fopen(target_file_name.cstr, "w");
    if (target_file == NULL) {
        printf("[ERROR] Could not open file '%s': %s\n", target_file_name.cstr, strerror(errno));
    }

    print_line(target_file, 0, "#ifndef %s",   include_guard.cstr);
    print_line(target_file, 0, "#define %s",   include_guard.cstr);
    line();

    // header code
    print_line(target_file, 0, "typedef unsigned char %s;", byte_type_name.cstr);
    line();
    print_line(target_file, 0, "typedef enum {");
    print_line(target_file, 1,     "%s_OK = 0,", prefix_caps.cstr);
    print_line(target_file, 0, "} %s;", return_type_name.cstr);
    line();
    print_line(target_file, 0, "typedef enum {");
    for (size_t i=0; i<element_count; i++) {
        for (size_t j=0; j<state_count; j++) {
            print_line(target_file, 1, "%s,", build_state_nr(i, j, prefix_caps).cstr);
        }
    }
    print_line(target_file, 0, "} %s;", state_type_name.cstr);
    line();
    print_line(target_file, 0, "const char *state_as_string[] = {");
    for (size_t i=0; i<element_count; i++) {
        for (size_t j=0; j<state_count; j++) {
            print_line(target_file, 1, "[%s] = \"%s\",", build_state_nr(i, j, prefix_caps).cstr, build_state_nr(i, j, prefix_caps).cstr);
        }
    }
    print_line(target_file, 0, "};");
    line();
    print_line(target_file, 0, "typedef struct {");
    print_line(target_file, 1,     "%s state;", state_type_name.cstr);
    print_line(target_file, 1,     "size_t bytes_left;");
    print_line(target_file, 0, "} %s;", parser_type_name.cstr);
    line();
    print_line(target_file, 0, "%s;", init_func_signature.cstr);
    print_line(target_file, 0, "%s;", parse_func_signature.cstr);
    print_line(target_file, 0, "%s;", eof_func_signature.cstr);
    print_line(target_file, 0, "%s;", print_func_signature.cstr);

    line();
    print_line(target_file, 0, "#endif // %s", include_guard.cstr);

    line();
    print_line(target_file, 0, "#ifdef %s",    implementation_guard.cstr);
    line();

    // implementation code
    print_line(target_file, 0, "size_t vint_length(%s b) {", byte_type_name.cstr);
    print_line(target_file, 1,     "if (b == 0) UNIMPLEMENTED(\"zero byte in vint_length\");");
    print_line(target_file, 1,     "size_t acc = 1;");
    print_line(target_file, 1,     "for (%s mark = 0x80; (mark & b) == 0; mark>>=1) acc++;", byte_type_name.cstr);
    print_line(target_file, 1,     "return acc;");
    print_line(target_file, 0, "}");
    line();
    print_line(target_file, 0, "%s {\n", init_func_signature.cstr);
    print_line(target_file, 1,     "p->state = %s;", build_state_nr(0, state_start, prefix_caps).cstr);
    print_line(target_file, 1,     "p->bytes_left = 0;");
    print_line(target_file, 0, "}\n");
    line();
    print_line(target_file, 0, "%s {\n", parse_func_signature.cstr);
    print_line(target_file, 0, "    switch (p->state) {");
    for (size_t i=0; i<element_count; i++) {
        print_line(target_file, 0, "        case %s:", build_state_nr(i, 0, prefix_caps).cstr);
        print_line(target_file, 0, "            p->bytes_left = vint_length(b) - 1;");
        print_line(target_file, 0, "            p->state = %s;", build_state_nr(i, 1, prefix_caps).cstr);
        print_line(target_file, 0, "            break;");
        print_line(target_file, 0, "        case %s:", build_state_nr(i, 1, prefix_caps).cstr);
        print_line(target_file, 0, "            if (p->bytes_left == 0) {");
        print_line(target_file, 0, "                p->bytes_left = vint_length(b) - 1;");
        print_line(target_file, 0, "                p->state = %s;", build_state_nr(i, 2, prefix_caps).cstr);
        print_line(target_file, 0, "            } else {");
        print_line(target_file, 0, "                p->bytes_left--;");
        print_line(target_file, 0, "            }");
        print_line(target_file, 0, "            break;");
        print_line(target_file, 0, "        case %s:", build_state_nr(i, 2, prefix_caps).cstr);
        print_line(target_file, 0, "            if (p->bytes_left == 0) {");
        print_line(target_file, 0, "                UNIMPLEMENTED(\"0 bytes left for %s\");", build_state_nr(i, 2, prefix_caps).cstr);
        print_line(target_file, 0, "            } else {");
        print_line(target_file, 0, "                p->bytes_left--;");
        print_line(target_file, 0, "            }");
        print_line(target_file, 0, "            break;");
        print_line(target_file, 0, "        case %s:", build_state_nr(i, 3, prefix_caps).cstr);
        print_line(target_file, 0, "            UNIMPLEMENTED(\"%s\");", build_state_nr(i, 3, prefix_caps).cstr);
    }
    print_line(target_file, 0, "    }");
    print_line(target_file, 0, "    return %s_OK;", prefix_caps.cstr);
    print_line(target_file, 0, "}");
    line();
    print_line(target_file, 0, "%s {", eof_func_signature.cstr);
    print_line(target_file, 0, "    UNUSED(p);");
    print_line(target_file, 0, "    return %s_OK;", prefix_caps.cstr);
    print_line(target_file, 0, "}");
    line();
    print_line(target_file, 0, "%s {", print_func_signature.cstr);
    print_line(target_file, 0, "    printf(\"[INFO] Parser\\n\");");
    print_line(target_file, 0, "    printf(\"[INFO]   state = %%s\\n\", state_as_string[p->state]);");
    print_line(target_file, 0, "    printf(\"[INFO]   bytes_left = %%zu\\n\", p->bytes_left);");
    print_line(target_file, 0, "}");

    line();
    print_line(target_file, 0, "#endif // %s", implementation_guard.cstr);

    fclose(target_file);
}
