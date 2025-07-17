#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 8000
#define BUFFER_SIZE 1024
#define FILENAME "received_file.txt"

char* REQUEST_LIST_FILE = "REQUEST_LIST_FILE\0";
char* REQUEST_FILE = "REQUEST_RECEIVE_FILE\0";
char* REQUEST_DISCONNECT_SERVER = "REQUEST_DISCONNECT_SERVER\0";
char* ACK = "ACK\0";

int flag_received = 0;
int flag_sended = 0;
int flag_wait_file =0;

#define INITIAL_BUFFER_SIZE 64

// Get filenaem
char* get_input_filename() {
    char* filename = (char*)malloc(100 * sizeof(char));  // Allocate memory for the filename (max 99 chars)
    if (filename == NULL) {
        printf("Memory allocation failed!\n");
        exit(1);  // Exit if memory allocation fails
    }
    
    // Use fgets to read the input (including spaces) safely
    if (fgets(filename, 100, stdin) != NULL) {
        // Remove newline character if present
        filename[strcspn(filename, "\n")] = '\0';  // Replaces newline with null terminator
    } else {
        printf("Error reading input.\n");
        free(filename);  // Free memory before returning
        return NULL;
    }
    return filename;  // Return the allocated string
}
char *get_input_options()
{
    size_t buffer_size = INITIAL_BUFFER_SIZE;
    char* buffer = malloc(buffer_size);
    char* request0;
    char request_file[1024];
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }
    size_t length = 0;
    int c;

    while ((c = getchar()) != '\n' && c != EOF) {
        if (length + 1 >= buffer_size) {
            buffer_size *= 2;
            char* new_buffer = realloc(buffer, buffer_size);
            if (!new_buffer) {
                fprintf(stderr, "Error: Memory reallocation failed\n");
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
        }
        buffer[length++] = c;
    }

    buffer[length] = '\0'; // Null-terminate the string
    if (strcmp(buffer,"1\0") == 0)
    {
        request0 = "REQUEST_LIST_FILE\0";
    }else
    if(strcmp(buffer, "2\0") == 0)
    {
        request0 = "REQUEST_RECEIVE_FILE\0";
    }else
    {
        request0 = "REQUEST_DISCONNECT_SERVER\0";
    }
    free(buffer);
    return request0;
}

// Read a file
void display_file_content(const char *filename) {
    int fd = open(filename, O_RDONLY); // Open the file in read-only mode
    if (fd < 0) {
        perror("Error opening file");
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    printf("File received from server:\n");
    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
        for (ssize_t i = 0; i < bytes_read; ++i) {
            putchar(buffer[i]);
        }
    }

    if (bytes_read < 0) {
        perror("Error reading file");
    }

    close(fd); // Close the file descriptor
}

// Write a message to a file
int write_message_to_file(const char *filename, const char *message) {
    // Open the file for writing (creates the file if it doesn't exist, truncates it if it does)
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("Failed to open file");
        return -1; // Return error if file cannot be opened
    }

    // Write the message to the file
    if (fputs(message, file) == EOF) {
        perror("Failed to write to file");
        fclose(file); // Close the file if there's an error
        return -1; // Return error if writing fails
    }

    printf("Message written to file successfully.\n");

    // Close the file
    fclose(file);

    return 0; // Success
}

// List option
void display_option()
{
    printf("\n\nChose options:\n");
    printf("1.List file from server\n");
    printf("2.Send request to receive file!!!\n");
    printf("3.Disconnect server!!!\n");
    printf("---------------------------------\n");
    printf("Options chosed: ");
}


// Function to send a message over a socket
void sendMessage(int socket, const char* message) {
    if (send(socket, message, strlen(message), 0) == -1) {
        perror("Error sending message");
        close(socket); // Close the socket on failure
        exit(EXIT_FAILURE); // Exit the program if sending fails
    }
    flag_sended = 1;
}

// Send request to server
int send_file_to_server(int client_socket, const char *filename_send) {
    FILE *file;
    char buffer[BUFFER_SIZE];
    int bytes_read, bytes_sent;

    // Open the file for reading in binary mode
    file = fopen(filename_send, "rb");
    if (file == NULL) {
        perror("Failed to open file");
        return -1; // Return error if file cannot be opened
    }

    printf("Sending file: %s\n", filename_send);

    // Read the file and send its contents to the server
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        // Send the chunk of data to the server
        bytes_sent = send(client_socket, buffer, bytes_read, 0);
        if (bytes_sent < 0) {
            perror("Error sending data to server");
            fclose(file); // Close the file on error
            return -1; // Return error
        }

        printf("Sent %d bytes...\n", bytes_sent);
    }

    // Check if the file was fully read without errors
    if (ferror(file)) {
        perror("Error reading the file");
        fclose(file);
        return -1; // Return error
    }
    flag_sended = 1;
    printf("File sent successfully to the server.\n");

    // Close the file
    fclose(file);

    return 0; // Success
}

// Function to receive a file over a socket
void  receive_file(int sock_fd, const char* filepath) {
    
    printf(">>> next......\n");
    FILE* file = fopen(filepath, "wb"); // Open the file for writing in binary mode
    if (file == NULL) {
        perror("Error opening file");
        close(sock_fd); // Close the socket if the file cannot be opened
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    
    printf("Receiving file and saving to '%s'...\n", filepath);
    // flag_wait_file=1;
    while ((bytes_received = recv(sock_fd, buffer, BUFFER_SIZE, 0)) > 0) {
        printf(">>>> buffer: %s\n",buffer);
        printf(">>> length: %ld\n", bytes_received);
        // if (strcmp(buffer, "END_OF_TRANSMISSION") == 0)
        // {
        //     printf("END_OF TRANSMISSION<:>");
        //     break;
        // }
        if (fwrite(buffer, 1, bytes_received, file) != bytes_received) {
            perror("Error writing to file");
            fclose(file);
            close(sock_fd);
            exit(EXIT_FAILURE);
        }
        
    }

    if (bytes_received < 0) {
        perror("Error receiving data");
    } else {
        flag_wait_file = 0;
        flag_received = 1;
        printf("File received successfully.\n");
    }
    fclose(file); // Close the file
    // flag_wait_file=0;
}

int check_sended()
{
    while (1)
        {
            if (flag_sended)
            {
                
                flag_sended = 0;
                return 1;
            }
        }
}
int check_received()
{
    while (1)
        {
            if (flag_received)
            {
                flag_received = 0;
                return 1;
            }
        }
}

//Read a file by pointer file
void read_file_by_pointer(FILE* file,const char* filepath)
{
    file = fopen(filepath, "r");
    if (check_received())
        display_file_content(filepath);
}
int main() 
{
    
    int sock_fd, file_fd;
    struct sockaddr_in6 server_addr;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received, bytes_written;

    printf("Client started, attempting to connect to server...\n");

    // Create socket
    if ((sock_fd = socket(AF_INET6, SOCK_STREAM, 0)) == -1) {
        printf("Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    printf("Socket created successfully\n");

    // Set up the server address (IPv6)
    memset(&server_addr, 0, sizeof(server_addr)); // Clear the structure
    server_addr.sin6_family = AF_INET6;            // IPv6 family
    server_addr.sin6_port = htons(PORT);           // Port number
    // You should replace this with the server's IPv6 address
    if (inet_pton(AF_INET6, "fe80::611c:5b53:b0ac:fe3a", &server_addr.sin6_addr) <= 0) { // Example IPv6 address for localhost
        printf("Invalid address format\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    server_addr.sin6_scope_id = if_nametoindex("wlp2s0"); // Change "eth0" to your interface name
    if (server_addr.sin6_scope_id == 0) {
        perror("Interface not found");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    // //Connect to the server
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        printf("Connection failed\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    
    // printf("Connected to server\n");
    // printf("CLient open!!!!!\n");
    
   while (1)
   {    
        // connect and reconnect to server
        // if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) 
        // {
        //     printf("Connection failed\n");
        //     close(sock_fd);
        //     exit(EXIT_FAILURE);
        // }
        // Display interface user and get input from user
        display_option();
        char *request = get_input_options();

        if (strcmp(request,REQUEST_FILE) == 0) // Check option 2: CLient request a file to server
        {   

            printf("Enter filename: "); // Enter request file name
            char* filename = get_input_filename();
            
            printf("Filename: %s",filename);
            sendMessage(sock_fd, filename); // Send namefile to server

            if (check_sended()) // Ensure data to be sended to server
            {
                printf("Send request option 2 to server\n");
                flag_wait_file = 1;
            }

            receive_file(sock_fd, filename); // Receive file from server
            while (flag_wait_file) // Ensure file be received : use flag_wait_file and check_reveived()
            {
                if (flag_wait_file == 0)
                {
                    printf("File is received in client\n");
                    break;
                }
            }
            if (check_received())
                display_file_content(filename);

            // After received file , client send a ACK to serser
            sendMessage(sock_fd, ACK);
            if (check_sended())
                printf("Send ACK to server \n");
        }else if (strcmp(request,REQUEST_LIST_FILE) == 0) // CHeck option 1: Request list available file in server side
        {
            sendMessage(sock_fd, request);

            receive_file(sock_fd,"file_receiver/list_file.txt"); // Receive list file and store to directory: "file_receiver/list_file.txt"
            if (check_received())
                display_file_content("file_receiver/list_file.txt");
            
            // After received file , client send a ACK to serser
            sendMessage(sock_fd, ACK);
            if (check_sended())
                printf("Send ACK to server \n");
        }else
        {   
            // End socket communication
            close(sock_fd);
            break;
        }
   }
    return 0;
}
