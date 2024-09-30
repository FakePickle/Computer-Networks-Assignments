#include "utils.h"

static int getProcessInfo(ProcessInfo *procInfo, int pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    fscanf(fp, "%d %s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
           &procInfo->pid, procInfo->name, &procInfo->userTime, &procInfo->kernelTime);
    fclose(fp);
    return 0;
}

static int compareCpu(const void *a, const void *b) {
    const ProcessInfo *proc1 = a;
    const ProcessInfo *proc2 = b;
    unsigned long totalTime1 = proc1->userTime + proc1->kernelTime;
    unsigned long totalTime2 = proc2->userTime + proc2->kernelTime;
    return (totalTime2 > totalTime1) - (totalTime2 < totalTime1);
}

static void sendTopTwoProcesses(int clientSocket) {
    DIR *procDir = opendir("/proc");
    printf("Sending top two processes\n");
    if (!procDir) {
        perror("Error opening /proc");
        return;
    }

    ProcessInfo processes[MAX_PROCESSES];
    int procCount = 0;
    struct dirent *entry;

    while ((entry = readdir(procDir)) != NULL && procCount < MAX_PROCESSES) {
        int pid = atoi(entry->d_name);
        if (pid > 0) {
            if (getProcessInfo(&processes[procCount], pid) == 0) {
                procCount++;
            }
        }
    }
    closedir(procDir);

    qsort(processes, procCount, sizeof(ProcessInfo), compareCpu);

    for (int i = 0; i < 2 && i < procCount; i++) {
        char data[MAX_BUFFER_SIZE];
        snprintf(data, sizeof(data), "Process Name: %s, PID: %d, User CPU Time: %lu, Kernel CPU Time: %lu\n",
                 processes[i].name, processes[i].pid, processes[i].userTime, processes[i].kernelTime);

        if (send(clientSocket, data, strlen(data), 0) < 0) {
            perror("Error sending data");
            return;
        }
    }
}

int main(void) {
    int server_socket, client_socket, max_sd, activity, sd;
    struct sockaddr_in server_address, client_address;
    socklen_t client_len = sizeof(client_address);
    char buffer[MAX_BUFFER_SIZE];
    fd_set readfds;
    int client_sockets[MAX_PENDING_CONNECTIONS];

    for (int i = 0; i < MAX_PENDING_CONNECTIONS; i++) {
        client_sockets[i] = 0;
    }

    // Create the server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error creating socket");
        return EXIT_FAILURE;
    }

    // Set up the server address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Error binding socket");
        close(server_socket);
        return EXIT_FAILURE;
    }

    // Listen for incoming connections
    if (listen(server_socket, MAX_PENDING_CONNECTIONS) < 0) {
        perror("Error listening for connections");
        close(server_socket);
        return EXIT_FAILURE;
    }

    printf("Server listening on port %d...\n", SERVER_PORT);

    // Initialize the master file descriptor set
    int addrlen = sizeof(server_address);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);
        max_sd = server_socket;

        for (int i = 0; i < MAX_PENDING_CONNECTIONS; i++) {
            sd = client_sockets[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // Wait for activity on any of the sockets
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0 && errno != EINTR) {
            perror("Error in select");
            break;
        }

        // Check if it's an incoming connection
        if (FD_ISSET(server_socket, &readfds)) {
            client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_len);
            if (client_socket < 0) {
                perror("Error accepting connection");
                continue;
            }

            printf("New connection: socket FD is %d\n", client_socket);

            // Send the top two processes to the new client immediately
            sendTopTwoProcesses(client_socket);

            // Add the new client socket to the set
            for (int i = 0; i < MAX_PENDING_CONNECTIONS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = client_socket;
                    printf("Adding to list of sockets as %d\n", i);
                    break; // Stop after adding
                }
            }
        }

        for (int i = 0; i < MAX_PENDING_CONNECTIONS; i++) {
            sd = client_sockets[i];
            if (FD_ISSET(sd, &readfds)) {
                int valread = read(sd, buffer, MAX_BUFFER_SIZE);
                if (valread == 0) {
                    getpeername(sd, (struct sockaddr *)&client_address, &client_len);
                    printf("Host disconnected, ip %s, port %d\n",
                           inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
                    close(sd);
                    client_sockets[i] = 0;
                }
            }
        }
    }

    close(server_socket);
    return EXIT_SUCCESS;
}

