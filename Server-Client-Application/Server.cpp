// Includes
#include "Server.h"
#include "OutputValues.h"
#include <iostream>
#include <fstream>
#include <ws2tcpip.h>
#include <thread>
#include <ctime>
#include <algorithm>
#include <stdexcept>

// Defines
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#pragma comment(lib, "Ws2_32.lib")
#pragma warning(disable: 4996)
Server::Server(int clientLimit, const char* listeningPort)
    : clientLimit(clientLimit), listeningPort(listeningPort), logPath("Record_of_chat.txt") {
    if (!initializeWinsock()) {
        exit(STARTUP_ERROR);
    }
    if (!setupServer()) {
        cleanupClients();
        exit(SETUP_ERROR);
    }
}

Server::~Server() {
    cleanupClients();
    closesocket(tcpSocket);
    WSACleanup();
}

bool Server::initializeWinsock() {
    WSADATA wsaData;
    int finalOutput = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (finalOutput != NO_ERROR) {
        displayError("Error initializing Winsock2", finalOutput);
        return false;
    }
    return true;
}

bool Server::setupServer() {
    struct addrinfo* result_addr = NULL, ideas;
    ZeroMemory(&ideas, sizeof(ideas));
    ideas.ai_flags = AI_PASSIVE;
    ideas.ai_family = AF_INET;
    ideas.ai_socktype = SOCK_STREAM;
    ideas.ai_protocol = IPPROTO_TCP;

    int finalOutput = getaddrinfo(NULL, listeningPort, &ideas, &result_addr);
    if (finalOutput != 0) {
        displayError("Error getting address info", finalOutput);
        WSACleanup();
        return false;
    }

    tcpSocket = socket(result_addr->ai_family, result_addr->ai_socktype, result_addr->ai_protocol);
    if (tcpSocket == INVALID_SOCKET) {
        displayError("Error creating server socket", WSAGetLastError());
        freeaddrinfo(result_addr);
        WSACleanup();
        return false;
    }

    finalOutput = bind(tcpSocket, result_addr->ai_addr, (int)result_addr->ai_addrlen);
    if (finalOutput == SOCKET_ERROR) {
        displayError("Error binding server socket", WSAGetLastError());
        cleanupSocketAndAddrInfo(tcpSocket, result_addr);
        return false;
    }

    if (!setupUDPServer()) {
        cleanupSocketAndAddrInfo(tcpSocket, result_addr);
        return false;
    }

    freeaddrinfo(result_addr);
    finalOutput = listen(tcpSocket, SOMAXCONN);
    if (finalOutput == SOCKET_ERROR) {
        displayError("Error setting server socket to listen", WSAGetLastError());
        closesocket(tcpSocket);
        closesocket(udpSocket);
        return false;
    }
    return true;
}

bool Server::setupUDPServer() {
    udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET) {
        displayError("Can't create UDP socket", WSAGetLastError());
        return false;
    }

    int broadcast = 1;
    int finalOutput = setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, sizeof(broadcast));
    if (finalOutput == SOCKET_ERROR) {
        displayError("Can't enable UDP broadcast", WSAGetLastError());
        closesocket(udpSocket);
        return false;
    }
    return true;
}

void Server::displayError(const char* errorMsg, int errorCode) {
    std::cerr << errorMsg << ": " << errorCode << std::endl;
}

void Server::cleanupSocketAndAddrInfo(SOCKET& socket, struct addrinfo* addrInfo) {
    closesocket(socket);
    freeaddrinfo(addrInfo);
    WSACleanup();
}

void Server::cleanupClients() {
    for (auto client : clientList) {
        closesocket(client->retrieveEndpoint());
        delete client;
    }
}



void Server::promptForServerIP() {
    std::cout << "Enter server IP address: ";
    std::cin >> hostIP;
}

void Server::displayServerInitialization() {
    std::cout << "Starting server..." << std::endl;
    std::cout << "IP: " << hostIP << ", Port: " << listeningPort << std::endl;
}

void Server::setupServerSocketForListening() {
    maxFd = tcpSocket;
    FD_ZERO(&masterSet);
    FD_ZERO(&activeSet);
    FD_SET(tcpSocket, &masterSet);
    waitDuration.tv_sec = 1;
}

void Server::startUdpBroadcastThread() {
    std::thread udpThread(&Server::sendUdpBroadcast, this);
    udpThread.detach();
}

int Server::getHighestFileDescriptor() {
    int highestFileDescriptor = tcpSocket;
    for (const auto& client : clientList) {
        if (client->retrieveEndpoint() > highestFileDescriptor) {
            highestFileDescriptor = client->retrieveEndpoint();
        }
    }
    return highestFileDescriptor;
}

void Server::handleSocketErrors(int finalOutput) {
    if (finalOutput == SOCKET_ERROR) {
        std::cerr << "Error in select: " << WSAGetLastError() << std::endl;
        closesocket(tcpSocket);
        WSACleanup();
        exit(PARAMETER_ERROR);
    }
}

void Server::checkAndHandleClientConnections() {
    if (FD_ISSET(tcpSocket, &activeSet)) {
        addNewClient();
    }
    for (int i = 0; i < clientList.size(); i++) {
        SOCKET soc_Client = clientList[i]->retrieveEndpoint();
        if (FD_ISSET(soc_Client, &activeSet)) {
            processClientQuery(clientList[i]);
        }
    }
}

void Server::removeDisconnectedClients() {
    clientList.erase(
        std::remove_if(clientList.begin(), clientList.end(), [](Client* client) {
            if (client->retrieveEndpoint() == INVALID_SOCKET) {
                std::cout << "(" << client->getUserAlias() << ") HAS DISCONNECTED" << std::endl;
                closesocket(client->retrieveEndpoint());
                delete client;
                return true;
            }
            return false;
            }),
        clientList.end());
}

void Server::execution() {
    promptForServerIP();
    displayServerInitialization();
    setupServerSocketForListening();
    startUdpBroadcastThread();

    while (true) {
        activeSet = masterSet;
        int highestFileDescriptor = getHighestFileDescriptor();
        int finalOutput = select(highestFileDescriptor + 1, &activeSet, NULL, NULL, &waitDuration);

        handleSocketErrors(finalOutput);
        checkAndHandleClientConnections();
        removeDisconnectedClients();
    }
}

void Server::sendUdpBroadcast() {
    sockaddr_in broadCastingAddressUDP{};
    broadCastingAddressUDP.sin_family = AF_INET;
    broadCastingAddressUDP.sin_addr.s_addr = INADDR_BROADCAST;
    broadCastingAddressUDP.sin_port = htons(static_cast<unsigned short>(std::stoi(listeningPort)));

    while (true) {
        std::string CliBroadcastMsg = hostIP + ":" + std::string(listeningPort);
        int finalOutput = sendto(udpSocket, CliBroadcastMsg.c_str(), CliBroadcastMsg.size(), 0, (sockaddr*)&broadCastingAddressUDP, sizeof(broadCastingAddressUDP));
        if (finalOutput == SOCKET_ERROR) {
            std::cerr << "Error sending UDP broadcast: " << WSAGetLastError() << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}


void Server::addNewClient() {
    struct sockaddr_in AddressOfClient {};
    int clientAddrLen = sizeof(AddressOfClient);

    SOCKET soc_Client = createClientSocket(AddressOfClient, clientAddrLen);

    if (soc_Client == INVALID_SOCKET) {
        return;
    }

    if (isServerFull()) {
        rejectClientDueToCapacity(soc_Client);
        return;
    }

    addClientToServer(soc_Client, AddressOfClient);
}

SOCKET Server::createClientSocket(sockaddr_in& clientAddress, int& clientAddressLength) {
    SOCKET soc_Client = accept(tcpSocket, (sockaddr*)&clientAddress, &clientAddressLength);

    if (soc_Client == INVALID_SOCKET) {
        std::cerr << "Error accepting client socket: " << WSAGetLastError() << std::endl;
    }

    return soc_Client;
}

bool Server::isServerFull() const {
    return clientList.size() >= clientLimit;
}

void Server::rejectClientDueToCapacity(SOCKET& clientSock) {
    std::string notification = "SERVER_LIMIT_REACHED";
    Client* disconnectingClient = new Client();
    disconnectingClient->assignEndpoint(clientSock);
    transmitToClient(notification, disconnectingClient);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    closesocket(clientSock);
    delete disconnectingClient;
}

void Server::addClientToServer(SOCKET& clientSock, sockaddr_in& clientAddress) {
    Client* newClient = new Client();
    newClient->assignEndpoint(clientSock);
    clientList.push_back(newClient);

    FD_SET(clientSock, &masterSet);
    updateMaxFD(clientSock);

    std::string notification = "SERVER_SUCCESS";
    send(clientSock, notification.c_str(), notification.size() + 1, 0);

    std::cout << "New client isActive from " << inet_ntoa(clientAddress.sin_addr) << ":" << ntohs(clientAddress.sin_port) << std::endl;
}

void Server::updateMaxFD(SOCKET& clientSock) {
    if (clientSock > maxFd) {
        maxFd = clientSock;
    }
}


bool Server::processClientQuery(Client* client) {
    // Receive the size of the incoming notification first.
    int SizeOfMsg = 0;
    int nbytes = recv(client->retrieveEndpoint(), reinterpret_cast<char*>(&SizeOfMsg), sizeof(SizeOfMsg), 0);
    if (nbytes <= 0) {
        // client disconnected
        closesocket(client->retrieveEndpoint());
        FD_CLR(client->retrieveEndpoint(), &masterSet);
        for (auto it = clientList.begin(); it != clientList.end(); ++it) {
            if ((*it)->retrieveEndpoint() == client->retrieveEndpoint()) {
                delete (*it);
                clientList.erase(it);
                break;
            }
        }
        return false;
    }

    // Allocate a holder of the appropriate size to receive the notification.
    char* holder = new char[SizeOfMsg];
    int bytesRead = 0;
    while (bytesRead < SizeOfMsg) {
        int finalOutput = recv(client->retrieveEndpoint(), holder + bytesRead, SizeOfMsg - bytesRead, 0);
        if (finalOutput == SOCKET_ERROR || finalOutput == 0) {
            // Error occurred or client disconnected
            delete[] holder;
            closesocket(client->retrieveEndpoint());
            FD_CLR(client->retrieveEndpoint(), &masterSet);
            for (auto it = clientList.begin(); it != clientList.end(); ++it) {
                if ((*it)->retrieveEndpoint() == client->retrieveEndpoint()) {
                    delete (*it);
                    clientList.erase(it);
                    break;
                }
            }
            return false;
        }
        bytesRead += finalOutput;
    }

    // Convert the received notification to a string.
    std::string notification(holder, SizeOfMsg);
    std::cout << "[Received] (" << client->getUserAlias() << "): " << notification << std::endl;
    delete[] holder;

    // Handle client request commands
    if (notification.find("$register") == 0) {
        handleRegisterRequest(client, notification);
    }
    else if (notification.find("$getlist") == 0) {
        handleGetListRequest(client);
    }
    else if (notification.find("$getlog") == 0) {
        handleGetLogRequest(client);
    }
    else if (notification.find("$exit") == 0) {
        handleExitRequest(client);
    }
    else if (notification.find("$chat") == 0) {
        handleChatRequest(client, notification);
    }
    else {
        handleDefaultChatRequest(client, notification);
    }
    return true;
}

void Server::handleRegisterRequest(Client* client, const std::string& notification) {
    std::string userAlias = notification.substr(10, notification.size() - 10);
    if (clientList.size() > clientLimit) {
        std::string notification = "SERVER_LIMIT_REACHED";
        transmitToClient(notification, client);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        closesocket(client->retrieveEndpoint());
        delete client;
    }
    else {
        // register user
        client->setUserAlias(userAlias);
        std::string recieveMessage_S = "SERVER_SUCCESS";
        send(client->retrieveEndpoint(), recieveMessage_S.c_str(), recieveMessage_S.size(), 0);
    }
}

void Server::handleGetListRequest(Client* client) {
    std::string listOfClients;
    listOfClients += "LIST ";
    for (auto& c : clientList) {
        listOfClients += c->getUserAlias() + ",";
    }
    if (clientList.size() > 1) {
        listOfClients.erase(listOfClients.size() - 1); // remove last comma
    }
    else {
        listOfClients = "LIST You are all alone in this server\n";
    }
    transmitToClient(listOfClients, client);
}

void Server::handleGetLogRequest(Client* client) {
    std::ifstream logDescriptor(logPath, std::ios::binary);
    if (!logDescriptor.good()) {
        std::cerr << "Error opening log file." << std::endl;
        return;
    }
    logDescriptor.seekg(0, std::ios::end);
    int LogSize = logDescriptor.tellg();
    logDescriptor.seekg(0, std::ios::beg);

    std::string logPrefix = "LOG ";
    int totalSize = LogSize + logPrefix.size();

    char* fileBuffer = new char[totalSize];
    memcpy(fileBuffer, logPrefix.c_str(), logPrefix.size());
    logDescriptor.read(fileBuffer + logPrefix.size(), LogSize);
    logDescriptor.close();

    std::string logString(fileBuffer, totalSize);
    transmitToClient(logString, client);
    delete[] fileBuffer;
}

void Server::handleExitRequest(Client* client) {
    std::string FinalMessage = "EXIT Goodbye! You have been disconnected.";
    transmitToClient(FinalMessage, client);
    std::cout << "(" << client->getUserAlias() << ") HAS DISCONNECTED\n";
    shutdown(client->retrieveEndpoint(), SD_SEND);

    char holder[256];
    recv(client->retrieveEndpoint(), holder, sizeof(holder), 0);
    int recvResult = recv(client->retrieveEndpoint(), holder, sizeof(holder), 0);
    if (recvResult == SOCKET_ERROR) {
        std::cerr << "Error receiving acknowledgment: " << WSAGetLastError() << std::endl;
    }
    closesocket(client->retrieveEndpoint());
    FD_CLR(client->retrieveEndpoint(), &masterSet);

    auto it = std::find_if(clientList.begin(), clientList.end(), [&client](const Client* c) { return c->retrieveEndpoint() == client->retrieveEndpoint(); });
    if (it != clientList.end()) {
        delete (*it);
        clientList.erase(it);
    }
}

void Server::handleChatRequest(Client* client, const std::string& notification) {
    std::string CliBroadcastMsg = "(" + client->getUserAlias() + "): " + notification.substr(6);
    CliBroadcastMsg = "\nCHAT " + CliBroadcastMsg;
    broadcastUdpMessage(CliBroadcastMsg, client);
    recordLog(CliBroadcastMsg);
}

void Server::handleDefaultChatRequest(Client* client, const std::string& notification) {
    std::string CliBroadcastMsg = "(" + client->getUserAlias() + "): " + notification;
    CliBroadcastMsg = "CHAT " + CliBroadcastMsg;
    broadcastUdpMessage(CliBroadcastMsg, client);
    recordLog(CliBroadcastMsg);
}

void Server::transmitToClient(const std::string& notification, Client* client) {
    sendMessageToSocket(notification, client->retrieveEndpoint());
}

void Server::broadcastUdpMessage(const std::string& notification, Client* sender) {
    for (auto& client : clientList) {
        if (client->retrieveEndpoint() != tcpSocket && client->retrieveEndpoint() != sender->retrieveEndpoint()) {
            sendMessageToSocket(notification, client->retrieveEndpoint());
        }
    }
}

void Server::sendMessageToSocket(const std::string& notification, SOCKET soc_Client) {
    uint32_t SizeOfMsg = static_cast<uint32_t>(notification.size());
    int finalOutput = send(soc_Client, reinterpret_cast<const char*>(&SizeOfMsg), sizeof(SizeOfMsg), 0);
    if (finalOutput == SOCKET_ERROR) {
        throw std::runtime_error("Failed to send notification size: " + std::to_string(WSAGetLastError()));
    }

    finalOutput = send(soc_Client, notification.c_str(), SizeOfMsg, 0);
    if (finalOutput == SOCKET_ERROR) {
        throw std::runtime_error("Failed to send notification: " + std::to_string(WSAGetLastError()));
    }
}

void Server::recordLog(const std::string& notification) {
    std::ofstream logDescriptor(logPath, std::ios::app);
    if (!logDescriptor.good()) {
        std::cerr << "Error opening log file." << std::endl;
        return;
    }
    std::time_t now = std::time(nullptr);
    std::tm* CurrentTime = std::localtime(&now);
    char holdTime[80];
    std::strftime(holdTime, sizeof(holdTime), "[%Y-%m-%d %H:%M:%S]", CurrentTime);
    logDescriptor << holdTime << " " << notification << std::endl;
    logDescriptor.close();
}

void Server::setup() {
    FD_ZERO(&masterSet);
    FD_ZERO(&activeSet);
    FD_SET(tcpSocket, &masterSet);
    waitDuration.tv_sec = 1;
    waitDuration.tv_usec = 0;
    maxFd = tcpSocket;
}
