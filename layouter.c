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

struct rectangle_t {
    uint64_t id;
    struct rectangle_t *next;
};

struct app_t {
    mem_pool_t pool;

    uint64_t next_id;

    struct linear_system_t layout_system;

    box_t screen;

    dvec4 background_color;
    dvec3 rectangle_color;

    struct rectangle_t *rectangles;
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
        struct rectangle_t *curr_rectangle = app->rectangles;
        while (curr_rectangle != NULL) {
            str_set_printf (&buffer, "%lu.min.x", curr_rectangle->id);
            double x = system_get_symbol_value (&app->layout_system, str_data(&buffer));

            str_set_printf (&buffer, "%lu.min.y", curr_rectangle->id);
            double y = system_get_symbol_value (&app->layout_system, str_data(&buffer));

            str_set_printf (&buffer, "%lu.width", curr_rectangle->id);
            double width = system_get_symbol_value (&app->layout_system, str_data(&buffer));

            str_set_printf (&buffer, "%lu.height", curr_rectangle->id);
            double height = system_get_symbol_value (&app->layout_system, str_data(&buffer));

            cairo_rectangle (cr, x, y, width, height);
            cairo_fill (cr);

            curr_rectangle = curr_rectangle->next;
        }
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
    str_set_printf (&buffer, "%ld.min.x + %ld.width - %ld.max.x", id, id, id);
    solver_expr_equals_zero (&app->layout_system, str_data(&buffer));

    str_set_printf (&buffer, "%ld.min.y + %ld.height - %ld.max.y", id, id, id);
    solver_expr_equals_zero (&app->layout_system, str_data(&buffer));

    str_set_printf (&buffer, "%ld.width", id);
    solver_symbol_assign (&app->layout_system, str_data(&buffer), size.x);

    str_set_printf (&buffer, "%ld.height", id);
    solver_symbol_assign (&app->layout_system, str_data(&buffer), size.y);

    str_free (&buffer);

    struct rectangle_t *new_rect = mem_pool_push_struct (&app->pool, struct rectangle_t);
    *new_rect = ZERO_INIT(struct rectangle_t);
    new_rect->id = id;
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

        str_set_printf (&buffer, "%ld.min.y + %ld.height - %ld.b.y", id, id, id);
        solver_expr_equals_zero (&app->layout_system, str_data(&buffer));

    } else if (strcmp (anchor_name, "d") == 0) {
        str_set_printf (&buffer, "%ld.min.x + %ld.width - %ld.d.x", id, id, id);
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

void print_system_solution (struct linear_system_t *system)
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

void basic_rectangle (struct app_t *app)
{
    uint64_t rectangle_1 = layout_rectangle_size (app, DVEC2(90, 20));
    layout_fix (app, rectangle_1, "min", DVEC2(100, 100));
}

void floating_rectangle (struct app_t *app)
{
    // FIXME: This caused an invalid pointer error in free() when restoring the
    // temporary memory used for the linear system matrix.
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

void linked_rectangles_system (struct linear_system_t *system)
{
    // Rectangle 1
    solver_expr_equals_zero (system, "rectangle_1.min.x + rectangle_1.width - rectangle_1.max.x");
    solver_expr_equals_zero (system, "rectangle_1.min.y + rectangle_1.height - rectangle_1.max.y");

    solver_symbol_assign (system, "rectangle_1.width", 90);
    solver_symbol_assign (system, "rectangle_1.height", 20);

    // Link
    solver_expr_equals_zero (system, "rectangle_1.min.x - rectangle_1.b.x");
    solver_expr_equals_zero (system, "rectangle_1.min.y + rectangle_1.height - rectangle_1.b.y");

    solver_expr_equals_zero (system, "rectangle_1.b.x + link_1.d.x - rectangle_2.min.x");
    solver_expr_equals_zero (system, "rectangle_1.b.y + link_1.d.y - rectangle_2.min.y");

    // FIXME: When these assignments are missing the linear system throws
    // garbage. We should show better errors.
    solver_symbol_assign (system, "link_1.d.x", 10);
    solver_symbol_assign (system, "link_1.d.y", 15);

    // Rectangle 2
    solver_expr_equals_zero (system, "rectangle_2.min.x + rectangle_2.width - rectangle_2.max.x");
    solver_expr_equals_zero (system, "rectangle_2.min.y + rectangle_2.height - rectangle_2.max.y");

    solver_symbol_assign (system, "rectangle_2.width", 90);
    solver_symbol_assign (system, "rectangle_2.height", 20);

    // Fix
    solver_symbol_assign (system, "rectangle_1.min.x", 100);
    solver_symbol_assign (system, "rectangle_1.min.y", 100);
}

void sample (struct app_t *app)
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

    sample (&app);

    string_t error = {0};
    bool success = solver_solve (&app.layout_system, &error);
    print_system_solution (&app.layout_system);
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
