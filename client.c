#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 4096

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char source_dir[BUFFER_SIZE];
    char destination[BUFFER_SIZE];
    int compression_level;
    char password[BUFFER_SIZE];
    char username[BUFFER_SIZE];
    char user_password[BUFFER_SIZE];

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("Invalid address/ Address not supported\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed\n");
        return -1;
    }

    // Get username and password from user
    printf("Enter username: ");
    fgets(username, BUFFER_SIZE, stdin);
    username[strcspn(username, "\n")] = '\0';

    printf("Enter password: ");
    fgets(user_password, BUFFER_SIZE, stdin);
    user_password[strcspn(user_password, "\n")] = '\0';

    // Send username and password to server for authentication
    snprintf(buffer, BUFFER_SIZE, "%s %s", username, user_password);
    send(sock, buffer, strlen(buffer), 0);

    // Read authentication response from server
    read(sock, buffer, BUFFER_SIZE);
    if (strncmp(buffer, "Authentication failed", 21) == 0) {
        printf("Authentication failed\n");
        close(sock);
        return -1;
    }

    printf("Authentication successful\n");

    // Get source directory, destination, compression level, and password from user
    printf("Enter source directory: ");
    fgets(source_dir, BUFFER_SIZE, stdin);
    source_dir[strcspn(source_dir, "\n")] = '\0';

    printf("Enter destination zip file: ");
    fgets(destination, BUFFER_SIZE, stdin);
    destination[strcspn(destination, "\n")] = '\0';

    printf("Enter compression level (0-9): ");
    scanf("%d", &compression_level);
    getchar(); // Consume the newline left by scanf

    printf("Enter password for zip file (optional): ");
    fgets(password, BUFFER_SIZE, stdin);
    password[strcspn(password, "\n")] = '\0';

    // Send compression request to server
    snprintf(buffer, BUFFER_SIZE, "%s %s %d %s", source_dir, destination, compression_level, password);
    send(sock, buffer, strlen(buffer), 0);

    // Read response from server
    read(sock, buffer, BUFFER_SIZE);
    printf("%s\n", buffer);

    close(sock);
    return 0;
}
