#pragma once
#include <stdbool.h>
#include "../tree/operator.h"
#include "../util/linked_list.h"
#include "../util/trie.h"

typedef struct
{
    Operator *glue_op;                // Points to a payload of a listnode of op_list
    LinkedList op_list;               // List of operators
    Trie op_tries[OP_NUM_PLACEMENTS]; // Tries for fast operator lookup (payload: pointer to listnode)
    Trie keywords_trie;               // Contains all names for keyword lookup in tokenizer (no payload)
} ParsingContext;

ParsingContext ctx_create();
void ctx_destroy(ParsingContext *ctx);
bool ctx_add_ops(ParsingContext *ctx, size_t count, ...);
Operator *ctx_add_op(ParsingContext *ctx, Operator op);
bool ctx_delete_op(ParsingContext *ctx, char *name, OpPlacement placement);
bool ctx_set_glue_op(ParsingContext *ctx, Operator *op);
Operator *ctx_lookup_op(ParsingContext *ctx, char *name, OpPlacement placement);
