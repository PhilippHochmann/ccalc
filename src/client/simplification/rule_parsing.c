#include <string.h>

#include "../../engine/util/alloc_wrappers.h"
#include "../../engine/util/console_util.h"
#include "../../engine/tree/node.h"
#include "../../engine/transformation/matching.h"
#include "../../engine/transformation/rewrite_rule.h"
#include "../../engine/parsing/parser.h"
#include "rule_parsing.h"

#define ARROW  "->"
#define WHERE  " WHERE "
#define AND    " AND "

bool parse_rule(char *string, ParsingContext *main_ctx, ParsingContext *extended_ctx, RewriteRule *out_rule)
{
    if (string[0] == '\0')
    {
        return true;
    }

    char *arrow_pos = strstr(string, ARROW);
    char *next_constr = strstr(string, WHERE); // Optional

    if (arrow_pos == NULL)
    {
        report_error("No arrow found.\n");
        return false;
    }

    // 1. Step: Overwrite "->" and "WHERE" with 0-terminators
    arrow_pos[0] = '\0';
    arrow_pos += strlen(ARROW);

    size_t num_constrs = 0;
    Node *constrs[MATCHING_MAX_CONSTRAINTS];

    // 2. Parse contraints of form ".... WHERE x=y ; a=b"
    while (next_constr != NULL)
    {
        *next_constr = '\0';
        next_constr += strlen(WHERE);

        char *next_and = strstr(next_constr, AND);
        if (next_and != NULL)
        {
            *next_and = '\0';
            next_and += strlen(AND);
        }

        constrs[num_constrs] = parse_conveniently(extended_ctx, next_constr);
        num_constrs++;
        next_constr = next_and;
    }

    Node *left_n = parse_conveniently(main_ctx, string); // Gives error message
    if (left_n == NULL)
    {
        return false;
    }

    Node *right_n = parse_conveniently(main_ctx, arrow_pos);
    if (right_n == NULL)
    {
        free_tree(left_n);
        return false;
    }

    *out_rule = get_rule(get_pattern(left_n, num_constrs, constrs), right_n);
    return true;
}

bool parse_ruleset_from_string(const char *string, ParsingContext *main_ctx, ParsingContext *extended_ctx, Vector *out_ruleset)
{
    // String is likely to be readonly - copy it (made explicit in signature)
    char *copy = malloc_wrapper(strlen(string) + 1);
    strcpy(copy, string);

    size_t line_no = 0;
    char *line = copy;
    while (line != NULL)
    {
        line_no++;
        char *next_line = strstr(line, "\n");
        if (next_line != NULL)
        {
            next_line[0] = '\0';
        }

        if (!parse_rule(line, main_ctx, extended_ctx, vec_push_empty(out_ruleset)))
        {
            report_error("Failed parsing ruleset in line %zu.\n", line_no);
            return false;
        }

        line = next_line;
        if (line != NULL) line++; // Skip newline char
    }

    vec_trim(out_ruleset);
    free(copy);
    return true;
}
