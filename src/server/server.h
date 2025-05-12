#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include "../common/socket_utils.h"
#include "llm_interface.h"

// Server configuration
typedef struct {
    int port;
    llm_config_t llm_config;
    bool verbose;
    int max_connections;
} server_config_t;

// Client connection data
typedef struct {
    int client_socket;
    pthread_t thread_id;
    bool active;
} client_connection_t;

// Server functions
bool server_initialize(server_config_t *config);
bool server_start(void);
void server_stop(void);
bool server_is_running(void);

// Client handling functions
void* handle_client(void *client_socket_ptr);

#endif /* SERVER_H */
