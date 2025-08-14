#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <sqlite3.h>
#include "MessageProtocol.h"

#pragma comment(lib, "ws2_32.lib")

struct ClientInfo {
    SOCKET socket;
    int userId;
    std::string username;
    std::thread* clientThread;
    bool isActive;
};

class ChatServer {
private:
    SOCKET serverSocket;
    std::vector<ClientInfo*> clients;
    std::mutex clientsMutex;
    sqlite3* db;
    bool isRunning;

public:
    ChatServer();
    ~ChatServer();

    bool Initialize(int port = 8080);
    void Start();
    void Stop();
    void HandleClient(ClientInfo* client);
    std::vector<std::thread*> completedThreads;
    std::mutex completedThreadsMutex;

    bool InitDatabase();
    bool RegisterUser(std::string username, std::string password);
    int LoginUser(std::string username, std::string password);
    bool UpdateUserStatus(int userId, std::string status);
    std::vector<UserInfo> GetOnlineUsers(int excludeUserId);
    bool SaveMessage(int senderId, int receiverId, std::string message);
    std::vector<ChatMessage> GetChatHistory(int user1, int user2);
    std::string GetCurrentTimestamp();
    std::string HashPassword(std::string password, std::string salt);
    std::string GenerateSalt();
    std::vector<UserInfo> GetAllUsers();

    void BroadcastUserList();
    void SendToClient(int userId, std::map<std::string, std::string> message);
    void RemoveClient(ClientInfo* client);


    void HandleLoginRequest(ClientInfo* client, std::map<std::string, std::string> data);
    void HandleRegisterRequest(ClientInfo* client, std::map<std::string, std::string> data);
    void HandleSendMessageRequest(ClientInfo* client, std::map<std::string, std::string> data);
    void HandleGetChatHistoryRequest(ClientInfo* client, std::map<std::string, std::string> data);
    void HandleLogoutRequest(ClientInfo* client);
};
