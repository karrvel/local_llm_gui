#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <pwd.h>

// JSON parsing - simple implementation for this project
// In a production environment, you would use a proper JSON library like cJSON
static char* read_file_content(const char *filename);
static bool parse_json_value(const char *json, const char *key, char *value, size_t value_size);
static bool parse_json_int(const char *json, const char *key, int *value);
static bool parse_json_float(const char *json, const char *key, float *value);
static bool parse_json_bool(const char *json, const char *key, bool *value);

bool config_load(const char *filename, config_t *config) {
    if (config == NULL) {
        fprintf(stderr, "Error: NULL configuration provided\n");
        return false;
    }
    
    // Set defaults first
    config_set_defaults(config);
    
    // Check if file exists
    if (access(filename, F_OK) == -1) {
        fprintf(stderr, "Warning: Configuration file %s not found, using defaults\n", filename);
        return false;
    }
    
    // Read file content
    char *json = read_file_content(filename);
    if (json == NULL) {
        fprintf(stderr, "Error: Failed to read configuration file %s\n", filename);
        return false;
    }
    
    // Parse server configuration
    parse_json_value(json, "server_host", config->server_host, sizeof(config->server_host));
    parse_json_int(json, "server_port", &config->server_port);
    parse_json_int(json, "max_connections", &config->max_connections);
    parse_json_bool(json, "verbose", &config->verbose);
    
    // Parse LLM configuration
    char llm_type_str[32] = {0};
    if (parse_json_value(json, "llm_type", llm_type_str, sizeof(llm_type_str))) {
        if (strcmp(llm_type_str, "llama") == 0) {
            config->llm_type = LLM_TYPE_LLAMA;
        } else if (strcmp(llm_type_str, "mistral") == 0) {
            config->llm_type = LLM_TYPE_MISTRAL;
        } else if (strcmp(llm_type_str, "gptj") == 0) {
            config->llm_type = LLM_TYPE_GPT_J;
        } else {
            config->llm_type = LLM_TYPE_CUSTOM;
        }
    }
    
    parse_json_value(json, "model_path", config->model_path, sizeof(config->model_path));
    parse_json_float(json, "temperature", &config->temperature);
    parse_json_int(json, "max_tokens", &config->max_tokens);
    parse_json_int(json, "context_size", &config->context_size);
    
    // Parse client configuration
    parse_json_bool(json, "dark_mode", &config->dark_mode);
    parse_json_value(json, "font_family", config->font_family, sizeof(config->font_family));
    parse_json_int(json, "font_size", &config->font_size);
    parse_json_int(json, "window_width", &config->window_width);
    parse_json_int(json, "window_height", &config->window_height);
    
    free(json);
    return true;
}

bool config_save(const char *filename, const config_t *config) {
    if (config == NULL) {
        fprintf(stderr, "Error: NULL configuration provided\n");
        return false;
    }
    
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        fprintf(stderr, "Error: Failed to open configuration file %s for writing: %s\n", 
                filename, strerror(errno));
        return false;
    }
    
    // Write configuration as JSON
    fprintf(fp, "{\n");
    
    // Server configuration
    fprintf(fp, "    \"server_host\": \"%s\",\n", config->server_host);
    fprintf(fp, "    \"server_port\": %d,\n", config->server_port);
    fprintf(fp, "    \"max_connections\": %d,\n", config->max_connections);
    fprintf(fp, "    \"verbose\": %s,\n", config->verbose ? "true" : "false");
    
    // LLM configuration
    fprintf(fp, "    \"llm_type\": \"%s\",\n", 
            config->llm_type == LLM_TYPE_LLAMA ? "llama" : 
            config->llm_type == LLM_TYPE_MISTRAL ? "mistral" : 
            config->llm_type == LLM_TYPE_GPT_J ? "gptj" : "custom");
    fprintf(fp, "    \"model_path\": \"%s\",\n", config->model_path);
    fprintf(fp, "    \"temperature\": %.2f,\n", config->temperature);
    fprintf(fp, "    \"max_tokens\": %d,\n", config->max_tokens);
    fprintf(fp, "    \"context_size\": %d,\n", config->context_size);
    
    // Client configuration
    fprintf(fp, "    \"dark_mode\": %s,\n", config->dark_mode ? "true" : "false");
    fprintf(fp, "    \"font_family\": \"%s\",\n", config->font_family);
    fprintf(fp, "    \"font_size\": %d,\n", config->font_size);
    fprintf(fp, "    \"window_width\": %d,\n", config->window_width);
    fprintf(fp, "    \"window_height\": %d\n", config->window_height);
    
    fprintf(fp, "}\n");
    
    fclose(fp);
    return true;
}

void config_set_defaults(config_t *config) {
    if (config == NULL) {
        return;
    }
    
    // Server defaults
    strcpy(config->server_host, "127.0.0.1");
    config->server_port = 8080;
    config->max_connections = 10;
    config->verbose = false;
    
    // LLM defaults
    config->llm_type = LLM_TYPE_CUSTOM;
    config->model_path[0] = '\0';
    config->temperature = 0.7f;
    config->max_tokens = 512;
    config->context_size = 2048;
    
    // Client defaults
    config->dark_mode = true;
    strcpy(config->font_family, "Sans");
    config->font_size = 12;
    config->window_width = 800;
    config->window_height = 600;
}

bool config_parse_args(int argc, char *argv[], config_t *config) {
    if (config == NULL) {
        return false;
    }
    
    for (int i = 1; i < argc; i++) {
        // Server configuration
        if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            strncpy(config->server_host, argv[i + 1], sizeof(config->server_host) - 1);
            i++;
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            config->server_port = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--max-connections") == 0 && i + 1 < argc) {
            config->max_connections = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            config->verbose = true;
        }
        
        // LLM configuration
        else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
            if (strcmp(argv[i + 1], "llama") == 0) {
                config->llm_type = LLM_TYPE_LLAMA;
            } else if (strcmp(argv[i + 1], "mistral") == 0) {
                config->llm_type = LLM_TYPE_MISTRAL;
            } else if (strcmp(argv[i + 1], "gptj") == 0) {
                config->llm_type = LLM_TYPE_GPT_J;
            } else {
                config->llm_type = LLM_TYPE_CUSTOM;
            }
            i++;
        } else if (strcmp(argv[i], "--model-path") == 0 && i + 1 < argc) {
            strncpy(config->model_path, argv[i + 1], sizeof(config->model_path) - 1);
            i++;
        } else if (strcmp(argv[i], "--temperature") == 0 && i + 1 < argc) {
            config->temperature = atof(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--max-tokens") == 0 && i + 1 < argc) {
            config->max_tokens = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--context-size") == 0 && i + 1 < argc) {
            config->context_size = atoi(argv[i + 1]);
            i++;
        }
        
        // Client configuration
        else if (strcmp(argv[i], "--light-mode") == 0) {
            config->dark_mode = false;
        } else if (strcmp(argv[i], "--font") == 0 && i + 1 < argc) {
            strncpy(config->font_family, argv[i + 1], sizeof(config->font_family) - 1);
            i++;
        } else if (strcmp(argv[i], "--font-size") == 0 && i + 1 < argc) {
            config->font_size = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            config->window_width = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            config->window_height = atoi(argv[i + 1]);
            i++;
        }
        
        // Help
        else if (strcmp(argv[i], "--help") == 0) {
            return false;
        }
    }
    
    return true;
}

void config_print(const config_t *config) {
    if (config == NULL) {
        return;
    }
    
    printf("Configuration:\n");
    printf("  Server:\n");
    printf("    Host: %s\n", config->server_host);
    printf("    Port: %d\n", config->server_port);
    printf("    Max Connections: %d\n", config->max_connections);
    printf("    Verbose: %s\n", config->verbose ? "Yes" : "No");
    
    printf("  LLM:\n");
    printf("    Type: %s\n", 
           config->llm_type == LLM_TYPE_LLAMA ? "LLaMA" : 
           config->llm_type == LLM_TYPE_MISTRAL ? "Mistral" : 
           config->llm_type == LLM_TYPE_GPT_J ? "GPT-J" : "Custom/Ollama");
    printf("    Model Path: %s\n", config->model_path[0] ? config->model_path : "(None)");
    printf("    Temperature: %.2f\n", config->temperature);
    printf("    Max Tokens: %d\n", config->max_tokens);
    printf("    Context Size: %d\n", config->context_size);
    
    printf("  Client:\n");
    printf("    Theme: %s\n", config->dark_mode ? "Dark" : "Light");
    printf("    Font: %s, %dpx\n", config->font_family, config->font_size);
    printf("    Window Size: %dx%d\n", config->window_width, config->window_height);
}

const char* config_get_default_path(void) {
    static char config_path[512] = {0};
    
    if (config_path[0] == '\0') {
        // Try to get user's home directory
        const char *home = getenv("HOME");
        if (home == NULL) {
            struct passwd *pwd = getpwuid(getuid());
            if (pwd != NULL) {
                home = pwd->pw_dir;
            }
        }
        
        if (home != NULL) {
            // Create .config directory if it doesn't exist
            char config_dir[256];
            snprintf(config_dir, sizeof(config_dir), "%s/.config", home);
            mkdir(config_dir, 0755);
            
            // Create app config directory
            char app_config_dir[384];
            snprintf(app_config_dir, sizeof(app_config_dir), "%s/llm-chat", config_dir);
            mkdir(app_config_dir, 0755);
            
            // Set config file path
            snprintf(config_path, sizeof(config_path), "%s/config.json", app_config_dir);
        } else {
            // Fallback to current directory
            strcpy(config_path, "config.json");
        }
    }
    
    return config_path;
}

// Simple JSON parsing functions
static char* read_file_content(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return NULL;
    }
    
    // Get file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    // Allocate memory for file content
    char *content = malloc(file_size + 1);
    if (content == NULL) {
        fclose(fp);
        return NULL;
    }
    
    // Read file content
    size_t read_size = fread(content, 1, file_size, fp);
    content[read_size] = '\0';
    
    fclose(fp);
    return content;
}

static bool parse_json_value(const char *json, const char *key, char *value, size_t value_size) {
    if (json == NULL || key == NULL || value == NULL || value_size == 0) {
        return false;
    }
    
    // Create key pattern
    char key_pattern[256];
    snprintf(key_pattern, sizeof(key_pattern), "\"%s\"\\s*:\\s*\"([^\"]*)\"", key);
    
    // Simple pattern matching
    char *key_pos = strstr(json, key);
    if (key_pos == NULL) {
        return false;
    }
    
    char *value_start = strchr(key_pos, ':');
    if (value_start == NULL) {
        return false;
    }
    
    value_start = strchr(value_start, '"');
    if (value_start == NULL) {
        return false;
    }
    
    value_start++;
    
    char *value_end = strchr(value_start, '"');
    if (value_end == NULL) {
        return false;
    }
    
    size_t value_length = value_end - value_start;
    if (value_length >= value_size) {
        value_length = value_size - 1;
    }
    
    strncpy(value, value_start, value_length);
    value[value_length] = '\0';
    
    return true;
}

static bool parse_json_int(const char *json, const char *key, int *value) {
    if (json == NULL || key == NULL || value == NULL) {
        return false;
    }
    
    // Create key pattern
    char key_pattern[256];
    snprintf(key_pattern, sizeof(key_pattern), "\"%s\"\\s*:\\s*([0-9]+)", key);
    
    // Simple pattern matching
    char *key_pos = strstr(json, key);
    if (key_pos == NULL) {
        return false;
    }
    
    char *value_start = strchr(key_pos, ':');
    if (value_start == NULL) {
        return false;
    }
    
    value_start++;
    
    // Skip whitespace
    while (*value_start == ' ' || *value_start == '\t' || *value_start == '\n' || *value_start == '\r') {
        value_start++;
    }
    
    if (*value_start < '0' || *value_start > '9') {
        return false;
    }
    
    *value = atoi(value_start);
    
    return true;
}

static bool parse_json_float(const char *json, const char *key, float *value) {
    if (json == NULL || key == NULL || value == NULL) {
        return false;
    }
    
    // Create key pattern
    char key_pattern[256];
    snprintf(key_pattern, sizeof(key_pattern), "\"%s\"\\s*:\\s*([0-9.]+)", key);
    
    // Simple pattern matching
    char *key_pos = strstr(json, key);
    if (key_pos == NULL) {
        return false;
    }
    
    char *value_start = strchr(key_pos, ':');
    if (value_start == NULL) {
        return false;
    }
    
    value_start++;
    
    // Skip whitespace
    while (*value_start == ' ' || *value_start == '\t' || *value_start == '\n' || *value_start == '\r') {
        value_start++;
    }
    
    if ((*value_start < '0' || *value_start > '9') && *value_start != '.') {
        return false;
    }
    
    *value = atof(value_start);
    
    return true;
}

static bool parse_json_bool(const char *json, const char *key, bool *value) {
    if (json == NULL || key == NULL || value == NULL) {
        return false;
    }
    
    // Create key pattern
    char key_pattern[256];
    snprintf(key_pattern, sizeof(key_pattern), "\"%s\"\\s*:\\s*(true|false)", key);
    
    // Simple pattern matching
    char *key_pos = strstr(json, key);
    if (key_pos == NULL) {
        return false;
    }
    
    char *value_start = strchr(key_pos, ':');
    if (value_start == NULL) {
        return false;
    }
    
    value_start++;
    
    // Skip whitespace
    while (*value_start == ' ' || *value_start == '\t' || *value_start == '\n' || *value_start == '\r') {
        value_start++;
    }
    
    if (strncmp(value_start, "true", 4) == 0) {
        *value = true;
        return true;
    } else if (strncmp(value_start, "false", 5) == 0) {
        *value = false;
        return true;
    }
    
    return false;
}
