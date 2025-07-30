/* Example of a multithreaded server. */
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "yatpool.h"

#define SERVERPORT 8989
#define BUFSIZE 4096
#define MAXLINE 4096
#define NUM_THREADS 4
#define QUEUE_SIZE 64
#define SERVER_BACKLOG 100 // Max. number of connections the server can handle

typedef struct sockaddr_in SA_IN;
typedef struct sockaddr SA;

// Flag to handle interrupt signals for server
static volatile sig_atomic_t g_shutdown_flag = 0;

// Helper function for printing errors
void check(int exp, const char *msg) {
    if (exp < 0) {
        int errno_curr = errno;
        fprintf(stdout, "%s, %s\n", msg, strerror(errno_curr));
        exit(EXIT_FAILURE);
    }
}

// Helper function to set the shutdown flag if the user presses
// CTRL+C to shut down server.
void handle_sigint(int _sig) {
    (void)_sig; // To avoid compiler warnings about unused variables
    printf("\nDetected shutdown signal. Shutting down ...\n");
    g_shutdown_flag = 1;
}

// Reads data from the client and echoes it back to the client.
void *handle_connection(void *client_socket_ptr) {
    int client_socket = *((int *)client_socket_ptr);
    char buffer[BUFSIZE];
    size_t bytes_read;

    free(client_socket_ptr);

    // Read message from client
    while ((bytes_read = read(client_socket, buffer, BUFSIZE)) > 0) {
        printf("Bytes read = %ld\n", bytes_read);
        printf("Sending %zu bytes ...\n", bytes_read);
        fflush(stdout);
        write(client_socket, buffer, bytes_read);
    }
    check(bytes_read, "Read error");

    printf("Closing connection ...\n");
    fflush(stdout);
    close(client_socket);
    return NULL;
}

int main(int argc, char **argv) {
    int server_socket;
    int client_socket;
    int addr_size;
    SA_IN server_addr, client_addr;

    // Set up signal handler
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    check((sigaction(SIGINT, &sa, NULL)), "Error setting up signal handler.");

    // Create socket for listening
    check(server_socket = socket(AF_INET, SOCK_STREAM, 0), "Failed to create socket.");

    // Initialize the address struct
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVERPORT);

    // Bind address to socket
    check(bind(server_socket, (SA *)&server_addr, sizeof(server_addr)), "Bind failed.");

    // Listen for incoming connections
    check(listen(server_socket, SERVER_BACKLOG), "Listen failed.");

    YATPool* pool = yatpool_init(NUM_THREADS, QUEUE_SIZE);
    if (!pool) {
        check(-1, "Failed to create threadpool.");
    }

    // Read any incoming requests
    while (!g_shutdown_flag) {
        printf("Multithreaded TCP server listening on port %d ...\n", SERVERPORT);
        printf("Press CTRL+C to shutdown.\n");
        fflush(stdout);

        char addr[MAXLINE + 1];
        addr_size = sizeof(SA_IN);

        // Accept connection
        client_socket = accept(server_socket, (SA *)&client_addr, (socklen_t *)&addr_size);
        if (client_socket < 0 && errno == EINTR) {
            // Keyboard interrupt while accepting client connection
            continue;
        } else if (client_socket < 0) {
            fprintf(stdout, "Accept failed, %s\n", strerror(errno));
            yatpool_destroy(pool);
            close(server_socket);
            printf("Server shut down complete. Exiting ...\n");
            return EXIT_FAILURE;
        }
        inet_ntop(AF_INET, &client_addr, addr, MAXLINE);
        printf("Connection received from client %s.\n", addr);
        fflush(stdout);

        // Handle connection
        int *client_ptr = (int *)malloc(sizeof(int));
        *client_ptr = client_socket;

        yatpool_put(pool, &handle_connection, client_ptr, NULL);
    }

    yatpool_destroy(pool);
    close(server_socket);
    printf("Server shut down complete. Exiting ...\n");
    return EXIT_SUCCESS;
}
