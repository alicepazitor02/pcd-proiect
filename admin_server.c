#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#define SOCKET_PATH "/tmp/admin_socket"
#define DATABASE_FILE "database.txt"
#define BUFFER_SIZE 4096

typedef struct {
    time_t timestamp;
    char *record;
} Record;

// Function to compare records based on timestamp
int compare_records(const void *a, const void *b) {
    Record *recA = (Record *)a;
    Record *recB = (Record *)b;
    return difftime(recA->timestamp, recB->timestamp);
}

// Function to parse the timestamp from the record
time_t parse_timestamp(const char *str) {
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));
    strptime(str, "%Y-%m-%d %H:%M:%S", &tm);
    return mktime(&tm);
}

void handle_admin_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Read the filter choice from the client
    bytes_read = read(client_socket, buffer, BUFFER_SIZE);
    if (bytes_read <= 0) {
        perror("Error reading filter choice from client");
        close(client_socket);
        return;
    }

    int choice = atoi(buffer);
    char date_filter[20] = ""; // To store the date if filtering by date

    if (choice == 2) {
        // Read the date from the client
        bytes_read = read(client_socket, date_filter, sizeof(date_filter));
        if (bytes_read <= 0) {
            perror("Error reading date from client");
            close(client_socket);
            return;
        }
    }

    // Read data from the database file
    int db_fd = open(DATABASE_FILE, O_RDONLY);
    if (db_fd == -1) {
        perror("Error opening database file");
        close(client_socket);
        return;
    }

    // Store records in a dynamic array
    Record *records = NULL;
    size_t record_count = 0;
    while ((bytes_read = read(db_fd, buffer, BUFFER_SIZE)) > 0) {
        // Split the buffer into lines (records)
        char *line = strtok(buffer, "\n");
        while (line != NULL) {
            // Find timestamp in the record
            char *timestamp_str = strstr(line, "Timestamp: ");
            if (timestamp_str == NULL) {
                // Try finding the timestamp at the beginning (alternative format)
                timestamp_str = line;
            } else {
                timestamp_str += 11; // Move past "Timestamp: "
            }

            // Parse the timestamp
            time_t timestamp = parse_timestamp(timestamp_str);

            // Check if the record matches the date filter
            if (choice != 2 || (choice == 2 && strncmp(timestamp_str, date_filter, 10) == 0)) {
                // Store the record
                records = realloc(records, (record_count + 1) * sizeof(Record));
                records[record_count].timestamp = timestamp;
                records[record_count].record = strdup(line);
                record_count++;
            }
            line = strtok(NULL, "\n");
        }
    }
    close(db_fd);

    // Sort records by timestamp if required
    if (choice == 2) {
        qsort(records, record_count, sizeof(Record), compare_records);
    }

    // Send the records to the client
    for (size_t i = 0; i < record_count; i++) {
        write(client_socket, records[i].record, strlen(records[i].record));
        write(client_socket, "\n", 1);
        free(records[i].record);
    }
    free(records);

    close(client_socket);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_un server_addr, client_addr;
    socklen_t client_len;

    // Create Unix socket
    server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Bind socket to a path
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, SOCKET_PATH);
    unlink(SOCKET_PATH); // Remove any previous socket with the same path
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_socket, 5) == -1) {
        perror("Error listening for connections");
        exit(EXIT_FAILURE);
    }

    printf("Admin server listening on Unix socket: %s\n", SOCKET_PATH);

    // Accept and handle client connections
    while (1) {
        client_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("Error accepting client connection");
            continue;
        }
        handle_admin_client(client_socket);
    }

    // Close server socket
    close(server_socket);
    unlink(SOCKET_PATH);

    return 0;
}
