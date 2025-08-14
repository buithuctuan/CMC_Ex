#include "pch.h"
#include "week2.h"
#include "afxdialogex.h"
#include "main_window.h"
#include "week2Dlg.h"
#include "MessageProtocol.h"
#include <thread>
#include <ws2tcpip.h>

IMPLEMENT_DYNAMIC(main_window, CDialogEx)

main_window::main_window(CWnd* pParent)
    : CDialogEx(IDD_DIALOG1, pParent)
    , m_currentUserId(0)
    , m_userId(0)
    , clientSocket(INVALID_SOCKET)
    , networkThread(nullptr)
    , isNetworkActive(false)
    , selectedUserId(-1)
{
}
main_window::~main_window()
{
    isNetworkActive = false;

    if (clientSocket != INVALID_SOCKET) {
        shutdown(clientSocket, SD_BOTH);
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }

    if (networkThread) {
        if (networkThread->joinable()) {
            networkThread->join();
        }
        delete networkThread;
        networkThread = nullptr;
    }
}

void main_window::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, LST_USER, ctr_lst_user);
    DDX_Control(pDX, EDT_CONVERS, ctr_edt_convers);
    DDX_Control(pDX, EDT_SMSG, ctr_edt_smsg);
    DDX_Control(pDX, BTN_SEND, ctr_btn_send);
}

BEGIN_MESSAGE_MAP(main_window, CDialogEx)
    ON_LBN_SELCHANGE(LST_USER, &main_window::OnLbnSelchangeUser)
    ON_BN_CLICKED(BTN_SEND, &main_window::OnBnClickedSend)
    ON_EN_CHANGE(EDT_SMSG, &main_window::OnEnChangeSmsg)
    ON_BN_CLICKED(BTN_LOGOUT, &main_window::OnBnClickedLogout)
    ON_WM_CLOSE()
    ON_MESSAGE(WM_USER + 1, &main_window::OnNewMessage)
    ON_MESSAGE(WM_USER + 2, &main_window::OnUserListUpdate)
    ON_MESSAGE(WM_USER + 4, &main_window::OnChatHistoryResponse)
    ON_MESSAGE(WM_USER + 5, &main_window::OnConnectionLost)
    ON_MESSAGE(WM_USER + 6, &main_window::OnSendMessageFailed)
END_MESSAGE_MAP()

BOOL main_window::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    ctr_btn_send.EnableWindow(FALSE);

    if (m_userId > 0)
    {
        SetCurrentUser(m_userId, m_username);
    }

    if (clientSocket != INVALID_SOCKET) {
        StartNetworkThread();
    }

    return TRUE;
}

void main_window::SetCurrentUser(int userId, CString username)
{
    m_currentUserId = userId;
    m_currentUsername = username;

    CString title;
    title.Format(_T("Chat - %d - %s"),userId, username.GetString());
    SetWindowText(title);
}

void main_window::SetClientSocket(SOCKET socket)
{
    clientSocket = socket;
    if (IsWindow(m_hWnd)) {
        StartNetworkThread();
    }
}

void main_window::StartNetworkThread()
{
    if (clientSocket != INVALID_SOCKET && !isNetworkActive) {
        isNetworkActive = true;
        networkThread = new std::thread(&main_window::NetworkThreadFunction, this);
    }
}

void main_window::StopNetworkThread()
{
    isNetworkActive = false;

    if (clientSocket != INVALID_SOCKET) {
        std::map<std::string, std::string> logoutRequest;
        logoutRequest["type"] = std::to_string(static_cast<int>(MessageType::LOGOUT_REQUEST));

        std::string logoutStr = MessageSerializer::Serialize(logoutRequest);
        MessageProtocolHelper::SendMessage(clientSocket, MessageSerializer::Serialize(logoutRequest));

        shutdown(clientSocket, SD_BOTH);
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }

    if (networkThread && networkThread->joinable()) {
        networkThread->join();
        delete networkThread;
        networkThread = nullptr;
    }
}

void main_window::NetworkThreadFunction() {
    while (isNetworkActive && clientSocket != INVALID_SOCKET) {
        int timeout = 5000; 
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

        std::string completeMessage = MessageProtocolHelper::ReceiveMessage(clientSocket);

        if (completeMessage.empty()) {
            if (!isNetworkActive) {
                break; 
            }

            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                continue; 
            }

            PostMessage(WM_USER + 5, 0, 0); 
            break;
        }

           std::map<std::string, std::string> message = MessageSerializer::Deserialize(completeMessage);
           if (!message.empty()) {
                HandleServerMessage(message);
            }
    }
}

bool main_window::SendMessageToServer(std::map<std::string, std::string> message) {
    if (clientSocket == INVALID_SOCKET) {
        return false;
    }

    std::string messageStr = MessageSerializer::Serialize(message);
    return MessageProtocolHelper::SendMessage(clientSocket, messageStr);
}

void main_window::HandleServerMessage(std::map<std::string, std::string> message)
{
    try {
        if (message.find("type") == message.end()) {
            return;
        }

        int messageType = std::stoi(message.at("type"));

        switch (static_cast<MessageType>(messageType)) {
        case MessageType::NEW_MESSAGE_NOTIFICATION:
        {
            if (message.find("sender_id") != message.end() &&
                message.find("sender_username") != message.end() &&
                message.find("message") != message.end() &&
                message.find("timestamp") != message.end()) {

                int senderId = std::stoi(message.at("sender_id"));
                CString senderUsername = CString(message.at("sender_username").c_str());
                CString messageContent = Utf8Converter::Utf8ToCString(message.at("message"));
                CString timestamp = CString(message.at("timestamp").c_str());

                int requiredSize = _tcslen(senderUsername.GetString()) +
                    _tcslen(messageContent.GetString()) +
                    _tcslen(timestamp.GetString()) + 50;

                TCHAR* newMsg = new TCHAR[requiredSize];
                _stprintf_s(newMsg, requiredSize, _T("%d|%s|%s|%s"),
                    senderId,
                    senderUsername.GetString(),
                    messageContent.GetString(),
                    timestamp.GetString());
                PostMessage(WM_USER + 1, (WPARAM)newMsg, 0);
            }
        }
        break;

        case MessageType::USER_LIST_UPDATE:
        {
            if (message.find("user_count") != message.end()) {
                int userCount = std::stoi(message.at("user_count"));

                char* userListData = new char[8192]; 
                userListData[0] = '\0';
                bool validData = true;

                for (int i = 0; i < userCount; ++i) {
                    std::string prefix = "user_" + std::to_string(i) + "_";
                    std::string idKey = prefix + "id";
                    std::string usernameKey = prefix + "username";
                    std::string statusKey = prefix + "status"; 

                    if (message.find(idKey) != message.end() &&
                        message.find(usernameKey) != message.end() &&
                        message.find(statusKey) != message.end()) {

                        std::string userId = message.at(idKey);
                        std::string username = message.at(usernameKey);
                        std::string status = message.at(statusKey); 

                        if (strlen(userListData) > 0) {
                            strcat_s(userListData, 8192, "|");
                        }

                        // Format: userId:username:status
                        char userInfo[512];
                        sprintf_s(userInfo, "%s:%s:%s", userId.c_str(), username.c_str(), status.c_str());
                        strcat_s(userListData, 8192, userInfo);
                    }
                    else {
                        validData = false;
                        break;
                    }
                }

                if (validData) {
                    PostMessage(WM_USER + 2, (WPARAM)userListData, 0);
                }
                else {
                    delete[] userListData;
                }
            }
        }
        break;

        case MessageType::SEND_MESSAGE_RESPONSE:
        {
            if (message.find("success") != message.end()) {
                bool success = (message.at("success") == "1");
                if (success) {
                    if (selectedUserId != -1) {
                        LoadChatHistory(selectedUserId);
                    }
                }
                else {
                    PostMessage(WM_USER + 6, 0, 0);
                }
            }
        }
        break;

        case MessageType::GET_CHAT_HISTORY_RESPONSE:
        {
            if (message.find("message_count") != message.end()) {
                int messageCount = std::stoi(message.at("message_count"));

                TCHAR* chatHistory = new TCHAR[8192];  
                chatHistory[0] = _T('\0'); 

                for (int i = 0; i < messageCount; ++i) {
                    std::string prefix = "msg_" + std::to_string(i) + "_";
                    std::string usernameKey = prefix + "sender_username";
                    std::string contentKey = prefix + "content";
                    std::string timestampKey = prefix + "timestamp";

                    if (message.find(usernameKey) != message.end() &&
                        message.find(contentKey) != message.end() &&
                        message.find(timestampKey) != message.end()) {

                        CString senderUsername = CString(message.at(usernameKey).c_str());
                        CString content = Utf8Converter::Utf8ToCString(message.at(contentKey));
                        CString timestamp = CString(message.at(timestampKey).c_str());

                        if (_tcslen(chatHistory) > 0) {
                            _tcscat_s(chatHistory, 8192, _T("\r\n"));
                        }

                        TCHAR messageDisplay[1024];
                        _stprintf_s(messageDisplay, _T("[%s] %s: %s"),
                            timestamp.GetString(),
                            senderUsername.GetString(),
                            content.GetString());
                        _tcscat_s(chatHistory, 8192, messageDisplay);
                    }
                }

                PostMessage(WM_USER + 4, (WPARAM)chatHistory, 0);
            }
        }
        break;

        default:
            break;
        }
    }
    catch (std::exception e) {
    }
}

LRESULT main_window::OnConnectionLost(WPARAM wParam, LPARAM lParam)
{
    int result = AfxMessageBox(_T("Connection to server lost. Do you want to reconnecting?"),
        MB_YESNO | MB_ICONWARNING);

    if (result == IDYES) {
        AttemptReconnect();
    }
    else {
        ShowWindow(SW_HIDE);
        Cweek2Dlg loginDlg;
        loginDlg.DoModal();
        EndDialog(0);
    }

    return 0;
}

void main_window::AttemptReconnect()
{
    StopNetworkThread();

    WSADATA wsaData;
    int er = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (er != 0)
    {
        AfxMessageBox(er);
    }

    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        AfxMessageBox(_T("Failed to create socket for reconnection"));
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
    serverAddr.sin_port = htons(12345);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        AfxMessageBox(_T("Failed to reconnect to server"));
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
        return;
    }

    StartNetworkThread();
    AfxMessageBox(_T("Reconnected! Please note that you may need to refresh your view."));
}

LRESULT main_window::OnNewMessage(WPARAM wParam, LPARAM lParam)
{
    TCHAR* messageData = (TCHAR*)wParam;
    if (messageData) {
        CString msgStr(messageData);

        int pos1 = msgStr.Find(_T('|'));
        int pos2 = msgStr.Find(_T('|'), pos1 + 1);
        int pos3 = msgStr.Find(_T('|'), pos2 + 1);

        if (pos1 != -1 && pos2 != -1 && pos3 != -1) {
            int senderId = _ttoi(msgStr.Left(pos1));
            CString senderUsername = msgStr.Mid(pos1 + 1, pos2 - pos1 - 1);
            CString messageContent = msgStr.Mid(pos2 + 1, pos3 - pos2 - 1);
            CString timestamp = msgStr.Mid(pos3 + 1);

            if (selectedUserId == senderId) {
                CString currentText;
                ctr_edt_convers.GetWindowText(currentText);

                CString newMessage;
                newMessage.Format(_T("[%s] %s: %s"),
                    timestamp.GetString(),
                    senderUsername.GetString(),
                    messageContent.GetString());

                if (!currentText.IsEmpty()) {
                    currentText += _T("\r\n");
                }
                currentText += newMessage;

                ctr_edt_convers.SetWindowText(currentText);

                int textLength = ctr_edt_convers.GetWindowTextLength();
                ctr_edt_convers.SetSel(textLength, textLength);
                ctr_edt_convers.LineScroll(ctr_edt_convers.GetLineCount());
            }
        }

        delete[] messageData;
    }
    return 0;
}

LRESULT main_window::OnUserListUpdate(WPARAM wParam, LPARAM lParam)
{
    char* userListData = (char*)wParam;
    if (userListData) {
        UpdateUserList(std::string(userListData));
        delete[] userListData;
    }
    return 0;
}

void main_window::UpdateUserList(std::string userListData)
{
    if (!IsWindowVisible()) {
        ShowWindow(SW_SHOW);
        SetForegroundWindow();
    }

    int currentSelectedUser = -1;
    int cursel = ctr_lst_user.GetCurSel();
    if (cursel != LB_ERR) {
        currentSelectedUser = (int)ctr_lst_user.GetItemData(cursel);
    }

    ctr_lst_user.ResetContent();

    if (userListData.empty()) {
        return;
    }

    std::istringstream iss(userListData);
    std::string userInfo;
    int userCount = 0;

    while (std::getline(iss, userInfo, '|')) {  // Format: userId:username:status
        size_t firstColon = userInfo.find(':');
        size_t secondColon = userInfo.find(':', firstColon + 1);

        if (firstColon != std::string::npos && secondColon != std::string::npos) {
                int userId = std::stoi(userInfo.substr(0, firstColon));
                std::string username = userInfo.substr(firstColon + 1, secondColon - firstColon - 1);
                std::string status = userInfo.substr(secondColon + 1);

                if (userId != m_currentUserId) {
                    CString userDisplay;
                    if (status == "online") {
                        userDisplay.Format(_T("%s (Online)"), CString(username.c_str()).GetString());
                    }
                    else {
                        userDisplay.Format(_T("%s (Offline)"), CString(username.c_str()).GetString());
                    }

                    int index = ctr_lst_user.AddString(userDisplay);
                    ctr_lst_user.SetItemData(index, userId);
                    userCount++;

                    if (userId == currentSelectedUser) {
                        ctr_lst_user.SetCurSel(index);
                        selectedUserId = userId;
                    }
                }
        }
    }

    CString msg;
    ctr_edt_smsg.GetWindowText(msg);
    cursel = ctr_lst_user.GetCurSel();
    ctr_btn_send.EnableWindow(!msg.IsEmpty() && cursel != LB_ERR);
}

LRESULT main_window::OnChatHistoryResponse(WPARAM wParam, LPARAM lParam)
{
    TCHAR* chatHistory = (TCHAR*)wParam;
    if (chatHistory) {
        ctr_edt_convers.SetWindowText(chatHistory);

        int textLength = ctr_edt_convers.GetWindowTextLength();
        ctr_edt_convers.SetSel(textLength, textLength);
        ctr_edt_convers.LineScroll(ctr_edt_convers.GetLineCount());

        delete[] chatHistory;
    }
    return 0;
}

void main_window::LoadChatHistory(int otherUserId)
{
    std::map<std::string, std::string> request;
    request["type"] = std::to_string(static_cast<int>(MessageType::GET_CHAT_HISTORY_REQUEST));
    request["other_user_id"] = std::to_string(otherUserId);

    SendMessageToServer(request);
}

void main_window::OnLbnSelchangeUser()
{
    int cursel = ctr_lst_user.GetCurSel();
    if (cursel != LB_ERR) {
        selectedUserId = (int)ctr_lst_user.GetItemData(cursel);
        LoadChatHistory(selectedUserId);

        CString msg;
        ctr_edt_smsg.GetWindowText(msg);
        ctr_btn_send.EnableWindow(!msg.IsEmpty());
    }
    else {
        selectedUserId = -1;
        ctr_edt_convers.SetWindowText(_T(""));
        ctr_btn_send.EnableWindow(FALSE);
    }
}

LRESULT main_window::OnSendMessageFailed(WPARAM wParam, LPARAM lParam)
{
    AfxMessageBox(_T("Failed to send message to server"));
    return 0;
}

std::string main_window::GetCurrentTimestamp() {
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

void main_window::OnBnClickedSend()
{
    CString msg;
    ctr_edt_smsg.GetWindowText(msg);
    msg.Trim();

    if (msg.GetLength() == 0) {
        return;
    }

    int cursel = ctr_lst_user.GetCurSel();
    if (cursel == LB_ERR) {
        AfxMessageBox(_T("Please select a user to send message"));
        return;
    }

    int receiverId = (int)ctr_lst_user.GetItemData(cursel);

    std::map<std::string, std::string> messageRequest;
    messageRequest["type"] = std::to_string(static_cast<int>(MessageType::SEND_MESSAGE_REQUEST));
    messageRequest["receiver_id"] = std::to_string(receiverId);
    messageRequest["message"] = Utf8Converter::CStringToUtf8(msg);

    if (SendMessageToServer(messageRequest)) {
        ctr_edt_smsg.SetWindowText(_T(""));
        ctr_btn_send.EnableWindow(FALSE);

        CString currentText;
        ctr_edt_convers.GetWindowText(currentText);

        CString newMessage;
        newMessage.Format(_T("[%s] %s: %s"),
            CString(GetCurrentTimestamp().c_str()).GetString(),
            m_currentUsername.GetString(),
            msg.GetString());

        if (!currentText.IsEmpty()) {
            currentText += _T("\r\n");
        }
        currentText += newMessage;

        ctr_edt_convers.SetWindowText(currentText);

        int textLength = ctr_edt_convers.GetWindowTextLength();
        ctr_edt_convers.SetSel(textLength, textLength);
        ctr_edt_convers.LineScroll(ctr_edt_convers.GetLineCount());
    }
    else {
        AfxMessageBox(_T("Failed to send message"));
    }
}

void main_window::OnEnChangeSmsg()
{
    CString msg;
    ctr_edt_smsg.GetWindowText(msg);
    int cursel = ctr_lst_user.GetCurSel();

    ctr_btn_send.EnableWindow(!msg.IsEmpty() && cursel != LB_ERR);
}

void main_window::OnBnClickedLogout()
{
    StopNetworkThread();
    ShowWindow(SW_HIDE);
    Cweek2Dlg lgd;
    lgd.DoModal();
    EndDialog(0);
}

void main_window::OnClose()
{
    StopNetworkThread();

    CDialogEx::OnClose();
}