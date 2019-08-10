#pragma once
#include <stdarg.h>
#include <stdbool.h>

#include "operator.h"

typedef bool (*TryParseHandler)(char *in, void *out);
typedef void (*ToStringHandler)(void *in, char *out);
typedef bool (*EqualsHandler)(void *a, void *b);

typedef struct
{
    size_t value_size; // e.g. sizeof(bool) for propositional logic, sizeof(double) for arithmetic
    size_t min_str_len; // Needed to let engine know how much data to allocate for stringed value
    size_t num_ops; // Current count of operators 
    size_t max_ops; // Maximum count of operators (limited by buffer size)
    TryParseHandler try_parse; // Used to detect a literal token
    ToStringHandler to_string; // Used to print trees
    EqualsHandler equals; // Relevant for unification of leaf nodes in rule_get_matching. Used in node_equals.
    Operator *glue_op; // Points to op in operators
    Operator *operators; // On heap!
} ParsingContext;

ParsingContext get_context(
    size_t value_size,
    size_t min_strbuf_length,
    size_t max_ops,
    TryParseHandler try_parse,
    ToStringHandler to_string,
    EqualsHandler handler);
void free_context(ParsingContext *ctx);
bool ctx_add_ops(ParsingContext *ctx, size_t count, ...);
int ctx_add_op(ParsingContext *ctx, Operator op);
bool ctx_set_glue_op(ParsingContext *ctx, Operator *op);
void ctx_remove_glue_op(ParsingContext *ctx);
Operator *ctx_lookup_op(ParsingContext *ctx, char *name, OpPlacement placement);
Operator *ctx_lookup_function(ParsingContext *ctx, char *name, size_t arity);
