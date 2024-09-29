#include <stdio.h> /* fprintf(), printf(), stderr */
#include <stdlib.h> /* EXIT_FAILURE, EXIT_SUCCESS, free(), malloc(), NULL */

#define HOJSON_IMPLEMENTATION
/* #define HOJSON_DEBUG */
#include "hojson.h"

int main(int argc, char** argv) {
    /* JSON content to parse. A string constant is used to keep this simple but content from disk would be typical. */
    const char* content = "{ \"first name\" : \"John\", "
                          "\"last name\" : \"Jacob Jingleheimer Schmidt\", "
                          "\"age\" : 30, "
                          "\"car\" : null }";
    size_t content_length = strlen(content);

    /* hojson initialization. Allocate the context object and buffer then call hojson_init(). */
    hojson_context_t hojson_context[1];
    char* buffer = (char*)malloc(content_length * 2);
    hojson_init(hojson_context, buffer, content_length * 2);

    /* Loop until the "end of document" code is returned */
    hojson_code_t code;
    int exit_status = EXIT_SUCCESS;
    while ((code = hojson_parse(hojson_context, content, content_length)) != HOJSON_END_OF_DOCUMENT &&
           exit_status == EXIT_SUCCESS) {
        /* Various codes will be returned as hojson_parse() finds names, values, etc. or runs into an error. Parsing */
        /* is finished when an (unrecoverable) error code or the "end of document" code is returned. */
        switch (code) {
        /* Error code cases: */
        case HOJSON_ERROR_INSUFFICIENT_MEMORY:
            fprintf(stderr, " Not enough memory\n");
            exit_status = EXIT_FAILURE;
            break;
        case HOJSON_ERROR_UNEXPECTED_EOF:
            fprintf(stderr, " Unexpected end of file\n");
            exit_status = EXIT_FAILURE;
            break;
        case HOJSON_ERROR_SYNTAX:
            fprintf(stderr, " Syntax error: line %d, column %d\n", hojson_context->line, hojson_context->column);
            exit_status = EXIT_FAILURE;
            break;
        case HOJSON_NAME:
            printf(" Name: \"%s\"\n", hojson_context->name);
            break;
        case HOJSON_VALUE:
            switch (hojson_context->value_type) {
            case HOJSON_TYPE_INTEGER:
                if (hojson_context->name == NULL)
                    printf(" Value: %ld\n", hojson_context->integer_value);
                else
                    printf(" Value: \"%s\" = %ld\n", hojson_context->name, hojson_context->integer_value);
                break;
            case HOJSON_TYPE_FLOAT:
                if (hojson_context->name == NULL)
                    printf(" Value: %g\n", hojson_context->float_value);
                else
                    printf(" Value: \"%s\" = %g\n", hojson_context->name, hojson_context->float_value);
                break;
            case HOJSON_TYPE_STRING:
                if (hojson_context->name == NULL)
                    printf(" Value: \"%s\"\n", hojson_context->string_value);
                else
                    printf(" Value: \"%s\" = \"%s\"\n", hojson_context->name, hojson_context->string_value);
                break;
            case HOJSON_TYPE_BOOLEAN:
                if (hojson_context->name == NULL)
                    printf(" Value: %s\n", hojson_context->bool_value ? "true" : "false");
                else {
                    printf(" Value: \"%s\" = %s\n", hojson_context->name,
                        hojson_context->bool_value ? "true" : "false");
                } break;
            case HOJSON_TYPE_NULL:
                if (hojson_context->name == NULL)
                    printf(" Value: null\n");
                else
                    printf(" Value: \"%s\" = null\n", hojson_context->name);
                break;
            default: break;
            } break;
        case HOJSON_OBJECT_BEGIN:
            if (hojson_context->name == NULL)
                printf(" Unnamed object began\n");
            else
                printf(" Object \"%s\" began\n", hojson_context->name);
            break;
        case HOJSON_OBJECT_END:
            if (hojson_context->name == NULL)
                printf(" Unnamed object ended\n");
            else
                printf(" Object \"%s\" ended\n", hojson_context->name);
            break;
        case HOJSON_ARRAY_BEGIN:
            if (hojson_context->name == NULL)
                printf(" Unnamed array began\n");
            else
                printf(" Array \"%s\" began\n", hojson_context->name);
            break;
        case HOJSON_ARRAY_END:
            if (hojson_context->name == NULL)
                printf(" Unnamed array ended\n");
            else
                printf(" Array \"%s\" ended\n", hojson_context->name);
            break;
        default:
            printf(" Unhandled code %d\n", code);
            break;
        }
    }

    free(buffer);
    if (exit_status == EXIT_SUCCESS)
        printf("\n Parsed to the end of the document\n");
    return exit_status;
}
