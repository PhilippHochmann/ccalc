#pragma once
#include <stdbool.h>
#include "../parsing/context.h"
#include "../tree/node.h"
#include "../transformation/rewrite_rule.h"

#define NUM_PREDEFINED_OPS 56
#define g_ctx (&__g_ctx)
#define g_composite_functions (&__g_composite_functions)

extern ParsingContext __g_ctx;
extern LinkedList __g_composite_functions;

void init_arith_ctx();
void unload_arith_ctx();

void add_composite_function(RewriteRule rule);
bool remove_composite_function(Operator *function);
void clear_composite_functions();
RewriteRule *get_composite_function(Operator *op);
bool arith_parse_raw(char *input, char *error_fmt, Node **out_res);
bool arith_parse_and_postprocess(char *input, char *error_fmt, Node **out_res);
bool arith_postprocess(Node **tree);
