#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define DEFAULT_PORT 8080
#define DEFAULT_SERVER "127.0.0.1"
#define BUFFER_SIZE 4096

// Socket creation and connection utilities
int create_server_socket(int port);
int accept_client_connection(int server_socket);
int connect_to_server(const char *server_address, int port);

// Data transmission utilities
int send_message(int socket, const char *message);
int receive_message(int socket, char *buffer, size_t buffer_size);

// Error handling
void handle_socket_error(const char *message);

#endif /* SOCKET_UTILS_H */
