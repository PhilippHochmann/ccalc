#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <float.h>
#include <math.h>

#include "arith_context.h"

#define EVAL(n) arith_eval(get_child(tree, n))

static const size_t ARITH_STRING_LENGTH = 30; // Including \0

ParsingContext __g_ctx;
Operator operators[ARITH_MAX_OPS];

double binomial(double n, double k)
{
    if (k == 0) return 1;
    if ((2 * k) > n) k = n - k;
    
    double res = 1;
    for (double i = 1; i <= k; i++)
    {
        res = (res * (n - k + i)) / i;
    }
    
    return res;
}

double fibonacci(double n)
{
    if (n == 0) return 0;

    double a = 0;
    double b = 1;

    while (n-- > 1)
    {
        double temp = a + b;
        a = b;
        b = temp;
    }

    return b;
}

/*
Returns: Random natural number between min and max - 1 (i.e. max is exclusive)
*/
double random_between(double min, double max)
{
    long diff = (long)(max - min);
    if (diff < 1) return -1;
    return rand() % diff + min;
}

/*
Summary: Evaluates operator tree
Returns: Result after recursive application of all operators
*/
double arith_eval(Node *tree)
{
    switch (get_type(tree))
    {
        case NTYPE_CONSTANT:
            return get_const_value(tree);
            
        case NTYPE_OPERATOR:
        {
            size_t num_children = get_num_children(tree);
            switch ((size_t)(get_op(tree) - operators))
            {
                case 0: // $x
                    return EVAL(0);
                case 1: // x+y
                    return EVAL(0) + EVAL(1);
                case 2: // x-y
                    return EVAL(0) - EVAL(1);
                case 3: // x*y
                    return EVAL(0) * EVAL(1);
                case 4: // x/y
                    return EVAL(0) / EVAL(1);
                case 5: // x^y
                    return pow(EVAL(0), EVAL(1));
                case 6: // x C y
                    return (double)binomial(
                        labs((long)trunc(EVAL(0))),
                        labs((long)trunc(EVAL(1))));
                case 7: // x mod y
                    return fmod(EVAL(0), EVAL(1));
                case 8: // -x
                    return -EVAL(0);
                case 9: // +x
                    return EVAL(0);
                case 10: // x!
                {
                    double res = 1;
                    for (double i = trunc(EVAL(0)); i > 1; i--)
                    {
                        res *= i;
                    }
                    return res;
                }
                case 11: // x%
                    return EVAL(0) / 100;
                case 12: // exp(x)
                    return exp(EVAL(0));
                case 13: // root(x, n)
                    return pow(EVAL(0), 1 / EVAL(1));
                case 14: // sqrt(x)
                    return sqrt(EVAL(0));
                case 15: // log(x, n)
                    return log(EVAL(0)) / log(EVAL(1));
                case 16: // ln(x)
                    return log(EVAL(0));
                case 17: // ld(x)
                    return log2(EVAL(0));
                case 18: // log(x)
                    return log10(EVAL(0));
                case 19: // sin(x)
                    return sin(EVAL(0));
                case 20: // cos(x)
                    return cos(EVAL(0));
                case 21: // tan(x)
                    return tan(EVAL(0));
                case 22: // asin(x)
                    return asin(EVAL(0));
                case 23: // acos(x)
                    return acos(EVAL(0));
                case 24: // atan(x)
                    return atan(EVAL(0));
                case 25: // sinh(x)
                    return sinh(EVAL(0));
                case 26: // cosh(x)
                    return cosh(EVAL(0));
                case 27: // tanh(x)
                    return tanh(EVAL(0));
                case 28: // asinh(x)
                    return asinh(EVAL(0));
                case 29: // acosh(x)
                    return acosh(EVAL(0));
                case 30: // atanh(x)
                    return atanh(EVAL(0));
                case 31: // max(x, y, ...)
                {
                    double res = -INFINITY;
                    for (size_t i = 0; i < num_children; i++)
                    {
                        double child_val = EVAL(i);
                        if (child_val > res) res = child_val;
                    }
                    return res;
                }
                case 32: // min(x, y, ...)
                {
                    double res = INFINITY;
                    for (size_t i = 0; i < num_children; i++)
                    {
                        double child_val = EVAL(i);
                        if (child_val < res) res = child_val;
                    }
                    return res;
                }
                case 33: // abs(x)
                    return fabs(EVAL(0));
                case 34: // ceil(x)
                    return ceil(EVAL(0));
                case 35: // floor(x)
                    return floor(EVAL(0));
                case 36: // round(x)
                    return round(EVAL(0));
                case 37: // trunc(x)
                    return trunc(EVAL(0));
                case 38: // frac(x)
                    return EVAL(0) - floor(EVAL(0));
                case 39: // sum(x, y, ...)
                {
                    double res = 0;
                    for (size_t i = 0; i < num_children; i++) res += EVAL(i);
                    return res;
                }
                case 40: // prod(x, y, ...)
                {
                    double res = 1;
                    for (size_t i = 0; i < num_children; i++) res *= EVAL(i);
                    return res;
                }
                case 41: // avg(x, y, ...)
                {
                    if (num_children == 0) return 0;
                    double res = 0;
                    for (size_t i = 0; i < num_children; i++) res += EVAL(i);
                    return res / num_children;
                }
                case 42: // rand(x, y)
                    return random_between(EVAL(0), EVAL(1));
                case 43: // gamma(x)
                    return tgamma(EVAL(0));
                case 44: // fib(x)
                    return fibonacci(trunc(EVAL(0)));
                case 45: // pi
                    return 3.14159265359;
                case 46: // e
                    return 2.71828182846;
                case 47: // phi
                    return 1.61803398874;
                case 48: // clight (m/s)
                    return 299792458;
                case 49: // csound (m/s)
                    return 343.2;

                default:
                    printf("Encountered operator without evaluation rule\n");
                    return -1;
            }
        }

        case NTYPE_VARIABLE:
            printf("Encountered variable\n");
            return -1;
            
        default:
            return 0;
    }
}

/*
Summary: Used to delete user-defined function operators
*/
void arith_reset_ctx()
{
    // As operator names are stored on heap, we need to free them
    for (size_t i = ARITH_NUM_OPS; i < __g_ctx.num_ops; i++)
    {
        free(__g_ctx.operators[i].name);
    }
    __g_ctx.num_ops = ARITH_NUM_OPS;
}

void arith_unload_ctx()
{
    // Free custom operator's names
    for (size_t i = ARITH_NUM_OPS; i < g_ctx->num_ops; i++)
    {
        free(g_ctx->operators[i].name);
    }
}

/*
Summary: Sets arithmetic context stored in global variable
*/
void arith_init_ctx()
{
    __g_ctx = get_context(ARITH_STRING_LENGTH, ARITH_MAX_OPS, operators);
    
    ctx_add_ops(g_ctx, ARITH_NUM_OPS,
        op_get_prefix("$", 0),
        op_get_infix("+", 2, OP_ASSOC_LEFT),
        op_get_infix("-", 2, OP_ASSOC_LEFT),
        op_get_infix("*", 3, OP_ASSOC_LEFT),
        op_get_infix("/", 3, OP_ASSOC_LEFT),
        op_get_infix("^", 4, OP_ASSOC_RIGHT),
        op_get_infix("C", 1, OP_ASSOC_LEFT),
        op_get_infix("mod", 1, OP_ASSOC_LEFT),
        op_get_prefix("-", 6),
        op_get_prefix("+", 6),
        op_get_postfix("!", 5),
        op_get_postfix("%", 5),
        op_get_function("exp", 1),
        op_get_function("root", 2),
        op_get_function("sqrt", 1),
        op_get_function("log", 2),
        op_get_function("ln", 1),
        op_get_function("ld", 1),
        op_get_function("lg", 1),
        op_get_function("sin", 1),
        op_get_function("cos", 1),
        op_get_function("tan", 1),
        op_get_function("asin", 1),
        op_get_function("acos", 1),
        op_get_function("atan", 1),
        op_get_function("sinh", 1),
        op_get_function("cosh", 1),
        op_get_function("tanh", 1),
        op_get_function("asinh", 1),
        op_get_function("acosh", 1),
        op_get_function("atanh", 1),
        op_get_function("max", OP_DYNAMIC_ARITY),
        op_get_function("min", OP_DYNAMIC_ARITY),
        op_get_function("abs", 1),
        op_get_function("ceil", 1),
        op_get_function("floor", 1),
        op_get_function("round", 1),
        op_get_function("trunc", 1),
        op_get_function("frac", 1),
        op_get_function("sum", OP_DYNAMIC_ARITY),
        op_get_function("prod", OP_DYNAMIC_ARITY),
        op_get_function("avg", OP_DYNAMIC_ARITY),
        op_get_function("rand", 2),
        op_get_function("gamma", 1),
        op_get_function("fib", 1),
        op_get_constant("pi"),
        op_get_constant("e"),
        op_get_constant("phi"),
        op_get_constant("clight"),
        op_get_constant("csound"));
    
    // Set multiplication as glue-op
    ctx_set_glue_op(g_ctx, &__g_ctx.operators[3]);

    srand(time(NULL));
}
