#include <string.h>

#include "cmd_table.h"
#include "console_util.h"
#include "../string_util.h"
#include "../tree/tree_to_string.h"
#include "../arithmetics/arith_context.h"
#include "../arithmetics/arith_rules.h"
#include "../table/table.h"

#define COMMAND "table "
#define FOLD_KEYWORD " fold "
#define FOLD_VAR_1 "x"
#define FOLD_VAR_2 "y"

bool cmd_table_check(char *input)
{
    return begins_with(COMMAND, input);
}

void cmd_table_exec(char *input)
{
    input += strlen(COMMAND);
    char *args[6];
    size_t num_args = split(input, args, 5, ";", ";", ";", FOLD_KEYWORD, ";");

    if (num_args != 4 && num_args != 6)
    {
        printf("Table Error: Invalid syntax.\n");
        return;
    }

    Node *expr = NULL;
    Node *start = NULL;
    Node *end = NULL;
    Node *step = NULL;
    Node *fold_expr = NULL;
    Node *fold_init = NULL;

    if (!parse_input_from_console(args[0], "Error in expression: %s.\n", true, &expr)) return;

    char *variables[count_variables(expr)];
    size_t num_vars = list_variables(expr, variables);
    if (num_vars > 1)
    {
        printf("Expression contains more than one variable.\n");
        goto exit;
    }

    if (!parse_input_from_console(args[1], "Error in start: %s.\n", true, &start)
        || !parse_input_from_console(args[2], "Error in end: %s.\n", true, &end)
        || !parse_input_from_console(args[3], "Error in step: %s.\n", true, &step))
    {
        goto exit;
    }

    if (count_variables(start) > 0
        || count_variables(end) > 0
        || count_variables(step) > 0)
    {
        printf("Start, end and step must be constant.\n");
        goto exit;
    }

    double start_val = arith_eval(start);
    double end_val = arith_eval(end);
    double step_val = arith_eval(step);

    if (step_val == 0)
    {
        printf("Step must not be zero.\n");
        goto exit;
    }

    // Optionally: Parse part of command after "fold"
    double fold_val = 0;
    if (num_args == 6)
    {
        // Parse initial fold-value
        if (!parse_input_from_console(args[4], "Error in fold expression: %s.\n", true, &fold_expr)
            || !parse_input_from_console(args[5], "Error in initial fold value: %s.\n", true, &fold_init))
        {
            goto exit;
        }

        if (count_variables(fold_expr)
            - count_variable_nodes(fold_expr, FOLD_VAR_1)
            - count_variable_nodes(fold_expr, FOLD_VAR_2) != 0)
        {
            printf("Fold expression must not contain any variables except '"
                FOLD_VAR_1 "' and '" FOLD_VAR_2 "'.\n");
            goto exit;
        }

        if (count_variables(fold_init) > 0)
        {
            printf("Initial fold value must be constant.\n");
            goto exit;
        }

        fold_val = arith_eval(fold_init);
    }
    // - - - End of parsing of fold-construct

    // Adjust step direction to not have an endless loop
    if ((start_val < end_val && step_val < 0)
        || (start_val > end_val && step_val > 0))
    {
        step_val = -step_val;
    }

    Table table = get_empty_table();
    
    // Header
    if (g_interactive)
    {
        add_cell(&table, ALIGN_CENTER, " # ");
        vline(&table, BORDER_SINGLE);
        if (num_vars != 0)
        {
            add_cell_fmt(&table, ALIGN_CENTER, VAR_COLOR " %s " COL_RESET, variables[0]);
        }
        else
        {
            add_cell(&table, ALIGN_RIGHT, "");
        }
        vline(&table, BORDER_SINGLE);
        char inlined_expr[sizeof_tree_to_string(expr, true)];
        unsafe_tree_to_string(expr, inlined_expr, true);
        add_cell_fmt(&table, ALIGN_CENTER, " %s ", inlined_expr);
        next_row(&table);
        hline(&table, BORDER_SINGLE);
    }

    // Loop through all values and add them to table
    for (size_t i = 1; step_val > 0 ? start_val <= end_val : start_val >= end_val; i++)
    {
        Node *current_expr = tree_copy(expr);
        Node *current_val = malloc_constant_node(start_val);
        replace_variable_nodes(&current_expr, current_val, variables[0]);
        double result = arith_eval(current_expr);

        add_cell_fmt(&table, ALIGN_RIGHT, " %zu ", i);
        add_cell_fmt(&table, ALIGN_NUMBERS, " " CONSTANT_TYPE_FMT " ", start_val);
        add_cell_fmt(&table, ALIGN_NUMBERS, " " CONSTANT_TYPE_FMT " ", result);

        if (num_args == 6)
        {
            Node *current_fold = tree_copy(fold_expr);
            Node *current_fold_x = malloc_constant_node(fold_val);
            Node *current_fold_y = malloc_constant_node(result);
            replace_variable_nodes(&current_fold, current_fold_x, FOLD_VAR_1);
            replace_variable_nodes(&current_fold, current_fold_y, FOLD_VAR_2);
            fold_val = arith_eval(current_fold);
            free_tree(current_fold);
            free_tree(current_fold_x);
            free_tree(current_fold_y);
        }

        free_tree(current_expr);
        free_tree(current_val);
        next_row(&table);
        start_val += step_val;
    }

    if (num_args == 6)
    {
        hline(&table, BORDER_SINGLE);
        add_cell_span(&table, ALIGN_CENTER, 2, 1, " Fold result ");
        add_cell_fmt(&table, ALIGN_NUMBERS, " " CONSTANT_TYPE_FMT " ", fold_val);
        update_ans(fold_val);
    }

    if (g_interactive) make_boxed(&table, BORDER_SINGLE);
    print_table(&table);
    free_table(&table);

    exit:
    free_tree(expr);
    free_tree(start);
    free_tree(end);
    free_tree(step);
    free_tree(fold_expr);
    free_tree(fold_init);
}
