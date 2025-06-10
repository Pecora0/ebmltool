#include <stdio.h>

#define UNIT_TESTING
#include "tool.c"

struct {
    Short_String spelling;
    EBML_Range compare;
} range_test[] = {
    {
        {""},         
        {.kind = RANGE_NONE}
    },
    {
        {"1"},
        {.kind = RANGE_EXACT, .type = UINTEGER, .lo_uint = 1, .hi_uint = 1}
    },
    {
        {"4518"},
        {.kind = RANGE_EXACT, .type = UINTEGER, .lo_uint = 4518, .hi_uint = 4518}
    },
    {
        {"not 0"},
        {.kind = RANGE_EXCLUDED, .type = UINTEGER, .lo_uint = 0, .hi_uint = 0}
    },
    {
        {"not0"},
        {.kind = RANGE_EXCLUDED, .type = UINTEGER, .lo_uint = 0, .hi_uint = 0}
    },
    {
        {"1-8"},
        {.kind = RANGE_UPLOW_BOUND, .type = UINTEGER, .lo_uint = 1, .lo_in = true, .hi_uint = 8, .hi_in = true}
    },
    {
        {"> 0x0p+0"},
        {.kind = RANGE_LOWER_BOUND, .type = FLOAT, .lo_float = 0x0p+0, .lo_in = false}
    },
    {
        {">= -0xB4p+0, <= 0xB4p+0"},
        {.kind = RANGE_UPLOW_BOUND, .type = FLOAT, .lo_float = -0xB4p+0, .lo_in = true, .hi_float = 0xB4p+0, .hi_in = true}
    },
};
const size_t range_test_count = sizeof(range_test) / sizeof(range_test[0]);

struct {
    Short_String spelling;
    EBML_Path compare;
} path_test[] = {
    {
        {"\\Files"},
        {.depth = 1, .names = {{"Files"}}, .recursive = {false}},
    },
    {
        {"\\Segment\\Chapters\\EditionEntry\\+ChapterAtom"},
        {.depth = 4, .names = {{"Segment"}, {"Chapters"}, {"EditionEntry"}, {"ChapterAtom"}}, .recursive = {false,false,false,true},}
    },
    {
        {"\\(1-\\)CRC-32"},
        {.depth = 2, .names = {{""}, {"CRC-32"}}, .recursive = {false, false}, .global = {true, false}, .min = {1}, .max = {SIZE_MAX},}
    },
    {
        {"\\(-\\)Void"},
        {.depth = 2, .names = {{""}, {"Void"}}, .recursive = {false, false}, .global = {true, false}, .min = {0}, .max = {SIZE_MAX},}
    },
};
const size_t path_test_count = sizeof(path_test) / sizeof(path_test[0]);

bool range_equal(EBML_Range r1, EBML_Range r2) {
    if (r1.kind != r2.kind) return false;
    switch (r1.kind) {
        case RANGE_NONE:
            return true;
        case RANGE_EXACT:
        case RANGE_EXCLUDED:
            assert(r1.type == UINTEGER);
            assert(r2.type == UINTEGER);
            assert(r1.lo_uint == r1.hi_uint);
            assert(r2.lo_uint == r2.hi_uint);
            return r1.lo_uint == r2.lo_uint;
        case RANGE_UPLOW_BOUND:
            if (r1.type != r2.type) return false;
            switch (r1.type) {
                case UINTEGER:
                    return (r1.lo_uint == r2.lo_uint) && (r1.lo_in == r2.lo_in) 
                        && (r1.hi_uint == r2.hi_uint) && (r1.hi_in == r2.hi_in);
                case FLOAT:
                    return (r1.lo_float == r2.lo_float) && (r1.lo_in == r2.lo_in) 
                        && (r1.hi_float == r2.hi_float) && (r1.hi_in == r2.hi_in);
                default:
                    UNREACHABLE("range has invalid type");
            }
        case RANGE_LOWER_BOUND:
            if (r1.type != r2.type) return false;
            switch (r1.type) {
                case UINTEGER:
                    UNIMPLEMENTED("range_equal: RANGE_LOWER_BOUND UINTEGER");
                case FLOAT:
                    return (r1.lo_float == r2.lo_float) && (r1.lo_in == r2.lo_in);
                default:
                    UNREACHABLE("range has invalid type");
            }
        case RANGE_UPPER_BOUND:
            UNIMPLEMENTED("range_equal");
    }
    UNREACHABLE("did not match any range kind");
}

void range_print(EBML_Range r) {
    switch (r.kind) {
        case RANGE_UPLOW_BOUND:
            assert(r.type == UINTEGER);
            printf("[INFO] RANGE_UPLOW_BOUND ");
            if (r.lo_in) {
                printf(">=%ld,", r.lo_uint);
            } else {
                printf(">%ld,", r.lo_uint);
            }
            if (r.hi_in) {
                printf("<=%ld\n", r.hi_uint);
            } else {
                printf("<%ld\n", r.hi_uint);
            }
            break;
        case RANGE_NONE:
            printf("[INFO] RANGE_NONE\n");
            break;
        case RANGE_EXACT:
            assert(r.type == UINTEGER);
            printf("[INFO] RANGE_EXACT %ld\n", r.lo_uint);
            break;
        case RANGE_EXCLUDED:
            printf("[INFO] RANGE_EXCLUDED \n");
            switch (r.type) {
                default:
                    UNIMPLEMENTED("range_print: RANGE_EXCLUDED");
            }
            break;
        case RANGE_UPPER_BOUND:
            UNIMPLEMENTED("range_print: RANGE_UPPER_BOUND");
        case RANGE_LOWER_BOUND:
            printf("[INFO] RANGE_LOWER_BOUND %s ", ebml_type_spelling[r.type]);
            if (r.lo_in) {
                printf(">=");
            } else {
                printf(">");
            }
            switch (r.type) {
                case UINTEGER:
                    printf("%ld\n", r.lo_uint);
                    break;
                case FLOAT:
                    printf("%f\n", r.lo_float);
                    break;
                default:
                    UNIMPLEMENTED("unsupported type for range");
            }
            break;
    }
}

bool path_equal(EBML_Path p1, EBML_Path p2) {
    if (p1.depth != p2.depth) return false;
    for (size_t i=0; i<p1.depth; i++) {
        if (p1.global[i] != p2.global[i]) return false;
        if (p1.global[i]) {
            if (p1.min[i] != p2.min[i]) return false;
            if (p1.max[i] != p2.max[i]) return false;
        } else {
            if (p1.recursive[i] != p2.recursive[i]) return false;
            if (strcmp(p1.names[i].cstr, p2.names[i].cstr) != 0) return false;
        }
    }
    return true;
}

int main() {
    bool failure = false;
    for (size_t i=0; i<range_test_count; i++) {
        printf("[INFO] running `parse_range` on string \"%s\"\n", range_test[i].spelling.cstr);
        EBML_Range r = parse_range(range_test[i].spelling);
        if (range_equal(r, range_test[i].compare)) {
            printf("[INFO] test passed\n");
        } else {
            failure = true;
            printf("[ERROR] test not passed\n");
            printf("[INFO] EXPECTATION =========================\n");
            range_print(range_test[i].compare);
            printf("[INFO] REALITY =============================\n");
            range_print(r);
            printf("[INFO] =====================================\n");
        }
    }
    for (size_t i=0; i<path_test_count; i++) {
        printf("[INFO] running `parse_path` on string \"%s\"\n", path_test[i].spelling.cstr);
        EBML_Path p = parse_path(path_test[i].spelling);
        if (path_equal(p, path_test[i].compare)) {
            printf("[INFO] test passed\n");
        } else {
            failure = true;
            printf("[ERROR] test not passed\n");
            printf("[INFO] EXPECTATION =========================\n");
            path_print(path_test[i].compare);
            printf("[INFO] REALITY =============================\n");
            path_print(p);
            printf("[INFO] =====================================\n");
        }
    }

    if (failure) {
        printf("[INFO] some tests have failed\n");
        exit(1);
    } else {
        printf("[INFO] all tests have passed\n");
        exit(0);
    }
}
