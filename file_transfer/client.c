#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <inttypes.h>
#include <stdint.h>

#define BUFFER_SIZE 1024

#define PORT 8080
#define SERVER_IP "192.168.0.223"
#define SAVE_LOCATION "./"



void format_bytes(long long fileSize, char *formattedSize);
int64_t get_time_ns();


int main() {
    int client_socket;
    struct sockaddr_in server;
    char buffer[BUFFER_SIZE];

    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    server.sin_port = htons(PORT);

    // Connect to server
    if (connect(client_socket, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to server\n");

    clock_t start, end;
    double elapsed_time;

    start = get_time_ns();

    // Receive the filename
    char filenameString[BUFFER_SIZE];
    ssize_t filenameLength = recv(client_socket, filenameString, BUFFER_SIZE, 0);
    if (filenameLength <= 0) {
        perror("Error receiving filename");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    // Null-terminate the received filename
    filenameString[filenameLength] = '\0';
    printf("Receiving file: \"%s\"\n", filenameString);

    // prepend the save location to the filename
    char saveLocation[BUFFER_SIZE];
    strcpy(saveLocation, SAVE_LOCATION);
    strcat(saveLocation, filenameString);
    // Create file with the received filename
    FILE *file = fopen(saveLocation, "wb");
    if (!file) {
        perror("File creation failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }


    long long int fileSize=0; // in bytes

    // Receive file content in chunks and write to the file
    while (1) {
        ssize_t bytesRead = recv(client_socket, buffer, BUFFER_SIZE, 0);
        fileSize+=bytesRead;
        if (bytesRead > 0) {
            fwrite(buffer, 1, bytesRead, file);
        } else if (bytesRead == 0) {
            break; // Connection closed by the server
        } else {
            perror("Error receiving file content");
            fclose(file);
            close(client_socket);
            exit(EXIT_FAILURE);
        }
    }

    end = get_time_ns();

    // Close file and socket
    fclose(file);
    close(client_socket);

    printf("File received successfully!\n");
    char formattedSize[100];
    format_bytes(fileSize, formattedSize);
    printf("File size: %s\t", formattedSize);
    double total_time = ((double)(end - start)) / 1e9;
    printf("Time taken: %.2f s\n", total_time);
    printf("Throughput: %.2f MB/s\n", (fileSize / (1024.0 * 1024)) / total_time);

    return 0;
}


void format_bytes(long long fileSize, char *formattedSize) {
    if (fileSize < 1024) {
        sprintf(formattedSize, "%lld B", fileSize);
    } else if (fileSize < 1024 * 1024) {
        sprintf(formattedSize, "%.2f KB", fileSize / 1024.0);
    } else if (fileSize < 1024 * 1024 * 1024) {
        sprintf(formattedSize, "%.2f MB", fileSize / (1024.0 * 1024));
    } else {
        sprintf(formattedSize, "%.2f GB", fileSize / (1024.0 * 1024 * 1024));
    }
}

int64_t get_time_ns() {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}
