# hojson

Header-Only JSON parser written in portable ANSI C.


## Features

- Portable ANSI C (C89), tested with GCC (Windows and Linux) and MSVC
- Supports UTF-8, UTF-16BE, and UTF-16LE including their BOMs
- Allows content to be passed in parts
- Does not require malloc() and allows for reallocation of the buffer
- No dependencies beyond the C standard library


## Limitations

- All numbers using exponent notation are parsed as floating-point, including those that may be valid integers


## Usage

Define the implementation before including hojson.
``` c
#define HOJSON_IMPLEMENTATION
#include "hojson.h"
```
As usual with header-only libraries, the implementation's definition can be limited to just a single file. This will depend on your specific build configuration.

Allocate *hojson*'s context object, which holds state and metadata information, and a buffer for *hojson* to use.
``` c
hojson_context_t hojson_context[1];
char* buffer = (char*)malloc(1024);
hojson_init(hojson_context, buffer, 1024);
```
The buffer length needed will depend on the amount of JSON content, its depth, and various other minor factors. As a rule of thumb, a buffer equal in length to the content is likley enough. In cases where *hojson* runs out of memory, more may be allocated (see [Error Recovery](#error-recovery)).

Continually call the parsing function until the *end of document* code is returned or an error code is returned.
``` c
hojson_code_t code;
while ((code = hojson_parse(hojson_context, content, content_length)) != HOJSON_END_OF_DOCUMENT) {
    switch (code) {
    case HOJSON_ERROR_SYNTAX:
        fprintf(stderr, " Syntax error: line %d, column %d\n", hojson_context->line, hojson_context->column);
        return EXIT_FAILURE;
    ...
    case HOJSON_NAME: printf(" Name: \"%s\"\n", hojson_context->name); break;
    case HOJSON_VALUE:
        switch (hojson_context->value_type) {
        case HOJSON_TYPE_INTEGER:
            printf(" Value: \"%s\" = %ld\n", hojson_context->name, hojson_context->integer_value);
            break;
        case HOJSON_TYPE_FLOAT:
            printf(" Value: \"%s\" = %g\n", hojson_context->name, hojson_context->float_value);
            break;
        case HOJSON_TYPE_STRING:
            printf(" Value: \"%s\" = \"%s\"\n", hojson_context->name, hojson_context->string_value);
            break;
        case HOJSON_TYPE_BOOLEAN:
            printf(" Value: \"%s\" = %s\n", hojson_context->name, hojson_context->bool_value ? "true" : "false");
            break;
        case HOJSON_TYPE_NULL:
            printf(" Value: \"%s\" = null\n", hojson_context->name);
            break;
        default: break;
        } break;
    case HOJSON_OBJECT_BEGIN: printf(" Object \"%s\" began\n", hojson_context->name); break;
    case HOJSON_OBJECT_END: printf(" Object \"%s\" ended\n", hojson_context->name); break;
    case HOJSON_ARRAY_BEGIN: printf(" Array \"%s\" began\n", hojson_context->name); break;
    case HOJSON_ARRAY_END: printf(" Array \"%s\" ended\n", hojson_context->name); break;
    }
}
```
The return codes and what they mean are listed in [Return Codes](#return-codes).

The JSON content string passed to `hojson_parse()` may contain partial content. All that's required is the first call be done with the beginning of the document and subsequent parts be passed contiguously.
The *unexpected EoF* error code will be returned when parsing has reached the end of the current content. At that time, pass the next portion(s) of content. The pointer passed may be the same; *hojson* will determine if the content is new based on the ability to decode the first character of the passed string. If a single character is split between two content strings, *hojson* will know and piece it together.



## Return Codes

`HOJSON_END_OF_DOCUMENT`: The root element has closed and parsing is done.
`HOJSON_NAME`: The name of a name-value pair is available in the `name` variable of the context object. A value, array, or object is expected to follow.
`HOJSON_VALUE`: The value of a name-value pair or array is available in the context object. Several variables can hold a value depending on the value's data type. The `value_type` variable indicates the type and type-specific variable to check.
`HOJSON_OBJECT_BEGIN`: A new object opened. If it has a name, its name is available in the `name` variable of the context object.
`HOJSON_OBJECT_END`: An object closed. If it had a name, its name is available in the `name` variable of the context object.
`HOJSON_ARRAY_BEGIN`: An ordered list of 0+ values opened. If it had a name, its name is available in the `name` variable of the context object.
`HOJSON_ARRAY_END`: An array closed. If it had a name, its name is available in the `name` variable of the context object.
`HOJSON_ERROR_INVALID_INPUT`: One or more parameter passed to an hojson function was unacceptable.
`HOJSON_ERROR_INTERNAL`: There's a bug in hojson and parsing must halt. This code is included as due diligence but should not be expected.
`HOJSON_ERROR_INSUFFICIENT_MEMORY`: Initialization or continued parsing requires more memory. This error is one of two that can be recovered (see [Error Recovery](#error-recovery)).
`HOJSON_ERROR_UNEXPECTED_EOF`: Reached the end of the JSON content before the end of the document. This error is one of two that can be recovered (see [Error Recovery](#error-recovery)).
`HOJSON_ERROR_TOKEN_MISMATCH`: A `{` or `[` that opened an object/array did not match its closing token.
`HOJSON_ERROR_SYNTAX`: Invalid syntax. The `line` and `column` variables of the context object will contain the line and column, respectively, where the error was first noticed but not necessarily where it exists.


## Error Recovery

Of the possible errors, two are recoverable: `HOJSON_ERROR_INSUFFICIENT_MEMORY` and `HOJSON_ERROR_UNEXPECTED_EOF`.

`HOJSON_ERROR_INSUFFICIENT_MEMORY` can be recovered by providing a larger buffer to hojson through the `hojson_realloc()` function.
``` c
hojson_context_t hojson_context[1];
void* buffer = malloc(1024);
hojson_init(hojson_context, buffer, 1024);
/* Given the above initialization, the buffer can be doubled with: */
void* new_buffer = malloc(2048);
hojson_realloc(hojson_context, new_buffer, 2048);
free(buffer);
buffer = new_buffer;
```
The next call to `hojson_parse()` will continue as if nothing happened.

`HOJSON_ERROR_UNEXPECTED_EOF` can be recovered by providing the continuation of the JSON content to `hojson_parse()`, assuming it exists.
``` c
const char* first_half = "{ \"name in the first part...\" : }";
const char* second_half = "\"...of a two-part string\" }";
hojson_code_t code;
while ((code = hojson_parse(hojson_context, first_half, strlen(first_half))) != HOJSON_ERROR_UNEXPECTED_EOF) ;
while ((code = hojson_parse(hojson_context, second_half, strlen(second_half))) != HOJSON_END_OF_DOCUMENT) ;
```


## Acknowledgements

*hojson* and its state machine design were inspired by [Yxml](https://dev.yorhel.nl/yxml).
