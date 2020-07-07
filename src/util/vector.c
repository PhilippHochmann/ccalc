#include <string.h>
#include "vector.h"

void vec_trim(Vector *vec)
{
    vec->buffer_size = vec->elem_count + 1;
    vec->buffer = realloc(vec->buffer, vec->elem_size * vec->buffer_size);
}

// Returns true if buffer needed to be extended.
bool vec_ensure_size(Vector *vec, size_t needed_size)
{
    bool res = false;
    while (needed_size > vec->buffer_size)
    {
        res = true;
        vec->buffer_size <<= 1;
    }
    vec->buffer = realloc(vec->buffer, vec->elem_size * vec->buffer_size);
    return res;
}

Vector vec_create(size_t elem_size, size_t start_size)
{
    if (start_size == 0)
    {
        start_size = 1;
    }

    return (Vector){
        .elem_size = elem_size,
        .elem_count = 0,
        .buffer_size = start_size,
        .buffer = malloc(elem_size * start_size)
    };
}

void vec_reset(Vector *vec)
{
    vec->elem_count = 0;
}

void vec_destroy(Vector *vec)
{
    free(vec->buffer);
}

bool vec_push(Vector *vec, void *elem)
{
    return vec_push_many(vec, 1, elem);
}

/*
Summary: To push a literal fast, for convenience use VEC_PUSH_LITERAL-Macro
*/
void *vec_push_empty(Vector *vec)
{
    vec_ensure_size(vec, vec->elem_count + 1);
    return vec_get(vec, vec->elem_count++);
}

bool vec_push_many(Vector *vec, size_t num, void *elem)
{
    bool res = vec_ensure_size(vec, vec->elem_count + num);
    memcpy((char*)vec->buffer + vec->elem_size * vec->elem_count, elem, num * vec->elem_size);
    vec->elem_count += num;
    return res;
}

// Very unsafe to store at caller site - buffer may realloc frequently
void *vec_get(Vector *vec, size_t index)
{
    return (char*)vec->buffer + vec->elem_size * index;
}

// Very unsafe to store at caller site - buffer may realloc frequently
void *vec_pop(Vector *vec)
{
    if (vec->elem_count == 0) return NULL;
    vec->elem_count--;
    return vec_get(vec, vec->elem_count);
}

// Very unsafe to store at caller site - buffer may realloc frequently
void *vec_peek(Vector *vec)
{
    if (vec->elem_count == 0) return NULL;
    return vec_get(vec, vec->elem_count - 1);
}

size_t vec_count(Vector *vec)
{
    return vec->elem_count;
}
