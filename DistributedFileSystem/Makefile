# Makefile for Distributed File System Project
# Compiles all 5 C programs: S1.c, S2.c, S3.c, S4.c, s25client.c

CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGETS = S1 S2 S3 S4 s25client

# Default target
all: $(TARGETS)

# Compile S1 (main server)
S1: S1.c
	$(CC) $(CFLAGS) -o S1 S1.c

# Compile S2 (PDF file server)
S2: S2.c
	$(CC) $(CFLAGS) -o S2 S2.c

# Compile S3 (TXT file server)
S3: S3.c
	$(CC) $(CFLAGS) -o S3 S3.c

# Compile S4 (ZIP file server)
S4: S4.c
	$(CC) $(CFLAGS) -o S4 S4.c

# Compile s25client (client application)
s25client: s25client.c
	$(CC) $(CFLAGS) -o s25client s25client.c

# Clean compiled files
clean:
	rm -f $(TARGETS)

# Install target (create directories)
install:
	mkdir -p ~/S1 ~/S2 ~/S3 ~/S4

# Help target
help:
	@echo "Available targets:"
	@echo "  all      - Compile all programs (default)"
	@echo "  S1       - Compile main server"
	@echo "  S2       - Compile PDF file server"
	@echo "  S3       - Compile TXT file server"
	@echo "  S4       - Compile ZIP file server"
	@echo "  s25client- Compile client application"
	@echo "  clean    - Remove compiled programs"
	@echo "  install  - Create required directories"
	@echo "  help     - Show this help message"

.PHONY: all clean install help
