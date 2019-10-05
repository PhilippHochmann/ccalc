#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "context.h"

/*
Summary: This method is used to create a new ParsingContext without glue-op and operators
    Use ctx_add_op and ctx_add_glue_op to add them to new context
Parameters:
    recommended_str_len: Minimum amount of chars (without \0) a buffer supplied to to_string should hold
        (not relevant for parsing, only for own account)
    max_ops:             Number of operators that should fit into reserved buffer
    operators:           Buffer to operators. Should hold max_ops operators.
*/
ParsingContext get_context(size_t recommended_str_len, size_t max_ops, Operator *op_buffer)
{
    ParsingContext res = (ParsingContext){
        .recommended_str_len = recommended_str_len,
        .max_ops = max_ops,
        .num_ops = 0,
        .operators = op_buffer,
        .glue_op = NULL,
    };

    return res;
}

/*
Summary: Adds given operators to context
Returns: true if all operators were successfully added, false if inconsistency occurred, buffer full or invalid arguments
*/
bool ctx_add_ops(ParsingContext *ctx, size_t count, ...)
{
    va_list args;
    va_start(args, count);
    
    for (size_t i = 0; i < count; i++)
    {
        if (ctx_add_op(ctx, va_arg(args, Operator)) == -1)
        {
            va_end(args);
            return false;
        }
    }
    
    va_end(args);
    return true;
}

/*
Summary: Adds operator to context
    To associate every operand with exactly one operator in a unique manner, infix operators with the same precedence must have the same associativity.
Returns: ID of new operator, -1 if one of the following:
    * buffer is full
    * ctx is NULL
    * infix operator with inconsistent associativity is given (infix operator with same precedence has different associativity)
    * function of same name and arity is present in context
*/
int ctx_add_op(ParsingContext *ctx, Operator op)
{
    if (ctx == NULL || ctx->num_ops == ctx->max_ops) return -1;
    
    // Check for name clash
    if (op.placement != OP_PLACE_FUNCTION)
    {
        if (ctx_lookup_op(ctx, op.name, op.placement) != NULL) return -1;
    }
    else
    {
        if (ctx_lookup_function(ctx, op.name, op.arity) != NULL) return -1;
    }

    // Consistency checks
    if (op.placement == OP_PLACE_INFIX)
    {
        for (size_t i = 0; i < ctx->num_ops; i++)
        {
            if (ctx->operators[i].placement == OP_PLACE_INFIX
                && ctx->operators[i].precedence == op.precedence)
            {                
                if (ctx->operators[i].assoc != op.assoc)
                {
                    return -1;
                }
            }
        }
    }

    ctx->operators[ctx->num_ops++] = op;
    return ctx->num_ops - 1;
}

/*
Summary: Sets glue-op, which is inserted between two subexpressions (such as 2a -> 2*a)
Returns: False, if null in arguments or operator with arity not equal to 2 given
*/
bool ctx_set_glue_op(ParsingContext *ctx, Operator *op)
{
    if (ctx == NULL || op == NULL || op->arity != 2) return false;
    ctx->glue_op = op;
    return true;
}

/*
Summary: Sets glue-op to NULL, two subexpressions next to each other will result in PERR_UNEXPECTED_SUBEXPRESSION
*/
void ctx_remove_glue_op(ParsingContext *ctx)
{
    if (ctx == NULL) return;
    ctx->glue_op = NULL;
}

// For function overloading: Returns function of given name. Favors zero-arity function when functions are overloaded.
Operator *lookup_tentative_function(ParsingContext *ctx, char *name)
{
    Operator *non_zero_func = NULL;

    for (size_t i = 0; i < ctx->num_ops; i++)
    {
        if (ctx->operators[i].placement == OP_PLACE_FUNCTION
            && strcmp(ctx->operators[i].name, name) == 0)
        {
            if (ctx->operators[i].arity == 0)
            {
                return &ctx->operators[i];
            }
            else
            {
                non_zero_func = &ctx->operators[i];
            }
        }
    }

    return non_zero_func;
}

/*
Summmary: Searches for operator of given name and placement
Returns: NULL if no operator has been found or invalid arguments given, otherwise pointer to operator in ctx->operators
*/
Operator *ctx_lookup_op(ParsingContext *ctx, char *name, OpPlacement placement)
{
    if (ctx == NULL || name == NULL) return NULL;
    if (placement == OP_PLACE_FUNCTION) return lookup_tentative_function(ctx, name);

    for (size_t i = 0; i < ctx->num_ops; i++)
    {
        if (ctx->operators[i].placement == placement
            && strcmp(ctx->operators[i].name, name) == 0)
        {
            return &ctx->operators[i];
        }
    }
    
    return NULL;
}

/*
Summmary: Searches for function of given name and arity
Returns: NULL if no function has been found or invalid arguments given, otherwise pointer to function in ctx->operators
*/
Operator *ctx_lookup_function(ParsingContext *ctx, char *name, size_t arity)
{
    if (ctx == NULL || name == NULL) return NULL;

    for (size_t i = 0; i < ctx->num_ops; i++)
    {
        if (ctx->operators[i].placement == OP_PLACE_FUNCTION
            && strcmp(ctx->operators[i].name, name) == 0
            && ctx->operators[i].arity == arity)
        {
            return &ctx->operators[i];
        }
    }
    
    return NULL;
}
