/*
 * Copiright (C) 2020 Santiago León O.
 */

enum symbol_state_t {
    SYMBOL_UNASSIGNED,
    SYMBOL_ASSIGNED,
    SYMBOL_SOLVED
};

struct symbol_definition_t {
    uint64_t id;
    string_t name; // rename is not allowed!

    enum symbol_state_t state;
    double value;
};

BINARY_TREE_NEW(id_to_symbol_definition, uint64_t, struct symbol_definition_t*, a <= b ? (a == b ? 0 : -1) : 1)
BINARY_TREE_NEW(name_to_symbol_definition, char*, struct symbol_definition_t*, strcmp(a,b))

struct symbol_t {
    bool is_negative;
    struct symbol_definition_t *definition;

    struct symbol_t *next;
};

struct expression_t {
    struct symbol_t *symbols;

    struct expression_t *next;
};

struct linear_system_t {
    mem_pool_t pool;
    struct id_to_symbol_definition_tree_t id_to_symbol_definition;
    struct name_to_symbol_definition_tree_t name_to_symbol_definition;

    uint64_t last_id;
    struct expression_t *expressions;

    bool success;
};

void solver_destroy (struct linear_system_t *system)
{
    id_to_symbol_definition_tree_destroy (&system->id_to_symbol_definition);
    name_to_symbol_definition_tree_destroy (&system->name_to_symbol_definition);
    mem_pool_destroy (&system->pool);
}

#define SOLVER_TOKEN_TABLE                     \
    SOLVER_TOKEN_ROW(SOLVER_TOKEN_IDENTIFIER)  \
    SOLVER_TOKEN_ROW(SOLVER_TOKEN_OPERATOR)    \

#define SOLVER_TOKEN_ROW(identifier) identifier,
enum solver_token_type_t {
    SOLVER_TOKEN_TABLE
};
#undef SOLVER_TOKEN_ROW

#define SOLVER_TOKEN_ROW(identifier) #identifier,
char *solver_token_names[] = {
    SOLVER_TOKEN_TABLE
};
#undef SOLVER_TOKEN_ROW

struct solver_parser_state_t {
    mem_pool_t pool;
    struct scanner_t scnr;

    enum solver_token_type_t type;
    string_t str;
};

void solver_parser_state_destroy (struct solver_parser_state_t *state)
{
    mem_pool_destroy (&state->pool);
}

void solver_parser_state_init (struct solver_parser_state_t *state, char *expr)
{
    str_pool (&state->pool, &state->str);
    state->scnr.pos = expr;
}

struct symbol_t* system_new_symbol (struct linear_system_t *system, bool is_negative, char *name)
{
    struct symbol_definition_t *symbol_definition =
        name_to_symbol_definition_get (&system->name_to_symbol_definition, name);
    if (symbol_definition == NULL) {
        symbol_definition = mem_pool_push_struct (&system->pool, struct symbol_definition_t);
        *symbol_definition = ZERO_INIT (struct symbol_definition_t);

        symbol_definition->id = system->last_id;
        system->last_id++;
        str_set (&symbol_definition->name, name);

        id_to_symbol_definition_tree_insert (&system->id_to_symbol_definition,
                                             symbol_definition->id, symbol_definition);
        name_to_symbol_definition_tree_insert (&system->name_to_symbol_definition,
                                               str_data(&symbol_definition->name), symbol_definition);
    }

    struct symbol_t *new_symbol =
        mem_pool_push_struct (&system->pool, struct symbol_t);
    new_symbol->definition = symbol_definition;
    new_symbol->is_negative = is_negative;

    return new_symbol;
}

// Shorthand error for when the only replacement is the value of a token.
#define solver_read_error_tok(state,format) solver_read_error(state,format,str_data(&(state)->str))
GCC_PRINTF_FORMAT(2, 3)
void solver_read_error (struct solver_parser_state_t *state, const char *format, ...)
{
    PRINTF_INIT (format, size, args);
    char *str = mem_pool_push_size (&state->pool, size);
    PRINTF_SET (str, size, format, args);

    // TODO: How would error messages look like?
    scanner_set_error (&state->scnr, str);
}

void solver_tokenizer_next (struct solver_parser_state_t *state)
{
    struct scanner_t *scnr = &state->scnr;

    scnr->eof_is_error = true;

    char *identifier_chars = "._-abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    // Reset the token
    str_set (&state->str, "");

    scanner_consume_spaces (scnr);

    if (scanner_char_any (scnr, "+-")) {
        state->type = SOLVER_TOKEN_OPERATOR;
        strn_set (&state->str, scnr->pos-1, 1);

    } else if (scanner_char_peek (scnr, identifier_chars)) {
        state->type = SOLVER_TOKEN_IDENTIFIER;
        
        while (scanner_char_peek (scnr, identifier_chars)) {
            strn_cat_c (&state->str, scnr->pos, 1);
            scanner_advance_char (scnr);
        }
    } else {
        solver_read_error (state, "Unexpected character '%c'.", *(scnr->pos));
    }

    scnr->eof_is_error = false;

    scanner_consume_spaces (scnr);
}

bool solver_token_match (struct solver_parser_state_t *state, enum solver_token_type_t type, char *value)
{
    bool match = false;
    if (state->type == type) {
        if (value == NULL) {
            match = true;

        } else if (type == SOLVER_TOKEN_IDENTIFIER || type == SOLVER_TOKEN_OPERATOR) {
            if (strcmp (str_data(&state->str), value) == 0) {
                match = true;
            }

        } else {
            // Uncomparable token type. Match is only a string we currently
            // don't support matching agaínst other token value types. We wpuld
            // have to make 'value' argument be a void* or have separate match
            // functions.
            invalid_code_path;
        }
    }

    return match;
}

void solver_tokenizer_expect (struct solver_parser_state_t *state,
                              enum solver_token_type_t type,
                              char *value)
{
    solver_tokenizer_next (state);
    if (!solver_token_match (state, type, value)) {
        if (state->type != type) {
            if (value == NULL) {
                solver_read_error (state, "Expected token of type %s, got '%s' of type %s.",
                                   solver_token_names[type], str_data(&state->str), solver_token_names[state->type]);
            } else {
                solver_read_error (state, "Expected token '%s' of type %s, got '%s' of type %s.",
                                   value, solver_token_names[type], str_data(&state->str), solver_token_names[state->type]);
            }

        } else {
            // Types are equal, the only way we could've gotten a failed match
            // is if the value didn't match.
            assert (value != NULL);
            solver_read_error (state, "Expected '%s', got '%s'.", value, str_data(&state->str));
        }
    }
}

void solver_expression_push_symbol (struct linear_system_t *system,
                                    struct expression_t *expression,
                                    bool is_negative, char *identifier)
{
    // TODO: Have a symbol identifier to ID map
    struct symbol_t *new_symbol = system_new_symbol (system, is_negative, identifier);
    LINKED_LIST_PUSH (expression->symbols, new_symbol);
}

void solver_expr_equals_zero (struct linear_system_t *system, char *expr)
{
    struct solver_parser_state_t _state = {0};
    struct solver_parser_state_t *state = &_state;
    solver_parser_state_init (state, expr);

    bool is_negative = false;

    struct expression_t *new_expression = mem_pool_push_struct (&system->pool, struct expression_t);
    *new_expression = ZERO_INIT (struct expression_t);
    LINKED_LIST_PUSH (system->expressions, new_expression)

    solver_tokenizer_next (state);
    if (solver_token_match (state, SOLVER_TOKEN_IDENTIFIER, NULL)) {
        solver_expression_push_symbol (system, new_expression, false, str_data(&state->str));

    } else if (solver_token_match (state, SOLVER_TOKEN_OPERATOR, NULL)) {
        if (strcmp (str_data(&state->str), "-") == 0) {
            is_negative = true;
        }
        solver_tokenizer_expect (state, SOLVER_TOKEN_IDENTIFIER, NULL);
        solver_expression_push_symbol (system, new_expression, is_negative, str_data(&state->str));
    }

    while (!state->scnr.error && !state->scnr.is_eof) {
        solver_tokenizer_expect (state, SOLVER_TOKEN_OPERATOR, NULL);
        if (strcmp (str_data(&state->str), "-") == 0) {
            is_negative = true;
        } else {
            is_negative = false;
        }

        solver_tokenizer_expect (state, SOLVER_TOKEN_IDENTIFIER, NULL);

        solver_expression_push_symbol (system, new_expression, is_negative, str_data(&state->str));
    }

    solver_parser_state_destroy (state);
}

void solver_symbol_assign (struct linear_system_t *system, char *identifier, double value)
{
    struct symbol_definition_t *symbol_definition =
        name_to_symbol_definition_get (&system->name_to_symbol_definition, identifier);
    symbol_definition->state = SYMBOL_ASSIGNED;
    symbol_definition->value = value;
}

void str_cat_matrix (string_t *str, double *matrix, size_t m, size_t n)
{
    for (int i=0; i<m; i++) {
        for (int j=0; j<n; j++) {
            if (j == n-1) {
                str_cat_c (str, "| ");
            }

            str_cat_printf (str, "%.2f ", matrix[n*i+j]);
        }
        str_cat_c (str, "\n");
    }
    str_cat_c (str, "\n");
}

void print_matrix (double *matrix, size_t m, size_t n)
{
    string_t str = {0};
    str_cat_matrix (&str, matrix, m, n);
    printf ("%s", str_data(&str));
    str_free (&str);
}

uint32_t system_num_symbols (struct linear_system_t *system)
{
    return system->id_to_symbol_definition.num_nodes;
}

uint32_t system_num_equations (struct linear_system_t *system)
{
    uint32_t num_equations = 0;
    struct expression_t *curr_expression = system->expressions;
    while (curr_expression != NULL) {
        num_equations++;
        curr_expression = curr_expression->next;
    }

    return num_equations;
}

double system_get_symbol_value (struct linear_system_t *system, char *name)
{
    struct symbol_definition_t *symbol_definition =
        name_to_symbol_definition_get (&system->name_to_symbol_definition, name);
    return symbol_definition->value;
}

// This implementation of the solver works only if the system is solvable. Some
// unsolvable systems may cause memory corruption and useless results. In
// theory, this function can never fail if used correctly, we don't care about
// error messages or even notifying something bad happened. If there is the
// slightest chance of something going wrong DON'T USE THIS, use solver_solve().
void solver_solve_unsafe (struct linear_system_t *system, string_t *error)
{
    uint64_t symbol_id_to_column[system->last_id];
    uint64_t column_to_symbol_id[system->last_id];
    int num_unassigned_symbols = 0;
    BINARY_TREE_FOR (name_to_symbol_definition, &system->name_to_symbol_definition, curr_node) {
        struct symbol_definition_t *symbol_definition = curr_node->value;
        if (symbol_definition->state == SYMBOL_UNASSIGNED) {
            symbol_id_to_column[symbol_definition->id] = num_unassigned_symbols;
            column_to_symbol_id[num_unassigned_symbols] = symbol_definition->id;
            num_unassigned_symbols++;
        }
    }

    uint32_t num_equations = system_num_equations (system);

    if (num_unassigned_symbols > 0) {
        // Create the matrix
        size_t m = num_equations;
        size_t n = num_unassigned_symbols+1;

        mem_pool_marker_t mrkr = mem_pool_begin_temporary_memory (&system->pool);
        double *augmented_matrix = mem_pool_push_array(&system->pool, m*n, double);
        memset (augmented_matrix, 0, m*n*sizeof(double));

        // Populate the matrix with the data from the parsed expressions
        int expression_idx = 0;
        struct expression_t *curr_expression = system->expressions;
        while (curr_expression != NULL) {
            double constant = 0;

            struct symbol_t *curr_symbol = curr_expression->symbols;
            while (curr_symbol != NULL) {
                if (curr_symbol->definition->state == SYMBOL_ASSIGNED) {
                    if (curr_symbol->is_negative) {
                        constant -= curr_symbol->definition->value;
                    } else {
                        constant += curr_symbol->definition->value;
                    }

                } else {
                    if (curr_symbol->is_negative) {
                        augmented_matrix[n*expression_idx + symbol_id_to_column[curr_symbol->definition->id]] = -1;
                    } else {
                        augmented_matrix[n*expression_idx + symbol_id_to_column[curr_symbol->definition->id]] = 1;
                    }
                }

                curr_symbol = curr_symbol->next;
            }

            augmented_matrix[n*expression_idx+num_unassigned_symbols] = -constant;

            expression_idx++;
            curr_expression = curr_expression->next;
        }

        // Compute row echelon form of the matrix
        {
            // Pivot indices
            size_t h = 0;
            size_t k = 0;

            //str_cat_matrix (error, augmented_matrix, m, n);

            while (h<(m-1) && k<(n-1)) {
                // Find the row with the leading coefficient of maximum absolute
                // value.
                size_t m_i = 0; // Index where the maximum value was found
                double maximum = 0;
                {
                    for (int i=h; i<m; i++) {
                        if (fabs(augmented_matrix[n*i+k]) > maximum) {
                            maximum = fabs(augmented_matrix[n*i+k]);
                            m_i = i;
                        }
                    }
                }

                if (maximum == 0) {
                    k++;

                } else {
                    // Swap the found row into row h.
                    for (int j=0; j<n; j++) {
                        double tmp = augmented_matrix[n*m_i+j];
                        augmented_matrix[n*m_i+j] = augmented_matrix[n*h+j];
                        augmented_matrix[n*h+j] = tmp;
                    }

                    // Operate all expressions below pivot to make 0 the k column
                    for (int i=h+1; i<m; i++) {
                        double c = augmented_matrix[n*i+k]/augmented_matrix[n*h+k];
                        augmented_matrix[n*i+k] = 0;
                        for (int j=k+1; j<n; j++) {
                            augmented_matrix[n*i+j] -= augmented_matrix[n*h+j]*c;
                        }
                    }

                    // Go to next pivot
                    h++;
                    k++;
                }

                //str_cat_matrix (error, augmented_matrix, m, n);
            }
        }

        // Perform back substitution
        {
            // This assumes the resulting matrix in row echelon form has the
            // following additional constraints:
            //
            //  - All leading coefficients are in a straight diagonal.
            //  - The number of linearly independent equations equals the number
            //    of unassigned symbols.
            
            int k = n-2;

            // If there were linearly dependent equations in the beginning,
            // these will show up as rows with all zeroes at the end. Make h
            // start at the last non zero row.
            int h = num_unassigned_symbols-1;

            //str_cat_matrix (error, augmented_matrix, m, n);

            while (h>=0) {
                if (augmented_matrix[n*h+k] == 0) {
                    k--;

                } else {
                    augmented_matrix[n*h+n-1] /= augmented_matrix[n*h+k];
                    augmented_matrix[n*h+k] = 1;

                    for (int i=h-1; i>=0; i--) {
                        augmented_matrix[n*i+n-1] -= augmented_matrix[n*h+n-1]*augmented_matrix[n*i+k];
                        augmented_matrix[n*i+k] = 0;
                    }

                    h--;
                    k--;
                }

                //str_cat_matrix (error, augmented_matrix, m, n);
            }

            //str_cat_matrix (error, augmented_matrix, m, n);
        }

        // Copy result back into symbol definitions as a solution
        // NOTE: Only read the results of the first num_unassigned rows.
        for (int i=0; i<num_unassigned_symbols; i++) {
            int col = -1;
            for (int j=0; j<n-1; j++) {
                if (augmented_matrix[n*i+j] != 0) {
                    if (col == -1) {
                        col = j;
                    }
                }
            }

            struct symbol_definition_t *symbol_definition =
                id_to_symbol_definition_get (&system->id_to_symbol_definition, column_to_symbol_id[col]);
            symbol_definition->value = augmented_matrix[n*i + n-1];
            symbol_definition->state = SYMBOL_SOLVED;
        }

        mem_pool_end_temporary_memory (mrkr);
    }

    system->success = true;
}

// This is a safe implementation of the solver that can generate partial
// solutions if only part of the system is unsolvable. It's slower, but will
// notify when an error happens (by returning false) and try to provide useful
// error messages.
bool solver_solve (struct linear_system_t *system, string_t *error)
{
    bool success = true;

    uint64_t symbol_id_to_column[system->last_id];
    uint64_t column_to_symbol_id[system->last_id];
    int num_unassigned_symbols = 0;
    BINARY_TREE_FOR (name_to_symbol_definition, &system->name_to_symbol_definition, curr_node) {
        struct symbol_definition_t *symbol_definition = curr_node->value;
        if (symbol_definition->state == SYMBOL_UNASSIGNED) {
            symbol_id_to_column[symbol_definition->id] = num_unassigned_symbols;
            column_to_symbol_id[num_unassigned_symbols] = symbol_definition->id;
            num_unassigned_symbols++;
        }
    }

    uint32_t num_equations = system_num_equations (system);

    if (num_unassigned_symbols > 0) {
        // Create the matrix
        size_t m = num_equations;
        size_t n = num_unassigned_symbols+1;

        mem_pool_marker_t mrkr = mem_pool_begin_temporary_memory (&system->pool);
        double *augmented_matrix = mem_pool_push_array(&system->pool, m*n, double);
        memset (augmented_matrix, 0, m*n*sizeof(double));

        // Populate the matrix with the data from the parsed expressions
        int expression_idx = 0;
        struct expression_t *curr_expression = system->expressions;
        while (curr_expression != NULL) {
            double constant = 0;

            struct symbol_t *curr_symbol = curr_expression->symbols;
            while (curr_symbol != NULL) {
                if (curr_symbol->definition->state == SYMBOL_ASSIGNED) {
                    if (curr_symbol->is_negative) {
                        constant -= curr_symbol->definition->value;
                    } else {
                        constant += curr_symbol->definition->value;
                    }

                } else {
                    if (curr_symbol->is_negative) {
                        augmented_matrix[n*expression_idx + symbol_id_to_column[curr_symbol->definition->id]] = -1;
                    } else {
                        augmented_matrix[n*expression_idx + symbol_id_to_column[curr_symbol->definition->id]] = 1;
                    }
                }

                curr_symbol = curr_symbol->next;
            }

            augmented_matrix[n*expression_idx+num_unassigned_symbols] = -constant;

            expression_idx++;
            curr_expression = curr_expression->next;
        }

        // Compute row echelon form of the matrix
        {
            // Pivot indices
            size_t h = 0;
            size_t k = 0;

            //str_cat_matrix (error, augmented_matrix, m, n);

            while (h<(m-1) && k<(n-1)) {
                // Find the row with the leading coefficient of maximum absolute
                // value.
                size_t m_i = 0; // Index where the maximum value was found
                double maximum = 0;
                {
                    for (int i=h; i<m; i++) {
                        if (fabs(augmented_matrix[n*i+k]) > maximum) {
                            maximum = fabs(augmented_matrix[n*i+k]);
                            m_i = i;
                        }
                    }
                }

                if (maximum == 0) {
                    k++;

                } else {
                    // Swap the found row into row h.
                    for (int j=0; j<n; j++) {
                        double tmp = augmented_matrix[n*m_i+j];
                        augmented_matrix[n*m_i+j] = augmented_matrix[n*h+j];
                        augmented_matrix[n*h+j] = tmp;
                    }

                    // Operate all expressions below pivot to make 0 the k column
                    for (int i=h+1; i<m; i++) {
                        // We want to know if a row becomes zero except for the
                        // constant coefficient, this means the symbol
                        // represented by the pivot's column was
                        // overconstrained. Store if this symbol was already
                        // zero, and check if after the update it becomes a zero
                        // row except for the element in the last column.
                        //
                        // TODO: We really can't know the specific
                        // overconstrained symbol, it can be any one in that
                        // connected component. We should mark the whole
                        // connected component as overconstrained. We can,
                        // however, keep track of the row positions and compute
                        // which expression couldn't be satisfied.
                        bool was_zero = augmented_matrix[n*i+k] == 0;
                        bool is_overconstrained = true;

                        double c = augmented_matrix[n*i+k]/augmented_matrix[n*h+k];
                        augmented_matrix[n*i+k] = 0;
                        for (int j=k+1; j<n; j++) {
                            augmented_matrix[n*i+j] -= augmented_matrix[n*h+j]*c;

                            if (j < n-1) {
                                is_overconstrained &= (augmented_matrix[n*i+j] == 0);
                            } else {
                                is_overconstrained &= (augmented_matrix[n*i+j] != 0);
                            }
                        }

                        if (is_overconstrained && !was_zero) {
                            struct symbol_definition_t *symbol = id_to_symbol_definition_get(&system->id_to_symbol_definition, column_to_symbol_id[k]);
                            str_cat_printf (error, "Overconstrained symbol '%s'\n", str_data(&symbol->name));
                            success = false;
                        }
                    }

                    // Go to next pivot
                    h++;
                    k++;
                }

                //str_cat_matrix (error, augmented_matrix, m, n);
            }
        }

        // Perform back substitution
        {
            // For each linearly dependent equation in the beginning, we will
            // get a row with all zeroes at the end. For each equation that
            // overconstrains the systemm, we get a row with all zeroes except
            // for the constant term. Make h start at the last non zero row.
            int h = m-1;
            while (augmented_matrix[h*n + n - 1] == 0 && augmented_matrix[h*n + n - 2] == 0) {
                h--;
            }

            //str_cat_matrix (error, augmented_matrix, m, n);

            while (h>=0) {
                // Compute index of the leading coefficient. We don't assume
                // leading coefficients are in a straight diagonal. This
                // produces a partially solution, even in systems that are not
                // fully solvable.
                int k = 0;
                while (augmented_matrix[h*n + k] == 0) {
                    k++;

                    // If we get a row of zeros in the middle, the matrix isn't in echelon form.
                    //assert (k < n - 1);
                }

                // Only do back substitution if the only non zero coefficient is
                // the leading one. Otherwise, skip the row.
                bool back_substitute = true;
                for (int i=k+1; i < n - 1 && back_substitute; i++) {
                    if (augmented_matrix[n*h+i] != 0) back_substitute = false;
                }

                if (!back_substitute) {
                    h--;

                } else {
                    augmented_matrix[n*h+n-1] /= augmented_matrix[n*h+k];
                    augmented_matrix[n*h+k] = 1;

                    for (int i=h-1; i>=0; i--) {
                        augmented_matrix[n*i+n-1] -= augmented_matrix[n*h+n-1]*augmented_matrix[n*i+k];
                        augmented_matrix[n*i+k] = 0;
                    }

                    h--;
                }

                //str_cat_matrix (error, augmented_matrix, m, n);
            }
        }

        // Copy result back into symbol definitions as a solution
        // NOTE: Only read the results of the first num_unassigned rows.
        for (int i=0; i<num_unassigned_symbols; i++) {
            int count = 0;
            int col = -1;
            for (int j=0; j<n-1; j++) {
                if (augmented_matrix[n*i+j] != 0) {
                    if (col == -1) {
                        col = j;
                    }
                    count++;
                }
            }

            if (count == 1 && augmented_matrix[n*i+col] == 1) {
                struct symbol_definition_t *symbol_definition =
                    id_to_symbol_definition_get (&system->id_to_symbol_definition, column_to_symbol_id[col]);
                symbol_definition->value = augmented_matrix[n*i + n-1];
                symbol_definition->state = SYMBOL_SOLVED;

            } else {
                // TODO: Store the list of unsolved symbols somewhere. As with
                // overconstrained symbols, we really only get information about
                // the connected component.
            }
        }

        // Check that all symbols are either assigned or solved
        {
            BINARY_TREE_FOR (name_to_symbol_definition, &system->name_to_symbol_definition, curr_node) {
                struct symbol_definition_t *symbol_definition = curr_node->value;
                if (symbol_definition->state == SYMBOL_UNASSIGNED) {
                    str_cat_printf (error, "Unsolved symbol '%s'\n", str_data(&symbol_definition->name));
                    success = false;
                }
            }
        }

        if (!success) {
            str_cat_c (error, "\n");
            str_cat_matrix (error, augmented_matrix, m, n);
        }

        mem_pool_end_temporary_memory (mrkr);
    }

    system->success = success;

    return success;
}

void solver_print_solution (struct linear_system_t *system)
{
    int num_unassigned_symbols = 0;
    {
        printf ("Assigned:\n");
        BINARY_TREE_FOR (name_to_symbol_definition, &system->name_to_symbol_definition, curr_node) {
            struct symbol_definition_t *symbol_definition = curr_node->value;
            if (symbol_definition->state == SYMBOL_ASSIGNED) {
                printf ("%s = %.2f\n", str_data(&symbol_definition->name), symbol_definition->value);
            } else {
                num_unassigned_symbols++;
            }
        }
        printf ("\n");
    }

    {
        printf ("Solved:\n");
        BINARY_TREE_FOR (name_to_symbol_definition, &system->name_to_symbol_definition, curr_node) {
            struct symbol_definition_t *symbol_definition = curr_node->value;
            if (symbol_definition->state == SYMBOL_SOLVED) {
                printf ("%s = %.2f\n", str_data(&symbol_definition->name), symbol_definition->value);
            }
        }
    }

    printf ("\n");
    printf ("Total symbols: %d\n", system_num_symbols (system));
    printf ("Symbols to solve: %d\n", num_unassigned_symbols);
    printf ("Equations: %d\n", system_num_equations (system));
}

