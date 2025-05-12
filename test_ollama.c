#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

int main() {
    // Ollama API endpoint
    const char *host = "localhost";
    const int port = 11434;
    const char *endpoint = "/api/generate";
    
    printf("Testing connection to Ollama API at %s:%d%s\n", host, port, endpoint);
    
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "Error: Failed to create socket: %s\n", strerror(errno));
        return 1;
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
        return 1;
    }
    
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    // Connect to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Error: Failed to connect to %s:%d: %s\n", host, port, strerror(errno));
        close(sock);
        return 1;
    }
    
    printf("Connected to Ollama server\n");
    
    // Simple JSON request for a basic test
    const char *json_request = "{\"model\":\"llama3.2:latest\",\"prompt\":\"Hello, how are you?\"}";
    
    // Build HTTP request
    char http_request[1024];
    sprintf(http_request, 
            "POST %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            endpoint, host, port, strlen(json_request), json_request);
    
    printf("Sending request:\n%s\n", http_request);
    
    // Send request
    if (send(sock, http_request, strlen(http_request), 0) < 0) {
        fprintf(stderr, "Error: Failed to send request: %s\n", strerror(errno));
        close(sock);
        return 1;
    }
    
    printf("Request sent, waiting for response...\n");
    
    // Receive response
    char buffer[4096];
    ssize_t bytes_received;
    
    // Set socket to non-blocking mode
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    // Receive with timeout
    fd_set readfds;
    struct timeval tv;
    int ready;
    
    // Try to receive for up to 10 seconds
    time_t start_time = time(NULL);
    int total_bytes = 0;
    
    printf("Receiving response:\n");
    
    while (time(NULL) - start_time < 10) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        ready = select(sock + 1, &readfds, NULL, NULL, &tv);
        
        if (ready < 0) {
            if (errno == EINTR) continue; // Interrupted, try again
            fprintf(stderr, "Error: Select failed: %s\n", strerror(errno));
            close(sock);
            return 1;
        } else if (ready == 0) {
            // Timeout, try again
            printf("Waiting...\n");
            continue;
        }
        
        memset(buffer, 0, sizeof(buffer));
        bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available, try again
                continue;
            }
            fprintf(stderr, "Error: Failed to receive data: %s\n", strerror(errno));
            close(sock);
            return 1;
        } else if (bytes_received == 0) {
            // Connection closed by server
            printf("Connection closed by server\n");
            break;
        }
        
        buffer[bytes_received] = '\0';
        printf("%s", buffer);
        total_bytes += bytes_received;
    }
    
    close(sock);
    
    if (total_bytes == 0) {
        printf("No response received from Ollama\n");
        return 1;
    }
    
    printf("\nTest completed successfully\n");
    return 0;
}
