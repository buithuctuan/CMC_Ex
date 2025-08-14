#pragma once
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <winsock2.h>

// Message types
enum class MessageType {
    LOGIN_REQUEST = 1,
    LOGIN_RESPONSE = 2,
    REGISTER_REQUEST = 3,
    REGISTER_RESPONSE = 4,
    SEND_MESSAGE_REQUEST = 5,
    SEND_MESSAGE_RESPONSE = 6,
    NEW_MESSAGE_NOTIFICATION = 7,
    GET_CHAT_HISTORY_REQUEST = 8,
    GET_CHAT_HISTORY_RESPONSE = 9,
    USER_LIST_UPDATE = 10,
    LOGOUT_REQUEST = 11,
    LOGOUT_RESPONSE = 12
};

struct ChatMessage {
    int senderId;
    std::string senderUsername;
    std::string content;
    std::string timestamp;
};

struct UserInfo {
    int userId;
    std::string username;
};

class MessageSerializer {
public:
    static std::string Serialize(const std::map<std::string, std::string>& data) {
        std::string result;
        for (const auto& pair : data) {
            result += pair.first + ":" + pair.second + "|";
        }
        return result;
    }

    static std::map<std::string, std::string> Deserialize(const std::string& data) {
        std::map<std::string, std::string> result;
        std::istringstream iss(data);
        std::string token;

        while (std::getline(iss, token, '|')) {
            if (token.empty()) continue;

            size_t colonPos = token.find(':');
            if (colonPos != std::string::npos) {
                std::string key = token.substr(0, colonPos);
                std::string value = token.substr(colonPos + 1);
                result[key] = value;
            }
        }
        return result;
    }

};

class MessageProtocolHelper {
public:
    static bool SendMessage(SOCKET socket, const std::string& message);

    static std::string ReceiveMessage(SOCKET socket);

private:
    static bool SendAll(SOCKET socket, const char* data, int length);
    static bool ReceiveAll(SOCKET socket, char* buffer, int length);

    static const int HEADER_SIZE = 4;
};