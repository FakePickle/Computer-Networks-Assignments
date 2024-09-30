#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#define SERVER_PORT 8080
#define MAX_PROCESSES 1024
#define MAX_BUFFER_SIZE 1024
#define MAX_PENDING_CONNECTIONS 10
#define IP_ADDRESS "127.0.0.1"

typedef struct process_info {
    int pid;
    char name[256];
    unsigned long userTime;
    unsigned long kernelTime;
} ProcessInfo;

// Server functions
int get_process_info(ProcessInfo *proc_info, int pid); // Get process information from /proc/[pid]/stat
int compare_cpu(const void *process_1, const void *process_2); // Comparison function for sorting processes by total CPU time
void send_top_two_processes(int client_socket); // Find and send the top two CPU-consuming processes
void *handle_client(void *arg); // Function to handle client requests

// Client functions
void connect_to_server(); // Function to handle single connection to the server
void *client_thread(void *arg); // Function for client thread (used when multiple clients are requested)

