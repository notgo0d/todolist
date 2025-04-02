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
GtkWidget *task_grid;
GtkCssProvider *css_provider;

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
void apply_styles();
void create_priority_dialog(GtkEntry *entry, const char *task_text);

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

void create_priority_dialog(GtkEntry *entry, const char *task_text) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Set Priority",
                                                   GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(entry))),
                                                   GTK_DIALOG_MODAL,
                                                   "Cancel", GTK_RESPONSE_CANCEL,
                                                   NULL);
    
    // Set dialog size
    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 200);
    
    // Create content area
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_container_add(GTK_CONTAINER(content_area), vbox);
    
    // Add label
    GtkWidget *label = gtk_label_new("Select task priority:");
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    
    // Create priority buttons
    GtkWidget *daily_btn = gtk_button_new_with_label("🌟 Daily");
    GtkWidget *low_btn = gtk_button_new_with_label("Low");
    GtkWidget *mid_btn = gtk_button_new_with_label("Medium");
    GtkWidget *high_btn = gtk_button_new_with_label("❗ High");
    
    // Style buttons
    gtk_widget_set_name(daily_btn, "daily-btn");
    gtk_widget_set_name(low_btn, "low-btn");
    gtk_widget_set_name(mid_btn, "mid-btn");
    gtk_widget_set_name(high_btn, "high-btn");
    
    // Pack buttons
    gtk_box_pack_start(GTK_BOX(vbox), daily_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), low_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), mid_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), high_btn, FALSE, FALSE, 0);
    
    // Connect button signals
    g_signal_connect(daily_btn, "clicked", G_CALLBACK(gtk_dialog_response), GINT_TO_POINTER(-1));
    g_signal_connect(low_btn, "clicked", G_CALLBACK(gtk_dialog_response), GINT_TO_POINTER(0));
    g_signal_connect(mid_btn, "clicked", G_CALLBACK(gtk_dialog_response), GINT_TO_POINTER(1));
    g_signal_connect(high_btn, "clicked", G_CALLBACK(gtk_dialog_response), GINT_TO_POINTER(2));
    
    gtk_widget_show_all(dialog);
    
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    if (response == GTK_RESPONSE_CANCEL) {
        return;
    }
    
    // Add the task
    if (task_count < MAX_TASKS && strlen(task_text) > 0) {
        strcpy(tasks[task_count].task, task_text);
        tasks[task_count].completed = FALSE;
        tasks[task_count].daily = (response == -1);
        tasks[task_count].priority = (response == -1) ? -1 : response;
        task_count++;
        
        gtk_entry_set_text(entry, "");
        update_task_list();
    }
}

void add_task(GtkEntry *entry, gpointer data) {
    const char *task_text = gtk_entry_get_text(entry);

    // Check for empty task
    if (strlen(task_text) == 0) {
        return;
    }

    // Check if the task already exists
    for (int i = 0; i < task_count; i++) {
        if (strcmp(tasks[i].task, task_text) == 0) {
            GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(entry))),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_OK,
                                    "Task already exists: %s", task_text);
            gtk_window_set_title(GTK_WINDOW(dialog), "Duplicate Task");
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
            return;
        }
    }

    create_priority_dialog(entry, task_text);
}

void toggle_completion(GtkWidget *widget, gpointer data) {
    int index = GPOINTER_TO_INT(data);
    tasks[index].completed = !tasks[index].completed;
    update_task_list();
}

void delete_task(GtkWidget *widget, gpointer data) {
    int index = GPOINTER_TO_INT(data);

    // Create confirmation dialog
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(gtk_widget_get_toplevel(widget)),
                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                GTK_MESSAGE_QUESTION,
                                GTK_BUTTONS_YES_NO,
                                "Are you sure you want to delete this task?");
    gtk_window_set_title(GTK_WINDOW(dialog), "Confirm Deletion");
    
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    if (response == GTK_RESPONSE_YES) {
        for (int i = index; i < task_count - 1; i++) {
            tasks[i] = tasks[i + 1];
        }
        task_count--;
        update_task_list();
    }
}

void show_task_menu(GtkWidget *widget, gpointer data) {
    int index = GPOINTER_TO_INT(data);

    GtkWidget *menu = gtk_menu_new();
    
    // Create menu items
    GtkWidget *complete_item = gtk_menu_item_new_with_label(tasks[index].completed ? "Undo Completion" : "Mark Complete");
    GtkWidget *delete_item = gtk_menu_item_new_with_label("Delete");
    
    g_signal_connect(complete_item, "activate", G_CALLBACK(toggle_completion), GINT_TO_POINTER(index));
    g_signal_connect(delete_item, "activate", G_CALLBACK(delete_task), GINT_TO_POINTER(index));
    
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), complete_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), delete_item);
    
    gtk_widget_show_all(menu);
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

    // Create a notebook (tabbed interface)
    GtkWidget *notebook = gtk_notebook_new();
    gtk_grid_attach(GTK_GRID(task_grid), notebook, 0, 0, 1, 1);
    
    // Create tabs for each priority
    const char *tabs[] = {"🌟 Daily", "📝 Low", "📋 Medium", "❗ High"};
    GtkWidget *boxes[4] = {
        gtk_box_new(GTK_ORIENTATION_VERTICAL, 5),
        gtk_box_new(GTK_ORIENTATION_VERTICAL, 5),
        gtk_box_new(GTK_ORIENTATION_VERTICAL, 5),
        gtk_box_new(GTK_ORIENTATION_VERTICAL, 5)
    };
    
    // Add some padding to the boxes
    for (int i = 0; i < 4; i++) {
        gtk_container_set_border_width(GTK_CONTAINER(boxes[i]), 10);
        GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                      GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(scrolled_window), boxes[i]);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scrolled_window, gtk_label_new(tabs[i]));
    }
    
    // Add tasks to appropriate boxes
    for (int i = 0; i < task_count; i++) {
        GtkWidget *task_frame = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(task_frame), GTK_SHADOW_ETCHED_IN);
        
        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
        
        // Create completion checkbox
        GtkWidget *check = gtk_check_button_new();
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), tasks[i].completed);
        g_signal_connect(check, "toggled", G_CALLBACK(toggle_completion), GINT_TO_POINTER(i));
        
        // Create task label with strike-through if completed
        GtkWidget *label = gtk_label_new(tasks[i].task);
        if (tasks[i].completed) {
            PangoAttrList *attrs = pango_attr_list_new();
            pango_attr_list_insert(attrs, pango_attr_strikethrough_new(TRUE));
            gtk_label_set_attributes(GTK_LABEL(label), attrs);
            gtk_widget_set_opacity(label, 0.6);
        }
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        
        // Create menu button
        GtkWidget *menu_btn = gtk_button_new_from_icon_name("view-more-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_relief(GTK_BUTTON(menu_btn), GTK_RELIEF_NONE);
        g_signal_connect(menu_btn, "clicked", G_CALLBACK(show_task_menu), GINT_TO_POINTER(i));
        
        // Pack widgets
        gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), menu_btn, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(task_frame), hbox);
        
        // Add to appropriate tab based on priority
        int priority_index = tasks[i].priority == -1 ? 0 : tasks[i].priority + 1;
        if (priority_index >= 0 && priority_index < 4) {
            gtk_box_pack_start(GTK_BOX(boxes[priority_index]), task_frame, FALSE, FALSE, 0);
        }
    }
    
    gtk_widget_show_all(notebook);
    save_tasks();
}

void apply_styles() {
    css_provider = gtk_css_provider_new();
    const char *css = 
        "window, .task-list {"
        "    background-color: #f5f5f5;"
        "}"
        ""
        "#task-entry {"
        "    font-size: 16px;"
        "    padding: 8px;"
        "    margin: 10px;"
        "    border-radius: 4px;"
        "    border: 1px solid #ddd;"
        "}"
        ""
        ".task-frame {"
        "    background: white;"
        "    border-radius: 4px;"
        "    margin: 5px;"
        "}"
        ""
        ".task-frame:hover {"
        "    background: #f0f8ff;"
        "}"
        ""
        "#menu-button {"
        "    background: none;"
        "    border: none;"
        "    padding: 0;"
        "    margin: 0;"
        "    color: #999;"
        "    font-size: 16px;"
        "}"
        ""
        "#menu-button:hover {"
        "    color: #555;"
        "}"
        ""
        "#daily-btn {"
        "    background-color: #fffacd;"
        "    padding: 8px;"
        "    border-radius: 4px;"
        "}"
        ""
        "#low-btn {"
        "    background-color: #e6ffe6;"
        "    padding: 8px;"
        "    border-radius: 4px;"
        "}"
        ""
        "#mid-btn {"
        "    background-color: #e6f3ff;"
        "    padding: 8px;"
        "    border-radius: 4px;"
        "}"
        ""
        "#high-btn {"
        "    background-color: #ffebee;"
        "    padding: 8px;"
        "    border-radius: 4px;"
        "}"
        ""
        "notebook tab {"
        "    padding: 8px 12px;"
        "}"
        ""
        "notebook tab:checked {"
        "    background-color: #fff;"
        "    border-color: #ddd;"
        "}";
    
    gtk_css_provider_load_from_data(css_provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

void on_window_destroy(GtkWidget *widget, gpointer data) {
    save_tasks();
    gtk_main_quit();
}

int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *header;

    gtk_init(&argc, &argv);

    load_tasks();

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "✨ Productivity Tracker");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);

    // Create header bar
    header = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "✨ Productivity Tracker");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Create entry with placeholder text
    task_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(task_entry), "Add a new task...");
    gtk_widget_set_name(task_entry, "task-entry");
    gtk_box_pack_start(GTK_BOX(vbox), task_entry, FALSE, FALSE, 0);
    g_signal_connect(task_entry, "activate", G_CALLBACK(add_task), NULL);

    // Create task grid
    task_grid = gtk_grid_new();
    gtk_widget_set_name(task_grid, "task-grid");
    gtk_box_pack_start(GTK_BOX(vbox), task_grid, TRUE, TRUE, 0);

    apply_styles();
    update_task_list();

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}