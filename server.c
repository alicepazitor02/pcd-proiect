#include <stdio.h>          // for printf(), perror()
#include <stdlib.h>         // for exit(), malloc(), free()
#include <string.h>         // for memset(), strlen(), strchr(), strrchr()
#include <unistd.h>         // for close(), read(), write()
#include <arpa/inet.h>      // for sockaddr_in, AF_INET, SOCK_STREAM, INADDR_ANY, htons(), bind(), listen(), accept()
#include <sys/socket.h>     // for socket(), connect(), send(), recv()
#include <pthread.h>        // for pthread_create(), pthread_join()
#include <minizip/zip.h>    // for zipFile, zipOpen(), zipOpenNewFileInZip3(), zipWriteInFileInZip(), zipClose(), ZIP_OK
#include <dirent.h>         // for opendir(), readdir(), closedir()
#include <sys/stat.h>       // for stat(), struct stat
#include <time.h>           // for struct tm, localtime()

#define PORT 8080               // port number
#define ADMIN_PORT 8081         // admin port number
#define BUFFER_SIZE 4096        // buffer size
#define MAX_COMPRESSIONS 100
#define MAX_USERS 10

// struct to store information about a compression
typedef struct {
    int active;
    char source_dir[BUFFER_SIZE];
    char destination[BUFFER_SIZE];
    int compression_level;
    char password[BUFFER_SIZE];
    size_t total_size;
    size_t total_read;
} compression_info_t;

typedef struct {
    char username[BUFFER_SIZE];
    char password[BUFFER_SIZE];
} user_info_t;

compression_info_t compressions[MAX_COMPRESSIONS];  // array to store compression information
user_info_t users[MAX_USERS]; // array to store user information
pthread_mutex_t compressions_mutex = PTHREAD_MUTEX_INITIALIZER;     // mutex to protect the compressions array

void *handle_client(void *client_socket);
void *handle_admin_client(void *client_socket);
void *client_listener(void *arg);
void *admin_listener(void *arg);
int add_file_to_zip(zipFile zf, const char *filepath, const char *password, int compression_level, compression_info_t *info);
int create_zip(const char *source_dir, const char *zip_path, const char *password, int compression_level, compression_info_t *info);
uLong tm_to_dosdate(const struct tm *ptm);
void print_progress(size_t current, size_t total);
int authenticate_user(const char *username, const char *password);

int main() {
    int server_fd, admin_fd;
    pthread_t client_thread, admin_thread;

    memset(compressions, 0, sizeof(compressions));              // Initialize compressions array
    // Initialize users
    strcpy(users[0].username, "admin");
    strcpy(users[0].password, "password");
    strcpy(users[1].username, "andrada");
    strcpy(users[1].password, "andrada12");
    strcpy(users[2].username, "alice");
    strcpy(users[2].password, "alice12");
    strcpy(users[3].username, "utilizator");
    strcpy(users[3].password, "parola");

    // creating socket file descriptor for clients
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Client socket failed");
        exit(EXIT_FAILURE);
    }

    // creating socket file descriptor for admin clients
    if ((admin_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Admin socket failed");
        exit(EXIT_FAILURE);
    }

    // bind the client socket to the network
    struct sockaddr_in client_address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    if (bind(server_fd, (struct sockaddr *)&client_address, sizeof(client_address)) < 0) {
        perror("Client bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Bind the admin socket to the network
    struct sockaddr_in admin_address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(ADMIN_PORT)
    };

    if (bind(admin_fd, (struct sockaddr *)&admin_address, sizeof(admin_address)) < 0) {
        perror("Admin bind failed");
        close(admin_fd);
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming client connections
    if (listen(server_fd, 3) < 0) {
        perror("Client listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming admin connections
    if (listen(admin_fd, 3) < 0) {
        perror("Admin listen failed");
        close(admin_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d for clients and port %d for admins\n", PORT, ADMIN_PORT);

    // Create threads to handle client and admin connections
    pthread_create(&client_thread, NULL, client_listener, &server_fd);
    pthread_create(&admin_thread, NULL, admin_listener, &admin_fd);

    // Wait for the threads to finish (they won't in this case)
    pthread_join(client_thread, NULL);
    pthread_join(admin_thread, NULL);

    close(server_fd);
    close(admin_fd);
    return 0;
}

// Function to handle client connections
void *client_listener(void *arg) {
    int server_fd = *(int *)arg;
    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t thread_id;

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Client accept failed");
            continue;
        }

        printf("Accepted new client connection\n");

        // Create a new thread for each client
        if (pthread_create(&thread_id, NULL, handle_client, (void *)&new_socket) != 0) {
            perror("Failed to create client thread");
            close(new_socket);
        }
    }
}

// Function to handle admin connections
void *admin_listener(void *arg) {
    int admin_fd = *(int *)arg;
    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t thread_id;

    while (1) {
        if ((new_socket = accept(admin_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {       // accept connection from client
            perror("Admin accept failed");
            continue;
        }

        printf("Accepted new admin connection\n");

        // Create a new thread for each admin client
        if (pthread_create(&thread_id, NULL, handle_admin_client, (void *)&new_socket) != 0) {
            perror("Failed to create admin thread");
            close(new_socket);
        }
    }
}

// Function to handle client connections
void *handle_client(void *client_socket) {
    int sock = *(int *)client_socket;
    char buffer[BUFFER_SIZE] = {0};
    char source_dir[BUFFER_SIZE];
    char destination[BUFFER_SIZE];
    int compression_level;
    char password[BUFFER_SIZE];
    char username[BUFFER_SIZE];
    char user_password[BUFFER_SIZE];
    int i;

    // Read username and password from client
    read(sock, buffer, BUFFER_SIZE);
    sscanf(buffer, "%s %s", username, user_password);

    // Authenticate the user
    if (authenticate_user(username, user_password) != 0) {
        send(sock, "Authentication failed", strlen("Authentication failed"), 0);
        close(sock);
        pthread_exit(NULL);
    }

    send(sock, "Authentication successful", strlen("Authentication successful"), 0);

    // Read the source directory path, destination file path, compression level, and password from client
    read(sock, buffer, BUFFER_SIZE);
    sscanf(buffer, "%s %s %d %[^\n]", source_dir, destination, &compression_level, password);

    // We delete anything unwanted
    password[strcspn(password, "\n")] = '\0';

    printf("Password: '%s'\n", password); // Verificare

    printf("Compressing directory: %s\n", source_dir);

    pthread_mutex_lock(&compressions_mutex);            // Lock the mutex to protect the compressions array

    // Find an empty slot in the compressions array
    for (i = 0; i < MAX_COMPRESSIONS; i++) {
        if (!compressions[i].active) {
            compressions[i].active = 1;
            strncpy(compressions[i].source_dir, source_dir, BUFFER_SIZE);
            strncpy(compressions[i].destination, destination, BUFFER_SIZE);
            compressions[i].compression_level = compression_level;
            strncpy(compressions[i].password, password, BUFFER_SIZE);
            compressions[i].total_size = 0;
            compressions[i].total_read = 0;
            break;
        }
    }
    pthread_mutex_unlock(&compressions_mutex);      // Unlock the mutex

    // Check if there are too many compressions
    if (i == MAX_COMPRESSIONS) {
        send(sock, "Server is busy, try again later", strlen("Server is busy, try again later"), 0);
        close(sock);
        pthread_exit(NULL);
    }

    // Create a ZIP archive of the directory
    if (create_zip(source_dir, destination, password, compression_level, &compressions[i]) != 0) {
        send(sock, "Directory compression failed", strlen("Directory compression failed"), 0);
    } else {
        send(sock, "Directory compressed successfully", strlen("Directory compressed successfully"), 0);
    }

    pthread_mutex_lock(&compressions_mutex);    // Lock the mutex to protect the compressions array
    compressions[i].active = 0;                 // Mark the compression as inactive
    pthread_mutex_unlock(&compressions_mutex);  // Unlock the mutex

    close(sock);
    pthread_exit(NULL);
}

// Function to handle admin client connections
void *handle_admin_client(void *client_socket) {
    int sock = *(int *)client_socket;
    char buffer[BUFFER_SIZE] = {0};
    int i;

    // Read the command from the admin client
    read(sock, buffer, BUFFER_SIZE);

    // Handle different admin commands
    if (strncmp(buffer, "list", 4) == 0) {
        pthread_mutex_lock(&compressions_mutex);        // Lock the mutex to protect the compressions array

        // Send information about active compressions to the admin client
        for (i = 0; i < MAX_COMPRESSIONS; i++) {
            if (compressions[i].active) {
                snprintf(buffer, BUFFER_SIZE, "Source directory: %s, Destination: %s, Compression level: %d, Password: %s\n",
                         compressions[i].source_dir, compressions[i].destination, compressions[i].compression_level, compressions[i].password);
                send(sock, buffer, strlen(buffer), 0);
            }
        }
        pthread_mutex_unlock(&compressions_mutex);          // Unlock the mutex

        send(sock, "End of list", strlen("End of list"), 0);
    } else if (strncmp(buffer, "terminate", 9) == 0) {
        // Extract the compression index from the command
        int index = atoi(buffer + 10);

        pthread_mutex_lock(&compressions_mutex);        // Lock the mutex to protect the compressions array
        if (index >= 0 && index < MAX_COMPRESSIONS && compressions[index].active) {
            compressions[index].active = 0;             // Terminate the compression
            send(sock, "Compression terminated", strlen("Compression terminated"), 0);
        } else {
            send(sock, "Invalid compression index", strlen("Invalid compression index"), 0);
        }
        pthread_mutex_unlock(&compressions_mutex);      // Unlock the mutex
    } else {
        send(sock, "Invalid command", strlen("Invalid command"), 0);
    }

    close(sock);
    pthread_exit(NULL);
}

// Function to authenticate a user
int authenticate_user(const char *username, const char *password) {
    for (int i = 0; i < MAX_USERS; i++) {
        if (strcmp(users[i].username, username) == 0 && strcmp(users[i].password, password) == 0) {
            return 0;
        }
    }
    return -1;
}

// Function to add a file to a zip archive
int add_file_to_zip(zipFile zf, const char *filepath, const char *password, int compression_level, compression_info_t *info) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        perror("Failed to open file");
        return -1;
    }

    zip_fileinfo zi = {0};
    struct stat file_stat;
    if (stat(filepath, &file_stat) == 0) {
        struct tm *ptm = localtime(&file_stat.st_mtime);
        zi.dosDate = tm_to_dosdate(ptm);
    }

    const char *filename_in_zip = strrchr(filepath, '/');
    if (!filename_in_zip) {
        filename_in_zip = filepath;
    } else {
        filename_in_zip++;
    }

    if (zipOpenNewFileInZip3(zf, filename_in_zip, &zi, NULL, 0, NULL, 0, NULL, Z_DEFLATED, compression_level, 0,
                             -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, password, 0) != ZIP_OK) {
        perror("Failed to open file in zip");
        fclose(fp);
        return -1;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        if (zipWriteInFileInZip(zf, buffer, bytes_read) != ZIP_OK) {
            perror("Failed to write to zip");
            fclose(fp);
            return -1;
        }
        info->total_read += bytes_read;
        print_progress(info->total_read, info->total_size);
    }

    fclose(fp);
    if (zipCloseFileInZip(zf) != ZIP_OK) {
        perror("Failed to close file in zip");
        return -1;
    }

    return 0;
}

// Function to create a zip archive of a directory
// Function to create a zip archive of a directory
int create_zip(const char *source_dir, const char *zip_path, const char *password, int compression_level, compression_info_t *info) {
    char destination[BUFFER_SIZE];

    // Ensure zip_path ends with .zip
    if (strlen(zip_path) > 4 && strcmp(zip_path + strlen(zip_path) - 4, ".zip") == 0) {
        strcpy(destination, zip_path);
    } else {
        snprintf(destination, BUFFER_SIZE, "%s.zip", zip_path);
    }

    zipFile zf = zipOpen(destination, 0);
    if (!zf) {
        perror("Failed to create zip file");
        return -1;
    }

    DIR *dir = opendir(source_dir);
    if (!dir) {
        perror("Failed to open directory");
        zipClose(zf, NULL);
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char filepath[BUFFER_SIZE];
        snprintf(filepath, BUFFER_SIZE, "%s/%s", source_dir, entry->d_name);

        struct stat file_stat;
        if (stat(filepath, &file_stat) == 0) {
            if (S_ISDIR(file_stat.st_mode)) {
                create_zip(filepath, destination, password, compression_level, info);
            } else {
                info->total_size += file_stat.st_size;
            }
        }
    }

    rewinddir(dir);
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char filepath[BUFFER_SIZE];
        snprintf(filepath, BUFFER_SIZE, "%s/%s", source_dir, entry->d_name);

        struct stat file_stat;
        if (stat(filepath, &file_stat) == 0) {
            if (!S_ISDIR(file_stat.st_mode)) {
                if (add_file_to_zip(zf, filepath, password, compression_level, info) != 0) {
                    closedir(dir);
                    zipClose(zf, NULL);
                    return -1;
                }
            }
        }
    }

    closedir(dir);
    zipClose(zf, NULL);
    return 0;
}

// Function to convert tm struct to DOS date
uLong tm_to_dosdate(const struct tm *ptm) {
    uLong year = (uLong)ptm->tm_year;
    if (year >= 1980) {
        year -= 1980;
    } else if (year >= 80) {
        year -= 80;
    } else {
        year = 0;
    }

    return (uLong)(((ptm->tm_mday) + (32 * (ptm->tm_mon + 1)) + (512 * year)) << 16) |
           ((ptm->tm_sec / 2) | (ptm->tm_min << 5) | (ptm->tm_hour << 11));
}

// Function to print progress
void print_progress(size_t current, size_t total) {
    int width = 50;
    float ratio = current / (float)total;
    int pos = width * ratio;

    printf("[");
    for (int i = 0; i < width; i++) {
        if (i < pos) {
            printf("=");
        } else if (i == pos) {
            printf(">");
        } else {
            printf(" ");
        }
    }
    printf("] %d%%\r", (int)(ratio * 100));
    fflush(stdout);
}
