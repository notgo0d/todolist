#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TASKS 100
#define TASK_LENGTH 256
#define FILENAME "tasks.txt"

typedef struct {
    char task[TASK_LENGTH];
    gboolean completed;
    gboolean daily;
    int priority; // -1 = Daily, 0 = Low, 1 = Mid, 2 = High
} Task;

Task tasks[MAX_TASKS];
int task_count = 0;

GtkWidget *task_entry;
GtkWidget *task_grid; // Use a grid to organize tasks

// Function prototypes
void save_tasks();
void load_tasks();
void add_task(GtkEntry *entry, gpointer data);
void toggle_completion(GtkWidget *widget, gpointer data);
void delete_task(GtkWidget *widget, gpointer data);
void set_priority(GtkWidget *widget, gpointer data);
void update_task_list();
void on_window_destroy(GtkWidget *widget, gpointer data);
void show_task_menu(GtkWidget *widget, gpointer data);

void save_tasks() {
    FILE *file = fopen(FILENAME, "w");
    if (!file) {
        perror("Failed to open file for writing");
        return;
    }

    for (int i = 0; i < task_count; i++) {
        fprintf(file, "%s|%d|%d|%d\n", tasks[i].task, tasks[i].completed, tasks[i].daily, tasks[i].priority);
    }

    fclose(file);
}

void load_tasks() {
    FILE *file = fopen(FILENAME, "r");
    if (!file) {
        return; // No file to load
    }

    while (fscanf(file, "%[^|]|%d|%d|%d\n", tasks[task_count].task, &tasks[task_count].completed, &tasks[task_count].daily, &tasks[task_count].priority) != EOF) {
        task_count++;
    }

    fclose(file);
}

void add_task(GtkEntry *entry, gpointer data) {
    const char *task_text = gtk_entry_get_text(entry);

    // Check if the task already exists
    for (int i = 0; i < task_count; i++) {
        if (strcmp(tasks[i].task, task_text) == 0) {
            // Show an error message
            GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(entry))),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_OK,
                                    "Task already exists: %s", task_text);
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
            return; // Exit the function without adding the task
        }
    }

    if (task_count < MAX_TASKS && strlen(task_text) > 0) {
        // Create a dialog to ask for priority
        GtkWidget *dialog = gtk_dialog_new_with_buttons("Set Priority",
                                                       GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(entry))),
                                                       GTK_DIALOG_MODAL,
                                                       "Daily", -1,
                                                       "Low", 0,
                                                       "Mid", 1,
                                                       "High", 2,
                                                       NULL);

        // Show the dialog and get the response
        gint response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        // Set the task properties based on the response
        strcpy(tasks[task_count].task, task_text);
        tasks[task_count].completed = FALSE;
        tasks[task_count].daily = (response == -1); // Daily if response is -1
        tasks[task_count].priority = (response == -1) ? -1 : response; // Set priority
        task_count++;

        // Clear the entry field
        gtk_entry_set_text(entry, "");

        // Refresh the task list to ensure the task is in the correct section
        update_task_list();
    }
}

void toggle_completion(GtkWidget *widget, gpointer data) {
    int index = GPOINTER_TO_INT(data);
    tasks[index].completed = !tasks[index].completed;

    // Update the task list
    update_task_list();
}

void delete_task(GtkWidget *widget, gpointer data) {
    int index = GPOINTER_TO_INT(data);

    // Remove the task from the array
    for (int i = index; i < task_count - 1; i++) {
        tasks[i] = tasks[i + 1];
    }
    task_count--;

    // Update the task list
    update_task_list();
}

void set_priority(GtkWidget *widget, gpointer data) {
    int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "index"));
    int priority = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "priority"));

    // Update the priority in the array
    tasks[index].priority = priority;
    tasks[index].daily = (priority == -1);

    // Update the task list
    update_task_list();
}

void show_task_menu(GtkWidget *widget, gpointer data) {
    int index = GPOINTER_TO_INT(data);

    // Create a popup menu
    GtkWidget *menu = gtk_menu_new();

    // Add "Mark as Completed" or "Undo" option
    GtkWidget *complete_item = gtk_menu_item_new_with_label(tasks[index].completed ? "Undo" : "Mark as Completed");
    g_signal_connect(complete_item, "activate", G_CALLBACK(toggle_completion), GINT_TO_POINTER(index));

    // Add "Delete" option
    GtkWidget *delete_item = gtk_menu_item_new_with_label("Delete");
    g_signal_connect(delete_item, "activate", G_CALLBACK(delete_task), GINT_TO_POINTER(index));

    // Add items to the menu
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), complete_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), delete_item);

    // Show the menu
    gtk_widget_show_all(menu);

    // Show the menu at the widget position
    gtk_menu_popup_at_widget(GTK_MENU(menu), widget, GDK_GRAVITY_SOUTH_EAST, GDK_GRAVITY_NORTH_WEST, NULL);
}

void update_task_list() {
    // Clear the grid
    GList *children, *iter;
    children = gtk_container_get_children(GTK_CONTAINER(task_grid));
    for (iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);

    // Add titles to each section
    GtkWidget *daily_title = gtk_label_new("Daily Tasks");
    GtkWidget *low_title = gtk_label_new("Low Priority Tasks");
    GtkWidget *mid_title = gtk_label_new("Mid Priority Tasks");
    GtkWidget *high_title = gtk_label_new("High Priority Tasks");

    gtk_grid_attach(GTK_GRID(task_grid), daily_title, 0, 0, 1, 1); // Top-left
    gtk_grid_attach(GTK_GRID(task_grid), low_title, 1, 0, 1, 1);   // Top-right
    gtk_grid_attach(GTK_GRID(task_grid), mid_title, 0, 2, 1, 1);   // Bottom-left
    gtk_grid_attach(GTK_GRID(task_grid), high_title, 1, 2, 1, 1);  // Bottom-right

    // Create vertical boxes for each priority section
    GtkWidget *daily_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *low_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *mid_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *high_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    gtk_grid_attach(GTK_GRID(task_grid), daily_box, 0, 1, 1, 1); // Top-left
    gtk_grid_attach(GTK_GRID(task_grid), low_box, 1, 1, 1, 1);   // Top-right
    gtk_grid_attach(GTK_GRID(task_grid), mid_box, 0, 3, 1, 1);   // Bottom-left
    gtk_grid_attach(GTK_GRID(task_grid), high_box, 1, 3, 1, 1);  // Bottom-right

    // Add tasks to the appropriate box based on their priority
    for (int i = 0; i < task_count; i++) {
        GtkWidget *task_frame = gtk_frame_new(NULL); // Wrap the task in a frame
        GtkWidget *task_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        GtkWidget *task_label = gtk_label_new(tasks[i].task);
        GtkWidget *menu_button = gtk_button_new_with_label("⋮"); // Vertical three-dots button

        // Set minimum size for the task box
        gtk_widget_set_size_request(task_box, 200, 50); // Width: 200px, Height: 50px

        // Style the menu button
        gtk_widget_set_name(menu_button, "menu-button"); // Assign a CSS class
        gtk_widget_set_size_request(menu_button, 20, 20); // Smaller size
        gtk_widget_set_halign(menu_button, GTK_ALIGN_END); // Align to the right

        gtk_box_pack_start(GTK_BOX(task_box), task_label, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(task_box), menu_button, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(task_frame), task_box); // Add the task box to the frame

        // Connect the menu button to show the popup menu
        g_signal_connect(menu_button, "clicked", G_CALLBACK(show_task_menu), GINT_TO_POINTER(i));

        // Add the task frame to the appropriate box
        switch (tasks[i].priority) {
            case -1: gtk_box_pack_start(GTK_BOX(daily_box), task_frame, FALSE, FALSE, 0); break; // Daily
            case 0: gtk_box_pack_start(GTK_BOX(low_box), task_frame, FALSE, FALSE, 0); break;    // Low
            case 1: gtk_box_pack_start(GTK_BOX(mid_box), task_frame, FALSE, FALSE, 0); break;    // Mid
            case 2: gtk_box_pack_start(GTK_BOX(high_box), task_frame, FALSE, FALSE, 0); break;   // High
        }
    }

    // Show all widgets
    gtk_widget_show_all(task_grid);
}

void on_window_destroy(GtkWidget *widget, gpointer data) {
    save_tasks(); // Save tasks before closing
    gtk_main_quit();
}

int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *vbox;

    // Initialize GTK
    gtk_init(&argc, &argv);

    // Load tasks from file
    load_tasks();

    // Create the main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "To-Do List");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);

    // Create a vertical box to hold widgets
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Create an entry field for adding tasks
    task_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(vbox), task_entry, FALSE, FALSE, 0);

    // Connect the "activate" signal of the entry field to add_task
    g_signal_connect(task_entry, "activate", G_CALLBACK(add_task), NULL);

    // Create a grid to organize tasks
    task_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(task_grid), 10); // Add spacing between rows
    gtk_grid_set_column_spacing(GTK_GRID(task_grid), 10); // Add spacing between columns
    gtk_box_pack_start(GTK_BOX(vbox), task_grid, TRUE, TRUE, 0);

    // Load tasks into the list
    update_task_list();

    // Apply CSS styling for the menu button
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider,
        "#menu-button {"
        "   background: none;"
        "   border: none;"
        "   padding: 0;"
        "   margin: 0;"
        "   color: gray;"
        "   font-size: 16px;"
        "}", -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // Show all widgets
    gtk_widget_show_all(window);

    // Start the GTK main loop
    gtk_main();

    return 0;
}