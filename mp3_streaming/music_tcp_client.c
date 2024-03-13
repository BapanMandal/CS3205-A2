#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAX_FILENAME 256 // The maximum filename length is 256 characters
#define BUFFER_SIZE 1024 // Buffer size for reading user's selection

#define SERVER_IP_ADDR "10.42.83.113" // Server IP address
#define PORT 8888                     // Server port

// Wrapper function to send a request to the server
void send_data(int client_socket, const char *request)
{
    send(client_socket, request, strlen(request), 0);
}

// Play the streamed mp3
void play_streamed_mp3(int client_socket)
{
    FILE *mpg123_pipe = popen("mpg123 -", "w");
    if (!mpg123_pipe)
    {
        perror("Error opening mpg123 pipe");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    while ((bytes_received = recv(client_socket, buffer, sizeof(buffer), 0)) > 0)
    {
        if (fwrite(buffer, 1, bytes_received, mpg123_pipe) != bytes_received)
        {
            perror("Error writing to mpg123 pipe");
            exit(EXIT_FAILURE);
        }
    }

    if (bytes_received == -1)
    {
        perror("Error receiving data");
        exit(EXIT_FAILURE);
    }

    // Close the write end of the pipe to signal end of input
    pclose(mpg123_pipe);

    // Close the client socket
    close(client_socket);
}

int main()
{
    int client_socket;
    struct sockaddr_in server_addr;

    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1)
    {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP_ADDR); // Server IP address
    server_addr.sin_port = htons(PORT);

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Error connecting to server");
        exit(EXIT_FAILURE);
    }

    // Read number of songs from the server
    char numSongs_str[BUFFER_SIZE];
    recv(client_socket, numSongs_str, sizeof(numSongs_str), 0);
    int numSongs = atoi(numSongs_str);

    // Read the list of songs from the server
    printf("Available songs:\n");
    for (int i = 0; i < numSongs; i++)
    {
        char songName[MAX_FILENAME];
        recv(client_socket, songName, sizeof(songName), 0);
        printf("%d. %s\n", i + 1, songName);
    }

    // Read user's selection
    printf("\n>> Enter the song number you want to play (1-%d): ", numSongs);
    char command[BUFFER_SIZE];
    fgets(command, sizeof(command), stdin);
    printf("\n**** Kill the process and rerun the program to listen to a different song ****\n\n");

    // Send the user's selection to the server
    send_data(client_socket, command);

    // Play the streamed mp3
    play_streamed_mp3(client_socket);

    // Close the client socket
    close(client_socket);

    exit(0);
}
