#include "server.h"
#include "PasswordGen.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <functional>

ChatServer::ChatServer() : db(nullptr), isRunning(false), serverSocket(INVALID_SOCKET) {
}

ChatServer::~ChatServer() {
    Stop();
    if (db) {
        sqlite3_close(db);
    }
}

bool ChatServer::Initialize(int port) {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        return false;
    }

    if (!InitDatabase()) {
        WSACleanup();
        return false;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }

    return true;
}

void ChatServer::Start() {
    isRunning = true;

    while (isRunning) {
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);

        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrSize);

        if (clientSocket == INVALID_SOCKET) {
            continue;
        }

        ClientInfo* client = new ClientInfo();
        client->socket = clientSocket;
        client->userId = 0;
        client->isActive = true;
        client->clientThread = new std::thread(&ChatServer::HandleClient, this, client);

        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clients.push_back(client);
        }
    }
}

void ChatServer::Stop() {
    isRunning = false;

    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (auto& client : clients) {
            client->isActive = false;
            closesocket(client->socket);
            if (client->clientThread && client->clientThread->joinable()) {
                client->clientThread->join();
            }
            delete client->clientThread;
            delete client;
        }
        clients.clear();
    }

    WSACleanup();
}

void ChatServer::HandleClient(ClientInfo* client) {
    while (client->isActive && isRunning) {
        std::string completeMessage = MessageProtocolHelper::ReceiveMessage(client->socket);

        if (completeMessage.empty()) {
            if (!isRunning || !client->isActive) {
                break;
            }

            int error = WSAGetLastError();
            std::cout << "Client " << client->userId << " receive error: " << error << std::endl;
            break;
        }

        std::cout << "Received message from client " << client->userId << ": "
            << completeMessage.substr(0, 100) << "..." << std::endl;

        try {
            std::map<std::string, std::string> message = MessageSerializer::Deserialize(completeMessage);

            if (message.empty()) {
                continue;
            }

            if (message.find("type") == message.end()) {
                continue;
            }

            int messageType = std::stoi(message["type"]);

            switch (static_cast<MessageType>(messageType)) {
            case MessageType::LOGIN_REQUEST:
                HandleLoginRequest(client, message);
                break;
            case MessageType::REGISTER_REQUEST:
                HandleRegisterRequest(client, message);
                break;
            case MessageType::SEND_MESSAGE_REQUEST:
                HandleSendMessageRequest(client, message);
                break;
            case MessageType::GET_CHAT_HISTORY_REQUEST:
                HandleGetChatHistoryRequest(client, message);
                break;
            case MessageType::LOGOUT_REQUEST:
                HandleLogoutRequest(client);
                break;
            default:
                break;
            }
        }
        catch (const std::exception& e) {
            std::cout << "Error processing message: " << e.what() << std::endl;
        }
    }

    RemoveClient(client);
}

void ChatServer::RemoveClient(ClientInfo* client) {
    std::cout << "Removing client (userId: " << client->userId << ", username: " << client->username << ")" << std::endl;

    if (client->userId > 0) {
        UpdateUserStatus(client->userId, "offline");
        BroadcastUserList();
        std::cout << "Updated user status to offline and broadcasted user list" << std::endl;
    }

    client->isActive = false;
    closesocket(client->socket);

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        auto it = std::find(clients.begin(), clients.end(), client);
        if (it != clients.end()) {
            clients.erase(it);
            std::cout << "Client removed from list. Remaining clients: " << clients.size() << std::endl;
        }
    }

    if (client->clientThread) {
        if (client->clientThread->joinable()) {
            client->clientThread->detach();
        }
        delete client->clientThread;
        client->clientThread = nullptr;
    }

    delete client;
    std::cout << "Client cleanup completed" << std::endl;
}

void ChatServer::HandleLoginRequest(ClientInfo* client, std::map<std::string, std::string> data) {
    if (data.find("username") == data.end() || data.find("password") == data.end()) {
        std::cout << "Missing username or password in login request" << std::endl;
        return;
    }

    std::string username = data["username"];
    std::string password = data["password"];

    std::cout << "Login request from: " << username << std::endl;

    int userId = LoginUser(username, password);

    std::map<std::string, std::string> response;
    response["type"] = std::to_string(static_cast<int>(MessageType::LOGIN_RESPONSE));

    if (userId > 0) {
        client->userId = userId;
        client->username = username;

        UpdateUserStatus(userId, "online");

        std::cout << "User " << username << " (ID: " << userId << ") logged in successfully" << std::endl;

        response["success"] = "1";
        response["user_id"] = std::to_string(userId);
        response["username"] = username;
        std::string responseStr = MessageSerializer::Serialize(response);
        bool sendResult = MessageProtocolHelper::SendMessage(client->socket, responseStr);
        std::cout << "Login response sent (result: " << sendResult << std::endl;

        BroadcastUserList();
        std::cout << "User list broadcasted" << std::endl;
    }
    else {
        response["success"] = "0";
        response["error"] = "Invalid username or password";

        std::cout << "Login failed for user: " << username << std::endl;

        std::string responseStr = MessageSerializer::Serialize(response);
        MessageProtocolHelper::SendMessage(client->socket, responseStr);
    }
}

void ChatServer::HandleRegisterRequest(ClientInfo* client, std::map<std::string, std::string> data) {
    if (data.find("username") == data.end() || data.find("password") == data.end()) {
        std::cout << "Missing username or password in register request" << std::endl;
        return;
    }

    std::string username = data["username"];
    std::string password = data["password"];

    std::map<std::string, std::string> response;
    response["type"] = std::to_string(static_cast<int>(MessageType::REGISTER_RESPONSE));

    if (RegisterUser(username, password)) {
        response["success"] = "1";
        response["message"] = "Account created successfully";
    }
    else {
        response["success"] = "0";
        response["error"] = "Username already exists or registration failed";
    }

    std::string responseStr = MessageSerializer::Serialize(response);
    MessageProtocolHelper::SendMessage(client->socket, responseStr);
}

void ChatServer::HandleSendMessageRequest(ClientInfo* client, std::map<std::string, std::string> data) {
    if (client->userId <= 0) {
        std::cout << "Error: Client not logged in" << std::endl;
        return;
    }

    if (data.find("receiver_id") == data.end() || data.find("message") == data.end()) {
        std::cout << "Missing receiver_id or message in send message request" << std::endl;
        return;
    }

    int receiverId = std::stoi(data["receiver_id"]);
    std::string message = data["message"];

    std::cout << "Handling send message request:" << std::endl;
    std::cout << "  Sender ID: " << client->userId << std::endl;
    std::cout << "  Sender Username: " << client->username << std::endl;
    std::cout << "  Receiver ID: " << receiverId << std::endl;
    std::cout << "  Message: " << message << std::endl;

    std::map<std::string, std::string> response;
    response["type"] = std::to_string(static_cast<int>(MessageType::SEND_MESSAGE_RESPONSE));

    if (SaveMessage(client->userId, receiverId, message)) {
        response["success"] = "1";
        std::cout << "Message saved successfully" << std::endl;

        std::string responseStr = MessageSerializer::Serialize(response);
        bool sendResult = MessageProtocolHelper::SendMessage(client->socket, responseStr);
        std::cout << "Send response result: " << sendResult << std::endl;

        std::map<std::string, std::string> notification;
        notification["type"] = std::to_string(static_cast<int>(MessageType::NEW_MESSAGE_NOTIFICATION));
        notification["sender_id"] = std::to_string(client->userId);
        notification["sender_username"] = client->username;
        notification["message"] = message;
        notification["timestamp"] = GetCurrentTimestamp();

        std::cout << "Sending notification to receiver..." << std::endl;
        SendToClient(receiverId, notification);
    }
    else {
        response["success"] = "0";
        response["error"] = "Failed to send message";
        std::cout << "Failed to save message to database" << std::endl;

        std::string responseStr = MessageSerializer::Serialize(response);
        MessageProtocolHelper::SendMessage(client->socket, responseStr);
    }
}

void ChatServer::HandleGetChatHistoryRequest(ClientInfo* client, std::map<std::string, std::string> data) {
    if (client->userId <= 0) return;

    if (data.find("other_user_id") == data.end()) {
        std::cout << "Missing other_user_id in get chat history request" << std::endl;
        return;
    }

    int otherUserId = std::stoi(data["other_user_id"]);
    std::vector<ChatMessage> history = GetChatHistory(client->userId, otherUserId);

    std::map<std::string, std::string> response;
    response["type"] = std::to_string(static_cast<int>(MessageType::GET_CHAT_HISTORY_RESPONSE));
    response["message_count"] = std::to_string(history.size());

    for (size_t i = 0; i < history.size(); ++i) {
        std::string prefix = "msg_" + std::to_string(i) + "_";
        response[prefix + "sender_id"] = std::to_string(history[i].senderId);
        response[prefix + "sender_username"] = history[i].senderUsername;
        response[prefix + "content"] = history[i].content;
        response[prefix + "timestamp"] = history[i].timestamp;
    }

    std::string responseStr = MessageSerializer::Serialize(response);
    MessageProtocolHelper::SendMessage(client->socket, responseStr);
}

void ChatServer::HandleLogoutRequest(ClientInfo* client) {
    std::map<std::string, std::string> response;
    response["type"] = std::to_string(static_cast<int>(MessageType::LOGOUT_RESPONSE));
    response["success"] = "1";

    std::string responseStr = MessageSerializer::Serialize(response);
    MessageProtocolHelper::SendMessage(client->socket, responseStr);

    if (client->userId > 0) {
        UpdateUserStatus(client->userId, "offline");
        BroadcastUserList();
    }
}


std::vector<UserInfo> ChatServer::GetAllUsers() {
    std::vector<UserInfo> allUsers;

    sqlite3_stmt* stmt;
    const char* query = "SELECT user_id, username FROM users ORDER BY username ASC";

    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            UserInfo user;
            user.userId = sqlite3_column_int(stmt, 0);
            user.username = (const char*)sqlite3_column_text(stmt, 1);
            user.isOnline = false;
            allUsers.push_back(user);
        }
        sqlite3_finalize(stmt);
    }

    return allUsers;
}

void ChatServer::BroadcastUserList() {
    std::lock_guard<std::mutex> lock(clientsMutex);

    std::vector<UserInfo> allUsers = GetAllUsers();

    for (auto& user : allUsers) {
        user.isOnline = false;
        for (auto& client : clients) {
            if (client->isActive && client->userId == user.userId) {
                user.isOnline = true;
                break;
            }
        }
    }

    std::cout << "Broadcasting user list to " << clients.size() << " clients. Total users: " << allUsers.size() << std::endl;
    for (auto& user : allUsers) {
        std::cout << "  - " << user.username << " (ID: " << user.userId << ") - " << (user.isOnline ? "Online" : "Offline") << std::endl;
    }

    std::map<std::string, std::string> broadcast;
    broadcast["type"] = std::to_string(static_cast<int>(MessageType::USER_LIST_UPDATE));
    broadcast["user_count"] = std::to_string(allUsers.size());

    for (size_t i = 0; i < allUsers.size(); ++i) {
        std::string prefix = "user_" + std::to_string(i) + "_";
        broadcast[prefix + "id"] = std::to_string(allUsers[i].userId);
        broadcast[prefix + "username"] = allUsers[i].username;
        broadcast[prefix + "status"] = allUsers[i].isOnline ? "online" : "offline";
    }

    std::string broadcastStr = MessageSerializer::Serialize(broadcast);
    int successCount = 0;
    for (auto& client : clients) {
        if (client->isActive && client->userId > 0) {
            if (MessageProtocolHelper::SendMessage(client->socket, broadcastStr)) {
                successCount++;
            }
        }
    }

    std::cout << "User list sent to " << successCount << " clients successfully" << std::endl;
}

void ChatServer::SendToClient(int userId, std::map<std::string, std::string> message) {
    std::lock_guard<std::mutex> lock(clientsMutex);

    bool messageSent = false;
    for (auto& client : clients) {
        if (client->userId == userId && client->isActive) {
            std::string messageStr = MessageSerializer::Serialize(message);
            MessageProtocolHelper::SendMessage(client->socket, messageStr);
            messageSent = true;
            std::cout << "Message sent to online user " << userId << std::endl;
            break;
        }
    }

    if (!messageSent) {
        std::cout << "User " << userId << " is offline. Message saved in database for later delivery." << std::endl;
    }
}

bool ChatServer::InitDatabase() {
    int rc = sqlite3_open("chat_server.db", &db);
    if (rc != SQLITE_OK) {
        std::cout << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    const char* createUsersTable = R"(
        CREATE TABLE IF NOT EXISTS users (
            user_id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            status TEXT DEFAULT 'offline',
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";

    rc = sqlite3_exec(db, createUsersTable, 0, 0, 0);
    if (rc != SQLITE_OK) {
        std::cout << "Can't create users table: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    const char* createMessagesTable = R"(
        CREATE TABLE IF NOT EXISTS messages (
            message_id INTEGER PRIMARY KEY AUTOINCREMENT,
            sender_id INTEGER NOT NULL,
            receiver_id INTEGER NOT NULL,
            content TEXT NOT NULL,
            sent_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (sender_id) REFERENCES users (user_id),
            FOREIGN KEY (receiver_id) REFERENCES users (user_id)
        )
    )";

    rc = sqlite3_exec(db, createMessagesTable, 0, 0, 0);
    if (rc != SQLITE_OK) {
        std::cout << "Can't create messages table: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    std::cout << "Database initialized successfully" << std::endl;
    return true;
}

std::string ChatServer::HashPassword(std::string password, std::string salt) {
    std::string combined = password + salt;

    std::hash<std::string> hasher;
    size_t hash = hasher(combined);

    std::stringstream ss;
    ss << std::hex << hash;
    return ss.str();
}

std::string ChatServer::GenerateSalt() {
    std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<size_t> dis(0, chars.size() - 1);

    std::string salt;
    for (int i = 0; i < 16; ++i) {
        salt += chars[dis(generator)];
    }
    return salt;
}

bool ChatServer::RegisterUser(std::string username, std::string password) {
    sqlite3_stmt* checkStmt;
    const char* checkQuery = "SELECT COUNT(*) FROM users WHERE username = ?";

    int rc = sqlite3_prepare_v2(db, checkQuery, -1, &checkStmt, NULL);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(checkStmt, 1, username.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(checkStmt);

    if (rc == SQLITE_ROW) {
        int count = sqlite3_column_int(checkStmt, 0);
        sqlite3_finalize(checkStmt);
        if (count > 0) return false;
    }
    else {
        sqlite3_finalize(checkStmt);
        return false;
    }

    std::string salt = GenerateSalt();
    std::string hashedPassword = HashPassword(password, salt);
    std::string passwordWithSalt = hashedPassword + ":" + salt;

    sqlite3_stmt* insertStmt;
    const char* insertQuery = "INSERT INTO users (username, password_hash) VALUES (?, ?)";

    rc = sqlite3_prepare_v2(db, insertQuery, -1, &insertStmt, NULL);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(insertStmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(insertStmt, 2, passwordWithSalt.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(insertStmt);
    sqlite3_finalize(insertStmt);

    return (rc == SQLITE_DONE);
}

int ChatServer::LoginUser(std::string username, std::string password) {
    sqlite3_stmt* stmt;
    const char* query = "SELECT user_id, password_hash, status FROM users WHERE username = ?";

    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {
        int userId = sqlite3_column_int(stmt, 0);
        std::string storedHash = (const char*)sqlite3_column_text(stmt, 1);
        std::string status = (const char*)sqlite3_column_text(stmt, 2);

        sqlite3_finalize(stmt);

        if (status == "online") {
            return -1;
        }

        size_t colonPos = storedHash.find(':');
        if (colonPos != std::string::npos) {
            std::string hashPart = storedHash.substr(0, colonPos);
            std::string salt = storedHash.substr(colonPos + 1);

            std::string inputHash = HashPassword(password, salt);
            if (inputHash == hashPart) {
                return userId;
            }
        }
    }
    else {
        sqlite3_finalize(stmt);
    }

    return -1;
}

bool ChatServer::UpdateUserStatus(int userId, std::string status) {
    sqlite3_stmt* stmt;
    const char* query = "UPDATE users SET status = ? WHERE user_id = ?";

    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, userId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

bool ChatServer::SaveMessage(int senderId, int receiverId, std::string message) {
    sqlite3_stmt* stmt;
    const char* query = "INSERT INTO messages (sender_id, receiver_id, content) VALUES (?, ?, ?)";

    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, senderId);
    sqlite3_bind_int(stmt, 2, receiverId);
    sqlite3_bind_text(stmt, 3, message.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE);
}

std::vector<ChatMessage> ChatServer::GetChatHistory(int user1, int user2) {
    std::vector<ChatMessage> history;

    sqlite3_stmt* stmt;
    const char* query = R"(
        SELECT m.sender_id, u.username, m.content, m.sent_at 
        FROM messages m 
        JOIN users u ON m.sender_id = u.user_id 
        WHERE ((m.sender_id = ? AND m.receiver_id = ?) 
           OR (m.sender_id = ? AND m.receiver_id = ?))
        ORDER BY m.sent_at ASC
        LIMIT 100
    )";

    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user1);
        sqlite3_bind_int(stmt, 2, user2);
        sqlite3_bind_int(stmt, 3, user2);
        sqlite3_bind_int(stmt, 4, user1);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ChatMessage msg;
            msg.senderId = sqlite3_column_int(stmt, 0);
            msg.senderUsername = (const char*)sqlite3_column_text(stmt, 1);
            msg.content = (const char*)sqlite3_column_text(stmt, 2);
            msg.timestamp = (const char*)sqlite3_column_text(stmt, 3);

            history.push_back(msg);
        }
        sqlite3_finalize(stmt);
    }

    return history;
}

std::vector<UserInfo> ChatServer::GetOnlineUsers(int excludeUserId) {
    std::vector<UserInfo> onlineUsers;

    sqlite3_stmt* stmt;
    const char* query = "SELECT user_id, username FROM users WHERE status = 'online' AND user_id != ?";

    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, excludeUserId);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            UserInfo user;
            user.userId = sqlite3_column_int(stmt, 0);
            user.username = (const char*)sqlite3_column_text(stmt, 1);
            onlineUsers.push_back(user);
        }
        sqlite3_finalize(stmt);
    }

    return onlineUsers;
}

std::string ChatServer::GetCurrentTimestamp() {
    time_t now = time(0);
    struct tm timeinfo;

    errno_t err = localtime_s(&timeinfo, &now);
    if (err != 0) {
        return "Error getting timestamp";
    }

    char timeStr[80];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);

    return std::string(timeStr);
}