#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <signal.h>
#include "include/become_daemon.h"

#define PORT 8000
#define BUFFER_SIZE 1024

int server_fd = -1;
int client_fd = -1;
int file_fd = -1;

// Handler function for SIGTERM to gracefully shut down the daemon
void handle_sigterm(int sig) {
    syslog(LOG_INFO, "Daemon received SIGTERM, terminating...");
    close(file_fd);
    close(client_fd);
    close(server_fd);
    exit(0);  // Terminate the daemon
}

// Function to send a file over a socket
int send_file(int client_fd, const char *filename) {
    int file_fd;
    ssize_t bytes_read, bytes_sent;
    char buffer[BUFFER_SIZE];

    file_fd = open(filename, O_RDONLY);  // Open file in read-only mode
    if (file_fd == -1) {
        syslog(LOG_ERR, "File open failed: %s", filename);
        return -1;
    }

    // Read from the file and send data over the socket
    while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
        
        bytes_sent = send(client_fd, buffer, bytes_read, 0);
        if (bytes_sent < 0) {
            syslog(LOG_ERR, "Send failed");
            close(file_fd);
            return -1;
        }
    }

    //Send the termination message to indicate the end of the transmission
    // const char *end_message = "END_OF_TRANSMISSION";
    // if (send(client_fd, end_message, strlen(end_message), 0) < 0) {
    //     syslog(LOG_ERR, "Failed to send end-of-transmission message");
    //     close(file_fd);
    //     return -1;
    // }

    syslog(LOG_INFO, "File transmitted successfully: %s", filename);
    close(file_fd);
    return 0;
}

// Function to receive a file from a socket
int receive_file(int client_fd, const char *filename) {
    int file_fd;
    ssize_t bytes_received, bytes_written;
    char buffer[BUFFER_SIZE];

    file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);  // Open file for writing
    if (file_fd == -1) {
        syslog(LOG_ERR, "File open failed: %s", filename);
        return -1;
    }

    // Receive data from the socket and write to the file
    while ((bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
        bytes_written = write(file_fd, buffer, bytes_received);
        if (bytes_written < 0) {
            syslog(LOG_ERR, "Write to file failed");
            close(file_fd);
            return -1;
        }
    }

    syslog(LOG_INFO, "File received successfully: %s", filename);

    syslog(LOG_INFO, "End of transmission message sent");
    close(file_fd);
    return 0;
}

int main() {
    const char *directory = "/home/anhngo/workcpp/linux_socket/dev/send_files";  // Folder to check for file names
    char received_message[BUFFER_SIZE];
    const char *list_file = "/home/anhngo/workcpp/linux_socket/dev/list_files.txt";  // Predefined file for "REQUEST_LIST"

    
    signal(SIGTERM, handle_sigterm);  // Set up the signal handler for SIGTERM
    
    // Become daemon
    if (becomeDaemon(0) == -1) {
        syslog(LOG_ERR, "Failed to become a daemon");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in6 server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    ssize_t bytes_received;

    openlog("FileServer", LOG_PID | LOG_CONS, LOG_USER);  // Initialize syslog
    syslog(LOG_INFO, "Server started successfully and is running.");

    // Create socket
    if ((server_fd = socket(AF_INET6, SOCK_STREAM, 0)) == -1) {
        syslog(LOG_ERR, "Socket creation failed");
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO, "Socket is created successfully");

    // Bind socket
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_addr = in6addr_any;
    server_addr.sin6_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO, "Server binds successfully, waiting for connections");

    // Listen for incoming connections
    if (listen(server_fd, 5) == -1) {
        syslog(LOG_ERR, "Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    } 
    // Accept a connection (block until a client connects)
    if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) == -1) {
        syslog(LOG_ERR, "Accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    int req_count_fail = 0;
    int connection_count = 1;
    while (1) { 
        //Thêm biến count sau khoảng 5-10 lần lặp mà byte_received vẫn không nhận được j => coi như ngắt kết nốí chờ kết nối mới
        if(connection_count<=0){
             if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) == -1) {
                syslog(LOG_ERR, "Accept failed");
                close(server_fd);
                exit(EXIT_FAILURE);
            }
            req_count_fail = 0;
        }
        // Main loop: Keep the daemon running
        // Receive the request message from the client
        bytes_received = recv(client_fd, received_message, sizeof(received_message), 0);
        if (bytes_received <= 0) {
            syslog(LOG_ERR, "Failed to receive message from client");
            req_count_fail++;
            if(req_count_fail == 10){
                req_count_fail = 0;
                connection_count--;
                close(client_fd);
            }
            continue;  // Continue the loop, allowing the daemon to accept new connections
        }

        received_message[bytes_received] = '\0';  // Null-terminate the received string
        received_message[strcspn(received_message, "\r\n")] = 0;  // Strip any newline characters

        syslog(LOG_INFO, "Received message: %s", received_message);

        // Check if the message is a special request (e.g., "REQUEST_LIST")
        if (strcmp(received_message, "REQUEST_LIST_FILE") == 0) {
            // Send the predefined "list.txt"
            if (send_file(client_fd, list_file) == -1) {
                syslog(LOG_ERR, "Failed to send list.txt");
                req_count_fail=0;
                close(client_fd);
                continue;  // Continue accepting new connections
            }
        } 
        else if(strcmp(received_message, "DISCONNECT_SERVER") == 0){
            syslog(LOG_INFO,"Disconnect the client");
            close(client_fd);
            req_count_fail = 0;
            connection_count--;
            continue;
        } 
        else {
            syslog(LOG_INFO, "send a file");
            // Check if the message matches any file in the "send" directory
            char file_path[BUFFER_SIZE];
            snprintf(file_path, sizeof(file_path), "%s/%s", directory, received_message);

            // Check if the file exists in the "send" folder
            if (access(file_path, F_OK) == -1) {
                syslog(LOG_ERR, "Requested file does not exist: %s", received_message);
            } else {
                // File exists, send it to the client
                if (send_file(client_fd, file_path) == -1) {
                    syslog(LOG_ERR, "Failed to send file: %s", file_path);
                    close(client_fd);
                    continue;  // Continue accepting new connections
                }
            }   
        }
        bytes_received = recv(client_fd, received_message, sizeof(received_message), 0); // Receive ACK from client
        received_message[bytes_received] = '\0';  // Null-terminate the received string
        received_message[strcspn(received_message, "\r\n")] = 0;  // Strip any newline characters
        if (bytes_received >= 0)
        {
            if (strcmp(received_message, "ACK") == 0)
            {
                syslog(LOG_INFO,"Received ACK from client \n");
            } else
            {
                syslog(LOG_INFO,"NOT received ACK from client, Error occurs\n");
            }
        }
    }
    
    syslog(LOG_INFO, "Close client.");
    close(client_fd);
    // Cleanup after exiting the loop (not reached in this case as the loop isp infinite)
    close(server_fd);
    syslog(LOG_INFO, "Server finished processing requests and is exiting.");
    return 0;
}
