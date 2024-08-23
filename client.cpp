#include <iostream>
#include <string>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <ctime>

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

// Function to handle receiving messages from the server
void receive_messages(int client_socket) {
    char buffer[1024];

    while (true) {
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            log_message("Disconnected from server.");
            close(client_socket);
            break;
        }
        buffer[bytes_received] = '\0';
        std::cout << buffer << std::endl;
    }
}

int main() {
    // Create a socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        log_message("Failed to create socket");
        return -1;
    }
    log_message("Socket created successfully");

    // Define the server address
    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(54000);
    inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);

    // Connect to the server
    if (connect(client_socket, (sockaddr*)&server_address, sizeof(server_address)) == -1) {
        log_message("Failed to connect to server");
        return -1;
    }
    log_message("Connected to server");

    // Start a thread to receive messages from the server
    std::thread receiver_thread(receive_messages, client_socket);
    receiver_thread.detach();

    // Main loop to send messages to the server
    std::string message;
    while (true) {
        std::getline(std::cin, message);
        if (message == "/quit") break;

        if (send(client_socket, message.c_str(), message.size(), 0) == -1) {
            log_message("Failed to send message");
            break;
        }
    }

    // Close the socket and clean up
    close(client_socket);
    log_message("Connection closed");
    return 0;
}
