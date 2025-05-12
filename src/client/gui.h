#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>
#include <stdbool.h>
#include <time.h>

// Message structure
typedef struct {
    char *text;
    bool is_user;
    time_t timestamp;
} chat_message_t;

// GUI configuration
typedef struct {
    char window_title[64];
    int width;
    int height;
    bool dark_mode;
    char font_family[32];
    int font_size;
} gui_config_t;

// GUI components
typedef struct {
    GtkWidget *window;
    GtkWidget *chat_box;        // Container for message bubbles
    GtkWidget *message_entry;
    GtkWidget *send_button;
    GtkWidget *scrolled_window;
    
    // Message history
    GArray *messages;
    
    // Callback for sending messages
    void (*send_callback)(const char *message, void *user_data);
    void *user_data;
} gui_t;

// GUI functions
bool gui_initialize(gui_config_t *config);
void gui_run(void);
void gui_cleanup(void);

// Message handling
void gui_add_message(const char *text, bool is_user);
void gui_set_send_callback(void (*callback)(const char *message, void *user_data), void *user_data);

// Utility functions
void gui_show_error(const char *message);
void gui_show_info(const char *message);

#endif /* GUI_H */
