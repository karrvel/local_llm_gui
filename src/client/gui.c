#include <gtk/gtk.h>
#include <pango/pango.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>
#include "gui.h"

// Very simple color map for code highlighting
static const char* get_syntax_color(const char* token, const char* language) {
    // Define some keywords for basic languages
    const char* c_keywords[] = {"int", "char", "void", "if", "else", "for", "while", "return", "struct", 
                             "switch", "case", "break", "continue", "default", "do", "extern", "static", "const", NULL};
    const char* py_keywords[] = {"def", "class", "if", "else", "elif", "for", "while", "in", "import", "from", "return", 
                              "try", "except", "finally", "with", "as", "lambda", "yield", "global", "nonlocal", NULL};
    const char* py_builtins[] = {"print", "len", "range", "int", "str", "float", "list", "dict", "tuple", "set", 
                              "True", "False", "None", "input", "open", "min", "max", "sum", "map", "filter", NULL};
    const char* bash_keywords[] = {"if", "then", "else", "elif", "fi", "for", "while", "do", "done", "case", "esac", 
                               "in", "function", "select", "until", "return", "exit", "break", "continue", NULL};
    const char* bash_builtins[] = {"echo", "printf", "read", "cd", "pwd", "pushd", "popd", "export", "source", "unset", 
                               "eval", "exec", "alias", "unalias", "set", "test", "declare", "local", NULL};
    
    // Check Python
    if (strcmp(language, "python") == 0) {
        // Keywords
        for (int i = 0; py_keywords[i] != NULL; i++) {
            if (strcasecmp(token, py_keywords[i]) == 0) {
                return "#569CD6"; // Blue for keywords
            }
        }
        
        // Built-in functions 
        for (int i = 0; py_builtins[i] != NULL; i++) {
            if (strcasecmp(token, py_builtins[i]) == 0) {
                return "#DCDCAA"; // Yellow for builtins
            }
        }
        
        // Check for Python string literals
        if ((token[0] == '\'' && token[strlen(token)-1] == '\'') ||
            (token[0] == '\"' && token[strlen(token)-1] == '\"')) {
            return "#CE9178"; // Brown/orange for strings
        }
        
        // Check for numbers
        if (isdigit(token[0]) || (token[0] == '-' && isdigit(token[1]))) {
            // Simple check for numeric values
            int i, has_non_digit = 0;
            for (i = (token[0] == '-' ? 1 : 0); token[i] != '\0'; i++) {
                if (!isdigit(token[i]) && token[i] != '.') {
                    has_non_digit = 1;
                    break;
                }
            }
            if (!has_non_digit) {
                return "#B5CEA8"; // Green for numbers
            }
        }
    }
    // Check C/C++ keywords
    else if (strcmp(language, "c") == 0 || strcmp(language, "cpp") == 0) {
        // Keywords
        for (int i = 0; c_keywords[i] != NULL; i++) {
            if (strcmp(token, c_keywords[i]) == 0) {
                return "#569CD6"; // Blue for keywords
            }
        }
    }
    // Check Bash/Shell keywords
    else if (strcmp(language, "bash") == 0 || strcmp(language, "sh") == 0 || strcmp(language, "shell") == 0) {
        // Keywords
        for (int i = 0; bash_keywords[i] != NULL; i++) {
            if (strcmp(token, bash_keywords[i]) == 0) {
                return "#569CD6"; // Blue for keywords
            }
        }
        
        // Built-in commands
        for (int i = 0; bash_builtins[i] != NULL; i++) {
            if (strcmp(token, bash_builtins[i]) == 0) {
                return "#DCDCAA"; // Yellow for builtins
            }
        }
        
        // Check for string literals
        if ((token[0] == '\'' && token[strlen(token)-1] == '\'') ||
            (token[0] == '\"' && token[strlen(token)-1] == '\"')) {
            return "#CE9178"; // Brown/orange for strings
        }
        
        // Check for numbers
        if (isdigit(token[0]) || (token[0] == '-' && isdigit(token[1]))) {
            // Simple check for numeric values
            int i, has_non_digit = 0;
            for (i = (token[0] == '-' ? 1 : 0); token[i] != '\0'; i++) {
                if (!isdigit(token[i]) && token[i] != '.') {
                    has_non_digit = 1;
                    break;
                }
            }
            if (!has_non_digit) {
                return "#B5CEA8"; // Green for numbers
            }
        }
    }
    
    return NULL; // No special color
}

// Forward declarations
static void safe_gui_cleanup(void);

// Global GUI state
static gui_t gui;
static gui_config_t current_config;
static volatile sig_atomic_t shutdown_flag = 0;

// Signal handler for Ctrl+C
static void signal_handler(int signum) {
    printf("\nReceived signal %d, shutting down client...\n", signum);
    shutdown_flag = 1;
    
    // Call our safe cleanup function through the GTK main thread
    if (gtk_main_level() > 0) {
        gdk_threads_add_idle((GSourceFunc)safe_gui_cleanup, NULL);
    }
}

static const char *css_template = 
    "window, .main-box { background-color: #343541; }\n"
    "label { color: #ffffff; }\n"
    "entry { background-color: #40414f; color: #ffffff; border-radius: 20px; padding: 12px 45px 12px 15px; border: 1px solid #565869; caret-color: white; }\n"
    "button { background-color: transparent; border: none; }\n"
    "button.send-button { min-width: 36px; min-height: 36px; padding: 0; margin: 0; }\n"
    "button.send-button image { color: #ffffff; }\n"
    ".message-box { padding: 8px; }\n"
    ".user-message { background-color: #343541; margin: 5px 30px 5px 100px; }\n"
    ".assistant-message { background-color: #444654; margin: 5px 100px 5px 30px; }\n"
    ".message-text { color: #ffffff; padding: 12px 16px; margin: 0; }\n"
    ".header-bar { background-color: #343541; border-bottom: 1px solid #565869; padding: 10px; }\n"
    ".input-box { background-color: #343541; padding: 10px; border-top: 1px solid #565869; }\n"
    ".chat-area { background-color: #343541; }\n"
    ".scrolled-window { background-color: #343541; }\n"
    ".message-content { background-color: transparent; }\n"
    ".header-title { font-weight: bold; font-size: 16px; }\n"
    ".user-icon, .assistant-icon { min-width: 30px; min-height: 30px; margin: 5px; }\n"
    ".user-icon { background-color: #5c7aaa; }\n"
    ".assistant-icon { background-color: #10a37f; }\n"
    ".icon-label { color: #ffffff; font-weight: bold; font-size: 14px; }\n";

// Forward declarations for internal functions
static void on_send_button_clicked(GtkWidget *widget, gpointer data);
static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data);
static void update_chat_view(void);
static void apply_css(void);
static GtkWidget* create_message_bubble(const char *text, gboolean is_user);
static char* format_timestamp(time_t timestamp);
static char* detect_language(const char *code);
static char* highlight_code(const char *code, const char *language);

// CSS colors for syntax highlighting
static const char *keyword_color = "#569CD6";
static const char *string_color = "#CE9178";
static const char *comment_color = "#6A9955";
static const char *type_color = "#4EC9B0";
static const char *numeric_color = "#B5CEA8";
static const char *function_color = "#DCDCAA";

/**
 * Detect the programming language from code content
 * This is a simple heuristic-based detection
 */
static char* detect_language(const char *code) {
    if (!code) return NULL;
    
    // Very simple language detection heuristics
    if (strstr(code, "def ") || strstr(code, "import ") || strstr(code, "class ")) {
        return g_strdup("python");
    } else if (strstr(code, "#include") || strstr(code, "int main")) {
        return g_strdup("c");
    } else if (strstr(code, "function ") || strstr(code, "var ") || strstr(code, "const ")) {
        return g_strdup("javascript");
    } else if (strstr(code, "#!/bin/bash") || strstr(code, "echo ") || strstr(code, "if [")) {
        return g_strdup("bash");
    }
    
    return NULL; // Unknown language
}

/**
 * Apply syntax highlighting to code based on language
 * Uses Pango markup for colors
 */
// Forward declaration for the helper function
static char* highlight_python_line(const char *line);

static char* highlight_code(const char *code, const char *language) {
    if (!code || !language) return g_strdup(code);
    
    // Create a result buffer
    GString *result = g_string_new("");
    
    // Escape special characters first
    GString *escaped = g_string_new("");
    for (int i = 0; code[i] != '\0'; i++) {
        if (code[i] == '<') g_string_append(escaped, "&lt;");
        else if (code[i] == '>') g_string_append(escaped, "&gt;");
        else g_string_append_c(escaped, code[i]);
    }
    
    // Split into lines for processing
    char **lines = g_strsplit(escaped->str, "\n", -1);
    g_string_free(escaped, TRUE);
    
    for (int i = 0; lines[i] != NULL; i++) {
        const char *line = lines[i];
        
        // Only handle Python highlighting, all other languages just show plain text
        if (strcmp(language, "python") == 0) {
            const char *comment = strstr(line, "#");
            if (comment) {
                // We have a Python comment - add the prefix normally
                int comment_pos = comment - line;
                g_string_append_len(result, line, comment_pos);
                g_string_append_printf(result, "<span foreground=\"#6A9955\">%s</span>", comment);
            } else {
                // Simple word-by-word coloring for Python
                char *highlighted = highlight_python_line(line);
                g_string_append(result, highlighted);
                g_free(highlighted);
            }
        } 
        else {
            // For all other languages, just use the escaped line
            g_string_append(result, line);
        }
        
        // Add a newline if this isn't the last line
        if (lines[i+1] != NULL) {
            g_string_append_c(result, '\n');
        }
    }
    
    g_strfreev(lines);
    return g_string_free(result, FALSE);
}

// Helper function for Python syntax highlighting
static char* highlight_python_line(const char *line) {
    if (!line) return g_strdup("");
    
    // Python keywords to highlight
    const char *keywords[] = {"def", "class", "if", "else", "elif", "for", "while", "in", "import", "from", "return", NULL};
    const char *builtins[] = {"print", "len", "range", "int", "str", "float", "list", "dict", NULL};
    
    GString *result = g_string_new("");
    char **tokens = g_strsplit(line, " ", -1);
    
    for (int i = 0; tokens[i] != NULL; i++) {
        gboolean highlighted = FALSE;
        
        // Check if this is a keyword
        for (int k = 0; keywords[k] != NULL; k++) {
            if (strcmp(tokens[i], keywords[k]) == 0) {
                g_string_append_printf(result, "<span foreground=\"#569CD6\">%s</span> ", tokens[i]);
                highlighted = TRUE;
                break;
            }
        }
        
        // Check if this is a builtin
        if (!highlighted) {
            for (int b = 0; builtins[b] != NULL; b++) {
                if (strcmp(tokens[i], builtins[b]) == 0) {
                    g_string_append_printf(result, "<span foreground=\"#DCDCAA\">%s</span> ", tokens[i]);
                    highlighted = TRUE;
                    break;
                }
            }
        }
        
        // Check if this is a string
        if (!highlighted && strlen(tokens[i]) >= 2) {
            if ((tokens[i][0] == '\'' && tokens[i][strlen(tokens[i])-1] == '\'') ||
                (tokens[i][0] == '\"' && tokens[i][strlen(tokens[i])-1] == '\"')) {
                g_string_append_printf(result, "<span foreground=\"#CE9178\">%s</span> ", tokens[i]);
                highlighted = TRUE;
            }
        }
        
        // Not a special token, just add it
        if (!highlighted) {
            g_string_append(result, tokens[i]);
            if (tokens[i+1] != NULL) {
                g_string_append_c(result, ' ');
            }
        }
    }
    
    g_strfreev(tokens);
    return g_string_free(result, FALSE);
}

bool gui_initialize(gui_config_t *config) {
    if (config == NULL) {
        fprintf(stderr, "Error: NULL configuration provided\n");
        return false;
    }
    
    // Store configuration
    memcpy(&current_config, config, sizeof(gui_config_t));
    
    // Set up signal handler for graceful termination
    signal(SIGINT, signal_handler);
    
    // Initialize GTK
    gtk_init(NULL, NULL);
    
    // Create main window
    gui.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(gui.window), "Local LLM");
    gtk_window_set_default_size(GTK_WINDOW(gui.window), config->width, config->height);
    gtk_container_set_border_width(GTK_CONTAINER(gui.window), 0);
    g_signal_connect(gui.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    // Create main vertical box for layout
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(gui.window), main_vbox);
    gtk_style_context_add_class(gtk_widget_get_style_context(main_vbox), "main-box");
    
    // Create header bar at the top
    GtkWidget *header_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(header_bar), "header-bar");
    gtk_box_pack_start(GTK_BOX(main_vbox), header_bar, FALSE, FALSE, 0);
    
    // Local LLM title with arrow
    GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *title_label = gtk_label_new("Local LLM");
    gtk_style_context_add_class(gtk_widget_get_style_context(title_label), "header-title");
    gtk_box_pack_start(GTK_BOX(title_box), title_label, FALSE, FALSE, 0);
    
    // Right arrow icon
    GtkWidget *arrow_icon = gtk_image_new_from_icon_name("pan-end-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(title_box), arrow_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_bar), title_box, FALSE, FALSE, 5);
    
    // Create scrolled window for chat messages
    gui.scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(gui.scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(gui.scrolled_window, TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(gui.scrolled_window), "scrolled-window");
    gtk_box_pack_start(GTK_BOX(main_vbox), gui.scrolled_window, TRUE, TRUE, 0);
    
    // Create vertical box container for chat messages
    gui.chat_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(gui.chat_box, GTK_ALIGN_FILL);
    gtk_style_context_add_class(gtk_widget_get_style_context(gui.chat_box), "chat-area");
    
    // Add the chat box to a viewport to enable scrolling
    GtkWidget *viewport = gtk_viewport_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(viewport), gui.chat_box);
    gtk_container_add(GTK_CONTAINER(gui.scrolled_window), viewport);
    
    // Create bottom input area
    GtkWidget *input_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(input_container), "input-box");
    gtk_box_pack_end(GTK_BOX(main_vbox), input_container, FALSE, FALSE, 0);
    
    // Create horizontal box for input area with proper padding
    GtkWidget *input_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(input_hbox), 10);
    gtk_box_pack_start(GTK_BOX(input_container), input_hbox, FALSE, FALSE, 0);
    
    // Create message entry with full width and rounded corners
    gui.message_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(gui.message_entry), "Ask anything");
    gtk_box_pack_start(GTK_BOX(input_hbox), gui.message_entry, TRUE, TRUE, 0);
    g_signal_connect(gui.message_entry, "key-press-event", G_CALLBACK(on_key_press), NULL);
    
    // Create send button with a modern send icon
    gui.send_button = gtk_button_new();
    GtkWidget *send_icon = gtk_image_new_from_icon_name("document-send-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(gui.send_button), send_icon);
    gtk_style_context_add_class(gtk_widget_get_style_context(gui.send_button), "send-button");
    gtk_box_pack_end(GTK_BOX(input_hbox), gui.send_button, FALSE, FALSE, 0);
    g_signal_connect(gui.send_button, "clicked", G_CALLBACK(on_send_button_clicked), NULL);
    
    // Initialize message history
    gui.messages = g_array_new(FALSE, FALSE, sizeof(chat_message_t));
    g_array_set_clear_func(gui.messages, (GDestroyNotify)free);
    
    // Apply CSS styling
    apply_css();
    
    // Show all widgets
    gtk_widget_show_all(gui.window);
    
    return true;
}

// Safe cleanup function to avoid memory errors
static void safe_gui_cleanup(void) {
    static gboolean cleanup_done = FALSE;
    
    // Ensure we only clean up once
    if (cleanup_done) return;
    cleanup_done = TRUE;
    
    // Perform any necessary cleanup here
    printf("Performing safe GUI cleanup...\n");
    
    // Make sure we're in the main thread for GTK operations
    if (gtk_main_level() > 0) {
        gtk_main_quit();
    }
}

void gui_run(void) {
    // Main GTK loop
    gtk_widget_show_all(gui.window);
    
    // Add a cleanup handler for the window destroy event
    g_signal_connect(gui.window, "destroy", G_CALLBACK(safe_gui_cleanup), NULL);
    
    gtk_main();
    
    // If we get here, make sure cleanup has been done
    safe_gui_cleanup();
}

void gui_cleanup(void) {
    // Free message history
    if (gui.messages) {
        for (guint i = 0; i < gui.messages->len; i++) {
            chat_message_t *msg = &g_array_index(gui.messages, chat_message_t, i);
            if (msg->text) {
                free(msg->text);
                msg->text = NULL;
            }
        }
        g_array_free(gui.messages, TRUE);
        gui.messages = NULL;
    }
}

void gui_add_message(const char *text, bool is_user) {
    if (text == NULL || strlen(text) == 0) {
        return;
    }
    
    // Create new message
    chat_message_t msg;
    msg.text = strdup(text);
    msg.is_user = is_user;
    msg.timestamp = time(NULL);
    
    // Add to message history
    g_array_append_val(gui.messages, msg);
    
    // Update chat view
    update_chat_view();
}

void gui_set_send_callback(void (*callback)(const char *message, void *user_data), void *user_data) {
    gui.send_callback = callback;
    gui.user_data = user_data;
}

void gui_show_error(const char *message) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(gui.window),
                                              GTK_DIALOG_DESTROY_WITH_PARENT,
                                              GTK_MESSAGE_ERROR,
                                              GTK_BUTTONS_CLOSE,
                                              "Error: %s", message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void gui_show_info(const char *message) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(gui.window),
                                              GTK_DIALOG_DESTROY_WITH_PARENT,
                                              GTK_MESSAGE_INFO,
                                              GTK_BUTTONS_CLOSE,
                                              "%s", message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// Internal functions

static void on_send_button_clicked(GtkWidget *widget, gpointer data) {
    const char *text = gtk_entry_get_text(GTK_ENTRY(gui.message_entry));
    
    if (text != NULL && strlen(text) > 0) {
        // Add message to chat
        gui_add_message(text, true);
        
        // Call send callback if set
        if (gui.send_callback) {
            gui.send_callback(text, gui.user_data);
        }
        
        // Clear entry
        gtk_entry_set_text(GTK_ENTRY(gui.message_entry), "");
    }
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    if (event->keyval == GDK_KEY_Return) {
        on_send_button_clicked(NULL, NULL);
        return TRUE;
    }
    return FALSE;
}

static void apply_css() {
    // Apply CSS
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css_template, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                             GTK_STYLE_PROVIDER(provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static char* format_timestamp(time_t timestamp) {
    struct tm *timeinfo = localtime(&timestamp);
    static char buffer[20];
    strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);
    return buffer;
}

// Simple Markdown to Pango markup conversion
static char* markdown_to_pango(const char *text) {
    if (!text) return NULL;
    
    // Escape special characters to avoid breaking Pango markup
    GString *escaped = g_string_new("");
    const char *p = text;
    while (*p) {
        if (*p == '<') {
            g_string_append(escaped, "&lt;");
        } else if (*p == '>') {
            g_string_append(escaped, "&gt;");
        } else if (*p == '&') {
            g_string_append(escaped, "&amp;");
        } else if (*p == '\\' && *(p+1) != '\0') {
            // Handle escape sequences
            switch (*(p+1)) {
                case 'n': 
                    g_string_append(escaped, "\n");
                    p++; // Skip the 'n'
                    break;
                case 't':
                    g_string_append(escaped, "    "); // 4-space tab
                    p++; // Skip the 't'
                    break;
                case 'r':
                    // Ignore carriage returns
                    p++; // Skip the 'r'
                    break;
                case '\\':
                    g_string_append_c(escaped, '\\');
                    p++; // Skip the second backslash
                    break;
                case 'u':
                    // Handle Unicode escape sequences like \u2713
                    if (isxdigit(*(p+2)) && isxdigit(*(p+3)) && isxdigit(*(p+4)) && isxdigit(*(p+5))) {
                        // Extract 4 hex digits
                        char hex[5] = {*(p+2), *(p+3), *(p+4), *(p+5), '\0'};
                        // Convert hex to integer
                        gunichar unicode_char = (gunichar)strtol(hex, NULL, 16);
                        // Append the actual Unicode character
                        g_string_append_unichar(escaped, unicode_char);
                        p += 5; // Skip 'u' and the 4 hex digits
                    } else {
                        // Not a valid Unicode escape, just add \u
                        g_string_append(escaped, "\\u");
                        p++; // Skip the 'u'
                    }
                    break;
                default:
                    // Just add the backslash
                    g_string_append_c(escaped, *p);
            }
        } else {
            g_string_append_c(escaped, *p);
        }
        p++;
    }
    
    // Create result string for Pango markup parsing
    GString *result = g_string_new("");
    
    // Improve the code block handling with syntax highlighting
    gboolean in_code_block = FALSE;
    GString *processed_text = g_string_new("");
    char *current_language = NULL;
    GString *code_content = NULL;
    
    // First pass: handle code blocks and extract content for syntax highlighting
    char **lines = g_strsplit(escaped->str, "\n", -1);
    for (int i = 0; lines[i] != NULL; i++) {
        // Check for code block delimiters (```)
        if (lines[i][0] == '`' && lines[i][1] == '`' && lines[i][2] == '`') {
            if (!in_code_block) {
                // Start of code block - check for language specifier
                in_code_block = TRUE;
                code_content = g_string_new("");
                
                // Check if there's a language specified after the backticks
                char *lang_start = lines[i] + 3;
                if (strlen(lang_start) > 0) {
                    current_language = g_strdup(g_strstrip(lang_start));
                } else {
                    // No language specified
                    current_language = NULL;
                }
                
                // Don't add the delimiter line to the output
            } else {
                // End of code block - apply syntax highlighting
                in_code_block = FALSE;
                
                // If we have content to highlight
                if (code_content && code_content->len > 0) {
                    g_string_append(processed_text, "\n");
                    
                    // Apply highlighting based on detected language
                    if (!current_language) {
                        // Try to auto-detect language from the code content
                        current_language = detect_language(code_content->str);
                    }
                    
                    // Add a basic monospace code block with a dark background
                    g_string_append(processed_text, "<span background=\"#1E1E1E\" foreground=\"#FFFFFF\"><tt>");
                    
                    // If language is detected, show it
                    if (current_language && strlen(current_language) > 0) {
                        g_string_append_printf(processed_text, "<span style=\"italic\" foreground=\"#888888\">Language: %s</span>\n", current_language);
                    }
                    
                    // Apply minimal processing to escape angle brackets
                    char *processed_code = highlight_code(code_content->str, current_language ? current_language : "text");
                    if (processed_code) {
                        g_string_append(processed_text, processed_code);
                        g_free(processed_code);
                    } else {
                        // Fallback in case of error
                        g_string_append(processed_text, code_content->str);
                    }
                    
                    g_string_append(processed_text, "</tt></span>\n");
                    
                    if (current_language) {
                        g_free(current_language);
                        current_language = NULL;
                    }
                }
                
                if (code_content) {
                    g_string_free(code_content, TRUE);
                    code_content = NULL;
                }
                
                // Don't add the delimiter line to the output
            }
        } else {
            // Add the line with proper handling based on whether we're in a code block
            if (in_code_block && code_content) {
                // Inside code block - collect for syntax highlighting
                g_string_append(code_content, lines[i]);
                if (lines[i+1] != NULL) {
                    g_string_append(code_content, "\n");
                }
            } else {
                // Outside code block - process normally
                g_string_append(processed_text, lines[i]);
                if (lines[i+1] != NULL) {
                    g_string_append(processed_text, "\n");
                }
            }
        }
    }
    g_strfreev(lines);
    
    // Now process the text with code blocks handled
    lines = g_strsplit(processed_text->str, "\n", -1);
    g_string_free(processed_text, TRUE);
    
    // Process each line
    for (int i = 0; lines[i] != NULL; i++) {
        char *line = lines[i];
        gboolean is_header = FALSE;
        int header_level = 0;
        
        // Skip empty lines
        if (strlen(line) == 0) {
            g_string_append(result, "\n");
            continue;
        }
        
        // Check for headers (# Header)
        if (line[0] == '#') {
            header_level = 1;
            while (line[header_level] == '#' && header_level < 6) {
                header_level++;
            }
            if (line[header_level] == ' ') {
                is_header = TRUE;
                g_string_append_printf(result, "<span weight=\"bold\" size=\"large\">%s</span>", line + header_level + 1);
            } else {
                is_header = FALSE;
            }
        } 
        // Check for blockquotes (> text)
        else if (line[0] == '>') {
            g_string_append(result, "<span background=\"#444444\" style=\"italic\">");
            g_string_append(result, line + 1);  // Skip the '>' character
            g_string_append(result, "</span>");
            is_header = TRUE;  // Mark as processed
        }
        // Check for unordered lists
        else if (line[0] == '-' && line[1] == ' ') {
            g_string_append(result, "• ");
            g_string_append(result, line + 2);  // Skip the '- ' characters
            is_header = TRUE;  // Mark as processed
        }
        // Check for horizontal rules (===== or -----)
        else if ((line[0] == '=' && strspn(line, "=") == strlen(line) && strlen(line) >= 3) ||
                 (line[0] == '-' && strspn(line, "-") == strlen(line) && strlen(line) >= 3)) {
            g_string_append(result, "<span foreground=\"#666666\">\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015\u2015</span>");
            is_header = TRUE;  // Mark as processed
        }
        
        if (!is_header) {
            GString *temp = g_string_new("");
            
            // Process inline markdown elements
            const char *pos = line;
            while (*pos) {
                // Combined bold and italic (***text***)
                if (pos[0] == '*' && pos[1] == '*' && pos[2] == '*' && pos[3] != ' ') {
                    const char *end = strstr(pos + 3, "***");
                    if (end) {
                        g_string_append(temp, "<b><i>");
                        g_string_append_len(temp, pos + 3, end - (pos + 3));
                        g_string_append(temp, "</i></b>");
                        pos = end + 3;
                        continue;
                    }
                }
                
                // Combined bold and italic (___text___)
                if (pos[0] == '_' && pos[1] == '_' && pos[2] == '_') {
                    const char *end = strstr(pos + 3, "___");
                    if (end) {
                        g_string_append(temp, "<b><i>");
                        g_string_append_len(temp, pos + 3, end - (pos + 3));
                        g_string_append(temp, "</i></b>");
                        pos = end + 3;
                        continue;
                    }
                }
                
                // Bold with asterisks (**text**)
                if (pos[0] == '*' && pos[1] == '*' && pos[2] != '*' && pos[2] != ' ') {
                    const char *end = strstr(pos + 2, "**");
                    if (end) {
                        g_string_append(temp, "<b>");
                        g_string_append_len(temp, pos + 2, end - (pos + 2));
                        g_string_append(temp, "</b>");
                        pos = end + 2;
                        continue;
                    }
                }
                
                // Bold with underscores (__text__)
                if (pos[0] == '_' && pos[1] == '_' && pos[2] != '_') {
                    const char *end = strstr(pos + 2, "__");
                    if (end) {
                        g_string_append(temp, "<b>");
                        g_string_append_len(temp, pos + 2, end - (pos + 2));
                        g_string_append(temp, "</b>");
                        pos = end + 2;
                        continue;
                    }
                }
                
                // Italic with asterisks (*text*)
                if (pos[0] == '*' && pos[1] != '*' && pos[1] != ' ') {
                    const char *end = strchr(pos + 1, '*');
                    if (end && end != pos + 1) { // Ensure it's not an empty italic tag
                        g_string_append(temp, "<i>");
                        g_string_append_len(temp, pos + 1, end - (pos + 1));
                        g_string_append(temp, "</i>");
                        pos = end + 1;
                        continue;
                    }
                }
                
                // Italic with underscores (_text_)
                if (pos[0] == '_' && pos[1] != '_' && pos[1] != ' ') {
                    const char *end = strchr(pos + 1, '_');
                    if (end && end != pos + 1) { // Ensure it's not an empty italic tag
                        g_string_append(temp, "<i>");
                        g_string_append_len(temp, pos + 1, end - (pos + 1));
                        g_string_append(temp, "</i>");
                        pos = end + 1;
                        continue;
                    }
                }
                
                // Strikethrough (~~text~~)
                if (pos[0] == '~' && pos[1] == '~') {
                    const char *end = strstr(pos + 2, "~~");
                    if (end) {
                        g_string_append(temp, "<s>");
                        g_string_append_len(temp, pos + 2, end - (pos + 2));
                        g_string_append(temp, "</s>");
                        pos = end + 2;
                        continue;
                    }
                }
                
                // Inline code (`code`)
                if (pos[0] == '`') {
                    const char *end = strchr(pos + 1, '`');
                    if (end) {
                        g_string_append(temp, "<tt>");
                        g_string_append_len(temp, pos + 1, end - (pos + 1));
                        g_string_append(temp, "</tt>");
                        pos = end + 1;
                        continue;
                    }
                }
                
                // Links ([text](url)) - use span with color instead of <a> tags
                if (pos[0] == '[') {
                    const char *text_end = strchr(pos + 1, ']');
                    if (text_end && text_end[1] == '(' && strchr(text_end + 2, ')')) {
                        const char *url_start = text_end + 2;
                        const char *url_end = strchr(url_start, ')');
                        
                        // Just style it as blue underlined text
                        g_string_append(temp, "<span foreground=\"blue\" underline=\"single\">");
                        g_string_append_len(temp, pos + 1, text_end - (pos + 1));
                        g_string_append(temp, "</span>");
                        
                        pos = url_end + 1;
                        continue;
                    }
                }
                
                // Regular character
                g_string_append_c(temp, *pos);
                pos++;
            }
            
            g_string_append(result, temp->str);
            g_string_free(temp, TRUE);
        }
        
        // Add newline except for the last line
        if (lines[i + 1] != NULL) {
            g_string_append(result, "\n");
        }
    }
    
    g_strfreev(lines);
    g_string_free(escaped, TRUE);
    
    char *markup = g_string_free(result, FALSE);
    return markup;
}



static GtkWidget* create_message_bubble(const char *text, gboolean is_user) {
    // Create a container for the entire message row
    GtkWidget *message_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    
    // Create the message container with proper styling based on sender
    GtkWidget *message_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(message_box, TRUE);
    
    // Create container for the icon
    GtkWidget *icon_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *icon_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(icon_box, 30, 30);
    
    // Set proper border radius in code instead of CSS to avoid GTK warnings
    GtkCssProvider *provider = gtk_css_provider_new();
    
    if (is_user) {
        // User message styling - right aligned, with user icon
        gtk_widget_set_halign(message_box, GTK_ALIGN_END);
        gtk_style_context_add_class(gtk_widget_get_style_context(message_box), "user-message");
        gtk_style_context_add_class(gtk_widget_get_style_context(icon_box), "user-icon");
        
        // Custom CSS for user message bubble shape
        gtk_css_provider_load_from_data(provider, 
            ".user-message { border-radius: 18px 0px 18px 18px; }", -1, NULL);
        gtk_style_context_add_provider(gtk_widget_get_style_context(message_box),
                                     GTK_STYLE_PROVIDER(provider),
                                     GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
                                     
        // Round the icon box
        GtkCssProvider *icon_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(icon_provider, 
            ".user-icon { border-radius: 6px; }", -1, NULL);
        gtk_style_context_add_provider(gtk_widget_get_style_context(icon_box),
                                    GTK_STYLE_PROVIDER(icon_provider),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(icon_provider);
        
        // Put icon at the right side
        gtk_box_pack_end(GTK_BOX(message_row), icon_container, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(icon_container), icon_box, FALSE, FALSE, 0);
        
        // Center the icon 'U' vertically
        GtkWidget *icon_label = gtk_label_new("U");
        gtk_style_context_add_class(gtk_widget_get_style_context(icon_label), "icon-label");
        gtk_container_add(GTK_CONTAINER(icon_box), icon_label);
    } else {
        // Assistant message styling - left aligned, with assistant icon
        gtk_widget_set_halign(message_box, GTK_ALIGN_START);
        gtk_style_context_add_class(gtk_widget_get_style_context(message_box), "assistant-message");
        gtk_style_context_add_class(gtk_widget_get_style_context(icon_box), "assistant-icon");
        
        // Custom CSS for assistant message bubble shape
        gtk_css_provider_load_from_data(provider, 
            ".assistant-message { border-radius: 0px 18px 18px 18px; }", -1, NULL);
        gtk_style_context_add_provider(gtk_widget_get_style_context(message_box),
                                     GTK_STYLE_PROVIDER(provider),
                                     GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
                                     
        // Round the icon box
        GtkCssProvider *icon_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(icon_provider, 
            ".assistant-icon { border-radius: 6px; }", -1, NULL);
        gtk_style_context_add_provider(gtk_widget_get_style_context(icon_box),
                                    GTK_STYLE_PROVIDER(icon_provider),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(icon_provider);
        
        // Put icon at the left side
        gtk_box_pack_start(GTK_BOX(message_row), icon_container, FALSE, FALSE, 5);
        gtk_box_pack_start(GTK_BOX(icon_container), icon_box, FALSE, FALSE, 0);
        
        // Center the icon 'A' vertically
        GtkWidget *icon_label = gtk_label_new("A");
        gtk_style_context_add_class(gtk_widget_get_style_context(icon_label), "icon-label");
        gtk_container_add(GTK_CONTAINER(icon_box), icon_label);
    }
    
    // Clean up the provider
    g_object_unref(provider);
    
    // Add the main message container to the row
    if (is_user) {
        gtk_box_pack_end(GTK_BOX(message_row), message_box, TRUE, TRUE, 0);
    } else {
        gtk_box_pack_start(GTK_BOX(message_row), message_box, TRUE, TRUE, 0);
    }
    
    // Convert Markdown to Pango markup
    char *markup_text = markdown_to_pango(text);
    
    // Create and style the message text with Markdown support
    GtkWidget *message_label = gtk_label_new(NULL);
    
    // Make text selectable
    gtk_label_set_selectable(GTK_LABEL(message_label), TRUE);
    
    // Enable clickable links
    gtk_label_set_track_visited_links(GTK_LABEL(message_label), TRUE);
    g_signal_connect(message_label, "activate-link", G_CALLBACK(gtk_show_uri_on_window), NULL);
    
    // Safely set markup and handle errors
    GError *error = NULL;
    if (!pango_parse_markup(markup_text, -1, 0, NULL, NULL, NULL, &error)) {
        // If there's a markup error, fall back to plain text
        g_warning("Markup parsing error: %s\nFalling back to plain text", error->message);
        g_error_free(error);
        gtk_label_set_text(GTK_LABEL(message_label), text);
    } else {
        // Markup is valid, set it
        gtk_label_set_markup(GTK_LABEL(message_label), markup_text);
    }
    
    gtk_label_set_line_wrap(GTK_LABEL(message_label), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(message_label), PANGO_WRAP_WORD_CHAR);
    gtk_widget_set_halign(message_label, GTK_ALIGN_FILL);
    gtk_widget_set_size_request(message_label, 100, -1); // Minimum width, helps with wrapping
    gtk_style_context_add_class(gtk_widget_get_style_context(message_label), "message-text");
    gtk_container_add(GTK_CONTAINER(message_box), message_label);
    
    // Free the markup text
    g_free(markup_text);
    
    gtk_widget_show_all(message_row);
    return message_row;
}

static void update_chat_view() {
    // Remove all existing messages from the chat box
    GList *children = gtk_container_get_children(GTK_CONTAINER(gui.chat_box));
    for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
    
    // Add a small spacing widget at the top for better appearance
    GtkWidget *top_spacing = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(top_spacing, -1, 10);
    gtk_box_pack_start(GTK_BOX(gui.chat_box), top_spacing, FALSE, FALSE, 0);
    
    // Add all messages to the chat box
    for (guint i = 0; i < gui.messages->len; i++) {
        chat_message_t *msg = &g_array_index(gui.messages, chat_message_t, i);
        
        // Create a message bubble and add it to the chat box
        GtkWidget *bubble = create_message_bubble(msg->text, msg->is_user);
        gtk_box_pack_start(GTK_BOX(gui.chat_box), bubble, FALSE, FALSE, 5);
    }
    
    // Add a small spacing widget at the bottom for better appearance
    GtkWidget *bottom_spacing = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(bottom_spacing, -1, 10);
    gtk_box_pack_start(GTK_BOX(gui.chat_box), bottom_spacing, FALSE, FALSE, 0);
    
    // Scroll to the bottom to show the latest messages
    GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(gui.scrolled_window));
    gtk_adjustment_set_value(adj, gtk_adjustment_get_upper(adj));
    
        // Note: Already scrolled to bottom above
}
