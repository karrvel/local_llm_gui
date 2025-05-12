#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include "../common/socket_utils.h"
#include "../common/config.h"
#include "gui.h"

// Global variables
static int server_socket = -1;
static pthread_t receive_thread;
static volatile bool running = false;
static pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
static void* receive_messages(void *arg);
static void send_message_callback(const char *message, void *user_data);
static void cleanup(void);

// Signal handler for graceful shutdown
static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\nReceived signal %d, shutting down client...\n", sig);
        cleanup();
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    // Load configuration
    config_t app_config;
    const char *config_file = "config.json";
    
    // Check if a config file was specified
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_file = argv[i + 1];
            break;
        }
    }
    
    // Load configuration from file
    if (!config_load(config_file, &app_config)) {
        // If loading failed, use defaults
        config_set_defaults(&app_config);
        printf("Using default configuration\n");
    } else {
        printf("Loaded configuration from %s\n", config_file);
    }
    
    // Override with command line arguments
    if (!config_parse_args(argc, argv, &app_config)) {
        printf("Usage: %s [options]\n", argv[0]);
        printf("Options:\n");
        printf("  --config FILE           Configuration file (default: config.json)\n");
        printf("  --host HOST             Server host (default: %s)\n", app_config.server_host);
        printf("  --port PORT             Server port (default: %d)\n", app_config.server_port);
        printf("  --light-mode            Use light mode instead of dark mode\n");
        printf("  --font FAMILY           Font family (default: %s)\n", app_config.font_family);
        printf("  --font-size SIZE        Font size (default: %d)\n", app_config.font_size);
        printf("  --width WIDTH           Window width (default: %d)\n", app_config.window_width);
        printf("  --height HEIGHT         Window height (default: %d)\n", app_config.window_height);
        printf("  --help                  Show this help message\n");
        return 0;
    }
    
    // Print configuration if verbose
    if (app_config.verbose) {
        config_print(&app_config);
    }
    
    // Set up signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Connect to server
    printf("Connecting to server at %s:%d...\n", app_config.server_host, app_config.server_port);
    server_socket = connect_to_server(app_config.server_host, app_config.server_port);
    if (server_socket < 0) {
        fprintf(stderr, "Failed to connect to server\n");
        return 1;
    }
    
    // Initialize GUI
    gui_config_t gui_config = {
        .window_title = "LLM Chat Client",
        .width = app_config.window_width,
        .height = app_config.window_height,
        .dark_mode = app_config.dark_mode,
        .font_family = "",
        .font_size = app_config.font_size
    };
    
    // Copy font family
    strncpy(gui_config.font_family, app_config.font_family, sizeof(gui_config.font_family) - 1);
    
    if (!gui_initialize(&gui_config)) {
        fprintf(stderr, "Failed to initialize GUI\n");
        close(server_socket);
        return 1;
    }
    
    // Set message callback
    gui_set_send_callback(send_message_callback, NULL);
    
    // Start receive thread
    running = true;
    if (pthread_create(&receive_thread, NULL, receive_messages, NULL) != 0) {
        fprintf(stderr, "Failed to create receive thread\n");
        close(server_socket);
        return 1;
    }
    
    // Run GUI main loop
    gui_run();
    
    // Clean up
    cleanup();
    
    return 0;
}

static void* receive_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    int consecutive_errors = 0;
    const int max_consecutive_errors = 5;
    
    while (running) {
        // Clear buffer before each receive
        memset(buffer, 0, BUFFER_SIZE);
        
        // Use a local copy of the socket to avoid race conditions
        int local_socket;
        pthread_mutex_lock(&socket_mutex);
        local_socket = server_socket;
        pthread_mutex_unlock(&socket_mutex);
        
        // Check if socket is valid
        if (local_socket < 0) {
            running = false;
            break;
        }
        
        // Receive message with mutex protection only during the actual receive call
        pthread_mutex_lock(&socket_mutex);
        int bytes_received = receive_message(local_socket, buffer, BUFFER_SIZE);
        pthread_mutex_unlock(&socket_mutex);
        
        if (bytes_received < 0) {
            consecutive_errors++;
            if (consecutive_errors >= max_consecutive_errors && running) {
                // Too many consecutive errors, assume connection is lost
                gui_show_error("Connection to server lost after multiple errors");
                running = false;
                break;
            }
            // Small sleep to prevent CPU hogging on errors
            usleep(100000); // 100ms
            continue;
        } else if (bytes_received == 0) {
            // No data available, just a timeout, which is normal
            // Reset error counter if we're not getting errors anymore
            if (consecutive_errors > 0) consecutive_errors--;
            usleep(50000); // 50ms sleep to prevent CPU hogging
            continue;
        } else {
            // Reset error counter on successful receive
            consecutive_errors = 0;
            
            // No need to create a separate copy since gui_add_message already creates one
            // Just pass the buffer directly to avoid double memory allocation
            gui_add_message(buffer, false);
        }
    }
    
    return NULL;
}

static void send_message_callback(const char *message, void *user_data) {
    pthread_mutex_lock(&socket_mutex);
    if (server_socket >= 0 && running) {
        if (send_message(server_socket, message) < 0) {
            gui_show_error("Failed to send message");
        }
    } else {
        gui_show_error("Not connected to server");
    }
    pthread_mutex_unlock(&socket_mutex);
}

static void cleanup(void) {
    running = false;
    
    // Close socket
    if (server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }
    
    // Wait for receive thread to finish
    if (pthread_join(receive_thread, NULL) != 0) {
        fprintf(stderr, "Failed to join receive thread\n");
    }
    
    // Clean up GUI
    gui_cleanup();
}
