#include "utils.h"

// Function to send data to the client
int send_data(int socket, char *data) {
    int bytes_sent = send(socket, data, strlen(data), 0);
    if (bytes_sent < 0) {
        perror("Error sending data");
        return -1;
    }
    return 0;
}

// Function to get process information from /proc/[pid]/stat
int get_process_info(struct process_info *proc_info, int pid) {
    char path[256];
    sprintf(path, "/proc/%d/stat", pid);

    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    // Parsing pid, process name, user CPU time, and kernel CPU time
    fscanf(fp, "%d %s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
           &proc_info->pid, proc_info->name, &proc_info->user_time, &proc_info->kernel_time);
    fclose(fp);
    return 0;
}

// Comparison function for sorting processes by total CPU time
int compare_cpu(const void *a, const void *b) {
    unsigned long cpu_a = ((struct process_info *)a)->user_time + ((struct process_info *)a)->kernel_time;
    unsigned long cpu_b = ((struct process_info *)b)->user_time + ((struct process_info *)b)->kernel_time;
    return (cpu_b > cpu_a) - (cpu_b < cpu_a);
}

// Function to find and send the top two CPU-consuming processes
void send_top_two_processes(int client_socket) {
    struct dirent *entry;
    DIR *proc_dir = opendir("/proc");
    struct process_info processes[1024];
    int proc_count = 0;

    if (!proc_dir) {
        perror("Error opening /proc");
        return;
    }

    // Iterate through /proc to find process directories
    while ((entry = readdir(proc_dir)) != NULL) {
        if (entry->d_type == DT_DIR && atoi(entry->d_name) > 0) {
            int pid = atoi(entry->d_name);
            struct process_info proc_info;
            if (get_process_info(&proc_info, pid) == 0) {
                processes[proc_count++] = proc_info;
            }
        }
    }
    closedir(proc_dir);

    // Sort processes by total CPU time (user + kernel)
    qsort(processes, proc_count, sizeof(struct process_info), compare_cpu);

    // Send the top two CPU-consuming processes
    for (int i = 0; i < 2 && i < proc_count; i++) {
        char data[MAX_BUFFER_SIZE];
        sprintf(data, "Process Name: %s, PID: %d, User CPU Time: %lu, Kernel CPU Time: %lu\n",
                processes[i].name, processes[i].pid, processes[i].user_time, processes[i].kernel_time);
        send_data(client_socket, data);
    }
}

// Function to handle each client connection
void *handle_client(void *arg) {
    int client_socket = *((int *)arg);
    free(arg);

    send_top_two_processes(client_socket);

    close(client_socket);
    return NULL;
}

// Main server function
int main() {
    int server_socket;
    struct sockaddr_in server_address;

    // Create the server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error creating socket");
        return -1;
    }

    // Set up the server address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the server address
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Error binding socket");
        close(server_socket);
        return -1;
    }

    // Listen for incoming connections
    if (listen(server_socket, MAX_PENDING_CONNECTIONS) < 0) {
        perror("Error listening for connections");
        close(server_socket);
        return -1;
    }

    printf("Server listening on port %d...\n", SERVER_PORT);

    while (1) {
        int *client_socket = malloc(sizeof(int));
        if (!client_socket) {
            perror("Memory allocation failed");
            continue;
        }

        *client_socket = accept(server_socket, NULL, NULL);
        if (*client_socket < 0) {
            perror("Error accepting connection");
            free(client_socket);
            continue;
        }

        printf("Client connected with IP address %s\n", inet_ntoa(server_address.sin_addr));

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, client_socket) != 0) {
            perror("Error creating thread");
            close(*client_socket);
            free(client_socket);
        }

        pthread_detach(thread);  // Detach the thread to handle clean-up automatically
    }

    close(server_socket);
    return 0;
}

