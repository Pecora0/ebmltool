# ebmltool

A tool that helps to parse (and create) various kinds of EBML files.
It aims to have minimal dependencies and be hackable.

## The plan

Build an executable that takes a xml schema and creates a single file library for parsing binary files specified by the schema.

## Documentation

The core of this project is `tool.c`.
It can be compiled by running `make build/tool`.
Running the executable (e.g. by running `make runtool`) reads in the schema file named `example.xml`
and generates a header only library in C. (the name of the generated library is controlled by `TARGET_LIBRARY_NAME `).

This library can be included in your project. For testing and demontrating purposes I have provided `test.c`.
(Which can be build and run by `make runtest`)

### API of generated library

TBD

### Testing

`unit_test.c` includes all functions in `tool.c` except `main` and provides his own `main` function.
It performs some additional tests for the tasks of interpreting the range and path values found in the schema.
We can build and run it by `make unittest`
