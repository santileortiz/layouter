/*
 * Copiright (C) 2019 Santiago León O.
 */

#define BINARY_TREE_NEW(PREFIX,KEY_TYPE,VALUE_TYPE,CMP_A_TO_B)                                           \
                                                                                                         \
struct PREFIX ## _tree_t {                                                                               \
    mem_pool_t pool;                                                                                     \
                                                                                                         \
    uint32_t num_nodes;                                                                                  \
                                                                                                         \
    struct PREFIX ## _tree_node_t *root;                                                                 \
};                                                                                                       \
                                                                                                         \
/*Leftmost node will be the smallest.*/                                                                  \
struct PREFIX ## _tree_node_t {                                                                          \
    KEY_TYPE key;                                                                                        \
                                                                                                         \
    VALUE_TYPE value;                                                                                    \
                                                                                                         \
    struct PREFIX ## _tree_node_t *right;                                                                \
    struct PREFIX ## _tree_node_t *left;                                                                 \
};                                                                                                       \
                                                                                                         \
void PREFIX ## _tree_destroy (struct PREFIX ## _tree_t *tree)                                            \
{                                                                                                        \
    mem_pool_destroy (&tree->pool);                                                                      \
}                                                                                                        \
                                                                                                         \
struct PREFIX ## _tree_node_t* PREFIX ## _tree_allocate_node (struct PREFIX ## _tree_t *tree)            \
{                                                                                                        \
    /* TODO: When we add removal of nodes, this should allocate them from a free
    list of nodes.*/                                                                                     \
    struct PREFIX ## _tree_node_t *new_node =                                                            \
        mem_pool_push_struct (&tree->pool, struct PREFIX ## _tree_node_t);                               \
    *new_node = ZERO_INIT(struct PREFIX ## _tree_node_t);                                                \
    return new_node;                                                                                     \
}                                                                                                        \
                                                                                                         \
void PREFIX ## _tree_insert (struct PREFIX ## _tree_t *tree, KEY_TYPE key, VALUE_TYPE value)             \
{                                                                                                        \
    bool key_found = false;                                                                              \
    if (tree->root == NULL) {                                                                            \
        struct PREFIX ## _tree_node_t *new_node = PREFIX ## _tree_allocate_node (tree);                  \
        new_node->key = key;                                                                             \
        new_node->value = value;                                                                         \
                                                                                                         \
        tree->root = new_node;                                                                           \
        tree->num_nodes++;                                                                               \
                                                                                                         \
    } else {                                                                                             \
        struct PREFIX ## _tree_node_t **curr_node = &tree->root;                                         \
        while (!key_found && *curr_node != NULL) {                                                       \
            KEY_TYPE a = key;                                                                            \
            KEY_TYPE b = (*curr_node)->key;                                                              \
            int c = CMP_A_TO_B;                                                                          \
            if (c < 0) {                                                                                 \
                curr_node = &(*curr_node)->left;                                                         \
                                                                                                         \
            } else if (c > 0) {                                                                          \
                curr_node = &(*curr_node)->right;                                                        \
                                                                                                         \
            } else {                                                                                     \
                /* Key already exists. Options of what we could do here:

                  - Assert that this will never happen.
                  - Overwrite the existing value with the new one. The problem
                    is if values are pointers in the future, then we could be
                    leaking stuff without knowing?
                  - Do nothing, but somehow let the caller know the key was
                    already there so we didn't insert the value they wanted.

                 I lean more towards the last option.*/                                                  \
                key_found = true;                                                                        \
                break;                                                                                   \
            }                                                                                            \
        }                                                                                                \
                                                                                                         \
        if (!key_found) {                                                                                \
            *curr_node = PREFIX ## _tree_allocate_node (tree);                                           \
            (*curr_node)->key = key;                                                                     \
            (*curr_node)->value = value;                                                                 \
                                                                                                         \
            tree->num_nodes++;                                                                           \
        }                                                                                                \
                                                                                                         \
        /* TODO: Rebalance the tree.*/                                                                   \
    }                                                                                                    \
}                                                                                                        \
                                                                                                         \
bool PREFIX ## _tree_lookup (struct PREFIX ## _tree_t *tree,                                             \
                             KEY_TYPE key,                                                               \
                             struct PREFIX ## _tree_node_t **result)                                     \
{                                                                                                        \
    bool key_found = false;                                                                              \
    struct PREFIX ## _tree_node_t **curr_node = &tree->root;                                             \
    while (*curr_node != NULL) {                                                                         \
        KEY_TYPE a = key;                                                                                \
        KEY_TYPE b = (*curr_node)->key;                                                                  \
        int c = CMP_A_TO_B;                                                                              \
        if (c < 0) {                                                                                     \
            curr_node = &(*curr_node)->left;                                                             \
                                                                                                         \
        } else if (c > 0) {                                                                              \
            curr_node = &(*curr_node)->right;                                                            \
                                                                                                         \
        } else {                                                                                         \
            key_found = true;                                                                            \
            break;                                                                                       \
        }                                                                                                \
    }                                                                                                    \
                                                                                                         \
    if (result != NULL) {                                                                                \
        if (key_found) {                                                                                 \
            *result = *curr_node;                                                                        \
        } else {                                                                                         \
            *result = NULL;                                                                              \
        }                                                                                                \
    }                                                                                                    \
                                                                                                         \
    return key_found;                                                                                    \
}                                                                                                        \
                                                                                                         \
/*
 * This is only a convenience function. A zeroed out value will be returned
 * if the key is not found. There is no way to differentiate a zeroed out
 * stored value from a non existing key, use *_tree_lookup() for that.
 */                                                                                                      \
VALUE_TYPE PREFIX ## _get (struct PREFIX ## _tree_t *tree,                                               \
                     KEY_TYPE key)                                                                       \
{                                                                                                        \
    VALUE_TYPE res = ZERO_INIT(VALUE_TYPE);                                                              \
    struct PREFIX ## _tree_node_t *result_node;                                                          \
    if (PREFIX ## _tree_lookup (tree, key, &result_node)) {                                              \
        res = result_node->value;                                                                        \
    }                                                                                                    \
                                                                                                         \
    return res;                                                                                          \
}

#define BINARY_TREE_FOR(PREFIX,TREE,VARNAME)                                                             \
                                                                                                         \
struct PREFIX ## _tree_node_t *VARNAME = (TREE)->root;                                                   \
for (struct {                                                                                            \
         bool break_needed;                                                                              \
         bool visit_node;                                                                                \
         int stack_idx;                                                                                  \
         struct PREFIX ## _tree_node_t **stack;                                                          \
     } _loop_ctx = {                                                                                     \
         false,                                                                                          \
         false,                                                                                          \
         0,                                                                                              \
         malloc ((TREE)->num_nodes*sizeof(struct PREFIX ## _tree_node_t))                                \
     };                                                                                                  \
                                                                                                         \
     _loop_ctx.break_needed = false,                                                                     \
     _loop_ctx.visit_node = false,                                                                       \
     (VARNAME != NULL ?                                                                                  \
        (_loop_ctx.stack[_loop_ctx.stack_idx++] = VARNAME,                                               \
         VARNAME = VARNAME->left,                                                                        \
         0)                                                                                              \
     :                                                                                                   \
        (_loop_ctx.stack_idx == 0 ?                                                                      \
            (_loop_ctx.break_needed = true, 0)                                                           \
        :                                                                                                \
            (VARNAME = _loop_ctx.stack[--_loop_ctx.stack_idx],                                           \
             _loop_ctx.visit_node = true,                                                                \
             0),                                                                                         \
        0)                                                                                               \
     ),                                                                                                  \
     _loop_ctx.break_needed ? free (_loop_ctx.stack), false : true;                                      \
                                                                                                         \
     _loop_ctx.visit_node ?                                                                              \
         (VARNAME = VARNAME->right, 0) : 0)                                                              \
                                                                                                         \
if (_loop_ctx.visit_node)
