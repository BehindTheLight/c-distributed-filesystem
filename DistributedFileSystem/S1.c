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
#include <sys/wait.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_PATH 256
#define MAX_COMMAND 512

// Server ports
#define S2_PORT 8081
#define S3_PORT 8082
#define S4_PORT 8083

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

// Function to connect to a server
int connect_to_server(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        return -1;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return -1;
    }
    
    return sock;
}

// Function to send file to another server
int send_file_to_server(int server_socket, const char* source_filepath, const char* filename) {
    char buffer[BUFFER_SIZE];
    FILE* source_file;
    long file_size;
    int bytes_read;
    
    // Open source file
    source_file = fopen(source_filepath, "rb");
    if (source_file == NULL) {
        printf("Error: Cannot open source file %s\n", source_filepath);
        return -1;
    }
    
    // Get file size
    fseek(source_file, 0, SEEK_END);
    file_size = ftell(source_file);
    fseek(source_file, 0, SEEK_SET);
    
    // Send filepath (destination path on target server)
    send(server_socket, source_filepath, strlen(source_filepath), 0);
    
    // Send filename
    send(server_socket, filename, strlen(filename), 0);
    
    // Send file size
    send(server_socket, &file_size, sizeof(file_size), 0);
    
    // Send file data
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, source_file)) > 0) {
        send(server_socket, buffer, bytes_read, 0);
    }
    
    fclose(source_file);
    
    // Receive response
    memset(buffer, 0, BUFFER_SIZE);
    recv(server_socket, buffer, BUFFER_SIZE, 0);
    
    if (strcmp(buffer, "SUCCESS") == 0) {
        return 0;
    } else {
        return -1;
    }
}

// Function to receive file from another server
int receive_file_from_server(int server_socket, const char* filepath) {
    char buffer[BUFFER_SIZE];
    char filename[MAX_PATH];
    long file_size;
    FILE* file;
    int bytes_received;
    
    // Send filepath
    send(server_socket, filepath, strlen(filepath), 0);
    
    // Receive filename
    memset(buffer, 0, BUFFER_SIZE);
    recv(server_socket, buffer, BUFFER_SIZE, 0);
    strcpy(filename, buffer);
    
    // Receive file size
    recv(server_socket, &file_size, sizeof(file_size), 0);
    
    // Open file for writing
    file = fopen(filepath, "wb");
    if (file == NULL) {
        printf("Error: Cannot create file %s\n", filepath);
        return -1;
    }
    
    // Receive file data
    long total_received = 0;
    while (total_received < file_size) {
        bytes_received = recv(server_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) break;
        fwrite(buffer, 1, bytes_received, file);
        total_received += bytes_received;
    }
    
    fclose(file);
    return 0;
}

// Function to handle uploadf command
void handle_uploadf_command(int client_socket, char* command) {
    char* command_token;
    char source_filenames[3][MAX_PATH];
    char destination_directory[MAX_PATH];
    int number_of_files = 0;
    char data_buffer[BUFFER_SIZE];
    FILE* source_file_handle;
    FILE* destination_file_handle;
    long file_size_bytes;
    int bytes_transferred;
    
    // Parse command
    command_token = strtok(command, " ");
    command_token = strtok(NULL, " "); // Skip "uploadf"
    
    // Get filenames (up to 3)
    while (command_token != NULL && number_of_files < 3) {
        if (strstr(command_token, "~S1") == NULL) {
            strcpy(source_filenames[number_of_files], command_token);
            number_of_files++;
        } else {
            strcpy(destination_directory, command_token);
            break;
        }
        command_token = strtok(NULL, " ");
    }
    
    // Get destination path if not found yet
    if (strlen(destination_directory) == 0) {
        command_token = strtok(NULL, " ");
        if (command_token != NULL) {
            strcpy(destination_directory, command_token);
        }
    }
    
    // Replace ~S1 with actual path
    if (strstr(destination_directory, "~S1") != NULL) {
        char* home_directory = getenv("HOME");
        char temp_path_buffer[MAX_PATH];
        strcpy(temp_path_buffer, destination_directory);
        strcpy(destination_directory, home_directory);
        strcat(destination_directory, "/S1");
        strcat(destination_directory, strstr(temp_path_buffer, "/S1") + 3);
    }
    
    // Create destination directory if it doesn't exist
    char* last_slash_position = strrchr(destination_directory, '/');
    if (last_slash_position) {
        *last_slash_position = '\0';
        create_directory_if_not_exists(destination_directory);
        *last_slash_position = '/';
    }
    
    // Process each file
    for (int file_index = 0; file_index < number_of_files; file_index++) {
        char* file_extension = get_file_extension(source_filenames[file_index]);
        char complete_destination_path[MAX_PATH];
        snprintf(complete_destination_path, MAX_PATH, "%s/%s", destination_directory, source_filenames[file_index]);
        
        // Receive file from client
        recv(client_socket, &file_size_bytes, sizeof(file_size_bytes), 0);
        
        if (file_size_bytes > 0) {
            // Create temporary file to receive from client
            char temporary_file_path[MAX_PATH];
            snprintf(temporary_file_path, MAX_PATH, "/tmp/temp_upload_%d", file_index);
            
            FILE* temporary_file_handle = fopen(temporary_file_path, "wb");
            if (temporary_file_handle == NULL) {
                printf("Error: Cannot create temporary file\n");
                send(client_socket, "ERROR", 5, 0);
                continue;
            }
            
            // Receive file data from client
            long total_bytes_received = 0;
            while (total_bytes_received < file_size_bytes) {
                bytes_transferred = recv(client_socket, data_buffer, BUFFER_SIZE, 0);
                if (bytes_transferred <= 0) break;
                fwrite(data_buffer, 1, bytes_transferred, temporary_file_handle);
                total_bytes_received += bytes_transferred;
            }
            fclose(temporary_file_handle);
            
            // Send success confirmation to client
            send(client_socket, "SUCCESS", 7, 0);
            
            // Handle file based on extension
            if (strcmp(file_extension, "c") == 0) {
                // Store .c files locally
                source_file_handle = fopen(temporary_file_path, "rb");
                destination_file_handle = fopen(complete_destination_path, "wb");
                
                while ((bytes_transferred = fread(data_buffer, 1, BUFFER_SIZE, source_file_handle)) > 0) {
                    fwrite(data_buffer, 1, bytes_transferred, destination_file_handle);
                }
                
                fclose(source_file_handle);
                fclose(destination_file_handle);
                printf("File %s stored locally in S1\n", source_filenames[file_index]);
                
            } else if (strcmp(file_extension, "pdf") == 0) {
                // Send to S2
                int s2_server_socket = connect_to_server(S2_PORT);
                if (s2_server_socket >= 0) {
                    send(s2_server_socket, "UPLOAD", 6, 0);
                    if (send_file_to_server(s2_server_socket, temporary_file_path, source_filenames[file_index]) == 0) {
                        printf("File %s sent to S2\n", source_filenames[file_index]);
                    }
                    close(s2_server_socket);
                }
                
            } else if (strcmp(file_extension, "txt") == 0) {
                // Send to S3
                int s3_server_socket = connect_to_server(S3_PORT);
                if (s3_server_socket >= 0) {
                    send(s3_server_socket, "UPLOAD", 6, 0);
                    if (send_file_to_server(s3_server_socket, temporary_file_path, source_filenames[file_index]) == 0) {
                        printf("File %s sent to S3\n", source_filenames[file_index]);
                    }
                    close(s3_server_socket);
                }
                
            } else if (strcmp(file_extension, "zip") == 0) {
                // Send to S4
                int s4_server_socket = connect_to_server(S4_PORT);
                if (s4_server_socket >= 0) {
                    send(s4_server_socket, "UPLOAD", 6, 0);
                    if (send_file_to_server(s4_server_socket, temporary_file_path, source_filenames[file_index]) == 0) {
                        printf("File %s sent to S4\n", source_filenames[file_index]);
                    }
                    close(s4_server_socket);
                }
            }
            
            // Clean up temporary file
            remove(temporary_file_path);
        }
    }
    
    send(client_socket, "UPLOAD_COMPLETE", 15, 0);
}

// Function to handle downlf command
void handle_downlf_command(int client_socket, char* command) {
    char* command_token;
    char file_paths[2][MAX_PATH];
    int number_of_files = 0;
    char data_buffer[BUFFER_SIZE];
    FILE* file_handle;
    long file_size_bytes;
    int bytes_transferred;
    
    // Parse command
    command_token = strtok(command, " ");
    command_token = strtok(NULL, " "); // Skip "downlf"
    
    // Get filepaths (up to 2)
    while (command_token != NULL && number_of_files < 2) {
        strcpy(file_paths[number_of_files], command_token);
        number_of_files++;
        command_token = strtok(NULL, " ");
    }
    
    // Process each file
    for (int file_index = 0; file_index < number_of_files; file_index++) {
        char* file_extension = get_file_extension(file_paths[file_index]);
        
        if (strcmp(file_extension, "c") == 0) {
            // Handle .c files locally
            file_handle = fopen(file_paths[file_index], "rb");
            if (file_handle == NULL) {
                send(client_socket, "ERROR: File not found", 20, 0);
                continue;
            }
            
            // Get file size
            fseek(file_handle, 0, SEEK_END);
            file_size_bytes = ftell(file_handle);
            fseek(file_handle, 0, SEEK_SET);
            
            // Send file size
            send(client_socket, &file_size_bytes, sizeof(file_size_bytes), 0);
            
            // Send file data
            while ((bytes_transferred = fread(data_buffer, 1, BUFFER_SIZE, file_handle)) > 0) {
                send(client_socket, data_buffer, bytes_transferred, 0);
            }
            
            fclose(file_handle);
            
        } else if (strcmp(file_extension, "pdf") == 0) {
            // Get from S2
            int s2_server_socket = connect_to_server(S2_PORT);
            if (s2_server_socket >= 0) {
                send(s2_server_socket, "DOWNLOAD", 8, 0);
                if (receive_file_from_server(s2_server_socket, file_paths[file_index]) == 0) {
                    // Send file to client
                    file_handle = fopen(file_paths[file_index], "rb");
                    fseek(file_handle, 0, SEEK_END);
                    file_size_bytes = ftell(file_handle);
                    fseek(file_handle, 0, SEEK_SET);
                    
                    send(client_socket, &file_size_bytes, sizeof(file_size_bytes), 0);
                    
                    while ((bytes_transferred = fread(data_buffer, 1, BUFFER_SIZE, file_handle)) > 0) {
                        send(client_socket, data_buffer, bytes_transferred, 0);
                    }
                    
                    fclose(file_handle);
                    remove(file_paths[file_index]); // Clean up temporary file
                }
                close(s2_server_socket);
            }
            
        } else if (strcmp(file_extension, "txt") == 0) {
            // Get from S3
            int s3_server_socket = connect_to_server(S3_PORT);
            if (s3_server_socket >= 0) {
                send(s3_server_socket, "DOWNLOAD", 8, 0);
                if (receive_file_from_server(s3_server_socket, file_paths[file_index]) == 0) {
                    // Send file to client
                    file_handle = fopen(file_paths[file_index], "rb");
                    fseek(file_handle, 0, SEEK_END);
                    file_size_bytes = ftell(file_handle);
                    fseek(file_handle, 0, SEEK_SET);
                    
                    send(client_socket, &file_size_bytes, sizeof(file_size_bytes), 0);
                    
                    while ((bytes_transferred = fread(data_buffer, 1, BUFFER_SIZE, file_handle)) > 0) {
                        send(client_socket, data_buffer, bytes_transferred, 0);
                    }
                    
                    fclose(file_handle);
                    remove(file_paths[file_index]); // Clean up temporary file
                }
                close(s3_server_socket);
            }
            
        } else if (strcmp(file_extension, "zip") == 0) {
            // Get from S4
            int s4_server_socket = connect_to_server(S4_PORT);
            if (s4_server_socket >= 0) {
                send(s4_server_socket, "DOWNLOAD", 8, 0);
                if (receive_file_from_server(s4_server_socket, file_paths[file_index]) == 0) {
                    // Send file to client
                    file_handle = fopen(file_paths[file_index], "rb");
                    fseek(file_handle, 0, SEEK_END);
                    file_size_bytes = ftell(file_handle);
                    fseek(file_handle, 0, SEEK_SET);
                    
                    send(client_socket, &file_size_bytes, sizeof(file_size_bytes), 0);
                    
                    while ((bytes_transferred = fread(data_buffer, 1, BUFFER_SIZE, file_handle)) > 0) {
                        send(client_socket, data_buffer, bytes_transferred, 0);
                    }
                    
                    fclose(file_handle);
                    remove(file_paths[file_index]); // Clean up temporary file
                }
                close(s4_server_socket);
            }
        }
    }
    
    send(client_socket, "DOWNLOAD_COMPLETE", 17, 0);
}

// Function to handle removef command
void handle_removef_command(int client_socket, char* command) {
    char* command_token;
    char file_paths[2][MAX_PATH];
    int number_of_files = 0;
    
    // Parse command
    command_token = strtok(command, " ");
    command_token = strtok(NULL, " "); // Skip "removef"
    
    // Get filepaths (up to 2)
    while (command_token != NULL && number_of_files < 2) {
        strcpy(file_paths[number_of_files], command_token);
        number_of_files++;
        command_token = strtok(NULL, " ");
    }
    
    // Process each file
    for (int file_index = 0; file_index < number_of_files; file_index++) {
        char* file_extension = get_file_extension(file_paths[file_index]);
        
        if (strcmp(file_extension, "c") == 0) {
            // Delete .c files locally
            if (remove(file_paths[file_index]) == 0) {
                printf("File %s deleted from S1\n", file_paths[file_index]);
            } else {
                printf("Error deleting file %s\n", file_paths[file_index]);
            }
            
        } else if (strcmp(file_extension, "pdf") == 0) {
            // Request S2 to delete
            int s2_server_socket = connect_to_server(S2_PORT);
            if (s2_server_socket >= 0) {
                send(s2_server_socket, "DELETE", 6, 0);
                send(s2_server_socket, file_paths[file_index], strlen(file_paths[file_index]), 0);
                close(s2_server_socket);
            }
            
        } else if (strcmp(file_extension, "txt") == 0) {
            // Request S3 to delete
            int s3_server_socket = connect_to_server(S3_PORT);
            if (s3_server_socket >= 0) {
                send(s3_server_socket, "DELETE", 6, 0);
                send(s3_server_socket, file_paths[file_index], strlen(file_paths[file_index]), 0);
                close(s3_server_socket);
            }
            
        } else if (strcmp(file_extension, "zip") == 0) {
            // Request S4 to delete
            int s4_server_socket = connect_to_server(S4_PORT);
            if (s4_server_socket >= 0) {
                send(s4_server_socket, "DELETE", 6, 0);
                send(s4_server_socket, file_paths[file_index], strlen(file_paths[file_index]), 0);
                close(s4_server_socket);
            }
        }
    }
    
    send(client_socket, "DELETE_COMPLETE", 15, 0);
}

// Function to handle downltar command
void handle_downltar_command(int client_socket, char* command) {
    char* command_token;
    char file_type[10];
    char data_buffer[BUFFER_SIZE];
    FILE* file_handle;
    long file_size_bytes;
    int bytes_transferred;
    
    // Parse command
    command_token = strtok(command, " ");
    command_token = strtok(NULL, " "); // Skip "downltar"
    strcpy(file_type, command_token);
    
    if (strcmp(file_type, ".c") == 0) {
        // Create tar of .c files locally
        system("cd ~/S1 && tar -cf cfiles.tar $(find . -name '*.c')");
        
        // Send tar file to client
        file_handle = fopen("~/S1/cfiles.tar", "rb");
        if (file_handle == NULL) {
            return;
        }
        
        fseek(file_handle, 0, SEEK_END);
        file_size_bytes = ftell(file_handle);
        fseek(file_handle, 0, SEEK_SET);
        
        send(client_socket, &file_size_bytes, sizeof(file_size_bytes), 0);
        
        while ((bytes_transferred = fread(data_buffer, 1, BUFFER_SIZE, file_handle)) > 0) {
            send(client_socket, data_buffer, bytes_transferred, 0);
        }
        
        fclose(file_handle);
        
    } else if (strcmp(file_type, ".pdf") == 0) {
        // Get tar from S2
        int s2_server_socket = connect_to_server(S2_PORT);
        if (s2_server_socket >= 0) {
            send(s2_server_socket, "TAR", 3, 0);
            
            // Receive tar file size
            recv(s2_server_socket, &file_size_bytes, sizeof(file_size_bytes), 0);
            
            // Send file size to client
            send(client_socket, &file_size_bytes, sizeof(file_size_bytes), 0);
            
            // Forward tar file to client
            long total_bytes_received = 0;
            while (total_bytes_received < file_size_bytes) {
                bytes_transferred = recv(s2_server_socket, data_buffer, BUFFER_SIZE, 0);
                if (bytes_transferred <= 0) break;
                send(client_socket, data_buffer, bytes_transferred, 0);
                total_bytes_received += bytes_transferred;
            }
            
            close(s2_server_socket);
        }
        
    } else if (strcmp(file_type, ".txt") == 0) {
        // Get tar from S3
        int s3_server_socket = connect_to_server(S3_PORT);
        if (s3_server_socket >= 0) {
            send(s3_server_socket, "TAR", 3, 0);
            
            // Receive tar file size
            recv(s3_server_socket, &file_size_bytes, sizeof(file_size_bytes), 0);
            
            // Send file size to client
            send(client_socket, &file_size_bytes, sizeof(file_size_bytes), 0);
            
            // Forward tar file to client
            long total_bytes_received = 0;
            while (total_bytes_received < file_size_bytes) {
                bytes_transferred = recv(s3_server_socket, data_buffer, BUFFER_SIZE, 0);
                if (bytes_transferred <= 0) break;
                send(client_socket, data_buffer, bytes_transferred, 0);
                total_bytes_received += bytes_transferred;
            }
            
            close(s3_server_socket);
        }
    }
    
    send(client_socket, "TAR_COMPLETE", 12, 0);
}



// Function to handle dispfnames command
void handle_dispfnames_command(int client_socket, char* command) {
    char* command_token;
    char directory_path[MAX_PATH];
    char data_buffer[BUFFER_SIZE];
    DIR* directory_handle;
    struct dirent* directory_entry;
    char c_files_list[BUFFER_SIZE * 10] = "";
    char pdf_files_list[BUFFER_SIZE * 10] = "";
    char txt_files_list[BUFFER_SIZE * 10] = "";
    char zip_files_list[BUFFER_SIZE * 10] = "";
    char final_file_list[BUFFER_SIZE * 10] = "";
    char temp_list[BUFFER_SIZE];
    
    // Parse command
    command_token = strtok(command, " ");
    command_token = strtok(NULL, " "); // Skip "dispfnames"
    strcpy(directory_path, command_token);
    
    // Replace ~S1 with actual path
    if (strstr(directory_path, "~S1") != NULL) {
        char* home = getenv("HOME");
        char temp_path[MAX_PATH];
        strcpy(temp_path, directory_path);
        strcpy(directory_path, home);
        strcat(directory_path, "/S1");
        strcat(directory_path, strstr(temp_path, "/S1") + 3);
    }
    
    // Get .c files from local directory
    directory_handle = opendir(directory_path);
    if (directory_handle != NULL) {
        while ((directory_entry = readdir(directory_handle)) != NULL) {
            if (directory_entry->d_type == DT_REG) {
                if (strstr(directory_entry->d_name, ".c") != NULL) {
                    snprintf(temp_list, BUFFER_SIZE, "%s\n", directory_entry->d_name);
                    strcat(c_files_list, temp_list);
                }
            }
        }
        closedir(directory_handle);
    }
    
    // Get .pdf files from S2
    int s2_server_socket = connect_to_server(S2_PORT);
    if (s2_server_socket >= 0) {
        send(s2_server_socket, "LIST", 4, 0);
        send(s2_server_socket, directory_path, strlen(directory_path), 0);
        
        memset(data_buffer, 0, BUFFER_SIZE);
        recv(s2_server_socket, data_buffer, BUFFER_SIZE, 0);
        strcat(pdf_files_list, data_buffer);
        
        close(s2_server_socket);
    }
    
    // Get .txt files from S3
    int s3_server_socket = connect_to_server(S3_PORT);
    if (s3_server_socket >= 0) {
        send(s3_server_socket, "LIST", 4, 0);
        send(s3_server_socket, directory_path, strlen(directory_path), 0);
        
        memset(data_buffer, 0, BUFFER_SIZE);
        recv(s3_server_socket, data_buffer, BUFFER_SIZE, 0);
        strcat(txt_files_list, data_buffer);
        
        close(s3_server_socket);
    }
    
    // Get .zip files from S4
    int s4_server_socket = connect_to_server(S4_PORT);
    if (s4_server_socket >= 0) {
        send(s4_server_socket, "LIST", 4, 0);
        send(s4_server_socket, directory_path, strlen(directory_path), 0);
        
        memset(data_buffer, 0, BUFFER_SIZE);
        recv(s4_server_socket, data_buffer, BUFFER_SIZE, 0);
        strcat(zip_files_list, data_buffer);
        
        close(s4_server_socket);
    }
    

    
    // Combine in correct order: .c, .pdf, .txt, .zip
    strcat(final_file_list, c_files_list);
    strcat(final_file_list, pdf_files_list);
    strcat(final_file_list, txt_files_list);
    strcat(final_file_list, zip_files_list);
    
    // Send combined file list to client
    send(client_socket, final_file_list, strlen(final_file_list), 0);
}

// Function to process client requests (prcclient function)
void prcclient(int client_socket) {
    char command[MAX_COMMAND];
    int bytes_received;
    
    printf("Client connected, starting prcclient() function\n");
    
    // Infinite loop waiting for client commands
    while (1) {
        // Receive command from client
        memset(command, 0, MAX_COMMAND);
        bytes_received = recv(client_socket, command, MAX_COMMAND, 0);
        
        if (bytes_received <= 0) {
            printf("Client disconnected\n");
            break;
        }
        
        printf("Received command: %s\n", command);
        
        // Process command based on first word
        if (strncmp(command, "uploadf", 7) == 0) {
            handle_uploadf_command(client_socket, command);
        } else if (strncmp(command, "downlf", 6) == 0) {
            handle_downlf_command(client_socket, command);
        } else if (strncmp(command, "removef", 7) == 0) {
            handle_removef_command(client_socket, command);
        } else if (strncmp(command, "downltar", 8) == 0) {
            handle_downltar_command(client_socket, command);
        } else if (strncmp(command, "dispfnames", 10) == 0) {
            handle_dispfnames_command(client_socket, command);
        } else if (strncmp(command, "quit", 4) == 0) {
            printf("Client requested quit\n");
            break;
        } else {
            printf("Unknown command: %s\n", command);
            send(client_socket, "UNKNOWN_COMMAND", 14, 0);
        }
    }
    
    close(client_socket);
    exit(0);
}

// Signal handler for zombie processes
void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pid_t child_pid;
    
    // Set up signal handler for zombie processes
    signal(SIGCHLD, sigchld_handler);
    
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
    
    printf("S1 Server started on port %d\n", PORT);
    printf("Waiting for client connections...\n");
    
    // Accept connections and fork child processes
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }
        
        printf("New client connection accepted\n");
        
        // Fork child process
        child_pid = fork();
        
        if (child_pid == 0) {
            // Child process
            close(server_socket); // Close server socket in child
            prcclient(client_socket); // Call prcclient function
        } else if (child_pid > 0) {
            // Parent process
            close(client_socket); // Close client socket in parent
            printf("Child process %d created for client\n", child_pid);
        } else {
            // Fork failed
            perror("Fork failed");
            close(client_socket);
        }
    }
    
    close(server_socket);
    return 0;
}
