#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_IP "10.42.83.171"

/**
 * CS3205 Assignment 2
 * Summary:
 * This file contains the code to run a chatroom client.
 */

// Global variables

bool chat_active = true;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for thread safety

// Subroutines in main flow

int create_socket();
void connect_to_server(int);
void set_user_name(int);
pthread_t setup_incoming_message_listener(int);
pthread_t setup_outgoing_message_listener(int);

// Message handlers

void *handle_incoming_message(void *);
void *handle_outgoing_message(void *);

// Utilities

void set_close_chat();
void read_and_sanitize_user_input(char *);
void reset_string(char *);
void remove_trailing_new_line(char *);

int main()
{
    int sock = create_socket();
    connect_to_server(sock);
    set_user_name(sock);
    pthread_t incoming_message_listener = setup_incoming_message_listener(sock);
    pthread_t outgoing_message_handler = setup_outgoing_message_listener(sock);
    while (chat_active)
    {
        // keep process alive
    }
    close(sock);
    return EXIT_SUCCESS;
}

int create_socket()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("[create_socket]: Socket creation error \n");
        exit(EXIT_FAILURE);
    }
    return sock;
}

void connect_to_server(int sock)
{
    struct sockaddr_in serv_addr;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0)
    {
        perror("[connect_to_server] Invalid address/ Address not supported \n");
        exit(EXIT_FAILURE);
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("[connect_to_server] Connection Failed \n");
        exit(EXIT_FAILURE);
    }
}

void set_user_name(int sock)
{
    char message[BUFFER_SIZE] = {0};
    int message_length = read(sock, message, BUFFER_SIZE);
    if (message_length < 1)
    {
        perror("[set_user_name] socket closed");
        exit(EXIT_FAILURE);
    }
    printf("%s\n", message);
    reset_string(message);
    read_and_sanitize_user_input(message);
    send(sock, message, strlen(message), 0);
}

pthread_t setup_incoming_message_listener(int sock)
{
    pthread_t message_listener_id;
    if (pthread_create(&message_listener_id, NULL, handle_incoming_message, &sock) != 0)
    {
        perror("[setup_incoming_message_listener] pthread_create\n");
        close(sock);
        exit(EXIT_FAILURE);
        
    }
    return message_listener_id;
}

pthread_t setup_outgoing_message_listener(int sock)
{
    pthread_t message_listener_id;
    if (pthread_create(&message_listener_id, NULL, handle_outgoing_message, &sock) != 0)
    {
        perror("[setup_outgoing_message_listener] pthread_create\n");
        close(sock);
        exit(EXIT_FAILURE);
    }
    return message_listener_id;
}

void *handle_incoming_message(void *socket_ptr)
{
    int sock = *(int *)socket_ptr;
    char message[BUFFER_SIZE] = {0};
    while (chat_active)
    {
        if (read(sock, message, BUFFER_SIZE) > 0)
        {
            printf("%s\n", message);
            reset_string(message);
        }
        else
        {
            set_close_chat();
            break;
        }
    }
    pthread_exit(NULL);
}

void *handle_outgoing_message(void *socket_ptr)
{
    int sock = *(int *)socket_ptr;
    while (1)
    {
        char message[BUFFER_SIZE] = {0};
        read_and_sanitize_user_input(message);
        send(sock, message, strlen(message), 0);
    }
    return NULL;
}

void set_close_chat()
{
    pthread_mutex_lock(&mutex);
    chat_active = false;
    pthread_mutex_unlock(&mutex);
}

void read_and_sanitize_user_input(char *buffer)
{
    int input_length;
    do
    {	
        input_length = -1;
        fgets(buffer, BUFFER_SIZE, stdin);
        remove_trailing_new_line(buffer);
        input_length = strlen(buffer);
        if (input_length > 0)
        {
            return;
        }
    } while (input_length < 0);
}

void reset_string(char *string)
{
    memset(string, 0, BUFFER_SIZE);
}

void remove_trailing_new_line(char *string)
{
    int string_length = strlen(string);
    string[string_length - 1] = '\0';
}
