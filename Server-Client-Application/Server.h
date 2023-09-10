#pragma once

#include <vector>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Client.h"

#pragma comment(lib, "Ws2_32.lib")

class Server {
public:
    Server(int clientLimit, const char* listeningPort);
    ~Server();
    void execution();
    void addNewClient();
    bool processClientQuery(Client* client);
    void sendUdpBroadcast();
    void transmitToClient(const std::string& notification, Client* client);
    void broadcastUdpMessage(const std::string& notification, Client* sender);
    void recordLog(const std::string& notification);
    

private:
    bool initializeWinsock();
    bool setupServer();
    bool setupUDPServer();
    void displayError(const char* errorMsg, int errorCode);
    void cleanupSocketAndAddrInfo(SOCKET& socket, struct addrinfo* addrInfo);
    void cleanupClients();
    void promptForServerIP();
    void displayServerInitialization();
    void setupServerSocketForListening();
    void startUdpBroadcastThread();
    int getHighestFileDescriptor();
    void handleSocketErrors(int finalOutput);
    void checkAndHandleClientConnections();
    void removeDisconnectedClients();
    SOCKET createClientSocket(sockaddr_in& clientAddress, int& clientAddressLength);
    bool isServerFull() const;
    void rejectClientDueToCapacity(SOCKET& clientSock);
    void addClientToServer(SOCKET& clientSock, sockaddr_in& clientAddress);
    void updateMaxFD(SOCKET& clientSock);
    void handleRegisterRequest(Client* client, const std::string& notification);
    void handleGetListRequest(Client* client);
    void handleGetLogRequest(Client* client);
    void handleExitRequest(Client* client);
    void handleChatRequest(Client* client, const std::string& notification);
    void handleDefaultChatRequest(Client* client, const std::string& notification);
    void sendMessageToSocket(const std::string& notification, SOCKET soc_Client);
    void setup();
    int clientLimit;
    std::vector<Client*> clientList;
    fd_set masterSet;
    fd_set activeSet;
    SOCKET tcpSocket;
    SOCKET udpSocket;
    int maxFd;
    const char* listeningPort;
    std::string logPath;
    int logDescriptor;
    timeval waitDuration;
    std::string hostIP;
};
