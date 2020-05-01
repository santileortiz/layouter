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

    struct symbol_t *solution;
};

#define SOLVER_TOKEN_TABLE                 \
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

bool solver_solve (struct linear_system_t *system, string_t *error)
{
    return false;
}
