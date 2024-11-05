/*
Copyright (c) 2024 Luke Philipsen

Permission to use, copy, modify, and/or distribute this software for
any purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED “AS IS” AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE
FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/* Usage

  Do this:
    #define HOJSON_IMPLEMENTATION
  before you include this file in *one* C or C++ file to create the implementation.

  You can define HOJSON_DECL with
    #define HOJSON_DECL static
  or
    #define HOJSON_DECL extern
  to specify HOJSON function declarations as static or extern, respectively.
  The default specifier is extern.
*/

#ifndef HOJSON_H
    #define HOJSON_H

#include <stddef.h> /* NULL, size_t */
#include <string.h> /* memcpy(), memset() */
#include <stdint.h> /* int8_t, uint8_t, uint16_t, uint32_t */
#include <stdlib.h> /* atof(), atoi() */

#ifndef HOJSON_DECL
    #define HOJSON_DECL
#endif /* HOJSON_DECL */

#ifdef __cplusplus
    extern "C" {
#endif /* __cpluspus */

/***************/
/* Definitions */

/**
 * Error and other codes returned after parsing.
 */
typedef enum {
    HOJSON_ERROR_INVALID_INPUT = -6, /**< One or more parameter passed to hojson was unacceptable. */
    HOJSON_ERROR_INTERNAL = -5, /**< There's a bug in hojson and parsing must halt. */
    HOJSON_ERROR_INSUFFICIENT_MEMORY = -4, /**< Initialization or continued parsing requires more memory. */
    HOJSON_ERROR_UNEXPECTED_EOF = -3, /**< Reached the end of the JSON content before the end of the document. */
    HOJSON_ERROR_TOKEN_MISMATCH = -2, /**< A '{' or '[' that opened an object/array did not match its closing token. */
    HOJSON_ERROR_SYNTAX = -1, /**< Generic syntax error. */
    HOJSON_NO_OP = 0, /**< No operation. If configured correctly, this should never be returned. */
    HOJSON_END_OF_DOCUMENT, /**< The root element has closed and parsing is done. */
    HOJSON_NAME, /**< The name of a name-value pair is available. A value, array, or object is expected to follow. */
    HOJSON_VALUE, /**< The value of a name-value pair or array is available. Its type and name are also available. */
    HOJSON_OBJECT_BEGIN, /**< A new object opened. If it has a name, its name is available. */
    HOJSON_OBJECT_END, /**< An object closed. If it had a name, its name is available. */
    HOJSON_ARRAY_BEGIN, /**< An ordered list of 0+ values opened. If it had a name, its name is available. */
    HOJSON_ARRAY_END /**< An array closed. If it had a name, its name is available. */
} hojson_code_t;

typedef enum _hojson_type_t {
    HOJSON_TYPE_NONE = 0, /**< There are is no value to be provided at this point in parsing. */
    HOJSON_TYPE_INTEGER, /**< A signed integer number. */
    HOJSON_TYPE_FLOAT, /**< A signed floating-point number. */
    HOJSON_TYPE_STRING, /**< A sequene of 0+ characters. */
    HOJSON_TYPE_BOOLEAN, /**< Only two possible values: true or false. */
    HOJSON_TYPE_NULL /**< An anti-value or the lack of a value. */
} hojson_type_t;

/**
 * Holds context and state information needed by hojson. Some of this information is public and holds the data parsed
 * from JSON content but some is private and only makes sense to hojson.
 */
typedef struct {
    /* Public */
    char* name; /**< The name of a name-value pair. Assigned to null for values without names. */
    char* string_value; /**< The value of a name-value pair or array for values whose type is HOJSON_TYPE_STRING. */
    long integer_value; /**< The value of a name-value pair or array for values whose type is HOJSON_TYPE_INTEGER. */
    double float_value; /**< The value of a name-value pair or array for values whose type is HOJSON_TYPE_FLOAT. */
    uint8_t bool_value; /**< The value of a name-value pair or array for values whose type is HOJSON_TYPE_BOOLEAN. */
    hojson_type_t value_type; /**< The type of the value of a name-value pair or array. */
    uint32_t line; /**< The line currently being parsed. Lines are determined by line feeds and carriage returns. */
    uint32_t column; /**< The column, on the current line, of the character last parsed. */
    uint32_t depth; /**< The nested level of objects/arrays. Assigned with the level in which the element was found. */

    /* Private (for internal use) */
    uint8_t is_initialized; /* Set to true by hojson_init() and indicates this context is safe to use */
    const char* json; /* JSON content to be parsed */
    size_t json_length; /* Length of the JSON content to parse */
    uint8_t encoding; /* Character encoding of the JSON content */
    const char* iterator; /* Pointer to the character in the JSON content being parsed */
    size_t bytes_iterated; /* Number of bytes iterated with the last iteration */
    char* buffer; /* Memory allocated for hojson to use */
    size_t buffer_length; /* Amount of memory allocated for hojson */
    int8_t state; /* Current parsing state, determines which characters are acceptable and when to return */
    int8_t escape_return_state; /* State to return to after processing a Unicode escape */
    int8_t error_return_state; /* State to return to after recovering from an error */
    char* stack; /* Pointer to the current node in the stack-like structure of objects and/or arrays */
    uint32_t stream; /* Holds the current character, whole or partial. May contain bytes from different strings. */
    size_t stream_length; /* Length of the 'stream' variable in bytes */
    uint32_t newline_character; /* The character used to increment the 'line' variable, \r or \n */
} hojson_context_t;

/**
 * Sets up the hojson context object to begin parsing. Following this, call hojson_parse() until
 * HOJSON_CODE_END_OF_DOCUMENT or one of the error values is returned.
 *
 * @param context Pointer to an allocated hojson context object. This instance will be modified.
 * @param buffer A pointer to some contiguous block of memory for hojson to use. This will also be modified, frequently.
 * @param buffer_length The length, in bytes, of the buffer handed to hojson as the 'buffer' parameter.
 */
HOJSON_DECL void hojson_init(hojson_context_t* context, char* buffer, const size_t buffer_length);

/**
 * Instruct hojson to use a new buffer. This maintains the current state of parsing meaning that the next call to
 * hojson_parse() will continue none the wiser.
 * The buffer must have a length greater than the current buffer and both buffers must be allocated at the time this
 * function is called. Once it returns, the original buffer may and should be freed.
 *
 * @param context An initialized hojson context object.
 * @param buffer A pointer to a new, contiguous block of memory for hojson to use.
 * @param buffer_length The length, in bytes, of the buffer handed to hojson as the 'buffer' parameter.
 */
HOJSON_DECL void hojson_realloc(hojson_context_t* context, char* buffer, const size_t buffer_length);

/**
 * Begin or continue parsing the given JSON content string.
 * The JSON content string does not need to contain the content in its entirety. If hojson finds a null terminator or
 * parses up to the indicated length of the content, HOJSON_ERROR_UNEXPECTED_EOF is returned and parsing will cease.
 * However, this error is recoverable and parsing will continue if the next call to hojson_parse() passes a new JSON
 * content string, using the same pointer or not.
 *
 * @param context An initialized hojson context object. This should be treated as read-only until parsing is done.
 * @param json JSON content as a string.
 * @param json_length Length of the JSON content in bytes.
 * @return A code indicating what information from the JSON content is available or an error.
 */
HOJSON_DECL hojson_code_t hojson_parse(hojson_context_t* context, const char* json, const size_t json_length);

#ifdef __cplusplus
    }
#endif /* __cplusplus */

#ifdef HOJSON_IMPLEMENTATION

#ifdef _MSC_VER
    #pragma warning(push) /* Supress MSVC warning C6011 due to false positives */
    #pragma warning(disable: 6011)
#endif /* _MSC_VER */

/******************/
/* Implementation */

enum {
    HOJSON_STATE_ERROR_INTERNAL = -5,
    HOJSON_STATE_ERROR_INSUFFICIENT_MEMORY = -4,
    HOJSON_STATE_ERROR_UNEXPECTED_EOF = -3,
    HOJSON_STATE_ERROR_TOKEN_MISMATCH = -2,
    HOJSON_STATE_ERROR_SYNTAX = -1,
    HOJSON_STATE_NONE = 0, /* Initial state meaning no JSON content has been found yet */
    HOJSON_STATE_UTF8_BOM1, /* The first byte of a UTF-8 byte order marker was found */
    HOJSON_STATE_UTF8_BOM2, /* The second byte of a UTF-8 byte order marker was found */
    HOJSON_STATE_UTF16BE_BOM, /* The first byte of a UTF-16BE byte order marker was found */
    HOJSON_STATE_UTF16LE_BOM, /* The first byte of a UTF-16LE byte order marker was found */
    HOJSON_STATE_NAME_EXPECTED, /* A name is expected due to beginning an object or finding a comma after a pair */
    HOJSON_STATE_NAME, /* A name was started by a double quote (") and characters are being appended */
    HOJSON_STATE_POST_NAME, /* A name was ended by a double quote (") and a colon (:) is expected */
    HOJSON_STATE_VALUE_EXPECTED, /* A value is expected due to a colon (:) after a name or a comma (,) in an array */
    HOJSON_STATE_STRING_VALUE, /* A double quote (") was found after a colon (:) or in an array */
    HOJSON_STATE_ESCAPE, /* A backslash (\) was found and an escaped or Unicode character is expected */
    HOJSON_STATE_UNICODE_1, /* Unicode escapement notation was found, a hex number is expected */
    HOJSON_STATE_UNICODE_2, /* Unicode escapement notation was found, a second hex number is expected */
    HOJSON_STATE_UNICODE_3, /* Unicode escapement notation was found, a third hex number is expected */
    HOJSON_STATE_UNICODE_4, /* Unicode escapement notation was found, a fourth hex number is expected */
    HOJSON_STATE_NUMBER_VALUE, /* A number character (0-9) was found after a colon (:) or in an array */
    HOJSON_STATE_TRUE_VALUE_T, /* A 't' was found after a colon (:) or in an array, an 'r' is expected */
    HOJSON_STATE_TRUE_VALUE_R, /* An 'r' was found after a 't', a 'u' is expected */
    HOJSON_STATE_TRUE_VALUE_U, /* A 'u' was found after an 'r', an 'e' is expected */
    HOJSON_STATE_FALSE_VALUE_F, /* An 'f' was found after a colon (:) or in an array, an 'a' is expected */
    HOJSON_STATE_FALSE_VALUE_A, /* An 'a' was found after an 'f', an 'l' is expected */
    HOJSON_STATE_FALSE_VALUE_L, /* An 'l' was found after an 'a', an 's' is expected */
    HOJSON_STATE_FALSE_VALUE_S, /* An 's' was found after an 'l', an 'e' is expected */
    HOJSON_STATE_NULL_VALUE_N, /* An 'n' was found after a colon (:) or in an array, a 'u' is expected */
    HOJSON_STATE_NULL_VALUE_U, /* A 'u' was found after an 'n', an 'l' is expected */
    HOJSON_STATE_NULL_VALUE_L, /* An 'l' was found after a 'u', another 'l' is expected */
    HOJSON_STATE_POST_VALUE, /* A value was found, a comma (,) or closing token (} or ]) is expected */
    HOJSON_STATE_DONE /* Parsing has completed after finding a closed root object or array */
};

enum {
    HOJSON_FLAG_IS_ARRAY = 1, /* marks a node as an array, not an object, only values are expected */
    HOJSON_FLAG_HAS_NAME = 2, /* the node has a name that would apply to a child object or array */
    HOJSON_FLAG_COMMA = 4, /* a comma appeared after a name-value pair and another is expected later */
    HOJSON_FLAG_DECIMAL = 8, /* a number value contains a decimal (.) */
    HOJSON_FLAG_EXPONENT = 16, /* a number value contains an exponent (e or E) */
    HOJSON_FLAG_PLUS_OR_MINUS = 32, /* a number value contains a plus (+) or minus (-) sign */
    HOJSON_FLAG_MUST_POP_STACK = 64, /* the stack must be popped on the next call to hojson_parse() */
    HOJSON_FLAG_POST_VALUE_CLEAN_UP = 128, /* context object's name and values must be nullified/zeroed */
    HOJSON_FLAG_INCREMENT_DEPTH = 256, /* context object's depth value should increase by one next hojson_parse() */
    HOJSON_FLAG_DECREMENT_DEPTH = 512 /* context object's depth value should decrease by one netx hojson_parse() */
};

enum {
    HOJSON_ENCODING_UNKNOWN = 0, /* The character encoding is unknown. UTF-8 is assumed. */
    HOJSON_ENCODING_UTF_8, /* Variable-length encoding (8, 16, 24, or 32 bits) compatible with ASCII */
    HOJSON_ENCODING_UTF_16_LE, /* Variable-length encoding (16 or 32 bits), little-endian variant */
    HOJSON_ENCODING_UTF_16_BE /* Variable-lenght encoding (16 or 32 bits), big-endian variant */
};

typedef struct _hojson_node_t hojson_node_t;
typedef struct _hojson_node_t {
    hojson_node_t* parent; /* Points to the parent node, or NULL if this is the root */
    char* end; /* Points to the last byte of this node's data */
    uint16_t flags; /* May contain any number of bit flags indicating various things */
    char data; /* Where characters will be stored in the buffer, must be defined last */
} hojson_node_t;

typedef struct _hojson_character_t {
    uint32_t raw; /* Character as it appeared in the content. In other words, the original, encoded character. */
    uint32_t value; /* Unicode value of the character. In other words, the decoded character. */
    size_t bytes; /* Number of eight-bit bytes of the encoded character, in the [1, 4] range */
} hojson_character_t;

#ifndef UINT32_MAX /* Defined in stdint.h with later revisions of C and C++ but not for some earlier ones */
    #define UINT32_MAX (0xffffffff)
#endif
#define HOJSON_STACK ((hojson_node_t*)context->stack)
#define HOJSON_TO_LOWER(c) (c >= 'A' && c <= 'Z' ? c + 32 : c)
#define HOJSON_IS_NEW_LINE(c) (c == '\n' || c == '\r')
#define HOJSON_IS_WHITESPACE(c) (c == ' ' || c == '\t' || HOJSON_IS_NEW_LINE(c))
#define HOJSON_IS_NUMERIC(c) (c >= '0' && c <= '9')
#define HOJSON_IS_HEX_CHAR(c) (HOJSON_IS_NUMERIC(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
#define HOJSON_MAXIMUM(a,b) (a >= b ? a : b)
#ifdef HOJSON_DEBUG
    #include <stdio.h> /* printf() */
    #define HOJSON_LOG_STATE(s) printf("%s\n", s);
#else
    #define HOJSON_LOG_STATE(s)
#endif

void hojson_stay(hojson_context_t* context);
void hojson_push_stack(hojson_context_t* context);
void hojson_pop_stack(hojson_context_t* context);
hojson_code_t hojson_append_character(hojson_context_t* context, hojson_character_t c);
hojson_code_t hojson_append_terminator(hojson_context_t* context);
hojson_code_t hojson_begin_token(hojson_context_t* context, char token);
hojson_code_t hojson_end_token(hojson_context_t* context, char token);
hojson_character_t hojson_decode_character(const char* str, size_t str_length, uint8_t encoding);
hojson_character_t hojson_encode_character(uint32_t value, uint8_t encoding);
uint32_t hojson_hex_character_to_decimal(uint32_t value);

HOJSON_DECL void hojson_init(hojson_context_t* context, char* buffer, const size_t buffer_length) {
    if (context == NULL || buffer == NULL || buffer_length <= 0)
        return;

    memset(context, 0, sizeof(hojson_context_t)); /* Assign all values of the context to zero */
    context->buffer = buffer; /* Use the provided buffer */
    context->buffer_length = buffer_length; /* Remember the length of the provided buffer */
    context->line = 1; /* This is meant to be human-readable and humans begin counting at one */
    context->is_initialized = 1;
    memset(buffer, 0, buffer_length); /* Fill the buffer with zeroes */
}

HOJSON_DECL void hojson_realloc(hojson_context_t* context, char* buffer, const size_t buffer_length) {
    if (context == NULL || context->is_initialized == 0 || buffer == NULL || buffer_length <= context->buffer_length)
        return;

    /* Reassign the end and parent pointers of each node, beginning at the tail and iterate to the head */
    hojson_node_t* node = HOJSON_STACK;
    while (node != NULL) {
        hojson_node_t* parent = node->parent;
        node->end = buffer + (node->end - context->buffer);
        if (node->parent != NULL)
            node->parent = (hojson_node_t*)(buffer + ((char*)node->parent - context->buffer));
        node = parent;
    }

    /* Use offsets from the original buffer pointer to reassign pointers such that they now point to the new buffer */
    if (context->name != NULL)
        context->name = buffer + (context->name - context->buffer);
    if (context->string_value != NULL)
        context->string_value = buffer + (context->string_value - context->buffer);
    if (context->stack != NULL)
        context->stack = buffer + ((char*)context->stack - context->buffer);

    memset(buffer, 0, buffer_length); /* Fill the new buffer with zeroes */
    memcpy(buffer, context->buffer, context->buffer_length); /* Copy the entire, current buffer to the new buffer */
    context->buffer = buffer;
    context->buffer_length = buffer_length;

    /* If this reallocation was done in order to recover from an "insufficient memory" error */
    if (context->state == HOJSON_STATE_ERROR_INSUFFICIENT_MEMORY) {
        /* Assign the state variables to their pre-error values so parsing can continue */
        context->state = context->error_return_state;
        context->error_return_state = HOJSON_STATE_NONE;
    }
}

HOJSON_DECL hojson_code_t hojson_parse(hojson_context_t* context, const char* json, const size_t json_length) {
    /* If there's no context object, the context is unintialized, or no JSON content was provided */
     if (context == NULL || context->is_initialized == 0 || json == NULL || json_length <= 0)
        return HOJSON_ERROR_INVALID_INPUT;

    if (HOJSON_STACK != NULL) {
        if (HOJSON_STACK->flags & HOJSON_FLAG_INCREMENT_DEPTH) /* If an object/array began, increasing nesting */
        {
            context->depth += 1;
            HOJSON_STACK->flags &= ~HOJSON_FLAG_INCREMENT_DEPTH; /* Clear the flag */
        }

        if (HOJSON_STACK->flags & HOJSON_FLAG_DECREMENT_DEPTH) /* If an object/array ended, decreasing nesting */
        {
            context->depth -= 1;
            HOJSON_STACK->flags &= ~HOJSON_FLAG_DECREMENT_DEPTH; /* Clear the flag */
        }

        /* If an object or array ended and, now that its name was provided, its node must be popped from the stack */
        if (HOJSON_STACK->flags & HOJSON_FLAG_MUST_POP_STACK) {
            hojson_node_t* parent = HOJSON_STACK->parent;
            hojson_pop_stack(context); /* (Potentially) return to the parent node */
            if (parent == NULL) { /* If there was no parent (i.e. the popped object or array was the root) */
                context->state = HOJSON_STATE_DONE;
                return HOJSON_END_OF_DOCUMENT;
            }
        }

        if (HOJSON_STACK->flags & HOJSON_FLAG_POST_VALUE_CLEAN_UP) {
            /* If data, like a name or string value, had previously been appended to this node */
            if (HOJSON_STACK->end - (char*)&(HOJSON_STACK->data) > 0) {
                /* Zero the memory from the first character of the node's data to its end. Any data in this memory */
                /* range is now stale and taking up space unnecessarily. */
                memset(&(HOJSON_STACK->data), 0, HOJSON_STACK->end - (char*)&(HOJSON_STACK->data) + 1);
                /* Reassign the end pointer so it points to the new end, one byte before the now-erased data */
                HOJSON_STACK->end = &(HOJSON_STACK->data) - 1;
            }

            /* This flag is set following the ending of a name-value pair or (array) value as a sort of cleanup. */
            /* Any name or value provided to the user in the context object has expired so all such values need to be */
            /* nullified or zeroed. */
            context->name = NULL;
            context->value_type = HOJSON_TYPE_NONE;
            context->string_value = NULL;
            context->integer_value = 0;
            context->float_value = 0.0f;
            context->bool_value = 0;

            /* Clear all flags related to values used in parsing because they no longer apply */
            HOJSON_STACK->flags &= ~(HOJSON_FLAG_HAS_NAME | HOJSON_FLAG_COMMA | HOJSON_FLAG_DECIMAL |
                HOJSON_FLAG_EXPONENT | HOJSON_FLAG_PLUS_OR_MINUS | HOJSON_FLAG_POST_VALUE_CLEAN_UP);
        }
    }

    switch (context->state) { /* Check for error states or the "parsing has already finished" state */
    /* Two errors are recoverable: HOJSON_ERROR_INSUFFICIENT_MEMORY and HOJSON_ERROR_UNEXPECTED_EOF. The former can */
    /* be recovered by assigning a new buffer with hojson_realloc(). The latter can be recovered by passing a */
    /* new JSON content string to hojson_parse() so we'll check for one before concluding we're still in error. */
    case HOJSON_STATE_ERROR_UNEXPECTED_EOF: {
        /* Try to decode a character, or remainder of a character, at the beginning of this hopefully-new string */
        uint32_t stream = context->stream;
        size_t bytes_to_copy = (json_length <= 4 ? json_length : 4) - context->stream_length;
        if (bytes_to_copy < 4)
            memcpy((char*)&stream + context->stream_length, json, bytes_to_copy);
        else
            stream = *(uint32_t*)json;
        hojson_character_t c = hojson_decode_character((const char*)&stream, json_length, context->encoding);
        if (c.value == 0 || c.value == UINT32_MAX) /* If a null terminator or there was not enough data */
            return HOJSON_ERROR_UNEXPECTED_EOF;
        context->state = context->error_return_state;
        context->error_return_state = HOJSON_STATE_NONE;
        /* Note: there is a check for a change to the input pointer a little further down */
    } break;
    case HOJSON_STATE_DONE: return HOJSON_END_OF_DOCUMENT;
    case HOJSON_STATE_ERROR_INTERNAL: return HOJSON_ERROR_INTERNAL;
    case HOJSON_STATE_ERROR_INSUFFICIENT_MEMORY: return HOJSON_ERROR_INSUFFICIENT_MEMORY;
    case HOJSON_STATE_ERROR_TOKEN_MISMATCH: return HOJSON_ERROR_TOKEN_MISMATCH;
    case HOJSON_STATE_ERROR_SYNTAX: return HOJSON_ERROR_SYNTAX;
    }

    if (context->json != json) { /* If the pointer to the JSON content string has changed */
        /* A few variables are now invalid: the pointer to the content, its length, and the iterator */
        context->json = json;
        context->json_length = json_length;
        context->iterator = json;
    }

    while (context->state >= HOJSON_STATE_NONE && context->state <= HOJSON_STATE_DONE) {
        /* Apart from the error states, "none" state, and BOM states, all states assume the stack is non-null */
        if (context->state >= HOJSON_STATE_NAME_EXPECTED && HOJSON_STACK == NULL) {
            /* Some unforseen bug has led us to a state in which continuing would cause an illegal memory access. */
            /* Parsing must halt. There is no way to recover. */
            context->state = HOJSON_STATE_ERROR_INTERNAL;
            return HOJSON_ERROR_INTERNAL;
        }

        size_t bytes_remaining = (size_t)(context->json_length - (context->iterator - context->json));
        size_t bytes_to_copy = (bytes_remaining <= 4 ? bytes_remaining : 4) - context->stream_length;
        if (bytes_to_copy < 4)
            memcpy((char*)&(context->stream) + context->stream_length, context->iterator, bytes_to_copy);
        else
            context->stream = *(uint32_t*)context->iterator;
        hojson_character_t c = hojson_decode_character((const char*)&(context->stream), bytes_remaining,
            context->encoding);

        /* If the character is the equivalent of a null terminator or there was not enough data to decode the value */
        if (c.value == 0 || c.value == UINT32_MAX) {
            context->stream_length = bytes_to_copy;
            context->error_return_state = context->state;
            context->state = HOJSON_STATE_ERROR_UNEXPECTED_EOF;
            return HOJSON_ERROR_UNEXPECTED_EOF;
        } else if (HOJSON_IS_NEW_LINE(c.value)) {
            if (context->newline_character == 0) /* If this is the first newline */
                context->newline_character = c.value; /* Remember this as the newline character to use for increments */
            if (c.value == context->newline_character) /* Avoid incrementing twice for files with \r\n endings */
                context->line++;
            context->column = 0;
        } else
            context->column++;

        /* Iterate by the number of bytes in the character (up to four). It's also possible part of this character */
        /* was carried over from a previous string. Those bytes would have been stashed in the context's  'stream' */
        /* variable where 'stream_length' tells us the number of said bytes. */
        context->bytes_iterated = c.bytes - context->stream_length;
        context->iterator += context->bytes_iterated;
        context->stream_length = 0;

        #ifdef HOJSON_DEBUG
            char debugValue = HOJSON_IS_NEW_LINE(c.value) ? ' ' : c.value;
            printf(" %c [%08X] [L%02dC%02d] [%c%c%c%c%c] -> ", debugValue, c.value, context->line, context->column,
                HOJSON_STACK ? (HOJSON_STACK->flags & HOJSON_FLAG_IS_ARRAY ? '1' : '0') : '0',
                HOJSON_STACK ? (HOJSON_STACK->flags & HOJSON_FLAG_COMMA ? '1' : '0') : '0',
                HOJSON_STACK ? (HOJSON_STACK->flags & HOJSON_FLAG_DECIMAL ? '1' : '0') : '0',
                HOJSON_STACK ? (HOJSON_STACK->flags & HOJSON_FLAG_EXPONENT ? '1' : '0') : '0',
                HOJSON_STACK ? (HOJSON_STACK->flags & HOJSON_FLAG_PLUS_OR_MINUS ? '1' : '0') : '0');
        #endif

        switch (context->state) {
        case HOJSON_STATE_NONE: /* Initial state meaning no JSON content has been found yet */
            HOJSON_LOG_STATE("HOJSON_STATE_NONE")
            if (c.value == '{' || c.value == '[')
                return hojson_begin_token(context, c.value);
            else if (c.value == 0xEF) { /* The UTF-8 Byte Order Marker (BOM) is [EF] BB BF, as hex bytes */
                context->state = HOJSON_STATE_UTF8_BOM1;
                context->column--; /* Don't count this as a column */
            } else if (c.value == 0xFE) { /* The UTF-16BE BOM is [FE] FF, as hex bytes */
                context->state = HOJSON_STATE_UTF16BE_BOM;
                context->column--; /* Don't count this as a column */
            } else if (c.value == 0xFF) { /* The UTF-16LE BOM is [FF] FE, as hex bytes */
                context->state = HOJSON_STATE_UTF16LE_BOM;
                context->column--; /* Don't count this as a column */
            } else if (!HOJSON_IS_WHITESPACE(c.value))
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_UTF8_BOM1: /* The first byte of a UTF-8 byte order marker was found */
            HOJSON_LOG_STATE("HOJSON_STATE_UTF8_BOM1")
            context->column--; /* Don't count this as a column */
            if (c.value == 0xBB) /* The UTF-8 BOM is EF [BB] BF, as hex bytes */
                context->state = HOJSON_STATE_UTF8_BOM2;
            else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_UTF8_BOM2: /* The second byte of a UTF-8 byte order marker was found */
            HOJSON_LOG_STATE("HOJSON_STATE_UTF8_BOM2")
            context->column--; /* Don't count this as a column */
            if (c.value == 0xBF) { /* The UTF-8 BOM is EF BB [BF], as hex bytes */
                context->state = HOJSON_STATE_NONE;
                context->encoding = HOJSON_ENCODING_UTF_8;
            } else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_UTF16BE_BOM: /* The first byte of a UTF-16BE byte order marker was found */
            HOJSON_LOG_STATE("HOJSON_STATE_UTF16BE_BOM")
            context->column--; /* Don't count this as a column */
            if (c.value == 0xFF) { /* The UTF-16BE BOM is FE [FF], as hex bytes */
                context->state = HOJSON_STATE_NONE;
                context->encoding = HOJSON_ENCODING_UTF_16_BE;
            } else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_UTF16LE_BOM: /* The first byte of a UTF-16LE byte order marker was found */
            HOJSON_LOG_STATE("HOJSON_STATE_UTF16LE_BOM")
            context->column--; /* Don't count this as a column */
            if (c.value == 0xFE) { /* The UTF-16LE BOM is FF [FE], as hex bytes */
                context->state = HOJSON_STATE_NONE;
                context->encoding = HOJSON_ENCODING_UTF_16_LE;
            } else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_NAME_EXPECTED: /* A name is expected due to beginning an object or finding a comma after a pair */
            HOJSON_LOG_STATE("HOJSON_STATE_NAME_EXPECTED")
            if (c.value == '\"') { /* If a name started */
                HOJSON_STACK->flags |= HOJSON_FLAG_HAS_NAME;
                context->state = HOJSON_STATE_NAME;
            } else if (c.value == '}' || c.value == ']') /* If an object or array potentially ended */
                return hojson_end_token(context, c.value);
            else if (!HOJSON_IS_WHITESPACE(c.value))
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_NAME: /* A name was started by a double quote (") and characters are being appended */
            HOJSON_LOG_STATE("HOJSON_STATE_NAME")
            if (c.value == '\"') {
                hojson_code_t code = hojson_append_terminator(context);
                if (code < HOJSON_NO_OP) /* If appending the terminator failed */
                    return code;
                else { /* If appending the terminator succeeded */
                    /* Assign the name pointer to this node's data so its availabe to a user. The name was appended */
                    /* beginning at the node's 'data' variable on the stack. */
                    context->name = &(HOJSON_STACK->data);
                    context->state = HOJSON_STATE_POST_NAME;
                    return HOJSON_NAME;
                }
            } else if (c.value == '\\') { /* If a character is being escaped */
                /* No need to append this character, it'll be discarded anyway. Just transition to escape state. */
                context->escape_return_state = context->state; /* Branch back to this state when done with the escape */
                context->state = HOJSON_STATE_ESCAPE;
            } else {
                hojson_code_t code = hojson_append_character(context, c);
                if (code < HOJSON_NO_OP) /* If appending the character failed */
                    return code;
            } break;
        case HOJSON_STATE_POST_NAME: /* A name was ended by a double quote (") and a colon (:) is expected */
            HOJSON_LOG_STATE("HOJSON_STATE_POST_NAME")
            if (c.value == ':')
                context->state = HOJSON_STATE_VALUE_EXPECTED;
            else if (!HOJSON_IS_WHITESPACE(c.value))
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_VALUE_EXPECTED: /* A value is expected due to a colon (:) or a comma (,) in an array */
            HOJSON_LOG_STATE("HOJSON_STATE_VALUE_EXPECTED")
            if (c.value == '"') { /* If a double quote (") was found " */
                context->string_value = HOJSON_STACK->end + 1; /* The value's string will begin here */
                context->state = HOJSON_STATE_STRING_VALUE; /* Expect a string value */
            } else if (HOJSON_IS_NUMERIC(c.value) || c.value == '-') { /* If a numeric (0-9) or '-' was found */
                /* The characters of the number will be appended as they appear with the string value variable being */
                /* used, temporarily, to build the full string to be parsed as a number */
                context->string_value = HOJSON_STACK->end + 1; /* The value's string will begin here */
                hojson_code_t code = hojson_append_character(context, c);
                if (code < HOJSON_NO_OP) /* If appending the character failed */
                    return code;
                else /* If appending the character succeeded */
                    context->state = HOJSON_STATE_NUMBER_VALUE; /* Expect a number value */
            } else if (c.value == 't') /* If the T in "true" was found */
                context->state = HOJSON_STATE_TRUE_VALUE_T; /* Expect the rest of "true" */
            else if (c.value == 'f') /* If the F in "false" was found */
                context->state = HOJSON_STATE_FALSE_VALUE_F; /* Expect the rest of "false" */
            else if (c.value == 'n') /* If the N in "null" was found */
                context->state = HOJSON_STATE_NULL_VALUE_N; /* "Expect the rest of "null" */
            else if (c.value == '{' || c.value == '[') /* If a beginning token ({ or [) was found */
                return hojson_begin_token(context, c.value);
            else if (c.value == '}' || c.value == ']') /* If an ending token (} or ]) was found */
                return hojson_end_token(context, c.value);
            else if (!HOJSON_IS_WHITESPACE(c.value))
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_STRING_VALUE: /* A double quote (") was found after a colon (:) or in an array */
            HOJSON_LOG_STATE("HOJSON_STATE_STRING_VALUE")
            if (c.value == '"') {
                context->value_type = HOJSON_TYPE_STRING;
                HOJSON_STACK->flags |= HOJSON_FLAG_POST_VALUE_CLEAN_UP; /* Clean up with the next hojson_parse() */
                context->state = HOJSON_STATE_POST_VALUE;
                return HOJSON_VALUE;
            } else if (c.value == '\\') { /* If a character is being escaped */
                /* No need to append this character, it'll be discarded anyway. Just transition to escape state. */
                context->escape_return_state = context->state; /* Branch back to this state when done with the escape */
                context->state = HOJSON_STATE_ESCAPE;
            } else {
                hojson_code_t code = hojson_append_character(context, c);
                if (code < HOJSON_NO_OP) /* If appending the character failed */
                    return code;
            } break;
        case HOJSON_STATE_ESCAPE: { /* A backslash (\) was found and an escaped or Unicode character is expected */
            HOJSON_LOG_STATE("HOJSON_STATE_ESCAPE")
            uint32_t characterToAppend;
            switch (c.value) {
            /* The non-default cases here represent the only characters that are acceptable after a backslash */
            case '"':  characterToAppend = '"';  break;
            case '\\': characterToAppend = '\\'; break;
            case '/':  characterToAppend = '/';  break;
            case 'b':  characterToAppend = '\b'; break;
            case 'f':  characterToAppend = '\f'; break;
            case 'n':  characterToAppend = '\n'; break;
            case 'r':  characterToAppend = '\r'; break;
            case 't':  characterToAppend = '\t'; break;
            /* 'u' is special: it is a Unicode substitution where four hex characters are expected to follow */
            case 'u':
                context->state = HOJSON_STATE_UNICODE_1;
                continue;
            /* All other characters are invalid syntax */
            default: context->state = HOJSON_STATE_ERROR_SYNTAX; continue;
            }
            hojson_character_t encodedCharacter = hojson_encode_character(characterToAppend, context->encoding);
            hojson_code_t code = hojson_append_character(context, encodedCharacter);
            if (code < HOJSON_NO_OP) /* If appending the character failed */
                return code;
            else {
                /* Return to the state we originally branched from */
                context->state = context->escape_return_state;
                context->escape_return_state = HOJSON_STATE_NONE;
            } } break;
        case HOJSON_STATE_UNICODE_1: /* Unicode escapement notation was found, a hex number is expected */
            HOJSON_LOG_STATE("HOJSON_STATE_UNICODE_1")
            if (HOJSON_IS_HEX_CHAR(c.value)) {
                /* Hexadecimal (base-16) can be converted to decimal (base-ten) iteratively. For example, given the */
                /* hex value ABCD, the decimal equivalent is (A * 16^3) + (B * 16^2) + (C * 16^1) + (D * 16^0). This */
                /* state is dedicated to the most significant digit so we multiply by 16^3 (4096). */
                /* Note: the use of the integer number value variable is only temporary. */
                context->integer_value = hojson_hex_character_to_decimal(c.value) * 4096;
                context->state = HOJSON_STATE_UNICODE_2;
            } else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_UNICODE_2: /* Unicode escapement notation was found, a second hex number is expected */
            HOJSON_LOG_STATE("HOJSON_STATE_UNICODE_2")
            if (HOJSON_IS_HEX_CHAR(c.value)) {
                context->integer_value += hojson_hex_character_to_decimal(c.value) * 256; /* 16^2 */
                context->state = HOJSON_STATE_UNICODE_3;
            } else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_UNICODE_3: /* Unicode escapement notation was found, a third hex number is expected */
            HOJSON_LOG_STATE("HOJSON_STATE_UNICODE_3")
            if (HOJSON_IS_HEX_CHAR(c.value)) {
                context->integer_value += hojson_hex_character_to_decimal(c.value) * 16; /* 16^1 */
                context->state = HOJSON_STATE_UNICODE_4;
            } else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_UNICODE_4: /* Unicode escapement notation was found, a fourth hex number is expected */
            HOJSON_LOG_STATE("HOJSON_STATE_UNICODE_4")
            if (HOJSON_IS_HEX_CHAR(c.value)) {
                context->integer_value += hojson_hex_character_to_decimal(c.value) * 1; /* 16^0 */
                hojson_character_t encodedCharacter = hojson_encode_character(context->integer_value,
                    context->encoding);
                hojson_code_t code = hojson_append_character(context, encodedCharacter);
                if (code < HOJSON_NO_OP) /* If appending the character failed */
                    return code;
                else {
                    context->integer_value = 0; /* Zero the integer number value; its use was only temporary */
                    context->state = context->escape_return_state; /* Return to the state we originally branched from */
                    context->escape_return_state = HOJSON_STATE_NONE;
                }
            } else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_NUMBER_VALUE: /* A number character (0-9) was found after a colon (:) or in an array */
            HOJSON_LOG_STATE("HOJSON_STATE_NUMBER_VALUE")
            if (HOJSON_IS_NUMERIC(c.value)) {
                hojson_code_t code = hojson_append_character(context, c);
                if (code < HOJSON_NO_OP) /* If appending the character failed */
                    return code;
            } else if (c.value == '.') {
                if (HOJSON_STACK->flags & HOJSON_FLAG_DECIMAL) /* If the number already has a decimal */
                    context->state = HOJSON_STATE_ERROR_SYNTAX;
                else {
                    hojson_code_t code = hojson_append_character(context, c);
                    if (code < HOJSON_NO_OP) /* If appending the character failed */
                        return code;
                    else /* If appending the character succeeded */
                        HOJSON_STACK->flags |= HOJSON_FLAG_DECIMAL; /* Set the "has a decimal" flag */
                }
            } else if (c.value == 'e' || c.value == 'E') {
                if (HOJSON_STACK->flags & HOJSON_FLAG_EXPONENT) /* If the number already has an 'e' or 'E' */
                    context->state = HOJSON_STATE_ERROR_SYNTAX;
                else {
                    hojson_code_t code = hojson_append_character(context, c);
                    if (code < HOJSON_NO_OP) /* If appending the character failed */
                        return code;
                    else /* If appending the character succeeded */
                        HOJSON_STACK->flags |= HOJSON_FLAG_EXPONENT; /* Set the "has an exponent" flag */
                }
            } else if (c.value == '-' || c.value == '+') {
                if (!(HOJSON_STACK->flags & HOJSON_FLAG_EXPONENT)) { /* If not preceded by an 'e' or 'E' */
                    context->state = HOJSON_STATE_ERROR_SYNTAX;
                } else if (HOJSON_STACK->flags & HOJSON_FLAG_PLUS_OR_MINUS) /* If there was a previous '+' or '-' */
                    context->state = HOJSON_STATE_ERROR_SYNTAX;
                else {
                    hojson_code_t code = hojson_append_character(context, c);
                    if (code < HOJSON_NO_OP) /* If appending the character failed */
                        return code;
                    else /* If appending the character succeeded */
                        HOJSON_STACK->flags |= HOJSON_FLAG_PLUS_OR_MINUS; /* Set the "has a + or -" flag */
                }
            } else if (HOJSON_IS_WHITESPACE(c.value) || c.value == ',' || c.value == ']' || c.value == '}') {
                /* Parse the string from the JSON content as a number. Strings with decimals or exponents will be */
                /* parsed as floating-point values. Strings without both will be parsed as integer values. */
                /* Note: while E notation could potentially describe an integer, atoi() does not support E notation. */
                if (HOJSON_STACK->flags & HOJSON_FLAG_DECIMAL || HOJSON_STACK->flags & HOJSON_FLAG_EXPONENT) {
                    context->value_type = HOJSON_TYPE_FLOAT; /* Indicate the value is a floating-point number type */
                    if (context->string_value != NULL) /* Quick error check */
                        context->float_value = atof(context->string_value);
                } else {
                    context->value_type = HOJSON_TYPE_INTEGER; /* Indicate the value is an integer number type */
                    if (context->string_value != NULL) /* Quick error check */
                        context->integer_value = atoi(context->string_value);
                }
                /* Nullify the value string. It was temporarily pointing to the number as a string. */
                context->string_value = NULL;

                HOJSON_STACK->flags |= HOJSON_FLAG_POST_VALUE_CLEAN_UP; /* Clean up with the next hojson_parse() */
                context->state = HOJSON_STATE_POST_VALUE;

                if (!HOJSON_IS_WHITESPACE(c.value)) /* If a ',' or ']' or '}' ended the number value */
                    hojson_stay(context); /* Rewind by one character so this character is parsed */

                return HOJSON_VALUE;
            } else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_TRUE_VALUE_T: /* A 't' was found after a colon (:) or in an array, an 'r' is expected */
            HOJSON_LOG_STATE("HOJSON_STATE_TRUE_VALUE_T")
            if (c.value == 'r')
                context->state = HOJSON_STATE_TRUE_VALUE_R;
            else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_TRUE_VALUE_R: /* An 'r' was found after a 't', a 'u' is expected */
            HOJSON_LOG_STATE("HOJSON_STATE_TRUE_VALUE_R")
            if (c.value == 'u')
                context->state = HOJSON_STATE_TRUE_VALUE_U;
            else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_TRUE_VALUE_U: /* A 'u' was found after an 'r', an 'e' is expected */
            HOJSON_LOG_STATE("HOJSON_STATE_TRUE_VALUE_U")
            if (c.value == 'e') {
                context->value_type = HOJSON_TYPE_BOOLEAN; /* Indicate the value is a boolean type */
                context->bool_value = 1; /* Non-zero values evalulate to true */
                HOJSON_STACK->flags |= HOJSON_FLAG_POST_VALUE_CLEAN_UP; /* Clean up with the next hojson_parse() */
                context->state = HOJSON_STATE_POST_VALUE;
                return HOJSON_VALUE;
            } else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_FALSE_VALUE_F: /* An 'f' was found after a colon (:) or in an array, an 'a' is expected */
            HOJSON_LOG_STATE("HOJSON_STATE_FALSE_VALUE_F")
            if (c.value == 'a')
                context->state = HOJSON_STATE_FALSE_VALUE_A;
            else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_FALSE_VALUE_A: /* An 'a' was found after an 'f', an 'l' is expected */
            HOJSON_LOG_STATE("HOJSON_STATE_FALSE_VALUE_A")
            if (c.value == 'l')
                context->state = HOJSON_STATE_FALSE_VALUE_L;
            else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_FALSE_VALUE_L: /* An 'l' was found after an 'a', an 's' is expected */
            HOJSON_LOG_STATE("HOJSON_STATE_FALSE_VALUE_L")
            if (c.value == 's')
                context->state = HOJSON_STATE_FALSE_VALUE_S;
            else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_FALSE_VALUE_S: /* An 's' was found after an 'l', an 'e' is expected */
            HOJSON_LOG_STATE("HOJSON_STATE_FALSE_VALUE_S")
            if (c.value == 'e') {
                context->value_type = HOJSON_TYPE_BOOLEAN; /* Indicate the value is a boolean type */
                context->bool_value = 0; /* Zero evalulates to false */
                HOJSON_STACK->flags |= HOJSON_FLAG_POST_VALUE_CLEAN_UP; /* Clean up with the next hojson_parse() */
                context->state = HOJSON_STATE_POST_VALUE;
                return HOJSON_VALUE;
            } else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_NULL_VALUE_N: /* An 'n' was found after a colon (:) or in an array, a 'u' is expected */
            HOJSON_LOG_STATE("HOJSON_STATE_NULL_VALUE_N")
            if (c.value == 'u')
                context->state = HOJSON_STATE_NULL_VALUE_U;
            else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_NULL_VALUE_U: /* A 'u' was found after an 'n', an 'l' is expected */
            HOJSON_LOG_STATE("HOJSON_STATE_NULL_VALUE_U")
            if (c.value == 'l')
                context->state = HOJSON_STATE_NULL_VALUE_L;
            else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_NULL_VALUE_L: /* An 'l' was found after a 'u', another 'l' is expected */
            HOJSON_LOG_STATE("HOJSON_STATE_NULL_VALUE_L")
            if (c.value == 'l') {
                context->value_type = HOJSON_TYPE_NULL; /* Indicate the value is a null/unset type */
                HOJSON_STACK->flags |= HOJSON_FLAG_POST_VALUE_CLEAN_UP; /* Clean up with the next hojson_parse() */
                context->state = HOJSON_STATE_POST_VALUE;
                return HOJSON_VALUE;
            } else
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        case HOJSON_STATE_POST_VALUE: /* A value was found, a comma (,) or closing token (} or ]) is expected */
            HOJSON_LOG_STATE("HOJSON_STATE_POST_VALUE")
            if (c.value == '}' || c.value == ']')
                return hojson_end_token(context, c.value);
            else if (c.value == ',') {
                if (HOJSON_STACK->flags & HOJSON_FLAG_COMMA)
                    context->state = HOJSON_STATE_ERROR_SYNTAX;
                else {
                    HOJSON_STACK->flags |= HOJSON_FLAG_COMMA;
                    if (HOJSON_STACK->flags & HOJSON_FLAG_IS_ARRAY)
                        context->state = HOJSON_STATE_VALUE_EXPECTED;
                    else
                        context->state = HOJSON_STATE_NAME_EXPECTED;
                }
            } else if (!HOJSON_IS_WHITESPACE(c.value))
                context->state = HOJSON_STATE_ERROR_SYNTAX;
            break;
        } /* switch (context->state) */
    } /* while (context->state >= HOJSON_STATE_NONE && context->state <= HOJSON_STATE_DONE) */

    return HOJSON_ERROR_SYNTAX; /* Any other error case not yet covered by previous checks is due to bad syntax */
}

void hojson_stay(hojson_context_t* context) {
    /* For the X-byte step forward, take an X-byte step back. With this, parsing will return to the last character. */
    context->iterator -= context->bytes_iterated;
    context->column--;
}

void hojson_push_stack(hojson_context_t* context) {
    /* If "allocating" a new node would overflow the buffer */
    if ((HOJSON_STACK == NULL && sizeof(hojson_node_t) >= context->buffer_length) || (HOJSON_STACK != NULL &&
            HOJSON_STACK->end + 1 + sizeof(hojson_node_t) >= context->buffer + context->buffer_length)) {
        context->error_return_state = context->state;
        context->state = HOJSON_STATE_ERROR_INSUFFICIENT_MEMORY;
        return;
    }

    hojson_node_t* node;
    if (HOJSON_STACK == NULL) /* If pushing the root node */
        node = (hojson_node_t*)context->buffer; /* Place the new node at the beginning of the buffer */
    else
        node = (hojson_node_t*)(HOJSON_STACK->end + 1); /* Place the new node right after the parent node */

    if (node != NULL) { /* Quick error check, just in case */
        node->parent = HOJSON_STACK; /* This new node's parent is the previous stack node */
        node->end = &(node->data) - 1; /* Point node's last byte, -1 because nothing has been appended yet */
    }

    context->stack = (char*)node;
}

void hojson_pop_stack(hojson_context_t* context) {
    if (HOJSON_STACK == NULL) /* Quick error check. If there is no node on the stack to pop. */
        return;

    hojson_node_t* popped_node = HOJSON_STACK;
    context->stack = (char*)popped_node->parent;

    /* Overwrite the memory used by this node with zeroes. The number of bytes overwritten will either be the number */
    /* of bytes in a node data type or, if appended characters caused it to grow further, the range of bytes from the */
    /* start of the node to the end pointer, inclusive. */
    memset(popped_node, 0, HOJSON_MAXIMUM(sizeof(hojson_node_t), (size_t)(popped_node->end - (char*)popped_node + 1)));
}

hojson_code_t hojson_append_character(hojson_context_t* context, hojson_character_t c) {
    if (HOJSON_STACK->end + c.bytes >= context->buffer + context->buffer_length) {
        hojson_stay(context); /* Rewind by one character */
        context->error_return_state = context->state;
        context->state = HOJSON_STATE_ERROR_INSUFFICIENT_MEMORY;
        return HOJSON_ERROR_INSUFFICIENT_MEMORY;
    }

    memcpy(HOJSON_STACK->end + 1, &(c.raw), c.bytes); /* Copy the character to the stack */
    HOJSON_STACK->end += c.bytes; /* Redirect the end pointer to the new end just after the appended character */
    return HOJSON_NO_OP;
}

hojson_code_t hojson_append_terminator(hojson_context_t* context) {
    /* If the document is encoded with UTF-16, two bytes will be appended. One byte otherwise. */
    uint8_t bytes = context->encoding >= HOJSON_ENCODING_UTF_16_BE ? 2 : 1;
    if (HOJSON_STACK->end + bytes >= context->buffer + context->buffer_length) {
        hojson_stay(context); /* Rewind by one character */
        context->error_return_state = context->state;
        context->state = HOJSON_STATE_ERROR_INSUFFICIENT_MEMORY;
        return HOJSON_ERROR_INSUFFICIENT_MEMORY;
    }

    memset(HOJSON_STACK->end + 1, '\0', bytes); /* Copy the terminator to the stack */
    HOJSON_STACK->end += bytes; /* Redirect the end pointer to the new end just after the appended terminator */
    return HOJSON_NO_OP;
}

hojson_code_t hojson_begin_token(hojson_context_t* context, char token) {
    /* If a node exists (i.e. we're not pushing the root) and there is a name for this object or array */
    if (HOJSON_STACK != NULL && HOJSON_STACK->flags & HOJSON_FLAG_HAS_NAME)
        context->name = &(HOJSON_STACK->data); /* Provide the name of the object or array to the user */
    else
        context->name = NULL;
    context->string_value = NULL;
    context->integer_value = 0;
    context->float_value = 0.0;
    context->bool_value = 0;

    hojson_push_stack(context); /* Create a new node for the new object or array */

    if (context->state >= HOJSON_STATE_NONE) { /* If pushing a new node was successful */
        HOJSON_STACK->flags |= HOJSON_FLAG_POST_VALUE_CLEAN_UP; /* Clean up with the next hojson_parse() */
        HOJSON_STACK->flags |= HOJSON_FLAG_INCREMENT_DEPTH; /* An object or array means one more level of nesting */

        if (token == '{') { /* If an object began */
            context->state = HOJSON_STATE_NAME_EXPECTED; /* Transition to the state appropriate for a new object */
            return HOJSON_OBJECT_BEGIN;
        } else if (token == '[') { /* If an array began */
            HOJSON_STACK->flags |= HOJSON_FLAG_IS_ARRAY; /* Flag the node as an array (as opposed to an object) */
            context->state = HOJSON_STATE_VALUE_EXPECTED; /* Transition to the state appropriate for a new array */
            return HOJSON_ARRAY_BEGIN;
        }
    } else { /* If pushing a new node was NOT successful */
        hojson_stay(context); /* Rewind by one character */
        return HOJSON_ERROR_INSUFFICIENT_MEMORY;
    }

    /* For all other cases, assume some unforseen bug led to an unrecoverable state */
    context->state = HOJSON_STATE_ERROR_INTERNAL;
    return HOJSON_ERROR_INTERNAL;
}

hojson_code_t hojson_end_token(hojson_context_t* context, char token) {
    if ((HOJSON_STACK->flags & HOJSON_FLAG_IS_ARRAY && token != ']') ||
            (!(HOJSON_STACK->flags & HOJSON_FLAG_IS_ARRAY) && token != '}')) {
        /* If a '{' was closed by a ']' or a '[' or was closed by a '}' */
        context->state = HOJSON_STATE_ERROR_TOKEN_MISMATCH;
        return HOJSON_ERROR_TOKEN_MISMATCH;
    } else if (HOJSON_STACK->flags & HOJSON_FLAG_COMMA) {
        /* If the object or array ended immediately after a comma */
        context->state = HOJSON_STATE_ERROR_SYNTAX;
        return HOJSON_ERROR_SYNTAX; /* Trailing commas are not allowed */
    }

    context->state = HOJSON_STATE_POST_VALUE; /* Objects/arrays are values so transition to the appropriate state */
    context->name = NULL; /* Initially assume the closed object or array has no name. This may change later. */

    /* Set a flag to pop the stack on the next call to hojson_parse(). This cannot be done yet because the name of */
    /* the object or array that just closed should be provided to the user and that string is within the memory of */
    /* the node we want to pop. */
    HOJSON_STACK->flags |= HOJSON_FLAG_MUST_POP_STACK;

    /* Ending an object or array means one less level of nesting */
    HOJSON_STACK->flags |= HOJSON_FLAG_DECREMENT_DEPTH;

    /* If a parent node exists and it has a name for this object or array */
    if (HOJSON_STACK->parent != NULL) {
        if (HOJSON_STACK->parent->flags & HOJSON_FLAG_HAS_NAME) /* If there's a name for this object or array */
            context->name = &(HOJSON_STACK->parent->data); /* Provide the name of the object or array to the user */

        HOJSON_STACK->parent->flags |= HOJSON_FLAG_POST_VALUE_CLEAN_UP; /* Clean up with the next hojson_parse() */
    }

    if (HOJSON_STACK->flags & HOJSON_FLAG_IS_ARRAY) /* If an array closed */
        return HOJSON_ARRAY_END;
    else /* If an object closed */
        return HOJSON_OBJECT_END;
}

hojson_character_t hojson_decode_character(const char* str, size_t str_length, uint8_t encoding) {
    hojson_character_t c;
    c.raw = c.value = 0; /* These default values are not valid so parsing will cease if returned */
    c.bytes = 0;
    switch (encoding) {
    case HOJSON_ENCODING_UNKNOWN:
        c.bytes = 1;
        break;
    case HOJSON_ENCODING_UTF_8:
        /* The first byte of a UTF-8 character can can begin with one of four bit patterns, each indicating the */
        /* number of remaining bytes: 0XXXXXXX = 1 byte, 110XXXXX = 2 bytes, 1110XXXX = 3 bytes, 11110XXX = 4 bytes. */
        /* NOTE: UTF-8 is *big* endian. */
        if (((str[0] >> 7) & 0x01) == 0x00)
            c.bytes = 1;
        else if (((str[0] >> 5) & 0x07) == 0x06)
            c.bytes = 2;
        else if (((str[0] >> 4) & 0x0F) == 0x0E)
            c.bytes = 3;
        else if (((str[0] >> 3) & 0x1F) == 0x1E)
            c.bytes = 4;
        break;
    case HOJSON_ENCODING_UTF_16_BE:
        /* UTF-16 characters are either two bytes or four bytes where the four-byte characters are encoded such that */
        /* the first two bytes begin with 110110XX and the second with 110111XX. The rest are two-byte characters. */
        if (((str[0] >> 2) & 0x3F) == 0x36 && ((str[2] >> 2) & 0x3F) == 0x37)
            c.bytes = 4;
        else
            c.bytes = 2;
        break;
    case HOJSON_ENCODING_UTF_16_LE:
        /* UTF-16LE (Little Endian) is just like UTF-16BE (Big Endian) but the most and least significant bytes in */
        /* any 16-bit sequence are swapped. (Technically, a byte isn't defined as eight bits but it is in practice.) */
        if (((str[1] >> 2) & 0x3F) == 0x36 && ((str[3] >> 2) & 0x3F) == 0x37)
            c.bytes = 4;
        else
            c.bytes = 2;
        break;
    }

    /* If the string doesn't have enough bytes in it to decode this character */
    if (c.bytes > str_length) {
        /* Set the decoded value to the maximum possible to indicate a failure, zero the rest, and return early */
        c.value = UINT32_MAX;
        c.raw = 0;
        c.bytes = 0;
        return c;
    }

    switch (encoding) {
    case HOJSON_ENCODING_UNKNOWN:
        c.value = (uint32_t)str[0] & 0x000000FF; /* The mask ensures the remainder of the bits are zero */
        break;
    case HOJSON_ENCODING_UTF_8:
        if (c.bytes == 1) {
            /* One-byte UTF-8 characters are encoded as 0XXXXXXX where the Xs represent the bits of the character's */
            /* value. For all decoding, we want to grab only those bits and transform them into an integer. */
            /* The method here takes one byte from the string, uses a mask to zero out any bit that is not part of */
            /* resulting value, casts the masked byte to an unsigned 32-bit integer, shifts those bits to the left to */
            /* place them at the indexes they're expected in the value, and then bitwise ORs these components into a */
            /* single unsigned 32-bit integer. This one-byte case does not need any shift but the remaining cases do. */
            c.value = (uint32_t)(str[0] & 0x7F);
        } else if (c.bytes == 2) {
            /* Two-byte UTF-8 characters are encoded as 110XXXXX 10XXXXXX */
            c.value = ((uint32_t)(str[0] & 0x1F) << 6) | (uint32_t)(str[1] & 0x3F);
        } else if (c.bytes == 3) {
            /* Three-byte UTF-8 characters are encoded as 1110XXXX 10XXXXXX 10XXXXXX */
            c.value = ((uint32_t)(str[0] & 0x0F) << 12) | ((uint32_t)(str[1] & 0x3F) << 6) |
                       (uint32_t)(str[2] & 0x3F);
        } else if (c.bytes == 4) {
            /* Four-byte UTF-8 characters are encoded as 11110XXX 10XXXXXX 10XXXXXX 10XXXXXX */
            c.value = ((uint32_t)(str[0] & 0x07) << 18) | ((uint32_t)(str[1] & 0x3F) << 12) |
                      ((uint32_t)(str[2] & 0x3F) << 6)  |  (uint32_t)(str[3] & 0x3F);
        } break;
    case HOJSON_ENCODING_UTF_16_BE:
        if (c.bytes == 2) {
            /* Concatenate the two bytes together to retrieve the original value */
            c.value = ((uint32_t)str[0] << 8) | (uint32_t)str[1];
        } else if (c.bytes == 4) {
            /* Four-byte UTF-16 characters are encoded as 110110XX XXXXXXXX 110111XX XXXXXXXX after first subtracting */
            /* 0x00010000 from the value. Here, that subtracted value is reconstructed and 0x00010000 is added back. */
            c.value = (((uint32_t)(str[0] & 0x03) << 18) | ((uint32_t)str[1] << 16) |
                       ((uint32_t)(str[2] & 0x03) << 8)  |  (uint32_t)str[3]) + 0x00010000;
        }
        break;
    case HOJSON_ENCODING_UTF_16_LE:
        if (c.bytes == 2)
            c.value = ((uint32_t)str[1] << 8) | (uint32_t)str[0];
        else if (c.bytes == 4) {
            c.value = (((uint32_t)(str[1] & 0x03) << 18) | ((uint32_t)str[0] << 16) |
                       ((uint32_t)(str[3] & 0x03) << 8)  |  (uint32_t)str[2]) + 0x00010000;
        }
        break;
    }

    switch (c.bytes) {
        /* The method here takes the char pointer, casts it to an unsigned 32-bit integer pointer (pointing to four */
        /* bytes rather than one), dereferences it to get its integer value, applies a mask to zero any unwanted bits */
        /* (e.g. the 0x000000FF mask retains just the last eight bits), and assigns this value to 'raw' value. */
        case 1: c.raw = *(uint32_t*)str & 0x000000FF; break;
        case 2: c.raw = *(uint32_t*)str & 0x0000FFFF; break;
        case 3: c.raw = *(uint32_t*)str & 0x00FFFFFF; break;
        case 4: c.raw = *(uint32_t*)str; break;
    }

    return c;
}

hojson_character_t hojson_encode_character(uint32_t value, uint8_t encoding) {
    hojson_character_t c;
    c.value = value;
    c.raw = 0;

    switch (encoding) {
    case HOJSON_ENCODING_UNKNOWN: /* If the encoding is somehow not specified, assume UTF-8 */
    case HOJSON_ENCODING_UTF_8:
        if (value <= 0x0000007F) { /* If the value will fit into one byte */
            c.raw = value;
            c.bytes = 1;
        } else if (value >= 0x000080 && value <= 0x000007FF) { /* If the value will fit into two bytes */
            /* For a value with bits XXXXXAAA AABBBBBB we want to transform the bits to the form 110AAAAA 10BBBBBB. */
            /* The method here treats c.raw as an array of unsigned, eight-bit integers. This is done to assign bytes */
            /* individually for the sake of endianness where UTF-8 is big endian. The value is masked in order to */
            /* zero any bits that are not used in the byte being assigned, then shifted all the way to the right. */
            /* prefixed "0xC0" and "0x80" bitwise ORs prepend the UTF-8 markers 110 and 10, respectively. The */
            ((uint8_t*)&c.raw)[0] = 0xC0 | (uint8_t)((value & 0x0000007C0) >> 6); /* 110AAAAAA */
            ((uint8_t*)&c.raw)[1] = 0x80 | (uint8_t) (value & 0x0000000FF); /* 10BBBBBB */
            c.bytes = 2;
        } else if ((value >= 0x00000800 && value <= 0x0000D7FF) || (value >= 0x0000E000 && value <= 0x0000FFFF)) {
            /* For a value with bits AAAABBBB BBCCCCCC we want 1110AAAA 10BBBBBB 10CCCCCC */
            ((uint8_t*)&c.raw)[0] = 0xE0 | (uint8_t)((value & 0x0000F000) >> 12); /* 1110AAAA */
            ((uint8_t*)&c.raw)[1] = 0x80 | (uint8_t)((value & 0x00000FC0) >> 6); /* 10BBBBBB */
            ((uint8_t*)&c.raw)[2] = 0x80 | (uint8_t) (value & 0x0000003F); /* 10CCCCCC */
            c.bytes = 3;
        } else if (value >= 0x00010000 && value <= 0x0010FFFF) {
            /* For a value with bits XXXAAABB BBBBCCCC CCDDDDDD we want 11110AAA 10BBBBBB 10CCCCCC 10DDDDDD */
            ((uint8_t*)&c.raw)[0] = 0xF0 | (uint8_t)((value & 0x001C0000) >> 18) ; /* 11110AAA */
            ((uint8_t*)&c.raw)[1] = 0x80 | (uint8_t)((value & 0x0003F000) >> 12); /* 10BBBBBB */
            ((uint8_t*)&c.raw)[2] = 0x80 | (uint8_t)((value & 0x00000FC0) >> 6); /* 10CCCCCC */
            ((uint8_t*)&c.raw)[3] = 0x80 | (uint8_t) (value & 0x0000003F); /* 10DDDDDD */
            c.bytes = 4;
        } else /* If the reference's value is not valid */
            c.bytes = 0; /* Don't even try */
        break;
    case HOJSON_ENCODING_UTF_16_BE:
        if (value <= 0x0000D7FF || (value >= 0x0000E000 && value <= 0x0000FFFF)) { /* If the value fits in two bytes */
            ((uint8_t*)&c.raw)[0] = (uint8_t)((value & 0x0000FF00) >> 8);
            ((uint8_t*)&c.raw)[1] = (uint8_t) (value & 0x000000FF);
            c.bytes = 2;
        } else if (value >= 0x00010000 && value <= 0x0010FFFF) { /* If the value fits in four bytes */
            /* For a value - 0x00010000 with bits XXXXXXXX XXXXAABB BBBBBBCC DDDDDDDD we want to transform the bits */
            /* to the form 110110AA BBBBBBBB 110111CC DDDDDDDD. When decoded, as per UTF-16, 0x00010000 is added. */
            /* The prefixed "0xD8" and "0xDC" bitwise ORs prepend the UTF-16 markers 110110 and 110111, respectively. */
            value -= 0x00010000;
            ((uint8_t*)&c.raw)[0] = 0xD8 | (uint8_t)((value & 0x000C0000) >> 20); /* 110110AA */
            ((uint8_t*)&c.raw)[1] =        (uint8_t)((value & 0x0003FC00) >> 18); /* BBBBBBBB */
            ((uint8_t*)&c.raw)[2] = 0xDC | (uint8_t)((value & 0x00000300) >> 8); /* 110111CC */
            ((uint8_t*)&c.raw)[3] =        (uint8_t) (value & 0x000000FF); /* DDDDDDDD */
            c.bytes = 4;
        } else /* If the reference's value is not valid */
            c.bytes = 0; /* Don't even try */
        break;
    case HOJSON_ENCODING_UTF_16_LE:
        /* UTF-16LE (Little Endian) is just like UTF-16BE (Big Endian) with the reverse endianness meaning that the */
        /* operations here are identical to those above but the indexes have been changed to reflect endianness */
        if (value <= 0x0000D7FF || (value >= 0x0000E000 && value <= 0x0000FFFF)) {
            ((uint8_t*)&c.raw)[1] = (uint8_t)((value & 0x0000FF00) >> 8);
            ((uint8_t*)&c.raw)[0] = (uint8_t) (value & 0x000000FF);
            c.bytes = 2;
        } else if (value >= 0x00010000 && value <= 0x0010FFFF) {
            value -= 0x00010000;
            ((uint8_t*)&c.raw)[3] = 0xD8 | (uint8_t)((value & 0x000C0000) >> 20); /* 110110AA */
            ((uint8_t*)&c.raw)[2] =        (uint8_t)((value & 0x0003FC00) >> 18); /* BBBBBBBB */
            ((uint8_t*)&c.raw)[1] = 0xDC | (uint8_t)((value & 0x00000300) >> 8); /* 110111CC */
            ((uint8_t*)&c.raw)[0] =        (uint8_t) (value & 0x000000FF); /* DDDDDDDD */
            c.bytes = 4;
        } else
            c.bytes = 0;
        break;
    }

    return c;
}

uint32_t hojson_hex_character_to_decimal(uint32_t character) {
    switch (character) {
    case '0':
    default:  return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a':
    case 'A': return 10;
    case 'b':
    case 'B': return 11;
    case 'c':
    case 'C': return 12;
    case 'd':
    case 'D': return 13;
    case 'e':
    case 'E': return 14;
    case 'f':
    case 'F': return 15;
    }
}

#ifdef _MSC_VER
    #pragma warning(pop) /* Supress MSVC warning C6011 due to false positives */
#endif /* _MSC_VER */

#endif /* HOJSON_IMPLEMENTATION */

#endif /* HOJSON_H */
