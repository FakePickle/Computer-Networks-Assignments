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
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Error creating socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in serverAddress = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("Error binding socket");
        close(serverSocket);
        return EXIT_FAILURE;
    }

    if (listen(serverSocket, 100) < 0) {
        perror("Error listening for connections");
        close(serverSocket);
        return EXIT_FAILURE;
    }

    printf("Server listening on port %d...\n", SERVER_PORT);

    while (1) {
        int clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket < 0) {
            perror("Error accepting connection");
            continue;
        }

        sendTopTwoProcesses(clientSocket);
        close(clientSocket);
    }

    close(serverSocket);
    return EXIT_SUCCESS;
}

