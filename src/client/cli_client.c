#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include "../common/socket_utils.h"
#include "../common/config.h"

// Global variables
static int server_socket = -1;
static pthread_t receive_thread;
static volatile bool running = false;
static pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
static void* receive_messages(void *arg);
static void cleanup(void);
static char* get_timestamp(void);

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
    
    printf("Connected to LLM Chat Server. Type your message and press Enter. Type 'exit' to quit.\n");
    
    // Start receive thread
    running = true;
    if (pthread_create(&receive_thread, NULL, receive_messages, NULL) != 0) {
        fprintf(stderr, "Failed to create receive thread\n");
        close(server_socket);
        return 1;
    }
    
    // Main input loop
    char input[BUFFER_SIZE];
    while (running) {
        printf("You: ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // Remove trailing newline
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
        }
        
        // Check for exit command
        if (strcmp(input, "exit") == 0) {
            break;
        }
        
        // Send message to server
        pthread_mutex_lock(&socket_mutex);
        if (server_socket >= 0 && running) {
            if (send_message(server_socket, input) < 0) {
                fprintf(stderr, "Failed to send message\n");
            }
        } else {
            fprintf(stderr, "Not connected to server\n");
        }
        pthread_mutex_unlock(&socket_mutex);
    }
    
    // Clean up
    cleanup();
    
    return 0;
}

static void* receive_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    int consecutive_errors = 0;
    const int max_consecutive_errors = 5;
    
    while (running) {
        pthread_mutex_lock(&socket_mutex);
        int local_socket = server_socket; // Get a local copy of the socket
        pthread_mutex_unlock(&socket_mutex);
        
        if (local_socket < 0) {
            running = false;
            break;
        }
        
        pthread_mutex_lock(&socket_mutex);
        int bytes_received = receive_message(local_socket, buffer, BUFFER_SIZE);
        pthread_mutex_unlock(&socket_mutex);
        
        if (bytes_received < 0) {
            consecutive_errors++;
            if (consecutive_errors >= max_consecutive_errors && running) {
                // Too many consecutive errors, assume connection is lost
                fprintf(stderr, "\nConnection to server lost after multiple errors\n");
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
            
            // Print received message with timestamp
            char *timestamp = get_timestamp();
            printf("\nLLM (%s): %s\n", timestamp, buffer);
            free(timestamp);
            printf("You: ");
            fflush(stdout);
        }
    }
    
    return NULL;
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
}

static char* get_timestamp(void) {
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    
    char *timestamp = malloc(20);
    if (timestamp) {
        strftime(timestamp, 20, "%H:%M:%S", timeinfo);
    } else {
        timestamp = strdup("??:??:??");
    }
    
    return timestamp;
}
