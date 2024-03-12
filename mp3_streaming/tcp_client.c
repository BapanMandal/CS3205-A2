#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

#define SERVER_IP_ADDR "10.21.225.102"
#define SERVER_PORT 8888


int main() {
    int clientSocket;
    struct sockaddr_in serverAddr;

    // Create socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address structure
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP_ADDR);  // Server IP address
    serverAddr.sin_port = htons(SERVER_PORT);  // Server port

    // Connect to the server
    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("Connection failed");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }

    // Receive and display the list of songs
    char buffer[BUFFER_SIZE];
    printf("Available Songs:\n");
    while (recv(clientSocket, buffer, sizeof(buffer), 0) > 0) {
        printf("%s", buffer);
    }

    // Select a song index
    int selectedSongIndex;
    printf("\nEnter the index of the song you want to play: ");
    scanf("%d", &selectedSongIndex);

    // Send the selected song index to the server
    snprintf(buffer, sizeof(buffer), "%d", selectedSongIndex);
    send(clientSocket, buffer, sizeof(buffer), 0);

    // Receive the file size of the selected song
    recv(clientSocket, buffer, sizeof(buffer), 0);
    long fileSize = atol(buffer);

    // Receive and play the song bytes using mpg123 and popen
    FILE *mpg123Pipe = popen("mpg123 -", "w");
    if (mpg123Pipe == NULL) {
        perror("Error opening mpg123 pipe");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }

    size_t totalBytesReceived = 0;
    while (totalBytesReceived < fileSize) {
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            perror("Error receiving song bytes");
            break;
        }

        fwrite(buffer, 1, bytesRead, mpg123Pipe);
        totalBytesReceived += bytesRead;
    }

    // Close the mpg123 pipe
    fclose(mpg123Pipe);

    // Close the client socket
    close(clientSocket);

    return 0;
}
