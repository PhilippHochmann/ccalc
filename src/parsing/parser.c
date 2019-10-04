#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "tokenizer.h"
#include "parser.h"

// Do not use this macro in auxiliary functions!
#define ERROR(type) { state.result = type; goto exit; }
// Maximum number of operator/node instances the parser can handle at once
#define MAX_STACK_SIZE 128

// Represents an operator (with metadata) while being parsed
struct OpData
{
    // Pointer to operator in context, NULL denotes opening parenthesis
    Operator *op;
    // Records number of operands for functions, or DYNAMIC_ARITY for non-functions, glue-op and opening parentheses
    size_t arity;
};

/* Encapsulates current state to be communicated to auxiliary functions
   (singleton) */
struct ParserState
{
    ParsingContext *ctx;
    size_t num_nodes;
    Node *node_stack[MAX_STACK_SIZE];
    size_t num_ops;
    struct OpData op_stack[MAX_STACK_SIZE];
    ParserError result;
};

bool is_opening_parenthesis(char *c)
{
    return strcmp(c, "(") == 0 || strcmp(c, "{") == 0 || strcmp(c, "[") == 0;
}

bool is_closing_parenthesis(char *c)
{
    return strcmp(c, ")") == 0 || strcmp(c, "}") == 0 || strcmp(c, "]") == 0;
}

bool is_delimiter(char *c)
{
    return strcmp(c, ",") == 0 || strcmp(c, ";") == 0;
}

bool try_parse_constant(char *in, ConstantType *out)
{
    char *end_ptr;
    *out = strtod(in, &end_ptr);
    return *end_ptr == '\0';
}

void *malloc_wrapper(struct ParserState *state, size_t size)
{
    void *res = malloc(size);
    if (res == NULL) state->result = PERR_OUT_OF_MEMORY;
    return res;
}

// Returns op_data on top of stack
struct OpData *op_peek(struct ParserState *state)
{
    return &state->op_stack[state->num_ops - 1];
}

bool node_push(struct ParserState *state, Node *value)
{
    // Check if there is still space on stack
    if (state->num_nodes == MAX_STACK_SIZE)
    {
        free(value);
        state->result = PERR_STACK_EXCEEDED;
        return false;
    }

    state->node_stack[state->num_nodes++] = value;
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
        // Functions with recorded arity of DYNAMIC_ARITY are glue-ops and shouldn't be computed as functions
        bool is_function = (op_peek(state)->arity != DYNAMIC_ARITY);
    
        // Function overloading: Find function with suitable arity
        // Actual function on op stack is only tentative with random arity (but same name)
        if (is_function)
        {
            if (op->arity != op_peek(state)->arity)
            {
                char *name = op->name;
                op = ctx_lookup_function(state->ctx, name, op_peek(state)->arity);
                
                // Fallback: Find function of dynamic arity
                if (op == NULL)
                {
                    op = ctx_lookup_function(state->ctx, name, DYNAMIC_ARITY);
                }
                
                if (op == NULL)
                {
                    state->result = PERR_FUNCTION_WRONG_ARITY;
                    return false;
                }
            }
        }

        // We try to allocate a new node and pop its children from node stack
        Node *op_node = malloc_wrapper(state, sizeof(Node));
        if (op_node == NULL) return false;
        *op_node = get_operator_node(op, is_function ? op_peek(state)->arity : op->arity);

        // Check if malloc of children buffer in get_operator_node failed
        if (op_node->children == NULL)
        {
            free(op_node);
            return false;
        }
        
        for (size_t i = 0; i < op_node->num_children; i++)
        {
            // Pop nodes from stack and append them in subtree
            if (!node_pop(state, &op_node->children[op_node->num_children - i - 1]))
            {
                // Free already appended children and new node on error
                for (size_t j = op_node->num_children - 1; j > op_node->num_children - i - 1; j--)
                {
                    free_tree(op_node->children[j]);
                }

                free(op_node->children);
                free(op_node);
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

    // Postfix operators are never on the op_stack, because their operands are directly available
    if (op != NULL && op->placement == OP_PLACE_POSTFIX)
    {
        if (!op_pop_and_insert(state)) return false;
    }

    return true;
}

// Pushes actual operator on op_stack. op must not be NULL!
bool push_operator(struct ParserState *state, Operator *op)
{
    // Set arity of functions to 0, everything else's arity to DYNAMIC_ARITY, since DYNAMIC_ARITY is only Arity that will never occur
    return op_push(state, (struct OpData){ op, op->placement == OP_PLACE_FUNCTION ? 0 : DYNAMIC_ARITY });
}

// Pushes opening parenthesis on op_stack
bool push_opening_parenthesis(struct ParserState *state)
{
    return op_push(state, (struct OpData){ NULL, DYNAMIC_ARITY });
}

ParserError parse_tokens(ParsingContext *ctx, size_t num_tokens, char **tokens, Node **out_res)
{
    // 1. Early outs
    if (ctx == NULL || tokens == NULL || out_res == NULL) return PERR_ARGS_MALFORMED;

    // 2. Initialize state
    struct ParserState state = {
        .ctx = ctx,
        .result = PERR_SUCCESS,
        .num_ops = 0,
        .num_nodes = 0,
    };

    // 3. Process each token
    bool await_subexpression = true;
    for (size_t i = 0; i < num_tokens; i++)
    {
        char *token = tokens[i];
        
        // I. Does glue-op need to be inserted?
        if (!await_subexpression && state.ctx->glue_op != NULL)
        {
            if (!is_closing_parenthesis(token)
                && !is_delimiter(token)
                && ctx_lookup_op(state.ctx, token, OP_PLACE_INFIX) == NULL
                && ctx_lookup_op(state.ctx, token, OP_PLACE_POSTFIX) == NULL)
            {
                if (!push_operator(&state, state.ctx->glue_op)) goto exit;

                // If glue-op was function, we can't count operands like we normally do
                // Disable function overloading mechanism
                op_peek(&state)->arity = DYNAMIC_ARITY;
                await_subexpression = true;
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
            await_subexpression = false;

            while (state.num_ops > 0 && op_peek(&state)->op != NULL)
            {
                if (!op_pop_and_insert(&state))
                {
                    ERROR(PERR_EXCESS_CLOSING_PARENTHESIS);
                }
            }
            
            if (state.num_ops > 0)
            {
                op_pop_and_insert(&state);
            }
            else
            {
                ERROR(PERR_EXCESS_CLOSING_PARENTHESIS);
            }
            
            bool empty_params = (i > 0 && is_opening_parenthesis(tokens[i - 1]));
            if (state.num_ops > 0 && op_peek(&state)->arity != DYNAMIC_ARITY && !empty_params)
            {
                if (op_peek(&state)->arity == MAX_ARITY)
                {
                    ERROR(PERR_CHILDREN_EXCEEDED);
                }
                else
                {
                    op_peek(&state)->arity++;
                }
            }
            
            continue;
        }
        
        if (is_delimiter(token))
        {
            while (state.num_ops > 0 && op_peek(&state)->op != NULL)
            {
                if (!op_pop_and_insert(&state))
                {
                    ERROR(PERR_UNEXPECTED_DELIMITER);
                }
            }
            
            // Increase operand counter
            if (state.num_ops > 1 && state.op_stack[state.num_ops - 2].arity != DYNAMIC_ARITY)
            {
                if (state.op_stack[state.num_ops - 2].arity == MAX_ARITY)
                {
                    ERROR(PERR_CHILDREN_EXCEEDED);
                }
                else
                {
                    state.op_stack[state.num_ops - 2].arity++;
                }
            }
            
            await_subexpression = true;
            continue;
        }
        // - - -
        
        // IV. Is token operator?
        Operator *op = NULL;
        
        // Infix, Prefix (await=true) -> Function (false), Leaf (false), Prefix (true)
        // Function, Leaf, Postfix (await=false) -> Infix (true), Postfix (false)
        if (await_subexpression)
        {
            op = ctx_lookup_op(state.ctx, token, OP_PLACE_FUNCTION);
            if (op != NULL) // Function operator found
            {
                if (!push_operator(&state, op)) goto exit;

                // Handle unary functions without parenthesis (e.g. "sin2")
                // If function is last token its arity will be set to 0
                if (i < num_tokens - 1 && !is_opening_parenthesis(tokens[i + 1]))
                {
                    op_peek(&state)->arity = 1;
                }
                await_subexpression = true;
                continue;
            }
            
            op = ctx_lookup_op(state.ctx, token, OP_PLACE_PREFIX);
            if (op != NULL) // Prefix operator found
            {
                if (!push_operator(&state, op)) goto exit;
                // Constants are modeled as prefix operators with arity of 0 and don't await a subexpression
                await_subexpression = (op->arity != 0);
                continue;
            }
        }
        else
        {
            op = ctx_lookup_op(state.ctx, token, OP_PLACE_INFIX);
            if (op != NULL) // Infix operator found
            {
                if (!push_operator(&state, op)) goto exit;
                await_subexpression = true;
                continue;
            }
            
            op = ctx_lookup_op(state.ctx, token, OP_PLACE_POSTFIX);
            if (op != NULL) // Postfix operator found
            {
                if (!push_operator(&state, op)) goto exit;
                await_subexpression = false;
                continue;
            }
            
            // We can fail here: no more tokens processable (no glue-op)
            ERROR(PERR_UNEXPECTED_SUBEXPRESSION);
        }
        
        // V. Token must be variable or constant (leaf)
        Node *node = malloc_wrapper(&state, sizeof(Node));

        // Out of memory - free partial results
        if (state.result == PERR_OUT_OF_MEMORY)
        {
            free(node);
            goto exit;
        }

        // Is token constant?
        ConstantType const_val;
        if (try_parse_constant(token, &const_val))
        {
            *node = get_constant_node(const_val);
        }
        else // Token must be variable
        {
            char *name = malloc_wrapper(&state, (strlen(token) + 1) * sizeof(char));
            if (state.result == PERR_OUT_OF_MEMORY) goto exit;
            strcpy(name, token);
            *node = get_variable_node(name);
        }

        await_subexpression = false;
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
        case 0:
            state.result = PERR_EMPTY; // We haven't constructed a single node
            break;
        case 1:
            *out_res = state.node_stack[0]; // We successfully constructed a single AST
            break;
        default:
            state.result = PERR_MISSING_OPERATOR; // We have multiple ASTs (need glue-op)
    }
    
    exit:
    
    // If parsing wasn't successful, free partial results
    if (state.result != PERR_SUCCESS)
    {
        while (state.num_nodes > 0)
        {
            free_tree(state.node_stack[state.num_nodes - 1]);
            state.num_nodes--;
        }
    }

    return state.result;
}

/* Parsing algorithm ends here. What follows are functions to invoke the parser conveniently. */

static const size_t MAX_TOKENS = 100;

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
Returns: AST or NULL when error occurred
*/
Node *parse_conveniently(ParsingContext *ctx, char *input)
{
    Node *result = NULL;
    parse_input(ctx, input, &result);
    return result;
}