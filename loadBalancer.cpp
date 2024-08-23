#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <mutex>

std::mutex log_mutex;

void log_message(const std::string &message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cout << message << std::endl;
}

// Function to handle communication between the client and the selected server
void handle_client(int client_socket, const sockaddr_in &server_address) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        log_message("Failed to create server socket for client.");
        close(client_socket);
        return;
    }

    if (connect(server_socket, (sockaddr*)&server_address, sizeof(server_address)) == -1) {
        log_message("Failed to connect to server.");
        close(client_socket);
        close(server_socket);
        return;
    }

    // Create threads to handle communication in both directions
    std::thread server_to_client([client_socket, server_socket]() {
        char buffer[1024];
        while (true) {
            int bytes_received = recv(server_socket, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0) break;
            send(client_socket, buffer, bytes_received, 0);
        }
        close(client_socket);
        close(server_socket);
    });

    std::thread client_to_server([client_socket, server_socket]() {
        char buffer[1024];
        while (true) {
            int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0) break;
            send(server_socket, buffer, bytes_received, 0);
        }
        close(client_socket);
        close(server_socket);
    });

    server_to_client.detach();
    client_to_server.detach();
}

int main() {
    std::vector<sockaddr_in> servers;
    int current_server = 0;

    // Example servers
    sockaddr_in server1{};
    server1.sin_family = AF_INET;
    server1.sin_port = htons(54001);  // Port for server 1
    inet_pton(AF_INET, "127.0.0.1", &server1.sin_addr);

    sockaddr_in server2{};
    server2.sin_family = AF_INET;
    server2.sin_port = htons(54002);  // Port for server 2
    inet_pton(AF_INET, "127.0.0.1", &server2.sin_addr);

    servers.push_back(server1);
    servers.push_back(server2);

    // Create the load balancer socket
    int lb_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (lb_socket == -1) {
        log_message("Failed to create load balancer socket.");
        return -1;
    }

    sockaddr_in lb_address{};
    lb_address.sin_family = AF_INET;
    lb_address.sin_port = htons(54000);  // Load balancer port
    lb_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(lb_socket, (sockaddr*)&lb_address, sizeof(lb_address)) == -1) {
        log_message("Failed to bind load balancer socket.");
        close(lb_socket);
        return -1;
    }

    if (listen(lb_socket, SOMAXCONN) == -1) {
        log_message("Failed to listen on load balancer socket.");
        close(lb_socket);
        return -1;
    }

    log_message("Load balancer is listening on port 54000.");

    // Main loop to accept client connections
    while (true) {
        sockaddr_in client_address{};
        socklen_t client_size = sizeof(client_address);
        int client_socket = accept(lb_socket, (sockaddr*)&client_address, &client_size);

        if (client_socket == -1) {
            log_message("Failed to accept client connection.");
            continue;
        }

        // Select the next server using round-robin
        sockaddr_in server_address = servers[current_server];
        current_server = (current_server + 1) % servers.size();

        log_message("Forwarding client to server on port " + std::to_string(ntohs(server_address.sin_port)));

        // Handle the client-server communication
        std::thread(handle_client, client_socket, server_address).detach();
    }

    close(lb_socket);
    return 0;
}
