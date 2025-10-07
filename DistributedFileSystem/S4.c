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

#define PORT 8083
#define BUFFER_SIZE 1024
#define MAX_PATH 256

// Function to create directory if it doesn't exist
void create_directory_if_not_exists(const char* path) {
    char temp_path[MAX_PATH];
    char* token;
    char* path_copy = strdup(path);
    
    // Create directories one by one
    token = strtok(path_copy, "/");
    strcpy(temp_path, "");
    
    while (token != NULL) {
        strcat(temp_path, "/");
        strcat(temp_path, token);
        mkdir(temp_path, 0755);
        token = strtok(NULL, "/");
    }
    
    free(path_copy);
}

// Function to get file extension
char* get_file_extension(const char* filename) {
    char* dot = strrchr(filename, '.');
    if (dot && dot != filename) {
        return dot + 1;
    }
    return "";
}

// Function to handle file upload from S1
void handle_file_upload(int client_socket) {
    char buffer[BUFFER_SIZE];
    char filepath[MAX_PATH];
    char filename[MAX_PATH];
    long file_size;
    FILE* file;
    int bytes_received;
    
    // Receive filepath
    memset(buffer, 0, BUFFER_SIZE);
    recv(client_socket, buffer, BUFFER_SIZE, 0);
    strcpy(filepath, buffer);
    
    // Replace S1 path with S4 path
    if (strstr(filepath, "/S1/") != NULL) {
        char* home = getenv("HOME");
        char temp_path[MAX_PATH];
        strcpy(temp_path, filepath);
        strcpy(filepath, home);
        strcat(filepath, "/S4");
        strcat(filepath, strstr(temp_path, "/S1/") + 4);
    }
    
    // Create directory if it doesn't exist
    char* last_slash = strrchr(filepath, '/');
    if (last_slash) {
        *last_slash = '\0';
        create_directory_if_not_exists(filepath);
        *last_slash = '/';
    }
    
    // Receive filename
    memset(buffer, 0, BUFFER_SIZE);
    recv(client_socket, buffer, BUFFER_SIZE, 0);
    strcpy(filename, buffer);
    
    // Receive file size
    recv(client_socket, &file_size, sizeof(file_size), 0);
    
    // Open file for writing
    file = fopen(filepath, "wb");
    if (file == NULL) {
        printf("Error: Cannot create file %s\n", filepath);
        send(client_socket, "ERROR", 5, 0);
        return;
    }
    
    // Receive file data
    long total_received = 0;
    while (total_received < file_size) {
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) break;
        fwrite(buffer, 1, bytes_received, file);
        total_received += bytes_received;
    }
    
    fclose(file);
    send(client_socket, "SUCCESS", 7, 0);
    printf("File uploaded successfully: %s\n", filepath);
}

// Function to handle file download for S1
void handle_file_download(int client_socket) {
    char buffer[BUFFER_SIZE];
    char filepath[MAX_PATH];
    FILE* file;
    long file_size;
    int bytes_read;
    
    // Receive filepath
    memset(buffer, 0, BUFFER_SIZE);
    recv(client_socket, buffer, BUFFER_SIZE, 0);
    strcpy(filepath, buffer);
    
    // Replace S1 path with S4 path
    if (strstr(filepath, "/S1/") != NULL) {
        char* home = getenv("HOME");
        char temp_path[MAX_PATH];
        strcpy(temp_path, filepath);
        strcpy(filepath, home);
        strcat(filepath, "/S4");
        strcat(filepath, strstr(temp_path, "/S1/") + 4);
    }
    
    // Check if file exists
    file = fopen(filepath, "rb");
    if (file == NULL) {
        printf("Error: File not found %s\n", filepath);
        send(client_socket, "ERROR", 5, 0);
        return;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Send file size
    send(client_socket, &file_size, sizeof(file_size), 0);
    
    // Send file data
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }
    
    fclose(file);
    printf("File downloaded successfully: %s\n", filepath);
}

// Function to handle file deletion
void handle_file_deletion(int client_socket) {
    char buffer[BUFFER_SIZE];
    char filepath[MAX_PATH];
    
    // Receive filepath
    memset(buffer, 0, BUFFER_SIZE);
    recv(client_socket, buffer, BUFFER_SIZE, 0);
    strcpy(filepath, buffer);
    
    // Replace S1 path with S4 path
    if (strstr(filepath, "/S1/") != NULL) {
        char* home = getenv("HOME");
        char temp_path[MAX_PATH];
        strcpy(temp_path, filepath);
        strcpy(filepath, home);
        strcat(filepath, "/S4");
        strcat(filepath, strstr(temp_path, "/S1/") + 4);
    }
    
    // Delete file
    if (remove(filepath) == 0) {
        printf("File deleted successfully: %s\n", filepath);
        send(client_socket, "SUCCESS", 7, 0);
    } else {
        printf("Error: Cannot delete file %s\n", filepath);
        send(client_socket, "ERROR", 5, 0);
    }
}

// Function to create tar file of all .zip files
void handle_tar_creation(int client_socket) {
    char buffer[BUFFER_SIZE];
    char command[MAX_PATH];
    char tar_filename[] = "zip.tar";
    
    // Create tar file of all .zip files in ~/S4
    snprintf(command, MAX_PATH, "cd ~/S4 && tar -cf %s $(find . -name '*.zip')", tar_filename);
    system(command);
    
    // Send tar file
    FILE* tar_file = fopen("~/S4/zip.tar", "rb");
    if (tar_file == NULL) {
        printf("Error: Cannot create tar file\n");
        send(client_socket, "ERROR", 5, 0);
        return;
    }
    
    // Get file size
    fseek(tar_file, 0, SEEK_END);
    long file_size = ftell(tar_file);
    fseek(tar_file, 0, SEEK_SET);
    
    // Send file size
    send(client_socket, &file_size, sizeof(file_size), 0);
    
    // Send file data
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, tar_file)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }
    
    fclose(tar_file);
    printf("Tar file created and sent successfully\n");
}

// Function to list all .zip files in a directory
void handle_file_listing(int client_socket) {
    char buffer[BUFFER_SIZE];
    char dirpath[MAX_PATH];
    DIR* dir;
    struct dirent* entry;
    char file_list[BUFFER_SIZE * 10] = "";
    char temp_list[BUFFER_SIZE];
    
    // Receive directory path
    memset(buffer, 0, BUFFER_SIZE);
    recv(client_socket, buffer, BUFFER_SIZE, 0);
    strcpy(dirpath, buffer);
    
    // Replace S1 path with S4 path
    if (strstr(dirpath, "/S1/") != NULL) {
        char* home = getenv("HOME");
        char temp_path[MAX_PATH];
        strcpy(temp_path, dirpath);
        strcpy(dirpath, home);
        strcat(dirpath, "/S4");
        strcat(dirpath, strstr(temp_path, "/S1/") + 4);
    }
    
    // Open directory
    dir = opendir(dirpath);
    if (dir == NULL) {
        printf("Error: Cannot open directory %s\n", dirpath);
        send(client_socket, "ERROR", 5, 0);
        return;
    }
    
    // Read directory entries
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { // Regular file
            if (strstr(entry->d_name, ".zip") != NULL) {
                snprintf(temp_list, BUFFER_SIZE, "%s\n", entry->d_name);
                strcat(file_list, temp_list);
            }
        }
    }
    
    closedir(dir);
    
    // Send file list
    send(client_socket, file_list, strlen(file_list), 0);
    printf("File list sent for directory: %s\n", dirpath);
}

// Function to handle client requests
void handle_client(int client_socket) {
    char command[BUFFER_SIZE];
    int bytes_received;
    
    while (1) {
        // Receive command from S1
        memset(command, 0, BUFFER_SIZE);
        bytes_received = recv(client_socket, command, BUFFER_SIZE, 0);
        
        if (bytes_received <= 0) {
            printf("Client disconnected\n");
            break;
        }
        
        printf("Received command: %s\n", command);
        
        // Process command
        if (strcmp(command, "UPLOAD") == 0) {
            handle_file_upload(client_socket);
        } else if (strcmp(command, "DOWNLOAD") == 0) {
            handle_file_download(client_socket);
        } else if (strcmp(command, "DELETE") == 0) {
            handle_file_deletion(client_socket);
        } else if (strcmp(command, "TAR") == 0) {
            handle_tar_creation(client_socket);
        } else if (strcmp(command, "LIST") == 0) {
            handle_file_listing(client_socket);
        } else if (strcmp(command, "QUIT") == 0) {
            printf("Client requested quit\n");
            break;
        } else {
            printf("Unknown command: %s\n", command);
            send(client_socket, "UNKNOWN_COMMAND", 14, 0);
        }
    }
    
    close(client_socket);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Bind socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if (listen(server_socket, 5) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("S4 Server started on port %d\n", PORT);
    printf("Waiting for connections from S1...\n");
    
    // Accept connections
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }
        
        printf("Connection accepted from S1\n");
        
        // Handle client in same process (simple approach)
        handle_client(client_socket);
    }
    
    close(server_socket);
    return 0;
}
