#include "server.h"
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include "../common/config.h"

// Global variables
static int server_socket = -1;
static bool running = false;
static server_config_t current_config;
static client_connection_t *clients = NULL;
static int client_count = 0;
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Signal handler for graceful shutdown
static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\nReceived signal %d, shutting down server...\n", sig);
        server_stop();
    }
}

bool server_initialize(server_config_t *config) {
    if (config == NULL) {
        fprintf(stderr, "Error: NULL configuration provided\n");
        return false;
    }
    
    // Store configuration
    memcpy(&current_config, config, sizeof(server_config_t));
    
    // Initialize LLM
    if (!llm_initialize(&config->llm_config)) {
        fprintf(stderr, "Failed to initialize LLM\n");
        return false;
    }
    
    // Allocate client connection array
    clients = calloc(config->max_connections, sizeof(client_connection_t));
    if (clients == NULL) {
        fprintf(stderr, "Failed to allocate memory for client connections\n");
        llm_cleanup();
        return false;
    }
    
    // Set up signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    printf("Server initialized with port %d and max %d connections\n", 
           config->port, config->max_connections);
    return true;
}

bool server_start(void) {
    if (running) {
        fprintf(stderr, "Server is already running\n");
        return false;
    }
    
    // Create server socket
    server_socket = create_server_socket(current_config.port);
    if (server_socket < 0) {
        fprintf(stderr, "Failed to create server socket\n");
        return false;
    }
    
    running = true;
    printf("Server started on port %d\n", current_config.port);
    
    // Main server loop
    while (running) {
        int client_socket = accept_client_connection(server_socket);
        if (client_socket < 0) {
            if (running) {
                fprintf(stderr, "Failed to accept client connection\n");
            }
            continue;
        }
        
        // Find an available slot for the new client
        pthread_mutex_lock(&clients_mutex);
        int slot = -1;
        for (int i = 0; i < current_config.max_connections; i++) {
            if (!clients[i].active) {
                slot = i;
                break;
            }
        }
        
        if (slot == -1) {
            pthread_mutex_unlock(&clients_mutex);
            fprintf(stderr, "Maximum number of clients reached\n");
            close(client_socket);
            continue;
        }
        
        // Create a thread to handle the client
        clients[slot].client_socket = client_socket;
        clients[slot].active = true;
        client_count++;
        
        if (pthread_create(&clients[slot].thread_id, NULL, handle_client, &clients[slot]) != 0) {
            fprintf(stderr, "Failed to create thread for client\n");
            clients[slot].active = false;
            client_count--;
            close(client_socket);
            pthread_mutex_unlock(&clients_mutex);
            continue;
        }
        
        // Detach the thread so it can clean up itself
        pthread_detach(clients[slot].thread_id);
        pthread_mutex_unlock(&clients_mutex);
        
        printf("Client connected. Active clients: %d\n", client_count);
    }
    
    return true;
}

void server_stop(void) {
    if (!running) {
        return;
    }
    
    running = false;
    printf("Stopping server...\n");
    
    // Close server socket to stop accept() from blocking
    if (server_socket >= 0) {
        close(server_socket);
        server_socket = -1;
    }
    
    // Wait for all client threads to finish
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < current_config.max_connections; i++) {
        if (clients[i].active) {
            close(clients[i].client_socket);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    
    // Give threads a moment to clean up
    sleep(1);
    
    // Clean up resources
    free(clients);
    clients = NULL;
    client_count = 0;
    
    // Clean up LLM
    llm_cleanup();
    
    printf("Server stopped\n");
}

bool server_is_running(void) {
    return running;
}

void* handle_client(void *arg) {
    client_connection_t *client = (client_connection_t *)arg;
    int client_socket = client->client_socket;
    char buffer[BUFFER_SIZE];
    int consecutive_timeouts = 0;
    const int max_consecutive_timeouts = 10; // Allow up to 10 consecutive timeouts
    
    // Send welcome message
    const char *welcome_msg = "Connected to LLM Chat Server. Type your message and press Enter.";
    if (send_message(client_socket, welcome_msg) < 0) {
        printf("Failed to send welcome message to client\n");
        goto cleanup;
    }
    
    // Handle client messages
    while (running && client->active) {
        int bytes_received = receive_message(client_socket, buffer, BUFFER_SIZE);
        
        if (bytes_received < 0) {
            // Real error occurred
            if (current_config.verbose) {
                printf("Error receiving from client, closing connection\n");
            }
            break;
        } else if (bytes_received == 0) {
            // Just a timeout, not a real error in our improved receive function
            consecutive_timeouts++;
            if (consecutive_timeouts > max_consecutive_timeouts) {
                // Only print this message once in a while to avoid log spam
                if (consecutive_timeouts % 100 == 0 && current_config.verbose) {
                    printf("Client idle for extended period (%d timeouts)\n", consecutive_timeouts);
                }
            }
            
            // Don't break the connection on timeouts, just continue waiting
            usleep(50000); // 50ms wait to prevent CPU hogging
            continue;
        }
        
        // Reset timeout counter since we got a real message
        consecutive_timeouts = 0;
        
        if (current_config.verbose) {
            printf("Received from client: %s\n", buffer);
        }
        
        if (current_config.verbose) {
            printf("Generating LLM response for: '%s'\n", buffer);
        }
        
        // Generate response using LLM
        char *response = llm_generate_response(buffer);
        
        if (current_config.verbose) {
            if (response) {
                printf("LLM response generated (first 50 chars): %.50s%s\n", 
                       response, strlen(response) > 50 ? "..." : "");
            } else {
                printf("Failed to generate LLM response\n");
            }
        }
        
        // Send response back to client
        if (response) {
            if (current_config.verbose) {
                printf("Sending response to client...\n");
            }
            if (send_message(client_socket, response) < 0) {
                printf("Failed to send response to client\n");
                free(response);
                break;
            }
            free(response);
        } else {
            if (send_message(client_socket, "Error: Failed to generate response") < 0) {
                printf("Failed to send error message to client\n");
                break;
            }
        }
    }
    
cleanup:
    // Clean up
    close(client_socket);
    
    pthread_mutex_lock(&clients_mutex);
    client->active = false;
    client_count--;
    pthread_mutex_unlock(&clients_mutex);
    
    printf("Client disconnected. Active clients: %d\n", client_count);
    return NULL;
}

// Main function to demonstrate server usage
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
        printf("  --model TYPE            Model type (llama, mistral, gptj, custom)\n");
        printf("  --model-path PATH       Path to model file\n");
        printf("  --temperature VALUE     Temperature for generation (default: %.1f)\n", app_config.temperature);
        printf("  --max-tokens VALUE      Maximum tokens to generate (default: %d)\n", app_config.max_tokens);
        printf("  --context-size VALUE    Context size for LLM (default: %d)\n", app_config.context_size);
        printf("  --max-connections VALUE Maximum client connections (default: %d)\n", app_config.max_connections);
        printf("  --verbose               Enable verbose output\n");
        printf("  --help                  Show this help message\n");
        return 0;
    }
    
    // Print configuration
    if (app_config.verbose) {
        config_print(&app_config);
    }
    
    // Convert to server_config_t
    server_config_t server_config = {
        .port = app_config.server_port,
        .llm_config = {
            .type = app_config.llm_type,
            .model_path = "",
            .context_size = app_config.context_size,
            .temperature = app_config.temperature,
            .max_tokens = app_config.max_tokens,
            .verbose = app_config.verbose
        },
        .verbose = app_config.verbose,
        .max_connections = app_config.max_connections
    };
    
    // Copy model path
    strncpy(server_config.llm_config.model_path, app_config.model_path, 
            sizeof(server_config.llm_config.model_path) - 1);
    
    // Initialize and start server
    if (!server_initialize(&server_config)) {
        fprintf(stderr, "Failed to initialize server\n");
        return 1;
    }
    
    if (!server_start()) {
        fprintf(stderr, "Failed to start server\n");
        return 1;
    }
    
    return 0;
}
