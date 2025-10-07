#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024
#define MAX_COMMAND 512
#define MAX_PATH 256

// Function to connect to S1 server
int connect_to_server() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        return -1;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to S1 failed");
        close(sock);
        return -1;
    }
    
    return sock;
}

// Function to validate command syntax
int validate_command_syntax(const char* command) {
    char command_copy[MAX_COMMAND];
    strcpy(command_copy, command);
    
    char* token = strtok(command_copy, " ");
    if (token == NULL) {
        return 0;
    }
    
    if (strcmp(token, "uploadf") == 0) {
        // uploadf filename1 filename2 filename3 destination_path
        int arg_count = 0;
        while (token != NULL) {
            token = strtok(NULL, " ");
            arg_count++;
        }
        if (arg_count < 2 || arg_count > 4) {
            printf("Error: uploadf requires 2-4 arguments (1-3 filenames + destination_path)\n");
            return 0;
        }
        
    } else if (strcmp(token, "downlf") == 0) {
        // downlf filename1 filename2
        int arg_count = 0;
        while (token != NULL) {
            token = strtok(NULL, " ");
            arg_count++;
        }
        if (arg_count < 1 || arg_count > 2) {
            printf("Error: downlf requires 1-2 arguments (filenames)\n");
            return 0;
        }
        
    } else if (strcmp(token, "removef") == 0) {
        // removef filename1 filename2
        int arg_count = 0;
        while (token != NULL) {
            token = strtok(NULL, " ");
            arg_count++;
        }
        if (arg_count < 1 || arg_count > 2) {
            printf("Error: removef requires 1-2 arguments (filenames)\n");
            return 0;
        }
        
    } else if (strcmp(token, "downltar") == 0) {
        // downltar filetype
        token = strtok(NULL, " ");
        if (token == NULL) {
            printf("Error: downltar requires 1 argument (filetype)\n");
            return 0;
        }
        if (strcmp(token, ".c") != 0 && strcmp(token, ".pdf") != 0 && strcmp(token, ".txt") != 0) {
            printf("Error: downltar only supports .c, .pdf, and .txt filetypes\n");
            return 0;
        }
        
    } else if (strcmp(token, "dispfnames") == 0) {
        // dispfnames pathname
        token = strtok(NULL, " ");
        if (token == NULL) {
            printf("Error: dispfnames requires 1 argument (pathname)\n");
            return 0;
        }
        if (strstr(token, "~S1") == NULL) {
            printf("Error: dispfnames pathname must start with ~S1\n");
            return 0;
        }
        
    } else if (strcmp(token, "quit") == 0) {
        // quit command is valid
        return 1;
        
    } else {
        printf("Error: Unknown command '%s'\n", token);
        return 0;
    }
    
    return 1;
}

// Function to check if file exists
int file_exists(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file != NULL) {
        fclose(file);
        return 1;
    }
    return 0;
}

// Function to get file extension
char* get_file_extension(const char* filename) {
    char* dot = strrchr(filename, '.');
    if (dot && dot != filename) {
        return dot + 1;
    }
    return "";
}

// Function to validate file type
int validate_file_type(const char* filename) {
    char* extension = get_file_extension(filename);
    if (strcmp(extension, "c") == 0 || strcmp(extension, "pdf") == 0 || 
        strcmp(extension, "txt") == 0 || strcmp(extension, "zip") == 0) {
        return 1;
    }
    return 0;
}

// Function to handle uploadf command
void handle_uploadf_command(int server_socket, char* command) {
    char* token;
    char filenames[3][MAX_PATH];
    int file_count = 0;
    char buffer[BUFFER_SIZE];
    FILE* file;
    long file_size;
    int bytes_read;
    
    // Parse command
    token = strtok(command, " ");
    token = strtok(NULL, " "); // Skip "uploadf"
    
    // Get filenames (up to 3)
    while (token != NULL && file_count < 3) {
        if (strstr(token, "~S1") == NULL) {
            strcpy(filenames[file_count], token);
            file_count++;
        } else {
            break;
        }
        token = strtok(NULL, " ");
    }
    
    // Validate files
    for (int i = 0; i < file_count; i++) {
        if (!file_exists(filenames[i])) {
            printf("Error: File '%s' does not exist in current directory\n", filenames[i]);
            return;
        }
        if (!validate_file_type(filenames[i])) {
            printf("Error: File '%s' is not a supported type (.c, .pdf, .txt, .zip)\n", filenames[i]);
            return;
        }
    }
    
    // Send command to server first
    send(server_socket, command, strlen(command), 0);
    
    // Process each file
    for (int i = 0; i < file_count; i++) {
        // Receive file size from server
        recv(server_socket, &file_size, sizeof(file_size), 0);
        
        // Send file data to server
        file = fopen(filenames[i], "rb");
        if (file != NULL) {
            long total_sent = 0;
            while (total_sent < file_size) {
                bytes_read = fread(buffer, 1, BUFFER_SIZE, file);
                if (bytes_read <= 0) break;
                send(server_socket, buffer, bytes_read, 0);
                total_sent += bytes_read;
            }
            fclose(file);
            
            // Send success confirmation
            send(server_socket, "SUCCESS", 7, 0);
        }
    }
    
    // Receive final response
    memset(buffer, 0, BUFFER_SIZE);
    recv(server_socket, buffer, BUFFER_SIZE, 0);
    
    if (strcmp(buffer, "UPLOAD_COMPLETE") == 0) {
        printf("Upload completed successfully\n");
    } else {
        printf("Upload failed: %s\n", buffer);
    }
}

// Function to handle downlf command
void handle_downlf_command(int server_socket, char* command) {
    char buffer[BUFFER_SIZE];
    long file_size;
    FILE* file;
    int bytes_received;
    char* token;
    char filenames[2][MAX_PATH];
    int file_count = 0;
    
    // Parse command
    token = strtok(command, " ");
    token = strtok(NULL, " "); // Skip "downlf"
    
    // Get filenames (up to 2)
    while (token != NULL && file_count < 2) {
        strcpy(filenames[file_count], token);
        file_count++;
        token = strtok(NULL, " ");
    }
    
    // Send command to server
    send(server_socket, command, strlen(command), 0);
    
    // Process each file
    for (int i = 0; i < file_count; i++) {
        // Extract filename from path
        char* filename = strrchr(filenames[i], '/');
        if (filename == NULL) {
            filename = filenames[i];
        } else {
            filename++; // Skip the '/'
        }
        
        // Receive file size from server
        recv(server_socket, &file_size, sizeof(file_size), 0);
        
        if (file_size > 0) {
            // Create file in current directory
            file = fopen(filename, "wb");
            if (file != NULL) {
                long total_received = 0;
                while (total_received < file_size) {
                    bytes_received = recv(server_socket, buffer, BUFFER_SIZE, 0);
                    if (bytes_received <= 0) break;
                    fwrite(buffer, 1, bytes_received, file);
                    total_received += bytes_received;
                }
                fclose(file);
                printf("File '%s' downloaded successfully\n", filename);
            } else {
                printf("Error: Cannot create file '%s'\n", filename);
            }
        } else {
            printf("Error: File '%s' not found on server\n", filename);
        }
    }
    
    // Receive final response
    memset(buffer, 0, BUFFER_SIZE);
    recv(server_socket, buffer, BUFFER_SIZE, 0);
    
    if (strcmp(buffer, "DOWNLOAD_COMPLETE") == 0) {
        printf("Download completed successfully\n");
    } else {
        printf("Download failed: %s\n", buffer);
    }
}

// Function to handle removef command
void handle_removef_command(int server_socket, char* command) {
    char buffer[BUFFER_SIZE];
    
    // Send command to server
    send(server_socket, command, strlen(command), 0);
    
    // Receive response
    memset(buffer, 0, BUFFER_SIZE);
    recv(server_socket, buffer, BUFFER_SIZE, 0);
    
    if (strcmp(buffer, "DELETE_COMPLETE") == 0) {
        printf("File deletion completed successfully\n");
    } else {
        printf("File deletion failed: %s\n", buffer);
    }
}

// Function to handle downltar command
void handle_downltar_command(int server_socket, char* command) {
    char buffer[BUFFER_SIZE];
    long file_size;
    FILE* file;
    int bytes_received;
    char* token;
    char filetype[10];
    char tar_filename[20];
    
    // Parse command
    token = strtok(command, " ");
    token = strtok(NULL, " "); // Skip "downltar"
    strcpy(filetype, token);
    
    // Determine tar filename
    if (strcmp(filetype, ".c") == 0) {
        strcpy(tar_filename, "cfiles.tar");
    } else if (strcmp(filetype, ".pdf") == 0) {
        strcpy(tar_filename, "pdf.tar");
    } else if (strcmp(filetype, ".txt") == 0) {
        strcpy(tar_filename, "text.tar");
    }
    
    // Send command to server
    send(server_socket, command, strlen(command), 0);
    
    // Receive file size from server
    recv(server_socket, &file_size, sizeof(file_size), 0);
    
    if (file_size > 0) {
        // Create tar file in current directory
        file = fopen(tar_filename, "wb");
        if (file != NULL) {
            long total_received = 0;
            while (total_received < file_size) {
                bytes_received = recv(server_socket, buffer, BUFFER_SIZE, 0);
                if (bytes_received <= 0) break;
                fwrite(buffer, 1, bytes_received, file);
                total_received += bytes_received;
            }
            fclose(file);
            printf("Tar file '%s' downloaded successfully\n", tar_filename);
        } else {
            printf("Error: Cannot create tar file '%s'\n", tar_filename);
        }
    } else {
        printf("Error: Tar file creation failed on server\n");
    }
    
    // Receive final response
    memset(buffer, 0, BUFFER_SIZE);
    recv(server_socket, buffer, BUFFER_SIZE, 0);
    
    if (strcmp(buffer, "TAR_COMPLETE") == 0) {
        printf("Tar download completed successfully\n");
    } else {
        printf("Tar download failed: %s\n", buffer);
    }
}

// Function to handle dispfnames command
void handle_dispfnames_command(int server_socket, char* command) {
    char buffer[BUFFER_SIZE];
    
    // Send command to server
    send(server_socket, command, strlen(command), 0);
    
    // Receive file list from server
    memset(buffer, 0, BUFFER_SIZE);
    recv(server_socket, buffer, BUFFER_SIZE, 0);
    
    printf("Files in the specified directory:\n");
    printf("%s", buffer);
}

int main() {
    int server_socket;
    char command[MAX_COMMAND];
    
    printf("s25client - Distributed File System Client\n");
    printf("Available commands:\n");
    printf("  uploadf filename1 filename2 filename3 destination_path\n");
    printf("  downlf filename1 filename2\n");
    printf("  removef filename1 filename2\n");
    printf("  downltar filetype (.c/.pdf/.txt)\n");
    printf("  dispfnames pathname\n");
    printf("  quit\n");
    printf("Enter 'quit' to exit\n\n");
    
    // Connect to S1 server
    server_socket = connect_to_server();
    if (server_socket < 0) {
        printf("Failed to connect to S1 server. Make sure S1 is running.\n");
        return 1;
    }
    
    printf("Connected to S1 server successfully!\n");
    printf("s25client$ ");
    
    // Main command loop
    while (1) {
        // Get command from user
        if (fgets(command, MAX_COMMAND, stdin) == NULL) {
            break;
        }
        
        // Remove newline
        command[strcspn(command, "\n")] = 0;
        
        // Check for quit command
        if (strcmp(command, "quit") == 0) {
            send(server_socket, command, strlen(command), 0);
            printf("Disconnecting from server...\n");
            break;
        }
        
        // Validate command syntax
        if (!validate_command_syntax(command)) {
            printf("s25client$ ");
            continue;
        }
        
        // Process command based on first word
        if (strncmp(command, "uploadf", 7) == 0) {
            handle_uploadf_command(server_socket, command);
        } else if (strncmp(command, "downlf", 6) == 0) {
            handle_downlf_command(server_socket, command);
        } else if (strncmp(command, "removef", 7) == 0) {
            handle_removef_command(server_socket, command);
        } else if (strncmp(command, "downltar", 8) == 0) {
            handle_downltar_command(server_socket, command);
        } else if (strncmp(command, "dispfnames", 10) == 0) {
            handle_dispfnames_command(server_socket, command);
        } else {
            printf("Unknown command. Type 'quit' to exit.\n");
        }
        
        printf("s25client$ ");
    }
    
    close(server_socket);
    return 0;
}
