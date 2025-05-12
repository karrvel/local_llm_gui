#include "llm_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>

// Internal state
static bool is_initialized = false;
static llm_config_t current_config;

// For this implementation, we'll use a simple command-line approach to interact with Ollama
// In a production environment, you might want to use a proper C binding for your LLM

bool llm_initialize(llm_config_t *config) {
    if (config == NULL) {
        fprintf(stderr, "Error: NULL configuration provided\n");
        return false;
    }
    
    // Store configuration
    memcpy(&current_config, config, sizeof(llm_config_t));
    
    // Check if model path exists for local models
    if (config->type != LLM_TYPE_CUSTOM) {
        if (access(config->model_path, F_OK) == -1) {
            fprintf(stderr, "Error: Model file not found at %s\n", config->model_path);
            return false;
        }
    }
    
    printf("Initialized LLM interface with model type: %s\n", llm_type_to_string(config->type));
    is_initialized = true;
    return true;
}

char* llm_generate_response(const char *prompt) {
    if (!is_initialized) {
        fprintf(stderr, "Error: LLM interface not initialized\n");
        return strdup("Error: LLM not initialized");
    }
    
    if (prompt == NULL || strlen(prompt) == 0) {
        return strdup("Error: Empty prompt");
    }
    
    // Ollama API endpoint
    const char *host = "localhost";
    const int port = 11434;
    const char *endpoint = "/api/generate";
    
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "Error: Failed to create socket: %s\n", strerror(errno));
        return strdup("Error: Failed to connect to Ollama");
    }
    
    // Set up server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    // Convert hostname to IP address
    struct hostent *he = gethostbyname(host);
    if (he == NULL) {
        fprintf(stderr, "Error: Could not resolve hostname %s: %s\n", host, strerror(errno));
        close(sock);
        return strdup("Error: Could not resolve Ollama hostname");
    }
    
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    // Connect to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Error: Failed to connect to %s:%d: %s\n", host, port, strerror(errno));
        close(sock);
        return strdup("Error: Failed to connect to Ollama server");
    }
    
    if (current_config.verbose) {
        printf("Connected to Ollama server at %s:%d\n", host, port);
    }
    
    // Prepare JSON request
    // Escape quotes in the prompt
    char *escaped_prompt = malloc(strlen(prompt) * 2 + 1);
    if (!escaped_prompt) {
        close(sock);
        return strdup("Error: Memory allocation failed");
    }
    
    const char *src = prompt;
    char *dst = escaped_prompt;
    while (*src) {
        if (*src == '"' || *src == '\\') {
            *dst++ = '\\';
        }
        *dst++ = *src++;
    }
    *dst = '\0';
    
    // Determine model name based on type
    const char *model_name;
    switch (current_config.type) {
        case LLM_TYPE_LLAMA:
            model_name = "llama3"; // Updated to use the modern name format
            break;
        case LLM_TYPE_MISTRAL:
            model_name = "mistral"; // Try to use mistral directly
            break;
        case LLM_TYPE_GPT_J:
            model_name = "phi3"; // Try to use phi3 as an available alternative
            break;
        case LLM_TYPE_CUSTOM:
        default:
            // Use model path if provided, otherwise default to llama3
            model_name = (strlen(current_config.model_path) > 0) ? 
                          current_config.model_path : "llama3";
            break;
    }
    
    if (current_config.verbose) {
        printf("Using Ollama model: %s\n", model_name);
    }
    
    // Build JSON request
    char *json_request = malloc(strlen(escaped_prompt) + 512);
    if (!json_request) {
        free(escaped_prompt);
        close(sock);
        return strdup("Error: Memory allocation failed");
    }
    
    // Build more complete JSON request according to Ollama API docs
    sprintf(json_request, "{\"model\":\"%s\",\"prompt\":\"%s\",\"stream\":true,\"temperature\":%.2f,\"max_tokens\":%d,\"options\":{\"num_ctx\":%d}}", 
            model_name, escaped_prompt, current_config.temperature, current_config.max_tokens, current_config.context_size);
    
    free(escaped_prompt);
    
    // Build HTTP request with correct headers and format
    char http_request[8192];
    snprintf(http_request, sizeof(http_request),
        "POST /api/generate HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/json\r\n"
        "Accept: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n"
        "%s",
        host, port, strlen(json_request), json_request);
    
    if (current_config.verbose) {
        printf("\n=======================================\n");
        printf("Sending request to Ollama:\n%s\n", http_request);
        printf("=======================================\n");
    }
    
    // Save the JSON request for debugging
    char *json_copy = strdup(json_request);
    free(json_request);
    
    if (current_config.verbose) {
        printf("JSON payload: %s\n", json_copy);
    }
    
    // Send request
    if (send(sock, http_request, strlen(http_request), 0) < 0) {
        if (current_config.verbose) {
            printf("Failed to send request: %s\n", strerror(errno));
        }
        close(sock);
        free(json_copy);
        return strdup("Error: Failed to send request to Ollama");
    }
    
    if (current_config.verbose) {
        printf("Request sent successfully, waiting for response...\n");
    }
    
    // Receive response
    char buffer[4096];
    char *response = malloc(1);
    if (!response) {
        close(sock);
        free(json_copy);
        return strdup("Error: Memory allocation failed");
    }
    response[0] = '\0';
    
    size_t total_size = 1;
    ssize_t bytes_received;
    
    if (current_config.verbose) {
        printf("\n=== STARTING OLLAMA RESPONSE RECEPTION ===\n");
    }
    
    // Set socket to non-blocking mode
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    // Receive with timeout
    fd_set readfds;
    struct timeval tv;
    int ready;
    
    // Try to receive for up to 30 seconds
    time_t start_time = time(NULL);
    int wait_count = 0;
    bool headers_done = false;
    char *body_start = NULL;
    while (time(NULL) - start_time < 30) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        
        // Set timeout to 1 second
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        if (current_config.verbose && wait_count % 5 == 0) {
            printf("Waiting for Ollama response... (elapsed: %ld seconds)\n", time(NULL) - start_time);
        }
        wait_count++;
        
        ready = select(sock + 1, &readfds, NULL, NULL, &tv);
        
        if (ready < 0) {
            if (current_config.verbose) {
                printf("Select error: %s\n", strerror(errno));
            }
            break;
        } else if (ready == 0) {
            // Timeout, try again
            continue;
        }
        
        if (current_config.verbose) {
            printf("Data available to read from Ollama\n");
        }
        
        memset(buffer, 0, sizeof(buffer));
        bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available, try again
                continue;
            }
            if (current_config.verbose) {
                printf("Error receiving data: %s\n", strerror(errno));
            }
            free(response);
            free(json_copy);
            close(sock);
            return strdup("Error: Failed to receive data from Ollama");
        } else if (bytes_received == 0) {
            // Connection closed by server
            if (current_config.verbose) {
                printf("Connection closed by Ollama server\n");
            }
            break;
        }
        
        buffer[bytes_received] = '\0';
        
        if (current_config.verbose) {
            printf("Received %zd bytes from Ollama:\n%.100s%s\n", 
                   bytes_received, buffer, strlen(buffer) > 100 ? "..." : "");
        }
        
        // Look for the end of HTTP headers if we haven't found it yet
        if (!headers_done) {
            char *headers_end = strstr(buffer, "\r\n\r\n");
            if (headers_end) {
                headers_done = true;
                body_start = headers_end + 4; // Skip \r\n\r\n
                if (current_config.verbose) {
                    printf("Found end of HTTP headers, body starts with: %.50s%s\n", 
                           body_start, strlen(body_start) > 50 ? "..." : "");
                }
                
                // Only append the body part
                char *new_response = realloc(response, total_size + strlen(body_start) + 1);
                if (!new_response) {
                    free(response);
                    free(json_copy);
                    close(sock);
                    return strdup("Error: Memory allocation failed");
                }
                response = new_response;
                strcat(response, body_start);
                total_size += strlen(body_start);
            } else {
                // No headers end found yet, don't append anything
                if (current_config.verbose) {
                    printf("Still looking for end of HTTP headers...\n");
                }
                continue;
            }
        } else {
            // Headers already processed, append the whole buffer
            char *new_response = realloc(response, total_size + bytes_received + 1);
            if (!new_response) {
                free(response);
                free(json_copy);
                close(sock);
                return strdup("Error: Memory allocation failed");
            }
            response = new_response;
            strcat(response, buffer);
            total_size += bytes_received;
            
            if (current_config.verbose) {
                printf("Appended %zd bytes to response, total size: %zu bytes\n", 
                       bytes_received, total_size);
            }
        }
    }
    
    if (current_config.verbose) {
        printf("Received raw response from Ollama (first 200 chars): %.200s\n", response);
    }
    
    // For streaming responses from Ollama, we need to extract and concatenate all response chunks
    // Each chunk is in the format: {"model":"...","created_at":"...","response":"...","done":false/true}
    
    char *full_response = malloc(8192); // Start with a larger buffer size
    if (!full_response) {
        free(response);
        free(json_copy);
        return strdup("Error: Memory allocation failed");
    }
    full_response[0] = '\0';
    size_t full_response_size = 8192;
    size_t full_response_len = 0;
    
    // Process each line of the response
    char *line = response;
    char *next_line;
    bool found_valid_response = false;
    
    if (current_config.verbose) {
        printf("Starting to process Ollama streaming response...");
    }
    
    while (line && *line) {
        // Find the end of the current line
        next_line = strstr(line, "\n");
        if (next_line) {
            *next_line = '\0'; // Temporarily terminate the line
            next_line++; // Move to the start of the next line
        }
        
        // Skip empty lines
        if (*line == '\0') {
            line = next_line;
            continue;
        }
        
        // Skip chunk size lines (hex numbers) - common in HTTP chunked encoding
        if (isxdigit(*line)) {
            // Check if this line only contains hex digits and possibly a CR (carriage return)
            char *p = line;
            bool only_hex = true;
            while (*p) {
                if (!(isxdigit(*p) || *p == '\r')) {
                    only_hex = false;
                    break;
                }
                p++;
            }
            if (only_hex) {
                line = next_line;
                continue;
            }
        }
        
        // Check if this line contains a valid JSON object
        if (*line == '{') {
            // Look for response field in this JSON object
            char *content_start = strstr(line, "\"response\":\"");
            if (content_start) {
                content_start += 12; // Skip "response":"
                char *content_end = strstr(content_start, "\"");
                if (content_end) {
                    // Extract the response fragment
                    size_t content_length = content_end - content_start;
                    found_valid_response = true;
                    
                    // Ensure we have enough space in the buffer
                    if (full_response_len + content_length + 1 > full_response_size) {
                        full_response_size = full_response_len + content_length + 4096; // Add more space
                        char *new_buffer = realloc(full_response, full_response_size);
                        if (!new_buffer) {
                            free(full_response);
                            free(response);
                            free(json_copy);
                            return strdup("Error: Memory allocation failed");
                        }
                        full_response = new_buffer;
                    }
                    
                    // Append this fragment to the full response
                    strncat(full_response + full_response_len, content_start, content_length);
                    full_response_len += content_length;
                    full_response[full_response_len] = '\0';
                    
                    if (current_config.verbose) {
                        printf("Extracted response fragment: %.20s%s\n", 
                               content_start, content_length > 20 ? "..." : "");
                    }
                }
            }
            
            // Check if this is the last message
            char *done_field = strstr(line, "\"done\":true");
            if (done_field && current_config.verbose) {
                printf("Reached final response message\n");
            }
        }
        
        // Move to the next line
        line = next_line;
    }
    
    // Free the original response and JSON copy
    free(response);
    free(json_copy);
    
    if (full_response_len == 0 || !found_valid_response) {
        free(full_response);
        if (current_config.verbose) {
            printf("Failed to extract valid response from Ollama. Raw response:\n%s\n", response);
        }
        return strdup("No valid response received from Ollama. Please check if Ollama is running correctly.");
    }
    
    if (current_config.verbose) {
        printf("Successfully extracted complete response from Ollama: %.50s%s\n", 
               full_response, strlen(full_response) > 50 ? "..." : "");
    }
    
    return full_response;
}

void llm_cleanup(void) {
    if (is_initialized) {
        printf("Cleaning up LLM interface\n");
        is_initialized = false;
    }
}

const char* llm_type_to_string(llm_type_t type) {
    switch (type) {
        case LLM_TYPE_LLAMA:
            return "LLaMA";
        case LLM_TYPE_MISTRAL:
            return "Mistral";
        case LLM_TYPE_GPT_J:
            return "GPT-J";
        case LLM_TYPE_CUSTOM:
            return "Custom/Ollama";
        default:
            return "Unknown";
    }
}

bool llm_is_initialized(void) {
    return is_initialized;
}
