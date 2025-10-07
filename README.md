# Distributed File System

A distributed file system implementation using C programming and socket communication, designed for educational purposes and demonstrating basic distributed computing concepts.

## System Architecture

The system consists of **4 servers** and **1 client** that work together to provide a distributed file storage solution:

- **S1 (Main Server)**: Primary server that handles client connections and coordinates file distribution
- **S2 (PDF Server)**: Dedicated server for storing PDF files
- **S3 (Text Server)**: Dedicated server for storing text files
- **S4 (ZIP Server)**: Dedicated server for storing ZIP files
- **s25client**: Client application for user interaction

## File Distribution Logic

Files are automatically distributed based on their extensions:

| File Type | Extension | Storage Location |
|-----------|-----------|------------------|
| C Source Files | `.c` | S1 (Main Server) |
| PDF Documents | `.pdf` | S2 (PDF Server) |
| Text Files | `.txt` | S3 (Text Server) |
| ZIP Archives | `.zip` | S4 (ZIP Server) |

## Features

- **Multi-client Support**: S1 uses `fork()` to handle multiple concurrent client connections
- **Automatic File Distribution**: Files are automatically routed to appropriate servers
- **Transparent Access**: Clients interact only with S1, unaware of the distributed nature
- **File Operations**: Upload, download, delete, and list files across the distributed system
- **Tar Archive Creation**: Create compressed archives of specific file types
- **Directory Management**: Automatic creation of directory structures

## Prerequisites

- **Operating System**: Linux/Unix environment
- **Compiler**: GCC with C99 support
- **Network**: All servers and client must be able to communicate via sockets
- **Permissions**: Write access to home directory for creating server directories

## Installation

1. **Clone the repository**:
   ```bash
   git clone <repository-url>
   cd DistributedFileSystem
   ```

2. **Compile the project**:
   ```bash
   make all
   ```

3. **Create required directories**:
   ```bash
   make install
   ```

## Usage

### Starting the Servers

Start each server in separate terminals:

```bash
# Terminal 1 - Main Server (S1)
./S1

# Terminal 2 - PDF Server (S2)
./S2

# Terminal 3 - Text Server (S3)
./S3

# Terminal 4 - ZIP Server (S4)
./S4
```

### Running the Client

```bash
# Terminal 5 - Client
./s25client
```

## Available Commands

### 1. Upload Files (`uploadf`)
Upload up to 3 files to the distributed system:
```bash
uploadf file1.c file2.pdf file3.txt ~S1/destination/path
```

### 2. Download Files (`downlf`)
Download up to 2 files from the system:
```bash
downlf ~S1/path/file1.c ~S1/path/file2.pdf
```

### 3. Remove Files (`removef`)
Delete up to 2 files from the system:
```bash
removef ~S1/path/file1.c ~S1/path/file2.pdf
```

### 4. Download Tar Archive (`downltar`)
Create and download a tar archive of specific file type:
```bash
downltar .c    # Downloads cfiles.tar
downltar .pdf  # Downloads pdf.tar
downltar .txt  # Downloads text.tar
```

### 5. Display File Names (`dispfnames`)
List all files in a directory:
```bash
dispfnames ~S1/directory/path
```

## Configuration

### Port Configuration
- **S1 (Main Server)**: Port 8080
- **S2 (PDF Server)**: Port 8081
- **S3 (Text Server)**: Port 8082
- **S4 (ZIP Server)**: Port 8083

### Directory Structure
```
~/S1/          # Main server files (.c files)
~/S2/          # PDF server files
~/S3/          # Text server files
~/S4/          # ZIP server files
```

## Testing

1. **Start all servers** in separate terminals
2. **Run the client** and test each command
3. **Verify file distribution** by checking the appropriate server directories
4. **Test concurrent connections** by running multiple clients

### Example Test Sequence
```bash
# Upload files
uploadf sample.c document.pdf readme.txt ~S1/test/

# List files
dispfnames ~S1/test/

# Download files
downlf ~S1/test/sample.c ~S1/test/document.pdf

# Create tar archive
downltar .c

# Remove files
removef ~S1/test/sample.c
```

## Project Structure

```
DistributedFileSystem/
├── S1.c              # Main server implementation
├── S2.c              # PDF server implementation
├── S3.c              # Text server implementation
├── S4.c              # ZIP server implementation
├── s25client.c       # Client application
├── Makefile          # Build configuration
└── README.md         # This file
```

## Technical Details

### Socket Communication
- **Protocol**: TCP sockets
- **Address**: 127.0.0.1 (localhost)
- **Communication**: Bidirectional client-server communication

### Process Management
- **Forking**: S1 forks child processes for each client connection
- **Process Isolation**: Each client connection runs in its own process
- **Signal Handling**: Proper cleanup of child processes

### File Operations
- **Binary Transfer**: Files are transferred in binary mode
- **Chunked Transfer**: Large files are transferred in chunks
- **Error Handling**: Basic error checking and validation

## Troubleshooting

### Common Issues

1. **Port Already in Use**:
   ```bash
   # Check if ports are in use
   netstat -tulpn | grep :808
   
   # Kill processes using the ports
   sudo kill -9 <PID>
   ```

2. **Permission Denied**:
   ```bash
   # Make sure directories are writable
   chmod 755 ~/S1 ~/S2 ~/S3 ~/S4
   ```

3. **Connection Refused**:
   - Ensure all servers are running
   - Check firewall settings
   - Verify port availability

### Debug Mode
Compile with debug flags for detailed output:
```bash
make debug
```

## Educational Value

This project demonstrates:
- **Socket Programming**: Network communication between processes
- **Process Management**: Forking and child process handling
- **File System Operations**: File I/O and directory management
- **Distributed Systems**: Basic concepts of distributed file storage
- **C Programming**: System programming and network protocols

## Author

**Your Name**
- GitHub: [@yourusername](https://github.com/yourusername)
- Email: your.email@example.com

## Acknowledgments

- Inspired by distributed systems concepts
- Built for educational purposes
- Uses standard C libraries and system calls

---

**Note**: This is an educational project designed to demonstrate basic distributed computing concepts. For production use, additional security, error handling, and performance optimizations would be required.