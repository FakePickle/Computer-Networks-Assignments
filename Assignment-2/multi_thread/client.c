#include "utils.h"

// Function for client thread (used when multiple clients are requested)
void *client_thread(void *arg) {
    int client_socket;
    struct sockaddr_in server_address;
    char buffer[MAX_BUFFER_SIZE];
    const char *server_ip = "127.0.0.1"; // Change to your server's IP address

    // Create a socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Error creating socket");
        return NULL;
    }

    // Set up the server address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, server_ip, &server_address.sin_addr);

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Error connecting to server");
        close(client_socket);
        return NULL;
    }

    // Receive data from the server
    int bytes_received = recv(client_socket, buffer, MAX_BUFFER_SIZE - 1, 0);
    if (bytes_received < 0) {
        perror("Error receiving data");
    } else if (bytes_received == 0) {
        printf("Server closed the connection\n");
    } else {
        buffer[bytes_received] = '\0';
        printf("Received from server:\n%s\n", buffer);
    }

    close(client_socket);

    return NULL;
}

int main(int argc, char *argv[]) {
    // Check if the number of clients is passed as an argument
    if (argc != 2) {
        printf("Insufficient arguments. Usage: %s <num_clients>\n", argv[0]);
        return 0;
    }

    // Parse the number of clients
    int num_clients = atoi(argv[1]);
    if (num_clients <= 0) {
        perror("Invalid number of clients");
        return 0;
    }

    pthread_t threads[num_clients];

    // Create 'n' client threads to connect to the server concurrently
    for (int i = 0; i < num_clients; i++) {
        printf("Creating client thread %d...\n", i + 1);
        if (pthread_create(&threads[i], NULL, client_thread, NULL) != 0) {
            perror("Error creating client thread");
            break; // Exit the loop if thread creation fails
        }
        usleep(1000); // Sleep for 1 ms between thread creations
    }

    // Wait for all client threads to finish
    for (int i = 0; i < num_clients; i++) {
        if (threads[i] != 0) { // Only join if the thread was successfully created
            pthread_join(threads[i], NULL);
        }
    }

    return 0;
}
