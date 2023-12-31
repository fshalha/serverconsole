#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "OutputValues.h"
#include "Client.h"
#include "Server.h"
#include <limits>

#pragma comment(lib, "Ws2_32.lib")

#define NUMBER_OF_CLIENTS 3

void startServer() {
    Server server(NUMBER_OF_CLIENTS, "5000");
    try {
        server.execution();
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        exit(OutputMessageType::STARTUP_ERROR);
    }
}

void startClient() {
    Client client_connected;
    try {
        client_connected.awaitUdpAnnouncement();
        std::string userAlias;
        std::cout << "Enter username: ";
        std::cin >> userAlias;
        client_connected.enrollUser(userAlias);
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        exit(OutputMessageType::CONNECT_ERROR);
    }

    bool End = false;

    std::thread serverListener([&]() {
        while (client_connected.isLinked() && !End) {
            try {
                std::string notification = client_connected.fetchCommunication(End);
                if (!notification.empty())
                    std::cout << notification;
            }
            catch (const std::exception& ex) {
                std::cerr << "Error: " << ex.what() << std::endl;
                break;
            }
        }
        exit(0);
        });

    std::thread Client_input([&]() {
        for (; client_connected.isLinked() && !End;) {
            std::string input_compare;
            std::cout << "Enter command or message: ";
            std::getline(std::cin >> std::ws, input_compare);
            if (input_compare == "$help")
            {
                std::string DetailsMessage = "Available commands:\n\n";
                DetailsMessage += "$register username: Registers a new user with the specified username. Returns SV_SUCCESS if successful, or SV_FULL if the server is at capacity.\n\n";
                DetailsMessage += "$getlist: Returns a list of connected users.\n\n";
                DetailsMessage += "$getlog: Returns the chat log.\n\n";
                DetailsMessage += "$exit: Disconnects the user from the server.\n\n";
                DetailsMessage += "$chat message: Sends a message to all connected clients.\n\n";
                DetailsMessage += "$help: Displays this help message.\n\n";
                DetailsMessage += "Enter command or message: ";
                std::cout << DetailsMessage;
                
            }
            if (input_compare == "$quit") {
                End = true;
                std::cout << "You have quitted the chat\n";
                break;
            }
            else if (input_compare.find("$chat") == 0) {
                try {
                    client_connected.sendMessage(input_compare.substr(6));
                }
                catch (const std::exception& ex) {
                    std::cerr << "Error: " << ex.what() << std::endl;
                }
            }
            else {
                try {
                    client_connected.runInstruction(input_compare);
                }
                catch (const std::exception& ex) {
                    std::cerr << "Error: " << ex.what() << std::endl;
                }
            }
        }
        });

    serverListener.join();
    Client_input.join();
    client_connected.terminateLink();
}

int main() {
    std::string input_compare;
    std::cout << "Start as [1] server or [2] client? (use ip 127.0.0.1)";
    std::cin >> input_compare;

    if (input_compare == "1") {
        startServer();
    }
    else if (input_compare == "2") {
        startClient();
    }
    else {
        std::cout << "Invalid input." << std::endl;
        return OutputMessageType::PARAMETER_ERROR;
    }

    return OutputMessageType::SUCCESS;
}
