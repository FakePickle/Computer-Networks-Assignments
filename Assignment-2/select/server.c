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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_CLIENTS 1024  // Adjust based on system limits

int main(void) {
    int serverSocket, clientSocket, maxSd, activity;
    struct sockaddr_in serverAddress;
    fd_set readfds;
    int clientSockets[MAX_CLIENTS] = {0};

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Error creating socket");
        return EXIT_FAILURE;
    }

    // Set socket to non-blocking
    int flags = fcntl(serverSocket, F_GETFL, 0);
    fcntl(serverSocket, F_SETFL, flags | O_NONBLOCK);

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(SERVER_PORT);

    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("Error binding socket");
        close(serverSocket);
        return EXIT_FAILURE;
    }

    if (listen(serverSocket, SOMAXCONN) < 0) {
        perror("Error listening for connections");
        close(serverSocket);
        return EXIT_FAILURE;
    }

    printf("Server listening on port %d...\n", SERVER_PORT);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);
        maxSd = serverSocket;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clientSockets[i];
            if (sd > 0) {
                FD_SET(sd, &readfds);
            }
            if (sd > maxSd) {
                maxSd = sd;
            }
        }

        activity = select(maxSd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            perror("select error");
            break;
        }

        if (FD_ISSET(serverSocket, &readfds)) {
            clientSocket = accept(serverSocket, NULL, NULL);
            if (clientSocket < 0) {
                if (errno != EWOULDBLOCK) {
                    perror("accept failed");
                }
                continue;
            }

            printf("New connection, socket fd is %d\n", clientSocket);

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clientSockets[i] == 0) {
                    clientSockets[i] = clientSocket;
                    break;
                }
            }

            sendTopTwoProcesses(clientSocket);
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clientSockets[i];
            if (FD_ISSET(sd, &readfds)) {
                char buffer[1024];
                int valread = read(sd, buffer, sizeof(buffer));
                if (valread == 0) {
                    close(sd);
                    clientSockets[i] = 0;
                }
            }
        }
    }

    close(serverSocket);
    return EXIT_SUCCESS;
}
