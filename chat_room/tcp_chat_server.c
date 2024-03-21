#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024
#define PORT 8080

// Data structures
typedef struct
{
    int socket;
    char username[BUFFER_SIZE];
} Client;

Client clients[BUFFER_SIZE];
int num_clients = 0;

pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes
void *handle_client(void *arg);
void send_to_all(char *message, int sender_socket);
void remove_client(int index);

int main()
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket to port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Chatroom server running on port %d...\n", PORT);

    // Accept incoming connections and handle clients
    while (1)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void *)&new_socket) != 0)
        {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}

void *handle_client(void *arg)
{
    int client_socket = *(int *)arg;
    char username[BUFFER_SIZE];
    bool username_set = false;

    // Send welcome message to client and prompt for username
    char welcome_message[BUFFER_SIZE] = "Welcome! Please enter your username:";
    send(client_socket, welcome_message, strlen(welcome_message), 0);
    memset(username, 0, BUFFER_SIZE);

    // Receive username from client
    if (recv(client_socket, username, BUFFER_SIZE, 0) <= 0)
    {
        perror("Failed to receive username");
        close(client_socket);
        pthread_exit(NULL);
    }

    // Set username for the client
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < num_clients; i++)
    {
        if (strcmp(clients[i].username, username) == 0)
        {
            char error_message[BUFFER_SIZE];
            sprintf(error_message, "Username '%s' already exists. Please choose a different username.", username);
            send(client_socket, error_message, strlen(error_message), 0);
            close(client_socket);
            pthread_mutex_unlock(&client_mutex);
            pthread_exit(NULL);
        }
    }
    strcpy(clients[num_clients].username, username);
    clients[num_clients].socket = client_socket;
    num_clients++;
    pthread_mutex_unlock(&client_mutex);

    // Send welcome message and list of users to client
    sprintf(welcome_message, "Welcome, %s! You are now connected.", username);
    send(client_socket, welcome_message, strlen(welcome_message), 0);

    char user_list[BUFFER_SIZE];
    strcpy(user_list, "Users currently online:\n");
    for (int i = 0; i < num_clients; i++)
    {
        strcat(user_list, clients[i].username);
        strcat(user_list, "\n");
    }
    send(client_socket, user_list, strlen(user_list), 0);

    // Notify other clients about new user
    char join_message[BUFFER_SIZE];
    sprintf(join_message, "Client %s joined the chatroom.", username);
    send_to_all(join_message, client_socket);

    // Handle incoming messages
    char message[BUFFER_SIZE];
    while (recv(client_socket, message, BUFFER_SIZE, 0) > 0)
    {
        if (strcmp(message, "\\list") == 0)
        {
            // Send list of users to client
            send(client_socket, user_list, strlen(user_list), 0);
        }
        else if (strcmp(message, "\\bye") == 0)
        {
            // Remove client from active list and notify others
            pthread_mutex_lock(&client_mutex);
            for (int i = 0; i < num_clients; i++)
            {
                if (clients[i].socket == client_socket)
                {
                    char leave_message[BUFFER_SIZE];
                    sprintf(leave_message, "Client %s left the chatroom.", clients[i].username);
                    send_to_all(leave_message, client_socket);
                    remove_client(i);
                    break;
                }
            }
            pthread_mutex_unlock(&client_mutex);
            close(client_socket);
            pthread_exit(NULL);
        }
        else
        {
            // Broadcast message to all clients
            char broadcast_message[BUFFER_SIZE];
            sprintf(broadcast_message, "%s: %s", username, message);
            send_to_all(broadcast_message, client_socket);
        }
        memset(message, 0, BUFFER_SIZE);
    }

    // Client disconnected
    close(client_socket);
    pthread_exit(NULL);
}

void send_to_all(char *message, int sender_socket)
{
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < num_clients; i++)
    {
        if (clients[i].socket != sender_socket)
        {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&client_mutex);
}

void remove_client(int index)
{
    for (int i = index; i < num_clients - 1; i++)
    {
        clients[i] = clients[i + 1];
    }
    num_clients--;
}
