#include "utils.h"

void *client_thread(void *arg) {
    int client_socket;
    struct sockaddr_in server_address;
    char buffer[MAX_BUFFER_SIZE];
    int server_port = SERVER_PORT;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Error creating socket");
        return NULL;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    inet_pton(AF_INET, IP_ADDRESS, &server_address.sin_addr);

    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Error connecting to server");
        close(client_socket);
        return NULL;
    }

    int bytes_received = recv(client_socket, buffer, MAX_BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("Received from server:\n%s\n", buffer);
    }

    close(client_socket);

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Insufficient arguments. Usage: %s <num_clients>\n", argv[0]);
        return 0;
    }

    int num_clients = atoi(argv[1]);
    pthread_t threads[num_clients];

    for (int i = 0; i < num_clients; i++) {
        if (pthread_create(&threads[i], NULL, client_thread, NULL) != 0) {
            perror("Error creating client thread");
        }
        usleep(1000);
    }

    for (int i = 0; i < num_clients; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}

