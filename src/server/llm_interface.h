#ifndef LLM_INTERFACE_H
#define LLM_INTERFACE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// LLM model types
typedef enum {
    LLM_TYPE_LLAMA,
    LLM_TYPE_MISTRAL,
    LLM_TYPE_GPT_J,
    LLM_TYPE_CUSTOM
} llm_type_t;

// LLM configuration
typedef struct {
    llm_type_t type;
    char model_path[256];
    int context_size;
    float temperature;
    int max_tokens;
    bool verbose;
} llm_config_t;

// LLM interface functions
bool llm_initialize(llm_config_t *config);
char* llm_generate_response(const char *prompt);
void llm_cleanup(void);

// Helper functions
const char* llm_type_to_string(llm_type_t type);
bool llm_is_initialized(void);

#endif /* LLM_INTERFACE_H */
