/*
 * Copiright (C) 2020 Santiago León O.
 */

#define _GNU_SOURCE // Used to enable strcasestr()
#define _XOPEN_SOURCE 700 // Required for strptime()
#include "common.h"
#include "color.h"

#include <gtk/gtk.h>

#define INFINITE_LEN 5000

struct rectangle_t {
    dvec2 min;
    dvec2 max;

    struct rectangle_t *next;
};

struct app_t {
    mem_pool_t pool;

    struct rectangle_t screen;

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

    struct rectangle_t *curr_rectangle = app->rectangles;
    while (curr_rectangle != NULL) {
        cairo_rectangle (cr, curr_rectangle->min.x, curr_rectangle->min.y,
                         curr_rectangle->max.x - curr_rectangle->min.x, 
                         curr_rectangle->max.y - curr_rectangle->min.y);
        cairo_fill (cr);

        curr_rectangle = curr_rectangle->next;
    }

    gtk_widget_show_all (widget);
    return TRUE;
}

void add_rectangle (struct app_t *app, dvec2 pos, dvec2 size)
{
    struct rectangle_t *new_rect = mem_pool_push_struct (&app->pool, struct rectangle_t);
    *new_rect = ZERO_INIT(struct rectangle_t);
    BOX_POS_SIZE(*new_rect,pos,size)
    LINKED_LIST_PUSH (app->rectangles, new_rect);
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
    add_rectangle (&app, DVEC2(0,0), DVEC2(10, INFINITE_LEN));

    gtk_main();

    mem_pool_destroy (&app.pool);

    struct linear_system_t system = {0};
    solver_expr_equals_zero (system, "rectangle_1.min.x + rectangle_1.width - rectangle_1.max.x");
    solver_expr_equals_zero (system, "rectangle_1.min.y + rectangle_1.height - rectangle_1.max.y");

    solver_symbol_assign (sysytem, "rectangle_1.min.x", 10);
    solver_symbol_assign (sysytem, "rectangle_1.min.y", 10);
    solver_symbol_assign (sysytem, "rectangle_1.width", 90);
    solver_symbol_assign (sysytem, "rectangle_1.height", 20);

    string_t error = {0};
    if (solver_solve (system, error)) {
        struct symbol_t *curr_symbol = sysytem.solution;
        while (curr_symbol != NULL) {
            printf ("%s = %d\n", solver_symbol_name(curr_symbol->id), curr_symbol->value);
        }
    } else {
        printf ("%s\n", str_data(&error));
    }

    return 0;
}
