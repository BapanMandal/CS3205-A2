#include <argp.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

const char *argp_program_version = "MP3_Server 0.1";
const char *argp_program_bug_address = "cs21b016@smail.iitm.ac.in";
static char doc[] = "TCP server compatible with the given TCP client that streams and plays mp3 music.";

// Define a struct to hold the parsed arguments
struct arguments
{
    int port_number;
    char *directory;
    int max_connections;
};

// Options for parsing command-line arguments
static struct argp_option options[] = {
    {"port", 'p', "PORT", 0, "The port number (PORT) on which the server should listen to."},
    {"directory", 'd', "DIR", 0, "The directory (DIR) containing the mp3 files."},
    {"connections", 'n', "N", 0, "The maximum number of connections (N) the server can handle."},
    {0} // Terminating element, required by argp
};

// Parse an option, storing the result in 'key' and 'arg'.
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    // Get the input argument from 'state'
    struct arguments *arguments = state->input;

    switch (key)
    {
    case 'p':
        arguments->port_number = atoi(arg);
        break;
    case 'd':
        arguments->directory = arg;
        break;
    case 'n':
        arguments->max_connections = atoi(arg);
        break;
    case ARGP_KEY_END:
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

// The argp object to define the parser
static struct argp argp = {options, parse_opt, NULL, doc, NULL, NULL, NULL};

#define PORT 8888        // Default Server port, if not specified
#define PATH_MAX 4096    // The maximum file path length is 4096 characters for the ext4 filesystem.
#define MAX_FILENAME 256 // The maximum filename length is 256 characters
#define MAX_FILES 1000   // We assume that there are at most 1000 files in the directory
#define BUFFER_SIZE 1024 // Buffer size for sending TCP segments
#define No_Backlogs 5              // Maximum listen backlogs

int no_connections = 0;


typedef struct
{
    int socket;
    char directory[PATH_MAX];
} Client_args;

char SONGS_FOLDER[] = "/home/bapan/Music/Songs"; // Default mp3 files folder, if not specified


// Comparison function for qsort
int compare_filenames(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

// List regular files in a directory
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
    int pid = pthread_self();
    Client_args *client = (Client_args *)arg;

    // Extract the client arguments, viz. file descriptor and directory
    int client_socket = client->socket;
    char *SONGS_FOLDER = strdup(client->directory);

    int *count = (int *)malloc(sizeof(int *)); // Number of songs available in SONGS_FOLDER
    char request[16];                          // Buffer for receiving user's song selection

    // List the songs available
    char **songList = list_files(SONGS_FOLDER, count);

    // Send the number of songs to the client
    char numSongs_str[16];
    sprintf(numSongs_str, "%d", *count);
    send(client_socket, numSongs_str, sizeof(numSongs_str), 0);

    // // Send the song list to the client
    // for(int i = 0; i < *count; i++)
    // {
    //     send(client_socket, songList[i], 256, 0);
    //     usleep(10000); // Sleep for 1ms (to avoid data loss in the client side)
    // }
    // // send(client_socket, song_list_str, strlen(song_list_str), 0);

    // Receive user's song selection
    int bytes_received = recv(client_socket, request, sizeof(request), 0);
    if (bytes_received == -1)
    {
        perror("Error receiving song request");
        close(client_socket);
        pthread_exit(NULL);
    }

    // Extract song number from request
    int song_number = atoi(request);

    // Validate song number (assuming 15 songs are available)
    if (song_number < 1 || song_number > *count)
    {
        // send(client_socket, "Invalid song number\n", strlen("Invalid song number\n"), 0);
        close(client_socket);
        pthread_exit(NULL);
    }

    // Build song path based on song number (modify this logic as needed)
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

    fclose(song_file);

    // Clean up
    for (int i = 0; i < *count; i++)
        free(songList[i]);
    free(songList);
    free(count);
    close(client_socket);
    printf("Song sent... Client is now disconnected\n");
    no_connections--;
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    struct arguments arguments;

    // Parse arguments
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    printf("Port number: %d\n", arguments.port_number);
    printf("Directory: %s\n", arguments.directory);
    printf("Max connections: %d\n", arguments.max_connections);

    // Create a socket
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_size;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }


    // Initialize server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    server_addr.sin_port = htons(arguments.port_number);

    // Bind the socket to the address
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Error binding socket");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, No_Backlogs) == -1)
    {
        perror("Error listening for connections");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", arguments.port_number);

    while (1)
    {
        // Accept incoming connection
        client_addr_size = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_socket == -1)
        {
            perror("\nError accepting connection");
            continue;
        }

        if(no_connections >= arguments.max_connections)
        {
            printf("\nMax connections reached\n");
            printf("Current no. of connections: %d\n", no_connections);
            close(client_socket);
            continue;
        }

        printf("\nClient connected from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        printf("Current no. of connections: %d\n", ++no_connections);

        // Create a new thread to handle the client
        pthread_t client_thread;
        Client_args *clientSocketPtr = (Client_args *)malloc(sizeof(Client_args));
        clientSocketPtr->socket = client_socket;
        strcpy(clientSocketPtr->directory, arguments.directory);
        if (pthread_create(&client_thread, NULL, handle_client, (Client_args *)clientSocketPtr) != 0)
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
    close(server_socket);

    return 0;
}
