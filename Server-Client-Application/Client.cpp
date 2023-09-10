#include "Client.h"
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <vector>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma warning(disable: 4996)

Client::Client() : logPath("connected_clients.txt"), isActive(false) {
    initializeWinsock();
    createTcpSocket();
    createUdpSocket();
    configureUdpSocket();
}

Client::~Client() {
    if (isActive) {
        terminateLink();
    }
    closesocket(udpClientEndpoint);
    cleanupWinsock();
}

void Client::initializeWinsock() {
    WSADATA wsaData;
    int finalOutput = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (finalOutput != 0) {
        throw std::runtime_error("WSAStartup failed: " + std::to_string(finalOutput));
    }
}

void Client::cleanupWinsock() {
    WSACleanup();
}

void Client::createTcpSocket() {
    soc_Client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (soc_Client == INVALID_SOCKET) {
        cleanupWinsock();
        handleError("Failed to create socket");
    }
}

void Client::createUdpSocket() {
    udpClientEndpoint = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpClientEndpoint == INVALID_SOCKET) {
        cleanupWinsock();
        handleError("Failed to create UDP socket");
    }
}

void Client::configureUdpSocket() {
    int setBroadcast = 1;
    int finalOutput = setsockopt(udpClientEndpoint, SOL_SOCKET, SO_BROADCAST, (char*)&setBroadcast, sizeof(setBroadcast));
    if (finalOutput == SOCKET_ERROR) {
        closesocket(udpClientEndpoint);
        cleanupWinsock();
        handleError("Failed to set UDP socket options");
    }
}

void Client::handleError(const std::string& errorMessage) {
    throw std::runtime_error(errorMessage + ": " + std::to_string(WSAGetLastError()));
}


// Helper function to bind the UDP socket
void Client::bindUdpSocket(sockaddr_in& AddressOfUdpClient) {
    int finalOutput = bind(udpClientEndpoint, (sockaddr*)&AddressOfUdpClient, sizeof(AddressOfUdpClient));
    if (finalOutput == SOCKET_ERROR) {
        throw std::runtime_error("[Error] Failed to bind UDP socket. Code: " + std::to_string(WSAGetLastError()));
    }
}

// Helper function to set the UDP socket to receive broadcast messages
void Client::setUdpSocketBroadcast() {
    int allowBroadcast = 1;
    if (setsockopt(udpClientEndpoint, SOL_SOCKET, SO_BROADCAST, (char*)&allowBroadcast, sizeof(allowBroadcast)) < 0) {
        throw std::runtime_error("[Error] Failed to set SO_BROADCAST for UDP client socket. Code: " + std::to_string(WSAGetLastError()));
    }
}

// Helper function to receive server broadcast
std::string Client::receiveUdpBroadcast() {
    char recvBuffer[256];
    sockaddr_in AddressOfServerBroadcast{};
    int serverBroadcastAddrSize = sizeof(AddressOfServerBroadcast);
    int finalOutput = recvfrom(udpClientEndpoint, recvBuffer, sizeof(recvBuffer) - 1, 0, (sockaddr*)&AddressOfServerBroadcast, &serverBroadcastAddrSize);
    if (finalOutput == SOCKET_ERROR) {
        throw std::runtime_error("[Error] Failed to receive UDP broadcast. Code: " + std::to_string(WSAGetLastError()));
    }
    recvBuffer[finalOutput] = '\0';
    return std::string(recvBuffer);
}

void Client::awaitUdpAnnouncement() {
    sockaddr_in AddressOfUdpClient{};
    AddressOfUdpClient.sin_family = AF_INET;
    AddressOfUdpClient.sin_addr.s_addr = htonl(INADDR_ANY);
    AddressOfUdpClient.sin_port = htons(5000);

    bindUdpSocket(AddressOfUdpClient);
    setUdpSocketBroadcast();

    std::string CliBroadcastMsg = receiveUdpBroadcast();

    size_t separatorPos = CliBroadcastMsg.find(':');
    if (separatorPos == std::string::npos) {
        throw std::runtime_error("[Error] Invalid broadcast notification received");
    }

    std::string receivedServerIP = CliBroadcastMsg.substr(0, separatorPos);
    std::string receivedServerPort = CliBroadcastMsg.substr(separatorPos + 1);

    connectToServer(receivedServerIP.c_str(), receivedServerPort.c_str());
    closesocket(udpClientEndpoint);
}

void Client::connectToServer(const char* hostIP, const char* listeningPort) {
    sockaddr_in addressOfServer;
    addressOfServer.sin_family = AF_INET;
    addressOfServer.sin_addr.s_addr = inet_addr(hostIP);
    addressOfServer.sin_port = htons(atoi(listeningPort));

    int finalOutput = connect(soc_Client, (SOCKADDR*)&addressOfServer, sizeof(addressOfServer));
    if (finalOutput == SOCKET_ERROR) {
        throw std::runtime_error("[Error] Failed to connect to server. Code: " + std::to_string(WSAGetLastError()));
    }

    isActive = true;
}

void Client::processServerResponse(const std::string& responseConst)
{
    std::string response = responseConst; // Create a non-const copy

    std::string success = "SERVER_SUCCESS\0";
    std::string serverLimitMsg = "SERVER_LIMIT_REACHED\0";

    if (response.back() == '\n')
    {
        response.pop_back(); // Now it's safe to call pop_back()
    }

    size_t i = 0;
    bool similar = true;
    while (i < response.length() && similar) // Changed for loop to while loop
    {
        if (response[i] != success[i])
        {
            similar = false;
        }
        i++;
    }

    i = 0;
    bool serverLimitReached = true;
    while (i < response.length() && serverLimitReached) // Changed for loop to while loop
    {
        if (response[i] != serverLimitMsg[i])
        {
            serverLimitReached = false;
        }
        i++;
    }

    if (serverLimitReached)
    {
        terminateLink();
        throw std::runtime_error("[Error] Server is full. Please try again later.");
    }
    else if (!similar)
    {
        throw std::runtime_error("[Error] Failed to register user: " + response);
    }
}
bool Client::isLinked()
{
    return isActive;
}
void Client::assignEndpoint(SOCKET newSocket)
{
    soc_Client = newSocket;
}
SOCKET Client::retrieveEndpoint() const
{
    return soc_Client;
}
std::string Client::getUserAlias() const
{
    return userAlias;
}
void Client::setUserAlias(std::string _username)
{
    this->userAlias = _username;
}

void Client::enrollUser(std::string userAlias)
{
    checkConnection();

    this->userAlias = userAlias;
    std::string CommandOfRegister = "$register " + userAlias;

    sendCommandSize(CommandOfRegister);
    sendCommand(CommandOfRegister);

    std::string response = receiveServerResponse();
    processServerResponse(response);
}

void Client::checkConnection()
{
    if (!isActive)
    {
        throw std::runtime_error("[Error] Client is not isActive to server.");
    }
}

void Client::sendCommandSize(const std::string& command)
{
    int lengthOfCommand = static_cast<int>(command.length());
    int finalOutput = send(soc_Client, reinterpret_cast<char*>(&lengthOfCommand), sizeof(lengthOfCommand), 0);
    if (finalOutput == SOCKET_ERROR)
    {
        throw std::runtime_error("[Error] Failed to send command size. Code: " + std::to_string(WSAGetLastError()));
    }
}

void Client::sendCommand(const std::string& command)
{
    int finalOutput = send(soc_Client, command.c_str(), command.length(), 0);
    if (finalOutput == SOCKET_ERROR)
    {
        throw std::runtime_error("[Error] Failed to send command. Code: " + std::to_string(WSAGetLastError()));
    }
}

std::string Client::receiveServerResponse()
{
    char responseHolder[256];
    int finalOutput = recv(soc_Client, responseHolder, sizeof(responseHolder), 0);
    if (finalOutput == SOCKET_ERROR)
    {
        throw std::runtime_error("[Error] Failed to receive server response. Code: " + std::to_string(WSAGetLastError()));
    }
    return std::string(responseHolder, finalOutput);
}



void Client::runInstruction(std::string command)
{
    checkConnection();
    sendCommandSize(command);
    sendCommand(command);
    std::cout << "[Executed] " << command << std::endl;
}

void Client::sendMessage(std::string notification)
{
    checkConnection();
    std::string chatCommand = "$chat " + notification;
    sendCommandSize(chatCommand);
    sendCommand(chatCommand);
    std::cout << "[Sent out] " << notification << std::endl;
}

void Client::terminateLink()
{
    if (!isActive)
    {
        return;
    }
    shutdownConnection();
    std::cout << "\nDisconnected\n";
    closesocket(soc_Client);
    isActive = false;
}

void Client::shutdownConnection()
{
    int finalOutput = shutdown(soc_Client, SD_SEND);
    if (finalOutput == SOCKET_ERROR)
    {
        throw std::runtime_error("[Error] Failed to shutdown connection. Code: " + std::to_string(WSAGetLastError()));
    }
}

std::string Client::processMessage(const std::string& notification, bool& indicator) {
    if (notification.find("SERVER_SUCCESS") == 0 || notification.find("SERVER_LIMIT_REACHED") == 0) {
        return notification;
    }
    else if (notification.find("CHAT") == 0) {
        return "\033[2K\r" + notification + "\nEnter command or notification: ";
    }
    else if (notification.find("LIST") == 0) {
        return "\033[2K\r" + notification.substr(5) + "\nEnter command or notification: ";
    }
    else if (notification.find("LOG") == 0) {
        recordLog(notification.substr(4));
        return "\033[2K\r" + notification.substr(4) + "\nEnter command or notification: ";
    }
    else if (notification.find("EXIT") == 0) {
        indicator = true;
        return "\033[2K\r" + notification.substr(5);
    }
    else if (notification.find("SERVER_LIMIT_REACHED") == 0) {
        indicator = true;
        return "Server is currently full";
    }
    else {
        return "";
    }
}

void Client::recordLog(const std::string& logMsg) {
    std::ofstream logDescriptor(logPath, std::ios::app);
    if (!logDescriptor.good()) {
        std::cerr << "Error: Unable to open log file." << std::endl;
    }
    else {
        logDescriptor << logMsg << std::endl;
    }
}

std::string Client::fetchCommunication(bool& indicator) {
    if (!isActive) {
        throw std::runtime_error("Error: Client not isActive.");
    }

    for (;;) {
        int SizeOfMsg = 0;
        int nbytes = recv(soc_Client, reinterpret_cast<char*>(&SizeOfMsg), sizeof(SizeOfMsg), 0);
        if (nbytes <= 0) {
            throw std::runtime_error("Error: Unable to receive notification size. Code: " + std::to_string(WSAGetLastError()));
        }

        std::vector<char> holder(SizeOfMsg);
        nbytes = recv(soc_Client, holder.data(), SizeOfMsg, 0);
        if (nbytes <= 0) {
            throw std::runtime_error("Error: Unable to receive notification. Code: " + std::to_string(WSAGetLastError()));
        }

        std::string notification(holder.begin(), holder.end());
        std::string processedMessage = processMessage(notification, indicator);
        if (!processedMessage.empty()) {
            return processedMessage;
        }
    }
}



