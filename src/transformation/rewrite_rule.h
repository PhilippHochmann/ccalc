#pragma once
#include "matching.h"
#include "../util/vector.h"
#include "../tree/node.h"

typedef Vector Ruleset;

typedef struct
{
    Node *before;
    Node *after;
    MappingFilter filter;
} RewriteRule;

RewriteRule get_rule(Node *before, Node *after, MappingFilter filter);
void free_rule(RewriteRule rule);
bool apply_rule(Node **tree, RewriteRule *rule);

Vector get_empty_ruleset();
void add_to_ruleset(Vector *rules, RewriteRule rule);
void free_ruleset(Ruleset *rules);
size_t apply_ruleset(Node **tree, Ruleset *rules);
size_t apply_rule_list(Node **tree, LinkedList *rules);
bool parse_ruleset_from_string(char *string, ParsingContext *ctx, MappingFilter default_filter, Ruleset *out_ruleset);
bool parse_ruleset_from_file(FILE *file, ParsingContext *ctx, MappingFilter default_filter, Ruleset *out_rulesets);
