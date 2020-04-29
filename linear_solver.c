/*
 * Copiright (C) 2020 Santiago León O.
 */

enum symbol_state_t {
    SYMBOL_UNASSIGNED,
    SYMBOL_ASSIGNED
}

struct symbol_t {
    bool is_negative;
    uint64_t id;

    enum symbol_state_t state;
    double value;
};
