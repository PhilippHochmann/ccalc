#include <stdio.h>
#include <string.h>

#include "core.h"
#include "util.h"
#include "evaluation.h"
#include "help.h"
#include "show_rules.h"
#include "load.h"
#include "assignments.h"
#include "arith_context.h"
#include "../engine/string_util.h"

#define INTERACTIVE_ASK_PREFIX "> "
#define COMMENT_PREFIX "'"
#define NUM_COMMANDS 8

struct Command
{
    void (*initHandler)();
    bool (*checkHandler)(char *input);
    void (*execHandler)(ParsingContext *ctx, char *input);
};

static ParsingContext *ctx;
static const struct Command commands[NUM_COMMANDS] = {
    { quit_init, quit_check, quit_exec },
    { debug_init, debug_check, debug_exec},
    { help_init, help_check, help_exec},
    { definition_init, definition_check, definition_exec},
    { rule_init, rule_check, rule_exec},
    { show_rules_init, show_rules_check, show_rules_exec},
    { load_init, load_check, load_exec},
    { evaluation_init, evaluation_check, evaluation_exec}
};

/*
Summary: Sets parsing context and initializes commands
*/
void init_commands()
{
    init_util();
    g_interactive = false;
    ctx = arith_get_ctx();

    for (size_t i = 0; i < NUM_COMMANDS; i++)
    {
        commands[i].initHandler();
    }
}

/*
Summary: Activates interactive mode, whispered messages will be displayed and readline instead of getline will be used to read input
*/
bool set_interactive(bool value)
{
    bool res = g_interactive;
    g_interactive = value;
    return res;
}

/*
Summary: Endless loop to ask user or file for command until ask_input() returns false, ignores comments
*/
void process_input(FILE *file)
{
    char *input = NULL;
    while (ask_input(INTERACTIVE_ASK_PREFIX, file, &input))
    {
        if (!begins_with(COMMENT_PREFIX, input)) parse_command(input);
        free(input);
    }
}

void parse_command(char *input)
{
    for (size_t i = 0; i < NUM_COMMANDS; i++)
    {
        if (commands[i].checkHandler(input))
        {
            commands[i].execHandler(ctx, input);
            return;
        }
    }
}
