#include <alsa/asoundlib.h>
#include <argp.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <lame/lame.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

const char *argp_program_version = "MP3_Server 0.1";
const char *argp_program_bug_address = "cs21b016@smail.iitm.ac.in";
static char doc[] = "TCP server compatible with the given TCP client that streams and plays mp3 music. Streams audio from the microphone on user request.";

// Define a struct to hold the parsed arguments
struct arguments
{
    int port_number;
    char *directory;
    int max_connections;
};

// Options for parsing command-line arguments
static struct argp_option options[] = {
    {"port", 'p', "PORT_NO", 0, "The port number (PORT_NO) on which the server should listen to."},
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
#define N 5              // Default maximum connections, if not specified
#define PCM_DEVICE "default"

#define SAMPLE_RATE 44100
#define CHANNELS 2
#define MP3_BIT_RATE 128

char SONGS_FOLDER[] = "/home/bapan/Music/Songs"; // Default mp3 files folder, if not specified

// Capture audio from the microphone
int capture_audio(snd_pcm_t *handle, char *buffer, int size)
{
    int err;
    if ((err = snd_pcm_readi(handle, buffer, size)) < 0)
    {
        fprintf(stderr, "audio read failed (%s)\n", snd_strerror(err));
        return -1;
    }
    return 0;
}

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
    int pid = getpid();

    // Extract the client socket file descriptor
    int client_socket = *((int *)arg);

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

    if (song_number == -1)
    {
        // ===================================== Code given by Sir ==============================================

        // snd_pcm_t *capture_handle;

        // // Open PCM device for recording (capture)
        // if (snd_pcm_open(&capture_handle, "default", SND_PCM_STREAM_CAPTURE, 0) < 0)
        // {
        //     fprintf(stderr, "Error opening PCM device\n");
        //     pthread_exit(NULL);
        // }

        // // Set parameters: Sample rate, channels, etc.
        // snd_pcm_hw_params_t *hw_params;
        // snd_pcm_hw_params_alloca(&hw_params);
        // snd_pcm_hw_params_any(capture_handle, hw_params);
        // snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
        // snd_pcm_hw_params_set_format(capture_handle, hw_params, SND_PCM_FORMAT_S16_LE);
        // snd_pcm_hw_params_set_channels(capture_handle, hw_params, 2);
        // unsigned int sample_rate = 44100; // 44KHz audio
        // snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &sample_rate, 0);
        // snd_pcm_hw_params(capture_handle, hw_params);

        // char buffer[BUFFER_SIZE];
        // // Send microphone data
        // while (1)
        // {
        //     // Capture audio from microphone
        //     if (capture_audio(capture_handle, buffer, BUFFER_SIZE) < 0)
        //         break;

        //     // Send microphone data to client
        //     send(client_socket, buffer, BUFFER_SIZE, 0);
        // }

        // // Close the PCM device
        // snd_pcm_close(capture_handle);


        // ========================================================================================================


        int err;
        unsigned int rate = 44100; // Sample rate
        snd_pcm_t *pcm_handle;
        snd_pcm_hw_params_t *params;
        snd_pcm_uframes_t frames;
        char *buffer;
        int size;

        // Open PCM device for recording (capture)
        if ((err = snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_CAPTURE, 0)) < 0)
        {
            fprintf(stderr, "Error opening PCM device: %s\n", snd_strerror(err));
            exit(EXIT_FAILURE);
        }

        // Allocate hardware parameters object
        snd_pcm_hw_params_malloc(&params);

        // Load default hardware parameters
        if ((err = snd_pcm_hw_params_any(pcm_handle, params)) < 0)
        {
            fprintf(stderr, "Can't configure PCM device: %s\n", snd_strerror(err));
            exit(EXIT_FAILURE);
        }

        // Set the desired hardware parameters
        if ((err = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
        {
            fprintf(stderr, "Error setting access: %s\n", snd_strerror(err));
            exit(EXIT_FAILURE);
        }

        if ((err = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE)) < 0)
        {
            fprintf(stderr, "Error setting format: %s\n", snd_strerror(err));
            exit(EXIT_FAILURE);
        }

        if ((err = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0)) < 0)
        {
            fprintf(stderr, "Error setting rate: %s\n", snd_strerror(err));
            exit(EXIT_FAILURE);
        }

        if ((err = snd_pcm_hw_params_set_channels(pcm_handle, params, 1)) < 0)
        {
            fprintf(stderr, "Error setting channels: %s\n", snd_strerror(err));
            exit(EXIT_FAILURE);
        }

        // Apply the hardware parameters
        if ((err = snd_pcm_hw_params(pcm_handle, params)) < 0)
        {
            fprintf(stderr, "Error setting hardware parameters: %s\n", snd_strerror(err));
            exit(EXIT_FAILURE);
        }

        // Allocate buffer to hold recorded data
        snd_pcm_hw_params_get_period_size(params, &frames, 0);
        size = frames * 2; // 2 bytes/sample, 1 channel
        buffer = (char *)malloc(size);

        // Initialize LAME MP3 encoder
        lame_t lame;
        lame = lame_init();
        lame_set_in_samplerate(lame, SAMPLE_RATE);
        lame_set_out_samplerate(lame, SAMPLE_RATE);
        lame_set_num_channels(lame, CHANNELS);
        lame_set_brate(lame, MP3_BIT_RATE);
        lame_set_quality(lame, 2);
        lame_init_params(lame);

        // Start capturing audio
        while (1)
        {
            if ((err = snd_pcm_readi(pcm_handle, buffer, frames)) != frames)
            {
                fprintf(stderr, "Error reading from PCM device: %s\n", snd_strerror(err));
                exit(EXIT_FAILURE);
            }
            printf("Captured %d bytes\n", size);

            int encoded_bytes = lame_encode_buffer(lame, (short int *)buffer, NULL, err, buffer, size);
            if (encoded_bytes < 0) perror("Failed to encode MP3");

            // Send microphone data to client
            if (send(client_socket, buffer, size, 0) == -1)
            {
                perror("Error sending data");
                break;
            }
        }

        // Free resources
        free(buffer);
        snd_pcm_drain(pcm_handle);
        snd_pcm_close(pcm_handle);
        lame_close(lame);


        // ========================================================================================================


        // int rc;
        // snd_pcm_t *capture_handle;
        // snd_pcm_hw_params_t *hw_params;
        // int mp3_buffer_size = 8192;
        // unsigned char mp3_buffer[mp3_buffer_size];
        // lame_t lame;
        // int bytes_read;
        // int sockfd;
        // struct sockaddr_in serv_addr;

        // // Initialize ALSA capture
        // rc = snd_pcm_open(&capture_handle, "default", SND_PCM_STREAM_CAPTURE, 0);
        // if (rc < 0) error("Failed to open ALSA capture device");

        // snd_pcm_hw_params_malloc(&hw_params);
        // snd_pcm_hw_params_any(capture_handle, hw_params);
        // snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
        // snd_pcm_hw_params_set_format(capture_handle, hw_params, SND_PCM_FORMAT_S16_LE);
        // snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &SAMPLE_RATE, 0);
        // snd_pcm_hw_params_set_channels(capture_handle, hw_params, CHANNELS);
        // snd_pcm_hw_params(capture_handle, hw_params);
        // snd_pcm_hw_params_free(hw_params);
        // snd_pcm_prepare(capture_handle);

        // // Initialize LAME MP3 encoder
        // lame = lame_init();
        // lame_set_in_samplerate(lame, SAMPLE_RATE);
        // lame_set_out_samplerate(lame, SAMPLE_RATE);
        // lame_set_num_channels(lame, CHANNELS);
        // lame_set_brate(lame, MP3_BIT_RATE);
        // lame_set_quality(lame, 2);
        // lame_init_params(lame);

        // // Initialize TCP socket
        // sockfd = socket(AF_INET, SOCK_STREAM, 0);
        // if (sockfd < 0) error("Failed to open socket");

        // serv_addr.sin_family = AF_INET;
        // serv_addr.sin_port = htons(12345); // Change port as needed
        // inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr); // Change IP address as needed

        // if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        //     error("Failed to connect to server");

        // // Main loop: Capture audio, encode to MP3, and send over TCP
        // while (1) {
        //     bytes_read = snd_pcm_readi(capture_handle, mp3_buffer, mp3_buffer_size);
        //     if (bytes_read < 0) error("Failed to read from ALSA");
            
        //     int encoded_bytes = lame_encode_buffer_ieee_float(lame, (float *)mp3_buffer, NULL, bytes_read, mp3_buffer, mp3_buffer_size);
        //     if (encoded_bytes < 0) error("Failed to encode MP3");

        //     if (send(sockfd, mp3_buffer, encoded_bytes, 0) < 0) error("Failed to send data");
        // }

        // // Cleanup
        // close(sockfd);
        // snd_pcm_close(capture_handle);
        // lame_close(lame);

    }
    else
    {
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
    }

    // Clean up
    for (int i = 0; i < *count; i++)
        free(songList[i]);
    free(songList);
    free(count);
    close(client_socket);
    pthread_exit(NULL);
}


int main(int argc, char **argv)
{
    struct arguments arguments;

    // Default values for arguments
    arguments.port_number = PORT; // Default port number
    arguments.directory = SONGS_FOLDER; // Default directory
    arguments.max_connections = N; // Default maximum connections

    // Parse arguments
    argp_parse(&argp, argc, argv, 0, 0, &arguments);



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
    if (listen(server_socket, 5) == -1)
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
    close(server_socket);

    return 0;
}
