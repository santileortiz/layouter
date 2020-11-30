/*
 * Copiright (C) 2020 Santiago León O.
 */

#define _GNU_SOURCE // Used to enable strcasestr()
#define _XOPEN_SOURCE 700 // Required for strptime()
#include "common.h"
#include "binary_tree.c"
#include "scanner.c"
#include "color.h"

#include <gtk/gtk.h>

#include "linear_solver.c"

#define INFINITE_LEN 5000

BINARY_TREE_NEW(id_set, uint64_t, void*,  a <= b ? (a == b ? 0 : -1) : 1)

// These are the kinds of things that can be added to the layout
#define TK_ENTITY_TYPE_TABLE \
    TK_ENTITY_TYPE_ROW (TK_RECTANGLE, "rectangle") \
    TK_ENTITY_TYPE_ROW (TK_LINK, "link")

#define TK_ENTITY_TYPE_ROW(v1, v2) v1,
enum entity_type_t {
    TK_ENTITY_TYPE_TABLE
};
#undef TK_ENTITY_TYPE_ROW

#define TK_ENTITY_TYPE_ROW(v1, v2) v2,
char *entity_type_names[] = {
    TK_ENTITY_TYPE_TABLE
};
#undef TK_ENTITY_TYPE_ROW

// A feature is some quantity of an entity that will be represented by a symbol
// in the linear system of equations
#define TK_FEATURE_TABLE \
    TK_FEATURE_ROW (TK_MIN, "min")   \
    TK_FEATURE_ROW (TK_B, "b")       \
    TK_FEATURE_ROW (TK_MAX, "max")   \
    TK_FEATURE_ROW (TK_D, "d")       \
    TK_FEATURE_ROW (TK_SIZE, "size") \
                                     \
    TK_FEATURE_ROW (TK_DX, "dx")     \
    TK_FEATURE_ROW (TK_DY, "dy")

#define TK_FEATURE_ROW(v1,v2) v1,
enum feature_identifier_t {
    TK_FEATURE_TABLE
};
#undef TK_FEATURE_ROW

#define TK_FEATURE_ROW(v1,v2) v2,
char *feature_names[] = {
    TK_FEATURE_TABLE
};
#undef TK_FEATURE_ROW

enum axis_t {
    TK_X,
    TK_Y
};

char *axis_names[] = {"x", "y"};

struct entity_definition_t {
    enum entity_type_t type;
    enum feature_identifier_t *features;
};

struct feature_t {
    enum entity_type_t type;
    uint64_t id;
    enum feature_identifier_t feature;
    enum axis_t axis;
};

struct entity_t {
    enum entity_type_t type;
    uint64_t id;
    struct entity_t *next;
};

// This sets the passed string_t to be the name of the wvariable used in the
// system of equations to represent the passed feature parameters. It's useful
// to the user if they are adding equations that relate to layout entities.
void str_set_feature_name (string_t *str,
                           struct entity_t *entity,
                           enum feature_identifier_t feature_name,
                           enum axis_t axis)
{
    // TODO: Check the passed features are valid
    str_set_printf (str, "%ld.%s.%s", entity->id, feature_names[feature_name], axis_names[axis]);
}

// Even though we provide a convenient API for adding entities, we want to
// expose the full flexibility of defining relationships through equations. This
// means we want the user to be able to add expressions to the system directly,
// in a way that our render understands. To do this, users can add equations
// with symbols in the following syntax:
//
//    {entity_type}_{id}.{feature_name}.{axis}
//
// NOTE: The {id} part is just an integer number used to differentiate multiple
// entities of the same type. This never creates an actual entity object.
//
// NOTE: This is only to let the user add equations that represent internal
// entitites, and are drawn by the renderer. This is not a restrictive syntax,
// users can add symbols not adhering to it. For example, they can add their own
// features.
//
// This function parses the user feature syntax and populates a struct with the
// parsed data. If the name doesn't follow the syntax or the feature is not
// valid false is returned.
bool get_user_feature (char *name, struct feature_t *feature)
{
    assert (name != NULL && feature != NULL);

    struct feature_t _l_feature = {0};
    struct feature_t *l_feature = &_l_feature;

    bool success = true;
    struct scanner_t _scnr = {0};
    struct scanner_t *scnr = &_scnr;
    scnr->pos = name;

    bool type_found = false;
    for (int type_enum=0; type_enum<ARRAY_SIZE(entity_type_names); type_enum++) {
        if (scanner_str (scnr, entity_type_names[type_enum])) {
            l_feature->type = type_enum;
            type_found = true;
            break;
        }
    }

    if (!type_found) {
        success = false;
    }

    if (success) {
        int id;
        if (scanner_char(scnr, '_') && scanner_int (scnr, &id)) {
            l_feature->id = id;
        }
    }

    // TODO: Check that the entity type does have the found l_feature
    if (success) {
        if (scanner_char(scnr, '.')) {
            bool feature_found = false;
            for (int feature_enum=0; feature_enum<ARRAY_SIZE(feature_names); feature_enum++) {
                if (scanner_str (scnr, feature_names[feature_enum])) {
                    l_feature->feature = feature_enum;
                    feature_found = true;
                    break;
                }
            }

            if (!feature_found) {
                success = false;
            }
        }
    }

    if (success) {
        if (scanner_char(scnr, '.')) {
            bool axis_found = false;
            for (int axis_enum=0; axis_enum<ARRAY_SIZE(axis_names); axis_enum++) {
                if (scanner_str (scnr, axis_names[axis_enum])) {
                    l_feature->axis = axis_enum;
                    axis_found = true;
                    break;
                }
            }

            if (!axis_found) {
                success = false;
            }
        }
    }

    if (success) {
        *feature = *l_feature;
    }

    return success;
}

// Like str_set_feature_name() but writes the name of a symbol that uses the
// user syntax.
void str_set_user_feature_name (string_t *str,
                                enum entity_type_t type,
                                uint64_t id,
                                enum feature_identifier_t feature_name,
                                enum axis_t axis)
{
    // TODO: Check the passed features are valid
    str_set_printf (str, "%s_%ld.%s.%s", entity_type_names[type], id, feature_names[feature_name], axis_names[axis]);
}

struct app_t {
    mem_pool_t pool;

    uint64_t next_id;

    struct linear_system_t layout_system;

    box_t screen;

    dvec4 background_color;
    dvec3 rectangle_color;

    struct entity_t *rectangles;
};

gboolean window_delete_handler (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    gtk_main_quit ();
    return FALSE;
}

gboolean draw_cb (GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
    struct app_t *app = (struct app_t*)user_data;

    cairo_set_source_rgb (cr, ARGS_RGB(app->background_color));
    cairo_paint (cr);

    cairo_set_source_rgb (cr, ARGS_RGB(app->rectangle_color));

    if (app->layout_system.success) {
        string_t buffer = {0};
        struct entity_t *curr_rectangle = app->rectangles;
        while (curr_rectangle != NULL) {
            str_set_feature_name (&buffer, curr_rectangle, TK_MIN, TK_X);
            double x = system_get_symbol_value (&app->layout_system, str_data(&buffer));

            str_set_feature_name (&buffer, curr_rectangle, TK_MIN, TK_Y);
            double y = system_get_symbol_value (&app->layout_system, str_data(&buffer));

            str_set_feature_name (&buffer, curr_rectangle, TK_SIZE, TK_X);
            double width = system_get_symbol_value (&app->layout_system, str_data(&buffer));

            str_set_feature_name (&buffer, curr_rectangle, TK_SIZE, TK_Y);
            double height = system_get_symbol_value (&app->layout_system, str_data(&buffer));

            cairo_rectangle (cr, x, y, width, height);
            cairo_fill (cr);

            curr_rectangle = curr_rectangle->next;
        }

        // TODO: Draw links?

        struct id_set_tree_t rectangle_ids = {0};
        struct id_set_tree_t link_ids = {0};

        BINARY_TREE_FOR (name_to_symbol_definition, &app->layout_system.name_to_symbol_definition, curr_node) {
            struct symbol_definition_t *symbol_definition = curr_node->value;
            struct feature_t feature = {0};
            if (get_user_feature(str_data(&symbol_definition->name), &feature)) {
                if (feature.type == TK_RECTANGLE) {
                    id_set_tree_insert (&rectangle_ids, feature.id, NULL);

                } else if (feature.type == TK_LINK) {
                    id_set_tree_insert (&link_ids, feature.id, NULL);
                }
            }
        }

        BINARY_TREE_FOR (id_set, &rectangle_ids, curr_id_node) {
            uint64_t id = curr_id_node->key;

            str_set_user_feature_name (&buffer, TK_RECTANGLE, id, TK_MIN, TK_X);
            double x = system_get_symbol_value (&app->layout_system, str_data(&buffer));

            str_set_user_feature_name (&buffer, TK_RECTANGLE, id, TK_MIN, TK_Y);
            double y = system_get_symbol_value (&app->layout_system, str_data(&buffer));

            str_set_user_feature_name (&buffer, TK_RECTANGLE, id, TK_SIZE, TK_X);
            double width = system_get_symbol_value (&app->layout_system, str_data(&buffer));

            str_set_user_feature_name (&buffer, TK_RECTANGLE, id, TK_SIZE, TK_Y);
            double height = system_get_symbol_value (&app->layout_system, str_data(&buffer));

            cairo_rectangle (cr, x, y, width, height);
            cairo_fill (cr);
        }

        // TODO: Draw links?

        id_set_tree_destroy (&rectangle_ids);
        id_set_tree_destroy (&link_ids);
        str_free (&buffer);
    }

    gtk_widget_show_all (widget);
    return TRUE;
}

uint64_t layout_rectangle_size (struct app_t *app, dvec2 size)
{
    uint64_t id = app->next_id;
    app->next_id++;

    string_t buffer = {0};
    str_set_printf (&buffer, "%ld.min.x + %ld.size.x - %ld.max.x", id, id, id);
    solver_expr_equals_zero (&app->layout_system, str_data(&buffer));

    str_set_printf (&buffer, "%ld.min.y + %ld.size.y - %ld.max.y", id, id, id);
    solver_expr_equals_zero (&app->layout_system, str_data(&buffer));

    str_set_printf (&buffer, "%ld.size.x", id);
    solver_symbol_assign (&app->layout_system, str_data(&buffer), size.x);

    str_set_printf (&buffer, "%ld.size.y", id);
    solver_symbol_assign (&app->layout_system, str_data(&buffer), size.y);

    str_free (&buffer);

    struct entity_t *new_rect = mem_pool_push_struct (&app->pool, struct entity_t);
    *new_rect = ZERO_INIT(struct entity_t);
    new_rect->id = id;
    new_rect->type = TK_RECTANGLE;
    LINKED_LIST_PUSH (app->rectangles, new_rect);

    return id;
}

void layout_add_rectangle_anchor (struct app_t *app, uint64_t id, char *anchor_name)
{
    string_t buffer = {0};

    // min and max are defining anchors of a rectangle they are added when
    // pushing the rectangle.
    if (strcmp (anchor_name, "b") == 0) {
        str_set_printf (&buffer, "%ld.min.x - %ld.b.x", id, id);
        solver_expr_equals_zero (&app->layout_system, str_data(&buffer));

        str_set_printf (&buffer, "%ld.min.y + %ld.size.y - %ld.b.y", id, id, id);
        solver_expr_equals_zero (&app->layout_system, str_data(&buffer));

    } else if (strcmp (anchor_name, "d") == 0) {
        str_set_printf (&buffer, "%ld.min.x + %ld.size.x - %ld.d.x", id, id, id);
        solver_expr_equals_zero (&app->layout_system, str_data(&buffer));

        str_set_printf (&buffer, "%ld.min.y - %ld.d.y", id, id);
        solver_expr_equals_zero (&app->layout_system, str_data(&buffer));
    }

    str_free (&buffer);
}

uint64_t layout_link_d (struct app_t *app,
                        uint64_t id1, char *feature1,
                        uint64_t id2, char *feature2,
                        dvec2 d)
{
    // Get an id for the link
    uint64_t id = app->next_id;
    app->next_id++;

    // It's possible that we are using non defining features to link things,
    // then we add their respective equations here.
    // TODO: Avoid adding extra equations if this anchor has been referred to
    // before.
    layout_add_rectangle_anchor (app, id1, feature1);
    layout_add_rectangle_anchor (app, id2, feature2);

    string_t buffer = {0};
    str_set_printf (&buffer, "%ld.%s.x + %ld.d.x - %ld.%s.x", id1, feature1, id, id2, feature2);
    solver_expr_equals_zero (&app->layout_system, str_data(&buffer));

    str_set_printf (&buffer, "%ld.%s.y + %ld.d.y - %ld.%s.y", id1, feature1, id, id2, feature2);
    solver_expr_equals_zero (&app->layout_system, str_data(&buffer));

    str_set_printf (&buffer, "%ld.d.x", id);
    solver_symbol_assign (&app->layout_system, str_data(&buffer), d.x);

    str_set_printf (&buffer, "%ld.d.y", id);
    solver_symbol_assign (&app->layout_system, str_data(&buffer), d.y);

    str_free (&buffer);

    return id;
}

void layout_fix (struct app_t *app,
                     uint64_t id, char *feature,
                     dvec2 pos)
{
    string_t buffer = {0};

    str_set_printf (&buffer, "%ld.%s.x", id, feature);
    solver_symbol_assign (&app->layout_system, str_data(&buffer), pos.x);

    str_set_printf (&buffer, "%ld.%s.y", id, feature);
    solver_symbol_assign (&app->layout_system, str_data(&buffer), pos.y);

    str_free (&buffer);
}

void basic_rectangle (struct app_t *app)
{
    uint64_t rectangle_1 = layout_rectangle_size (app, DVEC2(90, 20));
    layout_fix (app, rectangle_1, "min", DVEC2(100, 100));
}

void floating_rectangle (struct app_t *app)
{
    uint64_t rectangle_1 = layout_rectangle_size (app, DVEC2(90, 20));
    layout_rectangle_size (app, DVEC2(90, 20));
    layout_fix (app, rectangle_1, "min", DVEC2(100, 100));
}

void linked_rectangles (struct app_t *app)
{
    uint64_t rectangle_1 = layout_rectangle_size (app, DVEC2(90, 20));
    uint64_t rectangle_2 = layout_rectangle_size (app, DVEC2(90, 20));
    layout_link_d (app, rectangle_1, "b", rectangle_2, "min", DVEC2(10, 15));
    layout_fix (app, rectangle_1, "min", DVEC2(100, 100));
}

void linked_rectangles_system_floating (struct app_t *app)
{
    struct linear_system_t *system = &app->layout_system;

    // Rectangle 1
    solver_expr_equals_zero (system, "rectangle_1.min.x + rectangle_1.size.x - rectangle_1.max.x");
    solver_expr_equals_zero (system, "rectangle_1.min.y + rectangle_1.size.y - rectangle_1.max.y");

    solver_symbol_assign (system, "rectangle_1.size.x", 90);
    solver_symbol_assign (system, "rectangle_1.size.y", 20);

    // Link
    solver_expr_equals_zero (system, "rectangle_1.min.x - rectangle_1.b.x");
    solver_expr_equals_zero (system, "rectangle_1.min.y + rectangle_1.size.y - rectangle_1.b.y");

    solver_expr_equals_zero (system, "rectangle_1.b.x + link_1.d.x - rectangle_2.min.x");
    solver_expr_equals_zero (system, "rectangle_1.b.y + link_1.d.y - rectangle_2.min.y");

    solver_symbol_assign (system, "link_1.d.x", 10);
    solver_symbol_assign (system, "link_1.d.y", 15);

    // Rectangle 2
    solver_expr_equals_zero (system, "rectangle_2.min.x + rectangle_2.size.x - rectangle_2.max.x");
    solver_expr_equals_zero (system, "rectangle_2.min.y + rectangle_2.size.y - rectangle_2.max.y");

    solver_symbol_assign (system, "rectangle_2.size.x", 90);
    solver_symbol_assign (system, "rectangle_2.size.y", 20);
}

void linked_rectangles_system (struct app_t *app)
{
    linked_rectangles_system_floating (app);

    struct linear_system_t *system = &app->layout_system;
    // Fix
    solver_symbol_assign (system, "rectangle_1.min.x", 100);
    solver_symbol_assign (system, "rectangle_1.min.y", 100);
}

void sample (struct app_t *app, uint64_t *out_rectangle_1, uint64_t *out_rectangle_2)
{
    uint64_t rectangle_1 = layout_rectangle_size (app, DVEC2(90, 20));
    uint64_t rectangle_2 = layout_rectangle_size (app, DVEC2(90, 20));
    layout_link_d (app, rectangle_1, "b", rectangle_2, "min", DVEC2(10, 15));

    uint64_t rectangle_3 = layout_rectangle_size (app, DVEC2(90, 20));
    uint64_t rectangle_4 = layout_rectangle_size (app, DVEC2(90, 20));
    layout_link_d (app, rectangle_3, "b", rectangle_4, "min", DVEC2(10, 15));

    uint64_t rectangle_5 = layout_rectangle_size (app, DVEC2(25, 20));
    layout_link_d (app, rectangle_2, "d", rectangle_5, "min", DVEC2(10, 0));
    layout_link_d (app, rectangle_5, "d", rectangle_4, "min", DVEC2(10, 0));

    layout_fix (app, rectangle_1, "min", DVEC2(100, 100));

    if (out_rectangle_1 != NULL) {
        *out_rectangle_1 = rectangle_1;
    }

    if (out_rectangle_2 != NULL) {
        *out_rectangle_2 = rectangle_2;
    }
}

void mix_layout (struct app_t *app)
{
    linked_rectangles_system_floating (app);

    uint64_t rectangle_1, rectangle_2;
    sample (app, &rectangle_1, &rectangle_2);

    struct linear_system_t *system = &app->layout_system;
    string_t buffer = {0};
    // Align rectangle with left side of top left rectangle
    str_set_printf (&buffer, "%ld.min.x - rectangle_1.min.x", rectangle_1);
    solver_expr_equals_zero (system, str_data(&buffer));

    // Separate vertically rectangle from bottom left rectangle
    str_set_printf (&buffer, "%ld.min.y + %ld.size.y - %ld.b.y", rectangle_2, rectangle_2, rectangle_2);
    solver_expr_equals_zero (system, str_data(&buffer));
    str_set_printf (&buffer, "%ld.b.y + link_2.d.y - rectangle_1.min.y", rectangle_2);
    solver_expr_equals_zero (system, str_data(&buffer));

    solver_symbol_assign (system, "link_2.d.y", 15);
    str_free (&buffer);
}

int main (int argc, char **argv)
{
    struct app_t app = {0};
    gtk_init (&argc, &argv);

    BOX_POS_SIZE(app.screen, DVEC2(0,0),DVEC2(800, 700));

    GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_resize (GTK_WINDOW(window), BOX_WIDTH(app.screen), BOX_HEIGHT(app.screen));
    g_signal_connect (G_OBJECT(window), "delete-event", G_CALLBACK(window_delete_handler), NULL);

    GtkWidget *drawing_area = gtk_drawing_area_new ();
    g_signal_connect (G_OBJECT (drawing_area), "draw", G_CALLBACK (draw_cb), &app);

    gtk_container_add (GTK_CONTAINER(window), drawing_area);
    gtk_widget_show_all (window);

    app.background_color = RGB(0.164, 0.203, 0.223);
    get_next_color (&app.rectangle_color);

    mix_layout (&app);

    string_t error = {0};
    bool success = solver_solve (&app.layout_system, &error);

    solver_print_solution (&app.layout_system);
    if (!success) {
        printf ("\n");
        printf ("%s", str_data(&error));
    }
    str_free (&error);

    gtk_main();

    solver_destroy (&app.layout_system);
    mem_pool_destroy (&app.pool);

    return 0;
}
