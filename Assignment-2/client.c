#include "utils.h"

// Function to handle single connection to the server
void connect_to_server() {
    int client_socket;
    struct sockaddr_in server_address;
    char buffer[MAX_BUFFER_SIZE];
    int server_port = SERVER_PORT;
    char *server_ip = "127.0.0.1";

    // Create a socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Error creating socket");
        return;
    }

    // Set up the server address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_address.sin_addr);

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Error connecting to server");
        close(client_socket);
        return;
    }

    // Receive data from the server
    int bytes_received = recv(client_socket, buffer, MAX_BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("Received from server:\n%s\n", buffer);
    }

    close(client_socket);
}

// Function for client thread (used when multiple clients are requested)
void *client_thread(void *arg) {
    connect_to_server();
    return NULL;
}

int main(int argc, char *argv[]) {
    // If no argument is passed, connect in single-threaded mode
    if (argc == 1) {
        printf("Connecting in single-threaded mode...\n");
        connect_to_server();
        return 0;
    }

    // If argument is passed, create multiple threads to handle 'n' connections
    int num_clients = atoi(argv[1]);
    pthread_t threads[num_clients];

    // Create 'n' client threads to connect to the server concurrently
    for (int i = 0; i < num_clients; i++) {
        if (pthread_create(&threads[i], NULL, client_thread, NULL) != 0) {
            perror("Error creating client thread");
        }
    }

    // Wait for all client threads to finish
    for (int i = 0; i < num_clients; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}

