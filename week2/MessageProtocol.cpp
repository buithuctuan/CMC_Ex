#include "MessageProtocol.h"
#include <iostream>

bool MessageProtocolHelper::SendMessage(SOCKET socket, const std::string& message) {
    if (socket == INVALID_SOCKET || message.empty()) {
        return false;
    }
    uint32_t messageLength = static_cast<uint32_t>(message.length());
    uint32_t networkLength = htonl(messageLength);

    if (!SendAll(socket, (char*)&networkLength, HEADER_SIZE)) {
        std::cout << "Failed to send message header" << std::endl;
        return false;
    }

    if (!SendAll(socket, message.c_str(), static_cast<int>(messageLength))) {
        std::cout << "Failed to send message data" << std::endl;
        return false;
    }

    std::cout << "Successfully sent message of " << messageLength << " bytes" << std::endl;
    return true;
}

std::string MessageProtocolHelper::ReceiveMessage(SOCKET socket) {
    if (socket == INVALID_SOCKET) {
        return "";
    }

    char headerBuffer[HEADER_SIZE];
    if (!ReceiveAll(socket, headerBuffer, HEADER_SIZE)) {
        std::cout << "Failed to receive message header" << std::endl;
        return "";
    }

    uint32_t networkLength = *((uint32_t*)headerBuffer);
    uint32_t messageLength = ntohl(networkLength);

    if (messageLength == 0) {
        return "";
    }

    if (messageLength > 10 * 1024 * 1024) { 
        std::cout << "Message too large: " << messageLength << " bytes" << std::endl;
        return "";
    }

    std::vector<char> messageBuffer(messageLength + 1);
    if (!ReceiveAll(socket, messageBuffer.data(), static_cast<int>(messageLength))) {
        std::cout << "Failed to receive message data" << std::endl;
        return "";
    }

    messageBuffer[messageLength] = '\0';

    std::cout << "Successfully received message of " << messageLength << " bytes" << std::endl;
    return std::string(messageBuffer.data(), messageLength);
}

bool MessageProtocolHelper::SendAll(SOCKET socket, const char* data, int length) {
    int totalSent = 0;
    int bytesLeft = length;

    while (totalSent < length) {
        int bytesSent = send(socket, data + totalSent, bytesLeft, 0);

        if (bytesSent == SOCKET_ERROR) {
            int error = WSAGetLastError();
            std::cout << "Send error: " << error << std::endl;
            return false;
        }

        if (bytesSent == 0) {
            std::cout << "Connection closed by peer during send" << std::endl;
            return false;
        }

        totalSent += bytesSent;
        bytesLeft -= bytesSent;

        std::cout << "Sent " << bytesSent << " bytes, total: " << totalSent << "/" << length << std::endl;
    }

    return true;
}

bool MessageProtocolHelper::ReceiveAll(SOCKET socket, char* buffer, int length) {
    int totalReceived = 0;
    int bytesLeft = length;

    while (totalReceived < length) {
        int bytesReceived = recv(socket, buffer + totalReceived, bytesLeft, 0);

        if (bytesReceived == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                std::cout << "Receive timeout" << std::endl;
                continue;
            }
            std::cout << "Receive error: " << error << std::endl;
            return false;
        }

        if (bytesReceived == 0) {
            std::cout << "Connection closed by peer during receive" << std::endl;
            return false;
        }

        totalReceived += bytesReceived;
        bytesLeft -= bytesReceived;

        std::cout << "Received " << bytesReceived << " bytes, total: " << totalReceived << "/" << length << std::endl;
    }

    return true;
}