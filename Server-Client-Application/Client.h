#pragma once

#include <string>
#include <winsock2.h>

#pragma comment(lib, "Ws2_32.lib")

class Client {
public:
    Client();
    ~Client();
    void connectToServer(const char* hostIP, const char* listeningPort);
    void enrollUser(std::string userAlias);
    void runInstruction(std::string command);
    void sendMessage(std::string notification);
    void terminateLink();
    std::string fetchCommunication(bool& indicator);
    bool isLinked();
    void assignEndpoint(SOCKET newSocket);
    SOCKET retrieveEndpoint() const;
    std::string getUserAlias() const;
    void setUserAlias(std::string newUsername);
    void awaitUdpAnnouncement();

private:
    void initializeWinsock();
    void cleanupWinsock();
    void createTcpSocket();
    void createUdpSocket();
    void configureUdpSocket();
    void handleError(const std::string& errorMessage);
    std::string processMessage(const std::string& notification, bool& indicator);
    void recordLog(const std::string& logMsg);
    void checkConnection();
    void sendCommandSize(const std::string& command);
    void sendCommand(const std::string& command);
    std::string receiveServerResponse();
    void shutdownConnection();
    void processServerResponse(const std::string& response);
    void bindUdpSocket(sockaddr_in& AddressOfUdpClient);
    void setUdpSocketBroadcast();
    std::string receiveUdpBroadcast();
    SOCKET soc_Client;
    SOCKET udpClientEndpoint;
    bool isActive;
    std::string userAlias;
    std::string logPath;
};
