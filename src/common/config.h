#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include "../server/llm_interface.h"

// Configuration structure
typedef struct {
    // Server configuration
    char server_host[256];
    int server_port;
    int max_connections;
    bool verbose;
    
    // LLM configuration
    llm_type_t llm_type;
    char model_path[256];
    float temperature;
    int max_tokens;
    int context_size;
    
    // Client configuration
    bool dark_mode;
    char font_family[32];
    int font_size;
    int window_width;
    int window_height;
} config_t;

// Configuration functions
bool config_load(const char *filename, config_t *config);
bool config_save(const char *filename, const config_t *config);
void config_set_defaults(config_t *config);
bool config_parse_args(int argc, char *argv[], config_t *config);
void config_print(const config_t *config);

// Helper functions
const char* config_get_default_path(void);

#endif /* CONFIG_H */
