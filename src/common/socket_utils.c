#include "socket_utils.h"
#include <fcntl.h>

int create_server_socket(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    
    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        handle_socket_error("Socket creation failed");
        return -1;
    }
    
    // Set socket options to reuse address and port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        handle_socket_error("setsockopt failed");
        close(server_fd);
        return -1;
    }
    
    // Setup server address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    // Bind the socket to the network address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        handle_socket_error("Bind failed");
        close(server_fd);
        return -1;
    }
    
    // Start listening for connections
    if (listen(server_fd, 3) < 0) {
        handle_socket_error("Listen failed");
        close(server_fd);
        return -1;
    }
    
    printf("Server listening on port %d\n", port);
    return server_fd;
}

int accept_client_connection(int server_socket) {
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    int client_socket;
    
    if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addrlen)) < 0) {
        handle_socket_error("Accept failed");
        return -1;
    }
    
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    printf("New connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));
    
    return client_socket;
}

int connect_to_server(const char *server_address, int port) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    
    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        handle_socket_error("Socket creation failed");
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    
    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, server_address, &serv_addr.sin_addr) <= 0) {
        handle_socket_error("Invalid address or address not supported");
        close(sock);
        return -1;
    }
    
    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        handle_socket_error("Connection failed");
        close(sock);
        return -1;
    }
    
    printf("Connected to server at %s:%d\n", server_address, port);
    return sock;
}

int send_message(int socket, const char *message) {
    size_t message_len = strlen(message);
    size_t total_sent = 0;
    ssize_t bytes_sent;
    
    // Add debugging
    // printf("Attempting to send %zu bytes: '%.50s%s'\n", message_len, message, 
    //        message_len > 50 ? "..." : "");
    
    // Ensure the entire message is sent
    while (total_sent < message_len) {
        bytes_sent = send(socket, message + total_sent, message_len - total_sent, 0);
        
        if (bytes_sent <= 0) {
            if (errno == EINTR) {
                // Interrupted by signal, try again
                continue;
            }
            handle_socket_error("Send failed");
            return -1;
        }
        
        total_sent += bytes_sent;
    }
    
    // Add a brief delay after sending to prevent packet loss
    usleep(1000);  // 1ms delay
    
    return (int)total_sent;
}

int receive_message(int socket, char *buffer, size_t buffer_size) {
    memset(buffer, 0, buffer_size);
    
    // Set socket to non-blocking mode temporarily
    int flags = fcntl(socket, F_GETFL, 0);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);
    
    // Use select to wait for data with timeout
    fd_set read_fds;
    struct timeval tv;
    FD_ZERO(&read_fds);
    FD_SET(socket, &read_fds);
    tv.tv_sec = 1;  // 1 second timeout
    tv.tv_usec = 0;
    
    int select_result = select(socket + 1, &read_fds, NULL, NULL, &tv);
    
    if (select_result < 0) {
        // Error in select
        handle_socket_error("Select failed");
        fcntl(socket, F_SETFL, flags);  // Restore original flags
        return -1;
    } else if (select_result == 0) {
        // Timeout, no data available
        fcntl(socket, F_SETFL, flags);  // Restore original flags
        return 0;
    }
    
    // Data is available, receive it
    ssize_t bytes_received = recv(socket, buffer, buffer_size - 1, 0);
    
    // Restore original socket flags
    fcntl(socket, F_SETFL, flags);
    
    if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available even though select indicated there was
            return 0;
        }
        handle_socket_error("Receive failed");
        return -1;
    } else if (bytes_received == 0) {
        printf("Connection closed by peer\n");
        return 0;
    }
    
    buffer[bytes_received] = '\0';
    // printf("Received %zd bytes: '%.50s%s'\n", bytes_received, buffer, 
    //        bytes_received > 50 ? "..." : "");
    return (int)bytes_received;
}

void handle_socket_error(const char *message) {
    fprintf(stderr, "%s: %s\n", message, strerror(errno));
}
