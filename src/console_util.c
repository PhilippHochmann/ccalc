#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifdef USE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "string_util.h"
#include "arithmetics/arith_rules.h"
#include "console_util.h"

#define COL_RESET "\033[0m"
#define TTY_ASK_VARIABLE_PROMPT "? > "

static const size_t MAX_INPUT_LENGTH   = 100;
static const size_t MAX_INLINED_LENGTH = 200;

void unload_console_util()
{
#ifdef USE_READLINE
    rl_clear_history();
#endif
}

void init_console_util()
{
    g_interactive = false;
#ifdef USE_READLINE
    // Disable tab completion
    rl_bind_key('\t', rl_insert);
#endif
}

/*
Summary: Sets interactive mode
    When true, whispered messages are displayed and readline instead of getline is used to read input
*/
bool set_interactive(bool value)
{
    bool res = g_interactive;
    g_interactive = value;
    return res;
}

// Summary: Prints result of tree inline, including correct color even after truncation, and indicated abbreviation
void print_tree_inlined(ParsingContext *ctx, Node node, bool color)
{
    char buffer[MAX_INLINED_LENGTH + 1];
    size_t result = tree_inline(ctx, node, buffer, MAX_INLINED_LENGTH + 1, color);
    printf("%s", buffer);
    if (color) printf(COL_RESET);
    if (result > MAX_INLINED_LENGTH) printf("...");
}

/*
Summary: printf-wrapper to filter unimportant prints in non-interactive mode
*/
void whisper(const char *format, ...)
{
    if (g_interactive)
    {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

#ifdef USE_READLINE
static const size_t MAX_PROMPT_LENGTH = 10;
// File is stdin, g_interactive is true
bool ask_input_readline(char **out_input, char *prompt_fmt, va_list args)
{
    if (strcmp(prompt_fmt, "%s") == 0)
    {
        // Save stack space in the most common case
        *out_input = readline(va_arg(args, char*));
    }
    else
    {
        // Printing prompt beforehand causes overwrite when using arrow keys
        char prompt[MAX_PROMPT_LENGTH + 1];
        vsnprintf(prompt, MAX_PROMPT_LENGTH, prompt_fmt, args);
        *out_input = readline(prompt);
    }

    if (*out_input == NULL)
    {
        return false;
    }
    add_history(*out_input);
    return true;
}
#endif

bool ask_input_getline(FILE *file, char **out_input, char *prompt_fmt, va_list args)
{
    if (g_interactive)
    {
        vprintf(prompt_fmt, args);
    }
    
    // Would be no problem to put input in stack, but we want to have the same interface as readline, which puts input on heap
    size_t size = MAX_INPUT_LENGTH;
    *out_input = malloc(MAX_INPUT_LENGTH);
    if (getline(out_input, &size, file) == -1)
    {
        free(*out_input);
        return false;
    }

    // Overwrite newline char
    (*out_input)[strlen(*out_input) - 1] = '\0';
    return true;
}

/*
Summary: Used whenever input is requested. Prompt is only printed when interactive.
Params
    prompt: Prompt to display when interactive
    file: Used when not interactive - should be stdin when arguments are piped in or file when load command is used
    out_input: Pointer to string that will be read. String must be free'd after use.
*/
bool ask_input(FILE *file, char **out_input, char *prompt_fmt, ...)
{
    va_list args;
    va_start(args, prompt_fmt);
    bool res;

#ifdef USE_READLINE
    // Use readline if it is installed and input comes from a shell
    if (g_interactive)
    {
        res = ask_input_readline(out_input, prompt_fmt, args);
    }
    else
    {
        res = ask_input_getline(file, out_input, prompt_fmt, args);
    }
#else
    res = ask_input_getline(file, out_input, prompt_fmt, args);
#endif

    va_end(args);
    return res;
}

/*
Returns: String representation of ParserError
*/
char *perr_to_string(ParserError perr)
{
    switch (perr)
    {
        case PERR_SUCCESS:
            return "Success";
        case PERR_MAX_TOKENS_EXCEEDED:
            return "Max. Tokens exceeded";
        case PERR_STACK_EXCEEDED:
            return "Stack exceeded";
        case PERR_UNEXPECTED_SUBEXPRESSION:
            return "Unexpected Subexpression";
        case PERR_EXCESS_OPENING_PARENTHESIS:
            return "Missing closing parenthesis";
        case PERR_EXCESS_CLOSING_PARENTHESIS:
            return "Unexpected closing parenthesis";
        case PERR_UNEXPECTED_DELIMITER:
            return "Unexpected delimiter";
        case PERR_MISSING_OPERATOR:
            return "Unexpected operand";
        case PERR_MISSING_OPERAND:
            return "Missing operand";
        case PERR_OUT_OF_MEMORY:
            return "Out of memory";
        case PERR_FUNCTION_WRONG_ARITY:
            return "Wrong number of operands of function";
        case PERR_CHILDREN_EXCEEDED:
            return "Exceeded maximum number of operands of function";
        case PERR_EMPTY:
            return "Empty Expression";
        default:
            return "Unknown Error";
    }
}

/*
Summary:
    Parses input, does post-processing of input, gives feedback on command line
Returns:
    True when input was successfully parsed, false when syntax error in input or aborted when asked for constant
*/
bool parse_input_from_console(ParsingContext *ctx,
    char *input,
    char *error_fmt,
    Node *out_res,
    bool constant,
    bool transform)
{
    ParserError perr = parse_input(ctx, input, out_res);
    if (perr != PERR_SUCCESS)
    {
        printf(error_fmt, perr_to_string(perr));
        return false;
    }

    if (transform) transform_input(out_res);

    // Make expression constant by asking for values and binding them to variables
    if (constant)
    {
        char *vars[MAX_VAR_COUNT];
        size_t num_vars = list_variables(*out_res, vars);

        /*
         * Ask for variables interactively when we are connected to a terminal
         * When connected to a pipe, it binds variables silently
         * When expression was loaded from a file at a terminal, it asks interactively
         */
        bool temp = set_interactive(isatty(STDIN_FILENO));

        for (size_t i = 0; i < num_vars; i++)
        {
            char *input;
            if (ask_input(stdin, &input, "%s" TTY_ASK_VARIABLE_PROMPT, vars[i]))
            {
                Node res_var;
                if (!parse_input_from_console(ctx, input, error_fmt, &res_var, false, transform))
                {
                    // Error while parsing - ask again
                    free(input);
                    i--;
                    continue;
                }
                free(input);
                
                if (count_variables(res_var) > 0)
                {
                    // Not a constant given - ask again
                    printf("Not a constant expression\n");
                    free_tree(res_var);
                    i--;
                    continue;
                }
                
                replace_variable_nodes(out_res, res_var, vars[i]);
                free_tree(res_var);
            }
            else
            {
                // EOF when asked for constant
                printf("\n");
                set_interactive(temp);
                free_tree(*out_res);
                return false;
            }
        }
        // Restore previous value of g_interactive
        set_interactive(temp);
    }
    
    return true;
}
