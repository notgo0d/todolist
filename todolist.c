#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TASKS 100
#define TASK_LENGTH 256

typedef struct {
    char task[TASK_LENGTH];
    gboolean completed;
    gboolean daily;
    int priority; // 0 = Low, 1 = Mid, 2 = High
} Task;

Task tasks[MAX_TASKS];
int task_count = 0;

GtkWidget *task_entry;
GtkWidget *daily_checkbox;
GtkWidget *task_list;
gboolean show_daily_only = FALSE; // Toggle for showing daily tasks

void add_task(GtkButton *button, gpointer data) {
    const char *task_text = gtk_entry_get_text(GTK_ENTRY(task_entry));
    gboolean is_daily = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(daily_checkbox));

    if (task_count < MAX_TASKS && strlen(task_text) > 0) {
        strcpy(tasks[task_count].task, task_text);
        tasks[task_count].completed = FALSE;
        tasks[task_count].daily = is_daily;
        tasks[task_count].priority = 0; // Default priority is Low
        task_count++;

        // Add the task to the list
        GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(task_list)));
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, task_text, 1, is_daily ? "Daily" : "One-time", 2, "Not Completed", 3, "Low", -1);

        // Clear the entry field
        gtk_entry_set_text(GTK_ENTRY(task_entry), "");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(daily_checkbox), FALSE);
    }
}

void toggle_completion(GtkCellRendererToggle *cell, gchar *path_str, gpointer data) {
    GtkTreeModel *model = GTK_TREE_MODEL(data);
    GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
    GtkTreeIter iter;
    gboolean completed;

    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get(model, &iter, 2, &completed, -1);

    // Toggle the completion status
    completed = !completed;
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, 2, completed ? "Completed" : "Not Completed", -1);

    // Update the task in the array
    int index = atoi(path_str);
    tasks[index].completed = completed;

    gtk_tree_path_free(path);
}

void delete_task(GtkMenuItem *menu_item, gpointer data) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(task_list));
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        char *task_text;
        gtk_tree_model_get(model, &iter, 0, &task_text, -1);

        // Remove the task from the array
        for (int i = 0; i < task_count; i++) {
            if (strcmp(tasks[i].task, task_text) == 0) {
                for (int j = i; j < task_count - 1; j++) {
                    tasks[j] = tasks[j + 1];
                }
                task_count--;
                break;
            }
        }

        // Remove the task from the list
        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
    }
}

void set_priority(GtkMenuItem *menu_item, gpointer data) {
    int priority = GPOINTER_TO_INT(data); // Get the priority level from the menu item
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(task_list));
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        char *task_text;
        gtk_tree_model_get(model, &iter, 0, &task_text, -1);

        // Update the priority in the array
        for (int i = 0; i < task_count; i++) {
            if (strcmp(tasks[i].task, task_text) == 0) {
                tasks[i].priority = priority;
                break;
            }
        }

        // Update the priority in the list
        const char *priority_text;
        switch (priority) {
            case 0: priority_text = "Low"; break;
            case 1: priority_text = "Mid"; break;
            case 2: priority_text = "High"; break;
            default: priority_text = "Low"; break;
        }
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, 3, priority_text, -1);
    }
}

void toggle_daily_tasks(GtkButton *button, gpointer data) {
    show_daily_only = !show_daily_only; // Toggle the filter

    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(task_list)));
    gtk_list_store_clear(store);

    for (int i = 0; i < task_count; i++) {
        if (!show_daily_only || tasks[i].daily) {
            GtkTreeIter iter;
            gtk_list_store_append(store, &iter);
            const char *priority_text;
            switch (tasks[i].priority) {
                case 0: priority_text = "Low"; break;
                case 1: priority_text = "Mid"; break;
                case 2: priority_text = "High"; break;
                default: priority_text = "Low"; break;
            }
            gtk_list_store_set(store, &iter, 0, tasks[i].task, 1, tasks[i].daily ? "Daily" : "One-time", 2, tasks[i].completed ? "Completed" : "Not Completed", 3, priority_text, -1);
        }
    }
}

gboolean on_task_list_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->button == GDK_BUTTON_SECONDARY) { // Right-click
        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(task_list));
        GtkTreeModel *model;
        GtkTreeIter iter;

        if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
            // Create a popup menu
            GtkWidget *menu = gtk_menu_new();
            GtkWidget *delete_item = gtk_menu_item_new_with_label("Delete Task");

            // Add priority options
            GtkWidget *low_item = gtk_menu_item_new_with_label("Set Priority: Low");
            GtkWidget *mid_item = gtk_menu_item_new_with_label("Set Priority: Mid");
            GtkWidget *high_item = gtk_menu_item_new_with_label("Set Priority: High");

            g_signal_connect(delete_item, "activate", G_CALLBACK(delete_task), NULL);
            g_signal_connect(low_item, "activate", G_CALLBACK(set_priority), GINT_TO_POINTER(0));
            g_signal_connect(mid_item, "activate", G_CALLBACK(set_priority), GINT_TO_POINTER(1));
            g_signal_connect(high_item, "activate", G_CALLBACK(set_priority), GINT_TO_POINTER(2));

            gtk_menu_shell_append(GTK_MENU_SHELL(menu), delete_item);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), low_item);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), mid_item);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), high_item);

            gtk_widget_show_all(menu);

            // Show the menu at the cursor position
            gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
        }
    }

    return FALSE; // Propagate the event further
}

int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *add_button;
    GtkWidget *daily_button;
    GtkListStore *store;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    // Initialize GTK
    gtk_init(&argc, &argv);

    // Create the main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "To-Do List");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 400);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Create a header bar for the top navbar
    GtkWidget *header_bar = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), "To-Do List");
    gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);

    // Create a "Daily" button for the header bar
    daily_button = gtk_button_new_with_label("Daily");
    g_signal_connect(daily_button, "clicked", G_CALLBACK(toggle_daily_tasks), NULL);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), daily_button);

    // Create a vertical box to hold widgets
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Create an entry field for adding tasks
    task_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(vbox), task_entry, FALSE, FALSE, 0);

    // Create a checkbox for daily tasks
    daily_checkbox = gtk_check_button_new_with_label("Daily Task");
    gtk_box_pack_start(GTK_BOX(vbox), daily_checkbox, FALSE, FALSE, 0);

    // Create a button to add tasks
    add_button = gtk_button_new_with_label("Add Task");
    g_signal_connect(add_button, "clicked", G_CALLBACK(add_task), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), add_button, FALSE, FALSE, 0);

    // Create a list store and tree view for displaying tasks
    store = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    task_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Task", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(task_list), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Type", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(task_list), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Status", renderer, "text", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(task_list), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Priority", renderer, "text", 3, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(task_list), column);

    gtk_box_pack_start(GTK_BOX(vbox), task_list, TRUE, TRUE, 0);

    // Connect the right-click event to show the burger menu
    g_signal_connect(task_list, "button-press-event", G_CALLBACK(on_task_list_button_press), NULL);

    // Show all widgets
    gtk_widget_show_all(window);

    // Start the GTK main loop
    gtk_main();

    return 0;
}