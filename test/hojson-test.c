#include <stdio.h> /* FILE, fclose() fopen(), fprintf(), fread(), fseek(), ftell(), printf(), SEEK_END, SEEK_SET, */
                   /* stderr */
#include <stdlib.h> /* atoi(), EXIT_FAILURE, EXIT_SUCCESS, free(), malloc(), NULL */

#define HOJSON_IMPLEMENTATION
/* #define HOJSON_DEBUG */
#include "hojson.h"

#define NUM_DOCUMENTS 19
#define NUM_INVALID_DOCUMENTS 6
#define CONTENT_BUFFER_LENGTH 75 /* Small, odd number to force reallocation and to trigger "unexpected EoF" errors */
                                 /* halfway through UTF-16 characters */

int main(int argc, char** argv) {
    char* documents[NUM_DOCUMENTS];
    /* These documents are expected to return errors */
    documents[0]  = "invalid_early_eof.json";
    documents[1]  = "invalid_leading_comma.json";
    documents[2]  = "invalid_sequential_commas.json";
    documents[3]  = "invalid_token_mismatch.json";
    documents[4]  = "invalid_trailing_comma_array.json";
    documents[5]  = "invalid_trailing_comma_object.json";
    /* These documents are expected to be parsed successfully */
    documents[6]  = "valid_basic.json";
    documents[7]  = "valid_complex.json";
    documents[8]  = "valid_depth.json";
    documents[9]  = "valid_escapes.json";
    documents[10] = "valid_nameless_values.json";
    documents[11] = "valid_nested_arrays.json";
    documents[12] = "valid_nested_objects.json";
    documents[13] = "valid_no_space.json";
    documents[14] = "valid_numbers.json";
    documents[15] = "valid_root_array.json";
    documents[16] = "valid_unicode.json";
    documents[17] = "valid_utf16be.json";
    documents[18] = "valid_utf16le.json";

    int from = 1; /* Skip the EoF test document due to it conflicting with reallocation testing */
    int to = NUM_DOCUMENTS - 1;
    if (argc > 1) /* If a specific index was passed as a CLI argument */
        from = to = atoi(argv[1]); /* No sanitation here. You're a programmer. Be smart. */

    int document_index;
    for (document_index = from; document_index <= to; document_index++) {

        FILE* file;
        if ((file = fopen(documents[document_index], "r")) == NULL) {
            fprintf(stderr, "Couldn't open document: %s\n", documents[document_index]);
            return EXIT_FAILURE;
        }

        size_t content_length;
        fseek(file, 0, SEEK_END); /* Seek to the end of the file */
        content_length = ftell(file); /* Take the position in the file, the end, as the length */
        fseek(file, 0, SEEK_SET); /* Seek back to the beginning of the file to iterate through it */
        printf("\n\n\n --------- Parsing JSON document %s of length %lu\n", documents[document_index],
            (unsigned long)content_length);

        hojson_context_t hojson_context[1];
        size_t hojson_buffer_length = content_length / 8; /* Use a small length to force multiple line reads */
        void* hojson_buffer = malloc(hojson_buffer_length);
        hojson_init(hojson_context, hojson_buffer, hojson_buffer_length);
        printf(" --- Using an initial buffer length of %lu\n", (unsigned long)hojson_buffer_length);

        size_t bytes_read;
        char content[CONTENT_BUFFER_LENGTH], content_copy[CONTENT_BUFFER_LENGTH], *content_pointer = content;
        while ((bytes_read = fread(content, 1, CONTENT_BUFFER_LENGTH - 1, file)) != 0) {
            content[bytes_read] = '\0';
            memcpy(content_copy, content, bytes_read);
            content_pointer = content_pointer == content ? content_copy : content;
            int is_done_with_document = 0;
            while (is_done_with_document == 0) {
                hojson_code_t code = hojson_parse(hojson_context, content_pointer, bytes_read);
                if (code < HOJSON_NO_OP) { /* If an error was returned */
                    if (code == HOJSON_ERROR_UNEXPECTED_EOF) { /* Recoverable error */
                        /* Recover by exiting this loop leading to more JSON content being read from disk */
                        printf(" --- Parsed to end of the current content buffer - continuing to next string...\n");
                        break;
                    } else if (code == HOJSON_ERROR_INSUFFICIENT_MEMORY) { /* Recoverable error */
                        /* Recover by doubling the size of the buffer and telling hojson to use it */
                        printf(" --- Ran out of memory - increasing buffer from %lu to %lu\n",
                            (unsigned long)hojson_buffer_length, (unsigned long)hojson_buffer_length * 2);
                        hojson_buffer_length *= 2;
                        void* new_buffer = malloc(hojson_buffer_length);
                        hojson_realloc(hojson_context, new_buffer, hojson_buffer_length);
                        free(hojson_buffer);
                        hojson_buffer = new_buffer;
                    } else { /* NOT a recoverable error */
                        /* Some of the test documents are invalid and errors are expected. For them, just continue to */
                        /* the next document. For the rest, the valid documents, the error can't go ignored. */
                        if (document_index < NUM_INVALID_DOCUMENTS) { /* The document was invalid, this is expected */
                            printf(" --- Document %s returned error code %d on line %d, column %d as expected. Pass.\n",
                                documents[document_index], code, hojson_context->line, hojson_context->column);
                            is_done_with_document = 1;
                            break;
                        } else { /* The document was valid and something is wrong */
                            fprintf(stderr, "\n\n Error on line %d, column %d: ", hojson_context->line,
                                hojson_context->column);
                            switch(code)
                            {
                            case HOJSON_ERROR_INSUFFICIENT_MEMORY: printf("insufficient memory\n"); break;
                            case HOJSON_ERROR_UNEXPECTED_EOF: printf("unexpected EoF\n"); break;
                            case HOJSON_ERROR_TOKEN_MISMATCH: printf("token mismatch\n"); break;
                            case HOJSON_ERROR_SYNTAX: printf("syntax\n"); break;
                            default: printf("unrecognized error code: %d\n", code); break;
                            }
                            fclose(file);
                            free(hojson_buffer);
                            return EXIT_FAILURE;
                        }
                    }
                } else if (code == HOJSON_END_OF_DOCUMENT) {
                    /* Hopefully, this is one of the valid documents where we expect to eventually receive this code. */
                    /* But if this is not one of those documents, the test has failed. */
                    if (document_index < NUM_INVALID_DOCUMENTS) { /* The document was invalid, an error was expected */
                        fprintf(stderr, "\n\n Parsing of document %s completed successfully but was expected to fail\n",
                            documents[document_index]);
                        return EXIT_FAILURE;
                    } else { /* The document was valid and parsed to the end of it so we're done with it */
                        printf(" --- Parsing of document %s completed without error. Pass.\n",
                            documents[document_index]);
                        is_done_with_document = 1;
                        break;
                    }
                } else {
                    switch (code) {
                    case HOJSON_NAME:
                        if (hojson_context->name == NULL) {
                            fprintf(stderr, "\n\nReceived a name return code but no name was provided\n");
                        } else
                            printf("         name: \"%s\"\n", hojson_context->name);
                        break;
                    case HOJSON_VALUE:
                        switch (hojson_context->value_type) {
                        case HOJSON_TYPE_NONE:
                        default:
                            fprintf(stderr, "\n\nReceived a value return code but no value was provided\n");
                            return EXIT_FAILURE;
                        case HOJSON_TYPE_INTEGER:
                            printf("        value: %ld", hojson_context->integer_value);
                            break;
                        case HOJSON_TYPE_FLOAT:
                            printf("        value: %g",hojson_context->float_value);
                            break;
                        case HOJSON_TYPE_STRING:
                            printf("        value: \"%s\"", hojson_context->string_value);
                            break;
                        case HOJSON_TYPE_BOOLEAN:
                            printf("        value: %s", hojson_context->bool_value ? "true" : "false");
                            break;
                        case HOJSON_TYPE_NULL:
                            printf("        value: null");
                            break;
                        }
                        if (hojson_context->name != NULL)
                            printf(" with name \"%s\"\n", hojson_context->name);
                        else
                            printf("\n");
                        break;
                    case HOJSON_OBJECT_BEGIN:
                        printf(" object begin");
                        if (hojson_context->name != NULL)
                            printf(": \"%s\"\n", hojson_context->name);
                        else
                            printf("\n");
                        break;
                    case HOJSON_OBJECT_END:
                        printf("   object end");
                        if (hojson_context->name != NULL)
                            printf(": \"%s\"\n", hojson_context->name);
                        else
                            printf("\n");
                        break;
                    case HOJSON_ARRAY_BEGIN:
                        printf("  array begin");
                        if (hojson_context->name != NULL)
                            printf(": \"%s\"\n", hojson_context->name);
                        else
                            printf("\n");
                        break;
                    case HOJSON_ARRAY_END:
                        printf("    array end");
                        if (hojson_context->name != NULL)
                            printf(": \"%s\"\n", hojson_context->name);
                        else
                            printf("\n");
                        break;
                    default: break;
                    }
                }
            }

            if (is_done_with_document == 1)
                break;
        }

        fclose(file);
        free(hojson_buffer);
    }

    printf("\n\n\n PASS\n");
    return EXIT_SUCCESS;
}