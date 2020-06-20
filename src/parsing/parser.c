#include <stdbool.h>
#include <string.h>

#include "tokenizer.h"
#include "parser.h"
#include "../string_util.h"

// Do not use this macro in auxiliary functions!
#define ERROR(type) { state.result = type; goto exit; }
// Maximum number of operator/node instances the parser can handle at once
#define MAX_STACK_SIZE 150

// Represents an operator (with metadata) while being parsed
struct OpData
{
    Operator *op;        // Pointer to operator in context, NULL denotes opening parenthesis
    bool count_operands; // Indicates whether to count operands
    size_t arity;        // Records number of operands to pop
};

// Encapsulates current state of shunting-yard algo. to be communicated to auxiliary functions
struct ParserState
{
    ParsingContext *ctx;                    // Contains operators and glue-op
    size_t num_nodes;                       // Size of node stack
    Node *node_stack[MAX_STACK_SIZE];       // Constructed nodes
    size_t num_ops;                         // Size of operator stack
    struct OpData op_stack[MAX_STACK_SIZE]; // Parsed operators
    ParserError result;                     // Success when no error occurred
};

// Attempts to parse a substring to a double
bool try_parse_constant(char *in, ConstantType *out)
{
    char *end;
    *out = strtod(in, &end);
    return *end == '\0';
}

// Returns op_data on top of stack
struct OpData *op_peek(struct ParserState *state)
{
    return &state->op_stack[state->num_ops - 1];
}

bool node_push(struct ParserState *state, Node *node)
{
    // Check if there is still space on stack
    if (state->num_nodes == MAX_STACK_SIZE)
    {
        free(node);
        state->result = PERR_STACK_EXCEEDED;
        return false;
    }

    state->node_stack[state->num_nodes++] = node;
    return true;
}

bool node_pop(struct ParserState *state, Node **out)
{
    if (state->num_nodes == 0)
    {
        state->result = PERR_MISSING_OPERAND;
        return false;
    }
    
    *out = state->node_stack[--state->num_nodes];
    return true;
}

bool op_pop_and_insert(struct ParserState *state)
{
    if (state->num_ops == 0)
    {
        state->result = PERR_MISSING_OPERATOR;
        return false;
    }
    
    Operator *op = op_peek(state)->op;
    if (op != NULL) // Construct operator-node and append children
    {
        // Function overloading: Find function with suitable arity
        // Actual function on op stack is only tentative with random arity (but same name)
        // Only do this for functions we count operands for
        if (op_peek(state)->count_operands)
        {
            if (op->arity != op_peek(state)->arity)
            {
                char *name = op->name; // Save name in case of making op NULL
                op = ctx_lookup_function(state->ctx, name, op_peek(state)->arity);
                
                // Fallback: Find function of dynamic arity
                if (op == NULL)
                {
                    op = ctx_lookup_function(state->ctx, name, OP_DYNAMIC_ARITY);
                }
                
                if (op == NULL)
                {
                    state->result = PERR_FUNCTION_WRONG_ARITY;
                    return false;
                }
            }
        }

        // We try to allocate a new node and pop its children from node stack
        Node *op_node = malloc_operator_node(op, op_peek(state)->arity);

        // Check if malloc of children buffer in get_operator_node failed
        // (Could also be NULL on malformed arguments, but this should not happen)
        if (op_node == NULL)
        {
            state->result = PERR_OUT_OF_MEMORY;
            return false;
        }
        
        for (size_t i = 0; i < get_num_children(op_node); i++)
        {
            // Pop nodes from stack and append them in subtree
            if (!node_pop(state, get_child_addr(op_node, get_num_children(op_node) - i - 1)))
            {
                // Free already appended children and new node on error
                free_tree(op_node);
                return false;
            }
        }
        
        if (!node_push(state, op_node))
        {
            free_tree(op_node);
            return false;
        }
    }
    
    state->num_ops--;
    return true;
}

bool op_push(struct ParserState *state, struct OpData op_d)
{
    Operator *op = op_d.op;
    
    if (op != NULL)
    {
        // Shunting-yard algorithm: Pop until operator of higher precedence or '(' is on top of stack 
        if (op->placement == OP_PLACE_INFIX || op->placement == OP_PLACE_POSTFIX)
        {
            while (state->num_ops > 0
                && op_peek(state)->op != NULL
                && (op->precedence < op_peek(state)->op->precedence
                    || (op->precedence == op_peek(state)->op->precedence && op->assoc == OP_ASSOC_LEFT)))
            {
                if (!op_pop_and_insert(state)) return false;
            }
        }
    }
    
    // Check if there is still space on stack
    if (state->num_ops == MAX_STACK_SIZE)
    {
        state->result = PERR_STACK_EXCEEDED;
        return false;
    }

    state->op_stack[state->num_ops++] = op_d;

    return true;
}

// Pushes actual operator on op_stack. op must not be NULL!
bool push_operator(struct ParserState *state, Operator *op)
{
    // Set arity of functions to 0 and enable operand counting
    struct OpData opData;

    if (op->placement == OP_PLACE_FUNCTION)
    {
        opData = (struct OpData){ op, true, 0 };
    }
    else
    {
        opData = (struct OpData){ op, false, op->arity };
    }

    return op_push(state, opData);
}

// Pushes opening parenthesis on op_stack
bool push_opening_parenthesis(struct ParserState *state)
{
    return op_push(state, (struct OpData){ NULL, false, OP_DYNAMIC_ARITY });
}

// out_res can be NULL if you only want to check if an error occurred
ParserError parse_tokens(ParsingContext *ctx, int num_tokens, char **tokens, Node **out_res)
{
    // 1. Early outs
    if (ctx == NULL || tokens == NULL) return PERR_ARGS_MALFORMED;

    // 2. Initialize state
    struct ParserState state = {
        .ctx = ctx,
        .result = PERR_SUCCESS,
        .num_ops = 0,
        .num_nodes = 0,
    };

    // 3. Process each token
    bool await_infix = false; // Or postfix, or delimiter, or closing parenthesis
    for (int i = 0; i < num_tokens; i++)
    {
        char *token = tokens[i];
        
        // I. Does glue-op need to be inserted?
        if (await_infix && state.ctx->glue_op != NULL)
        {
            if (!is_closing_parenthesis(token)
                && !is_delimiter(token)
                && ctx_lookup_op(state.ctx, token, OP_PLACE_INFIX) == NULL
                && ctx_lookup_op(state.ctx, token, OP_PLACE_POSTFIX) == NULL)
            {
                if (!push_operator(&state, state.ctx->glue_op)) goto exit;

                // If glue-op was function, disable function overloading mechanism
                // Arity of 2 needed for DYNAMIC_ARITY functions set as glue-op
                op_peek(&state)->count_operands = false;
                op_peek(&state)->arity = 2;
                await_infix = false;
            }
        }
        
        // II. Is token opening parenthesis?
        if (is_opening_parenthesis(token))
        {
            if (!push_opening_parenthesis(&state)) goto exit;
            continue;
        }

        // III. Is token closing parenthesis or argument delimiter?
        if (is_closing_parenthesis(token))
        {
            await_infix = true;

            // Pop ops until opening parenthesis on op-stack
            while (state.num_ops > 0 && op_peek(&state)->op != NULL)
            {
                if (!op_pop_and_insert(&state))
                {
                    ERROR(PERR_EXCESS_CLOSING_PARENTHESIS);
                }
            }
            
            if (state.num_ops > 0)
            {
                // Remove opening parenthesis on top of op-stack
                op_pop_and_insert(&state);
            }
            else
            {
                // We did not stop because an opening parenthesis was found, but because op-stack was empty
                ERROR(PERR_EXCESS_CLOSING_PARENTHESIS);
            }
            
            /*
             * When count_operands is true for the operator on top of the stack,
             * the closing parenthesis was actually the end of its parameter list.
             * Increment operand count one last time if it was not the empty parameter list.
             */
            if (op_peek(&state)->count_operands)
            {
                if (state.num_ops > 0 && i > 0 && !is_opening_parenthesis(tokens[i - 1]))
                {
                    if (op_peek(&state)->arity == OP_MAX_ARITY)
                    {
                        ERROR(PERR_CHILDREN_EXCEEDED);
                    }
                    else
                    {
                        op_peek(&state)->arity++;
                    }
                }
            }
            
            continue;
        }
        
        if (is_delimiter(token))
        {
            // Pop ops until opening parenthesis on op-stack
            while (state.num_ops > 0 && op_peek(&state)->op != NULL)
            {
                if (!op_pop_and_insert(&state))
                {
                    ERROR(PERR_UNEXPECTED_DELIMITER);
                }
            }

            // Increment arity counter for function whose parameter list this delimiter is in
            if (state.num_ops > 1 && state.op_stack[state.num_ops - 2].op->placement == OP_PLACE_FUNCTION)
            {
                if (state.op_stack[state.num_ops - 2].count_operands)
                {
                    if (state.op_stack[state.num_ops - 2].arity == OP_MAX_ARITY)
                    {
                        ERROR(PERR_CHILDREN_EXCEEDED);
                    }
                    else
                    {
                        state.op_stack[state.num_ops - 2].arity++;
                    }
                }
            }
            else
            {
                // Not in "func("-construct
                ERROR(PERR_UNEXPECTED_DELIMITER);
            }
              
            await_infix = false;
            continue;
        }
        // - - -
        
        // IV. Is token operator?
        Operator *op = NULL;
        
        // Infix, Prefix, Delimiter (await=false) -> Function (true), Leaf (true), Prefix (false)
        // Function, Leaf, Postfix (await=true) -> Infix (false), Postfix (true), Delimiter (false)
        if (!await_infix)
        {
            op = ctx_lookup_op(state.ctx, token, OP_PLACE_FUNCTION);
            if (op != NULL) // Function operator found
            {
                if (!push_operator(&state, op)) goto exit;

                if (op->arity == 0 || op->arity == OP_DYNAMIC_ARITY)
                {
                    /*
                     * Directly pop constants that are not overloaded or not followed by an opening parenthesis
                     * OP_DYNAMIC_ARITY functions are overloaded, treat them as a constant when no opening 
                     * parenthesis follows.
                     */
                    if (!ctx_is_function_overloaded(state.ctx, op->name)
                        || i == num_tokens - 1
                        || !is_opening_parenthesis(tokens[i + 1]))
                    {
                        // Skip parsing of empty parameter list
                        if (i < num_tokens - 2 && is_opening_parenthesis(tokens[i + 1])
                            && is_closing_parenthesis(tokens[i + 2]))
                        {
                            i += 2;
                        }

                        op_pop_and_insert(&state);
                        await_infix = true;
                        continue;
                    }
                }
                
                // Handle omitted parenthesis after unary function (e.g. sin2)
                if (i < num_tokens - 1 && !is_opening_parenthesis(tokens[i + 1]))
                {
                    op_peek(&state)->arity = 1;
                }

                await_infix = false;
                continue;
            }
            
            op = ctx_lookup_op(state.ctx, token, OP_PLACE_PREFIX);
            if (op != NULL) // Prefix operator found
            {
                if (!push_operator(&state, op)) goto exit;
                await_infix = false;
                continue;
            }
        }
        else
        {
            op = ctx_lookup_op(state.ctx, token, OP_PLACE_INFIX);
            if (op != NULL) // Infix operator found
            {
                if (!push_operator(&state, op)) goto exit;
                await_infix = false;
                continue;
            }
            
            op = ctx_lookup_op(state.ctx, token, OP_PLACE_POSTFIX);
            if (op != NULL) // Postfix operator found
            {
                if (!push_operator(&state, op)) goto exit;
                // Postfix operators are never on the op_stack, because their operands are directly available
                op_pop_and_insert(&state);
                await_infix = true;
                continue;
            }
            
            // We can fail here: no more tokens processable (no glue-op)
            ERROR(PERR_UNEXPECTED_SUBEXPRESSION);
        }
        
        // V. Token must be variable or constant (leaf)
        Node *node;

        // Is token constant?
        ConstantType const_val;
        if (try_parse_constant(token, &const_val))
        {
            node = malloc_constant_node(const_val);
            if (node == NULL) ERROR(PERR_OUT_OF_MEMORY);
        }
        else // Token must be variable
        {
            node = malloc_variable_node(token);
            if (node == NULL) ERROR(PERR_OUT_OF_MEMORY);
        }

        await_infix = true;
        if (!node_push(&state, node)) goto exit;
    }
    
    // 4. Pop all remaining operators
    while (state.num_ops > 0)
    {
        if (op_peek(&state)->op == NULL)
        {
            ERROR(PERR_EXCESS_OPENING_PARENTHESIS);
        }
        else
        {
            if (!op_pop_and_insert(&state)) goto exit;
        }
    }
    
    // 5. Build result and return value
    switch (state.num_nodes)
    {
        // We haven't constructed a single node
        case 0:
            state.result = PERR_EMPTY;
            break;
        // We successfully constructed a single AST
        case 1:
            if (out_res != NULL) *out_res = state.node_stack[0];
            break;
        // We have multiple ASTs (need glue-op)
        default:
            state.result = PERR_MISSING_OPERATOR;
    }
    
    exit:
    // If parsing wasn't successful or result is discarded, free partial results
    if (state.result != PERR_SUCCESS || out_res == NULL)
    {
        while (state.num_nodes > 0)
        {
            free_tree(state.node_stack[state.num_nodes - 1]);
            state.num_nodes--;
        }
    }

    return state.result;
}

/* Parsing algorithm ends here. The following functions can be used to invoke parsing conveniently. */

static const size_t MAX_TOKENS = 200;

/*
Summary: Parses string, tokenized with default tokenizer, to abstract syntax tree
Returns: Result code to indicate whether string was parsed successfully or which error occurred
*/
ParserError parse_input(ParsingContext *ctx, char *input, Node **out_res)
{
    size_t num_tokens;
    char *tokens[MAX_TOKENS];

    // Parsing
    if (!tokenize(ctx, input, MAX_TOKENS, &num_tokens, tokens)) return PERR_MAX_TOKENS_EXCEEDED;
    ParserError result = parse_tokens(ctx, num_tokens, tokens, out_res);
    // Cleanup
    for (size_t i = 0; i < num_tokens; i++) free(tokens[i]);
    
    return result;
}

/*
Summary: Calls parse_input, omits ParserError
Returns: Operator tree or NULL when error occurred
*/
Node *parse_conveniently(ParsingContext *ctx, char *input)
{
    Node *result = NULL;
    parse_input(ctx, input, &result);
    return result;
}