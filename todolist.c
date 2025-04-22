#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>

#define MAX_TASKS 100
#define TASK_LENGTH 256
#define NOTE_LENGTH 1024
#define DB_FILENAME "todo.db"

typedef enum {
    PRIORITY_DAILY = -1,
    PRIORITY_LOW = 0,
    PRIORITY_MEDIUM = 1,
    PRIORITY_HIGH = 2
} TaskPriority;

typedef struct {
    char task[TASK_LENGTH];
    gboolean completed;
    gboolean daily;
    TaskPriority priority;
    char note[NOTE_LENGTH];
    time_t created_at;
    time_t completed_at;
    gboolean note_visible;
} Task;

Task tasks[MAX_TASKS];
int task_count = 0;
sqlite3 *db;

GtkWidget *task_entry;
GtkWidget *task_grid;

// Function prototypes
void init_database();
void save_tasks();
void load_tasks();
void add_task(GtkEntry *entry, gpointer data);
void toggle_completion(GtkWidget *widget, gpointer data);
void delete_task(GtkWidget *widget, gpointer data);
void edit_task(GtkWidget *widget, gpointer data);
void update_task_list();
void on_window_destroy(GtkWidget *widget, gpointer data);
void show_task_context_menu(GtkWidget *widget, gpointer data);
void toggle_note_visibility(GtkWidget *widget, gpointer data);
void edit_note(GtkWidget *widget, gpointer data);
void clear_completed_tasks(GtkWidget *widget, gpointer data);
void show_about_dialog(GtkWidget *widget, gpointer data);
GtkWidget* create_task_widget(int index);
const char* get_priority_name(TaskPriority priority);
void show_error_dialog(GtkWindow *parent, const char *message);
void show_info_dialog(GtkWindow *parent, const char *message);

void init_database() {
    int rc = sqlite3_open(DB_FILENAME, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }

    char *sql = "CREATE TABLE IF NOT EXISTS tasks ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "task TEXT NOT NULL,"
                "completed INTEGER DEFAULT 0,"
                "daily INTEGER DEFAULT 0,"
                "priority INTEGER DEFAULT 0,"
                "note TEXT DEFAULT '',"
                "created_at INTEGER,"
                "completed_at INTEGER,"
                "note_visible INTEGER DEFAULT 0);";
    
    char *err_msg = 0;
    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        exit(1);
    }
}

void save_tasks() {
    // Clear existing data
    char *sql = "DELETE FROM tasks;";
    char *err_msg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return;
    }

    // Insert all current tasks
    sqlite3_stmt *stmt;
    const char *insert_sql = "INSERT INTO tasks (task, completed, daily, priority, note, created_at, completed_at, note_visible) "
                            "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    
    rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return;
    }

    for (int i = 0; i < task_count; i++) {
        sqlite3_bind_text(stmt, 1, tasks[i].task, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, tasks[i].completed);
        sqlite3_bind_int(stmt, 3, tasks[i].daily);
        sqlite3_bind_int(stmt, 4, tasks[i].priority);
        sqlite3_bind_text(stmt, 5, tasks[i].note, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 6, tasks[i].created_at);
        sqlite3_bind_int64(stmt, 7, tasks[i].completed_at);
        sqlite3_bind_int(stmt, 8, tasks[i].note_visible);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "Execution failed: %s\n", sqlite3_errmsg(db));
        }

        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);
}

void load_tasks() {
    task_count = 0;
    
    const char *sql = "SELECT task, completed, daily, priority, note, created_at, completed_at, note_visible FROM tasks;";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW && task_count < MAX_TASKS) {
        Task *t = &tasks[task_count];
        
        strncpy(t->task, (const char*)sqlite3_column_text(stmt, 0), TASK_LENGTH - 1);
        t->task[TASK_LENGTH - 1] = '\0';
        
        t->completed = sqlite3_column_int(stmt, 1);
        t->daily = sqlite3_column_int(stmt, 2);
        t->priority = sqlite3_column_int(stmt, 3);
        
        const char *note = (const char*)sqlite3_column_text(stmt, 4);
        strncpy(t->note, note ? note : "", NOTE_LENGTH - 1);
        t->note[NOTE_LENGTH - 1] = '\0';
        
        t->created_at = sqlite3_column_int64(stmt, 5);
        t->completed_at = sqlite3_column_int64(stmt, 6);
        t->note_visible = sqlite3_column_int(stmt, 7);
        
        task_count++;
    }

    sqlite3_finalize(stmt);
}

void show_error_dialog(GtkWindow *parent, const char *message) {
    GtkWidget *dialog = gtk_message_dialog_new(parent,
                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_OK,
                                             "%s", message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void show_info_dialog(GtkWindow *parent, const char *message) {
    GtkWidget *dialog = gtk_message_dialog_new(parent,
                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                             GTK_MESSAGE_INFO,
                                             GTK_BUTTONS_OK,
                                             "%s", message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void toggle_completion(GtkWidget *widget, gpointer data) {
    int index = GPOINTER_TO_INT(data);
    tasks[index].completed = !tasks[index].completed;
    tasks[index].completed_at = tasks[index].completed ? time(NULL) : 0;
    save_tasks();
    update_task_list();
}

void delete_task(GtkWidget *widget, gpointer data) {
    int index = GPOINTER_TO_INT(data);
    
    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(gtk_widget_get_toplevel(widget)),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "Are you sure you want to delete the task: %s?",
        tasks[index].task
    );
    
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    if (response == GTK_RESPONSE_YES) {
        for (int i = index; i < task_count - 1; i++) {
            tasks[i] = tasks[i + 1];
        }
        task_count--;
        save_tasks();
        update_task_list();
    }
}

void edit_task(GtkWidget *widget, gpointer data) {
    int index = GPOINTER_TO_INT(data);
    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_toplevel(widget));
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Edit Task",
                                                  parent,
                                                  GTK_DIALOG_MODAL,
                                                  "_Cancel", GTK_RESPONSE_CANCEL,
                                                  "_Save", GTK_RESPONSE_ACCEPT,
                                                  NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), tasks[index].task);
    gtk_container_add(GTK_CONTAINER(content_area), entry);
    
    GtkWidget *priority_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(priority_combo), "Daily");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(priority_combo), "Low");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(priority_combo), "Medium");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(priority_combo), "High");
    gtk_combo_box_set_active(GTK_COMBO_BOX(priority_combo), tasks[index].priority + 1);
    gtk_container_add(GTK_CONTAINER(content_area), priority_combo);
    
    gtk_widget_show_all(dialog);
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (result == GTK_RESPONSE_ACCEPT) {
        const char *new_text = gtk_entry_get_text(GTK_ENTRY(entry));
        int new_priority = gtk_combo_box_get_active(GTK_COMBO_BOX(priority_combo)) - 1;
        
        strncpy(tasks[index].task, new_text, TASK_LENGTH - 1);
        tasks[index].task[TASK_LENGTH - 1] = '\0';
        tasks[index].priority = new_priority;
        tasks[index].daily = (new_priority == PRIORITY_DAILY);
        
        save_tasks();
        update_task_list();
    }
    
    gtk_widget_destroy(dialog);
}

void toggle_note_visibility(GtkWidget *widget, gpointer data) {
    int index = GPOINTER_TO_INT(data);
    tasks[index].note_visible = !tasks[index].note_visible;
    update_task_list();
}

void edit_note(GtkWidget *widget, gpointer data) {
    int index = GPOINTER_TO_INT(data);
    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_toplevel(widget));
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Edit Note",
                                                  parent,
                                                  GTK_DIALOG_MODAL,
                                                  "_Cancel", GTK_RESPONSE_CANCEL,
                                                  "_Save", GTK_RESPONSE_ACCEPT,
                                                  NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *text_view = gtk_text_view_new();
    
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);
    gtk_container_add(GTK_CONTAINER(content_area), scrolled_window);
    gtk_widget_set_size_request(scrolled_window, 400, 300);
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_buffer_set_text(buffer, tasks[index].note, -1);
    
    gtk_widget_show_all(dialog);
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (result == GTK_RESPONSE_ACCEPT) {
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        gchar *note_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
        strncpy(tasks[index].note, note_text, NOTE_LENGTH - 1);
        tasks[index].note[NOTE_LENGTH - 1] = '\0';
        g_free(note_text);
        save_tasks();
        tasks[index].note_visible = TRUE;
        update_task_list();
    }
    
    gtk_widget_destroy(dialog);
}

const char* get_priority_name(TaskPriority priority) {
    switch (priority) {
        case PRIORITY_DAILY: return "Daily";
        case PRIORITY_LOW: return "Low";
        case PRIORITY_MEDIUM: return "Medium";
        case PRIORITY_HIGH: return "High";
        default: return "Unknown";
    }
}

void show_task_context_menu(GtkWidget *widget, gpointer data) {
    int index = GPOINTER_TO_INT(data);
    
    GtkWidget *menu = gtk_menu_new();
    
    GtkWidget *edit_item = gtk_menu_item_new_with_label("Edit Task");
    g_signal_connect(edit_item, "activate", G_CALLBACK(edit_task), GINT_TO_POINTER(index));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), edit_item);
    
    GtkWidget *note_item = gtk_menu_item_new_with_label("Edit Note");
    g_signal_connect(note_item, "activate", G_CALLBACK(edit_note), GINT_TO_POINTER(index));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), note_item);
    
    GtkWidget *delete_item = gtk_menu_item_new_with_label("Delete");
    g_signal_connect(delete_item, "activate", G_CALLBACK(delete_task), GINT_TO_POINTER(index));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), delete_item);
    
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_widget(GTK_MENU(menu), widget, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
}

void clear_completed_tasks(GtkWidget *widget, gpointer data) {
    int new_count = 0;
    for (int i = 0; i < task_count; i++) {
        if (!tasks[i].completed) {
            tasks[new_count++] = tasks[i];
        }
    }
    
    if (new_count != task_count) {
        task_count = new_count;
        save_tasks();
        update_task_list();
        show_info_dialog(NULL, "Completed tasks have been cleared.");
    } else {
        show_info_dialog(NULL, "No completed tasks to clear.");
    }
}

void show_about_dialog(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_about_dialog_new();
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), "To-Do List");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "1.0");
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog), "(c) 2023");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), 
                                "A simple GTK-based to-do list application with sliding notes.");
    gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(dialog), GTK_LICENSE_MIT_X11);
    
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

GtkWidget* create_task_widget(int index) {
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    GtkWidget *task_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    
    GtkWidget *check = gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), tasks[index].completed);
    g_signal_connect(check, "toggled", G_CALLBACK(toggle_completion), GINT_TO_POINTER(index));
    gtk_box_pack_start(GTK_BOX(task_box), check, FALSE, FALSE, 0);
    
    GtkWidget *label = gtk_label_new(tasks[index].task);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(task_box), label, TRUE, TRUE, 0);
    
    GtkWidget *note_btn = gtk_button_new_with_label("💬");
    g_signal_connect(note_btn, "clicked", G_CALLBACK(toggle_note_visibility), GINT_TO_POINTER(index));
    gtk_box_pack_start(GTK_BOX(task_box), note_btn, FALSE, FALSE, 0);
    
    if (strlen(tasks[index].note) > 0) {
        GtkWidget *edit_note_btn = gtk_button_new_with_label("✏️");
        g_signal_connect(edit_note_btn, "clicked", G_CALLBACK(edit_note), GINT_TO_POINTER(index));
        gtk_box_pack_start(GTK_BOX(task_box), edit_note_btn, FALSE, FALSE, 0);
    }
    
    GtkWidget *menu_btn = gtk_button_new_with_label("⋮");
    g_signal_connect(menu_btn, "clicked", G_CALLBACK(show_task_context_menu), GINT_TO_POINTER(index));
    gtk_box_pack_start(GTK_BOX(task_box), menu_btn, FALSE, FALSE, 0);
    
    if (tasks[index].completed) {
        GtkStyleContext *context = gtk_widget_get_style_context(task_box);
        gtk_style_context_add_class(context, "completed-task");
    }
    
    gtk_box_pack_start(GTK_BOX(main_box), task_box, FALSE, FALSE, 0);
    
    if (tasks[index].note_visible && strlen(tasks[index].note) > 0) {
        GtkWidget *note_frame = gtk_frame_new(NULL);
        GtkWidget *note_label = gtk_label_new(tasks[index].note);
        gtk_label_set_line_wrap(GTK_LABEL(note_label), TRUE);
        gtk_label_set_xalign(GTK_LABEL(note_label), 0.0);
        gtk_container_add(GTK_CONTAINER(note_frame), note_label);
        
        GtkRevealer *revealer = GTK_REVEALER(gtk_revealer_new());
        gtk_revealer_set_transition_type(revealer, GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
        gtk_revealer_set_reveal_child(revealer, TRUE);
        gtk_container_add(GTK_CONTAINER(revealer), note_frame);
        
        gtk_box_pack_start(GTK_BOX(main_box), GTK_WIDGET(revealer), FALSE, FALSE, 0);
    }
    
    return main_box;
}

void update_task_list() {
    GList *children, *iter;
    children = gtk_container_get_children(GTK_CONTAINER(task_grid));
    for (iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
    
    GtkWidget *high_frame = gtk_frame_new("High Priority");
    GtkWidget *medium_frame = gtk_frame_new("Medium Priority");
    GtkWidget *low_frame = gtk_frame_new("Low Priority");
    GtkWidget *daily_frame = gtk_frame_new("Daily Tasks");
    
    GtkWidget *high_list = gtk_list_box_new();
    GtkWidget *medium_list = gtk_list_box_new();
    GtkWidget *low_list = gtk_list_box_new();
    GtkWidget *daily_list = gtk_list_box_new();
    
    gtk_container_add(GTK_CONTAINER(high_frame), high_list);
    gtk_container_add(GTK_CONTAINER(medium_frame), medium_list);
    gtk_container_add(GTK_CONTAINER(low_frame), low_list);
    gtk_container_add(GTK_CONTAINER(daily_frame), daily_list);
    
    gtk_grid_attach(GTK_GRID(task_grid), high_frame, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(task_grid), medium_frame, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(task_grid), low_frame, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(task_grid), daily_frame, 1, 1, 1, 1);
    
    for (int i = 0; i < task_count; i++) {
        GtkWidget *task_widget = create_task_widget(i);
        GtkWidget *list_item = gtk_list_box_row_new();
        gtk_container_add(GTK_CONTAINER(list_item), task_widget);
        
        if (tasks[i].priority == PRIORITY_HIGH) {
            gtk_container_add(GTK_CONTAINER(high_list), list_item);
        } 
        else if (tasks[i].priority == PRIORITY_MEDIUM) {
            gtk_container_add(GTK_CONTAINER(medium_list), list_item);
        }
        else if (tasks[i].priority == PRIORITY_LOW) {
            gtk_container_add(GTK_CONTAINER(low_list), list_item);
        }
        else if (tasks[i].priority == PRIORITY_DAILY) {
            gtk_container_add(GTK_CONTAINER(daily_list), list_item);
        }
    }
    
    gtk_widget_show_all(task_grid);
}

void add_task(GtkEntry *entry, gpointer data) {
    const char *task_text = gtk_entry_get_text(entry);
    
    if (strlen(task_text) == 0) {
        show_error_dialog(NULL, "Task cannot be empty.");
        return;
    }
    
    for (int i = 0; i < task_count; i++) {
        if (strcmp(tasks[i].task, task_text) == 0 && !tasks[i].completed) {
            show_error_dialog(NULL, "This task already exists in your list.");
            return;
        }
    }
    
    if (task_count >= MAX_TASKS) {
        show_error_dialog(NULL, "Maximum number of tasks reached.");
        return;
    }
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Set Priority",
                                                   GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(entry))),
                                                   GTK_DIALOG_MODAL,
                                                   "_Cancel", GTK_RESPONSE_CANCEL,
                                                   "_Daily", PRIORITY_DAILY,
                                                   "_Low", PRIORITY_LOW,
                                                   "_Medium", PRIORITY_MEDIUM,
                                                   "_High", PRIORITY_HIGH,
                                                   NULL);
    
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    if (response == GTK_RESPONSE_CANCEL) {
        return;
    }
    
    Task *new_task = &tasks[task_count];
    strncpy(new_task->task, task_text, TASK_LENGTH - 1);
    new_task->task[TASK_LENGTH - 1] = '\0';
    new_task->completed = FALSE;
    new_task->daily = (response == PRIORITY_DAILY);
    new_task->priority = response;
    new_task->note[0] = '\0';
    new_task->created_at = time(NULL);
    new_task->completed_at = 0;
    new_task->note_visible = FALSE;
    
    task_count++;
    gtk_entry_set_text(entry, "");
    save_tasks();
    update_task_list();
}

void on_window_destroy(GtkWidget *widget, gpointer data) {
    save_tasks();
    sqlite3_close(db);
    gtk_main_quit();
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    // Initialize database
    init_database();
    
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        ".completed-task { background-color: #e0f8e0; opacity: 0.7; } "
        "frame { padding: 10px; margin: 5px; } "
        "frame > label { font-weight: bold; } "
        "list { background: white; } "
        "list row { padding: 5px; margin: 2px; } "
        "note-frame { margin: 5px; padding: 5px; background: #f5f5f5; }", -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                            GTK_STYLE_PROVIDER(provider),
                                            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    load_tasks();
    
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "To-Do List with Notes");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    GtkWidget *menu_bar = gtk_menu_bar_new();
    GtkWidget *file_menu = gtk_menu_new();
    
    GtkWidget *file_item = gtk_menu_item_new_with_label("File");
    GtkWidget *clear_item = gtk_menu_item_new_with_label("Clear Completed");
    GtkWidget *about_item = gtk_menu_item_new_with_label("About");
    GtkWidget *quit_item = gtk_menu_item_new_with_label("Quit");
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), clear_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), about_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), quit_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), file_item);
    
    g_signal_connect(clear_item, "activate", G_CALLBACK(clear_completed_tasks), NULL);
    g_signal_connect(about_item, "activate", G_CALLBACK(show_about_dialog), NULL);
    g_signal_connect(quit_item, "activate", G_CALLBACK(on_window_destroy), NULL);
    
    gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);
    
    task_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(task_entry), "Enter a new task...");
    gtk_box_pack_start(GTK_BOX(vbox), task_entry, FALSE, FALSE, 0);
    g_signal_connect(task_entry, "activate", G_CALLBACK(add_task), NULL);
    
    task_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(task_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(task_grid), 5);
    gtk_grid_set_row_homogeneous(GTK_GRID(task_grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(task_grid), TRUE);
    
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled_window), task_grid);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
    
    update_task_list();
    
    gtk_widget_show_all(window);
    gtk_main();
    
    return 0;
}