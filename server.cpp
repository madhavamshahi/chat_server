#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <string>
#include <netinet/in.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <ctime>

std::vector<int> clients; // List of client sockets
std::mutex clients_mutex; // Mutex to protect shared resources

// Utility function to get the current time as a string
std::string current_time() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    return std::ctime(&now_time);
}

// Function to log messages
void log_message(const std::string& message) {
    std::cout << "[" << current_time() << "] " << message << std::endl;
}

// Function to handle communication with a single client
void handle_client(int client_socket) {
    char buffer[1024];

    log_message("Client connected: " + std::to_string(client_socket));

    while (true) {
        // Receive message from the client
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            log_message("Client disconnected: " + std::to_string(client_socket));
            close(client_socket);

            // Remove the client from the list
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.erase(std::remove(clients.begin(), clients.end(), client_socket), clients.end());
            break;
        }

        buffer[bytes_received] = '\0';
        std::string message(buffer);

        log_message("Received message from client " + std::to_string(client_socket) + ": " + message);

        // Broadcast the message to all other connected clients
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (int client : clients) {
            if (client != client_socket) {
                send(client, message.c_str(), message.size(), 0);
            }
        }
    }
}

int main() {
    // Create a socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        log_message("Failed to create socket");
        return -1;
    }
    log_message("Socket created successfully");

    // Bind the socket to an IP address and port
    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(54002);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (sockaddr*)&server_address, sizeof(server_address)) == -1) {
        log_message("Failed to bind socket");
        return -1;
    }
    log_message("Socket bound to port 54000");

    // Start listening for incoming connections
    if (listen(server_socket, SOMAXCONN) == -1) {
        log_message("Failed to listen on socket");
        return -1;
    }
    log_message("Server is listening for connections");

    // Main server loop to accept new clients
    while (true) {
        sockaddr_in client_address{};
        socklen_t client_size = sizeof(client_address);
        int client_socket = accept(server_socket, (sockaddr*)&client_address, &client_size);

        if (client_socket == -1) {
            log_message("Failed to accept client connection");
            continue;
        }

        // Add the new client to the list
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.push_back(client_socket);
        }

        // Start a new thread to handle the client
        std::thread client_thread(handle_client, client_socket);
        client_thread.detach();
    }

    // Close the server socket
    close(server_socket);
    return 0;
}