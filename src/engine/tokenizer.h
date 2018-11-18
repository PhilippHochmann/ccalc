#include <stdbool.h>

#include "constants.h"
#include "context.h"

bool tokenize(ParsingContext *ctx, char *input, char *out_tokens[MAX_TOKENS], size_t *out_num_tokens);

bool is_space(char c);
bool is_letter(char c);
bool is_digit(char c);
bool is_opening_parenthesis(char c);
bool is_closing_parenthesis(char c);
bool is_delimiter(char c);
