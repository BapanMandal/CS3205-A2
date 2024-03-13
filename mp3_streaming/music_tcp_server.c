#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>

#define PORT 8888        // Server port
#define PATH_MAX 4096    // The maximum file path length is 4096 characters for the ext4 filesystem.
#define MAX_FILENAME 256 // The maximum filename length is 256 characters
#define MAX_FILES 1000   // We assume that there are at most 1000 files in the directory
#define BUFFER_SIZE 1024 // Buffer size for sending TCP segments

#define MAX_LISTEN_BACKLOGS 5                  // Maximum number of pending connections in the server socket's listen queue
#define SONGS_FOLDER "/home/bapan/Music/Songs" // Folder containing the songs

// Comparison function for qsort
int compare_filenames(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

// Return list of names of regular files in a directory
char **list_files(const char *folder_path, int *cnt) // returns list of files and updates count of files
{
    DIR *dir;

    struct dirent *entry;
    // entry->d_name will give the filename, MAX length of filename is 256 bytes

    char **filenames = (char **)malloc(MAX_FILES * sizeof(char *));

    // Open the directory
    dir = opendir(folder_path);

    if (dir == NULL)
    {
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }

    // Store filenames
    int count = 0;
    while ((entry = readdir(dir)) != NULL)
    {
        struct stat statbuf;
        char full_path[PATH_MAX];

        snprintf(full_path, sizeof(full_path), "%s/%s", folder_path, entry->d_name);

        if (stat(full_path, &statbuf) == 0 && S_ISREG(statbuf.st_mode))
        {
            filenames[count] = (char *)malloc(MAX_FILENAME * sizeof(char));
            strcpy(filenames[count], entry->d_name);
            count++;
        }
    }

    // Update the count
    *cnt = count;

    // Close the directory
    closedir(dir);

    // Sort the filenames
    qsort(filenames, count, sizeof(char *), compare_filenames);

    return filenames;
}

// Thread function to handle a client
void *handle_client(void *arg)
{
    // Extract the client socket file descriptor
    int client_socket = *((int *)arg);

    // List the songs available
    int *numSongs = (int *)malloc(sizeof(int *)); // Number of songs available in SONGS_FOLDER
    char **songList = list_files(SONGS_FOLDER, numSongs);

    // Send the number of songs to the client
    char numSongs_str[BUFFER_SIZE];
    sprintf(numSongs_str, "%d", *numSongs);
    send(client_socket, numSongs_str, sizeof(numSongs_str), 0);

    // Send the song list to the client
    for (int i = 0; i < *numSongs; i++)
    {
        send(client_socket, songList[i], MAX_FILENAME, 0);
    }

    // Receive user's song selection
    char request[BUFFER_SIZE]; // Buffer for receiving user's song selection
    int bytes_received = recv(client_socket, request, sizeof(request), 0);
    if (bytes_received == -1)
    {
        perror("Error receiving song request");
        close(client_socket);
        pthread_exit(NULL);
    }
    int song_number = atoi(request);

    // Validate song number (assuming 15 songs are available)
    if (song_number < 1 || song_number > *numSongs)
    {
        // send(client_socket, "Invalid song number\n", strlen("Invalid song number\n"), 0);
        close(client_socket);
        pthread_exit(NULL);
    }

    // Build song path based on song number
    char song_path[PATH_MAX];
    sprintf(song_path, "%s/%s", SONGS_FOLDER, songList[song_number - 1]);

    // Open the song file
    FILE *song_file = fopen(song_path, "rb");
    if (!song_file)
    {
        // send(client_socket, "Error opening song file\n", strlen("Error opening song file\n"), 0);
        close(client_socket);
        pthread_exit(NULL);
    }

    // Send the song content in chunks
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), song_file)) > 0)
    {
        if (send(client_socket, buffer, bytes_read, 0) == -1)
        {
            perror("Error sending data");
            break;
        }
    }

    // Clean up
    for (int i = 0; i < *numSongs; i++)
        free(songList[i]);
    free(songList);
    free(numSongs);
    fclose(song_file);
    close(client_socket);
    pthread_exit(NULL);
}

int main()
{
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_size;

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    server_addr.sin_port = htons(PORT);

    // Bind the socket to the address
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Error binding socket");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, MAX_LISTEN_BACKLOGS) == -1)
    {
        perror("Error listening for connections");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while (1)
    {
        // Accept incoming connection
        client_addr_size = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_socket == -1)
        {
            perror("Error accepting connection");
            continue;
        }

        printf("Client connected from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Create a new thread to handle the client
        pthread_t client_thread;
        int *clientSocketPtr = (int *)malloc(sizeof(int));
        *clientSocketPtr = client_socket;

        if (pthread_create(&client_thread, NULL, handle_client, clientSocketPtr) != 0)
        {
            perror("Error creating thread");
            free(clientSocketPtr);
            close(client_socket);
            continue;
        }

        // Detach the thread to clean up after it terminates
        pthread_detach(client_thread);
    }

    // Close the server socket
    puts("Server is shutting down...");
    close(server_socket);

    return EXIT_SUCCESS;
}
