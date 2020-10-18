/*
 * Copiright (C) 2020 Santiago León O.
 */

#define _GNU_SOURCE // Used to enable strcasestr()
#define _XOPEN_SOURCE 700 // Required for strptime()
#include "common.h"
#include "binary_tree.c"
#include "scanner.c"

#include "linear_solver.c"

// This implements the simple approach of just counting expressions and symbol
// assignments, unfortunaltely this only works to detect some kinds of errors.
// Examples of how this can fail are breaks_simple_computation() and
// linear_dependency().
void simple_computation_print (struct linear_system_t *system)
{
    int num_expressions = 0;
    struct expression_t *curr_expr = system->expressions;
    while (curr_expr != NULL) {
        num_expressions++;
        curr_expr = curr_expr->next;
    }

    int num_symbols = system->id_to_symbol_definition.num_nodes;
    int num_assigned_symbols = 0;
    BINARY_TREE_FOR (id_to_symbol_definition, &system->id_to_symbol_definition, symbol_node) {
        struct symbol_definition_t *curr_symbol = symbol_node->value;

        if (curr_symbol->state == SYMBOL_ASSIGNED) {
            num_assigned_symbols++;
        }
    }

    if (num_symbols - num_assigned_symbols == num_expressions) {
        printf ("%i (symbols) - %i (assigned) = %i (equations) -> solvable", num_symbols, num_assigned_symbols, num_expressions);
    } else if (num_symbols - num_assigned_symbols > num_expressions) {
        printf ("%i (symbols) - %i (assigned) > %i (equations) -> underconstrained", num_symbols, num_assigned_symbols, num_expressions);
    } else {
        printf ("%i (symbols) - %i (assigned) < %i (equations) -> overconstrained", num_symbols, num_assigned_symbols, num_expressions);
    }
    printf ("\n\n");
}

void solver_compute_solvability (struct linear_system_t *system)
{
}

void solver_solve_and_print (struct linear_system_t *system)
{
    simple_computation_print (system);

    solver_compute_solvability (system);

    string_t err = {};
    solver_solve (system, &err);
    if (!system->success) {
        printf ("%s\n", str_data(&err));
    }
    solver_print_solution (system);
    str_free (&err);
}

// This shows why the simple computation isn't enough. A part of the system is
// underconstrained and a disjoint one is overconstrained. In the end the number
// of expressions and assignments adds up to make it look like it is solvable,
// but it isn't.
void breaks_simple_computation ()
{
    struct linear_system_t _system = {};
    struct linear_system_t *system = &_system;

    // Overconstrained component
    solver_expr_equals_zero (system, "x1 + w1 - x2");
    solver_expr_equals_zero (system, "x1 + w2 - x2");
    solver_symbol_assign (system, "x2", 100);
    solver_symbol_assign (system, "w1", 10);
    solver_symbol_assign (system, "w2", 20);

    // Underconstrained component
    solver_expr_equals_zero (system, "x3 + w3 - x4");
    solver_symbol_assign (system, "x4", 200);

    solver_solve_and_print (system);
}

// It's also possible to have linearly dependent equations. In a sense, this
// means the user added the same equation twice, but not necessarily using the
// exact same syntax. As long as the system is solvable after removal of
// duplicate equations (linearly dependant), things should solve correctly. The
// simple solvability computation also fails here, as it thinks there are more
// expressions than necessary, marking the system as overconstrained.
void linear_dependency ()
{
    struct linear_system_t _system = {};
    struct linear_system_t *system = &_system;

    solver_expr_equals_zero (system, "x1 + w1 - x2");
    solver_expr_equals_zero (system, "-x1 - w1 + x2");
    solver_symbol_assign (system, "x2", 100);
    solver_symbol_assign (system, "w1", 10);

    solver_solve_and_print (system);
}

void underconstrained_minimal ()
{
    struct linear_system_t _system = {};
    struct linear_system_t *system = &_system;

    solver_expr_equals_zero (system, "x1 + w1 - x2");
    solver_symbol_assign (system, "x2", 100);

    solver_solve_and_print (system);
}

void underconstrained ()
{
    struct linear_system_t _system = {};
    struct linear_system_t *system = &_system;

    solver_expr_equals_zero (system, "x1 + w1 - x2");
    solver_expr_equals_zero (system, "x2 + w2 - x3");
    solver_expr_equals_zero (system, "x3 + w3 - x4");
    solver_expr_equals_zero (system, "x4 + w4 - x5");
    solver_expr_equals_zero (system, "x5 + w5 - x6");
    solver_symbol_assign (system, "w1", 10);
    solver_symbol_assign (system, "w2", 20);
    solver_symbol_assign (system, "w3", 30);
    solver_symbol_assign (system, "w4", 40);
    solver_symbol_assign (system, "w5", 50);

    solver_solve_and_print (system);
}

void underconstrained_partial ()
{
    struct linear_system_t _system = {};
    struct linear_system_t *system = &_system;

    // Underconstrained equations
    solver_expr_equals_zero (system, "x1 + w1 - x2");
    solver_expr_equals_zero (system, "x2 + w2 - x8");
    solver_expr_equals_zero (system, "x8 + w3 - x4");
    solver_expr_equals_zero (system, "x4 + w4 - x5");
    solver_expr_equals_zero (system, "x5 + w5 - x6");
    solver_symbol_assign (system, "w1", 10);
    solver_symbol_assign (system, "w2", 20);
    solver_symbol_assign (system, "w3", 30);
    solver_symbol_assign (system, "w4", 40);
    solver_symbol_assign (system, "w5", 50);

    // Solvable equations
    solver_expr_equals_zero (system, "x7 + w6 - x3");
    solver_expr_equals_zero (system, "x3 + w7 - x9");
    solver_symbol_assign (system, "x7", 200);
    solver_symbol_assign (system, "w6", 10);
    solver_symbol_assign (system, "w7", 20);


    solver_solve_and_print (system);
}

void overconstrained ()
{
    struct linear_system_t _system = {};
    struct linear_system_t *system = &_system;

    solver_expr_equals_zero (system, "x1 + w1 - x2");
    solver_expr_equals_zero (system, "x1 + w2 - x2");
    solver_symbol_assign (system, "x2", 100);
    solver_symbol_assign (system, "w1", 10);
    solver_symbol_assign (system, "w2", 20);

    solver_solve_and_print (system);
}

int main(int argc, char **argv)
{
    underconstrained_partial ();
}
