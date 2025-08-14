// SignUpDlg.cpp : implementation file
//

#include "pch.h"
#include "week2.h"
#include "afxdialogex.h"
#include "SignUpDlg.h"
#include "week2Dlg.h"
#include "MessageProtocol.h"
#include <winsock2.h>
#include <cstdlib>
#include <ctime>

// SignUpDlg dialog

IMPLEMENT_DYNAMIC(SignUpDlg, CDialogEx)

SignUpDlg::SignUpDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_SignUpDlg, pParent), clientSocket(INVALID_SOCKET)
{
}

SignUpDlg::~SignUpDlg()
{
    // Don't close socket here - it belongs to parent dialog
}

void SignUpDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, EDT_SUNAME, ctr_edt_name);
    DDX_Control(pDX, EDT_SUPASS, ctr_edt_pass);
    DDX_Control(pDX, EDT_SUCFPASS, ctr_edt_cfpass);
}

BEGIN_MESSAGE_MAP(SignUpDlg, CDialogEx)
    ON_BN_CLICKED(BTN_SUSIGNUP, &SignUpDlg::OnBnClickedSignup)
END_MESSAGE_MAP()

BOOL SignUpDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    return TRUE;
}

void SignUpDlg::SetClientSocket(SOCKET socket)
{
    clientSocket = socket;
}

bool SignUpDlg::SendMessage(std::map<std::string, std::string> message)
{
    if (clientSocket == INVALID_SOCKET) {
        return false;
    }

    try {
        std::string messageStr = MessageSerializer::Serialize(message);
        return MessageProtocolHelper::SendMessage(clientSocket, messageStr);
    }
    catch (const std::exception& e) {
        return false;
    }
}

std::map<std::string, std::string> SignUpDlg::ReceiveMessage()
{
    std::map<std::string, std::string> emptyMap;

    if (clientSocket == INVALID_SOCKET) {
        return emptyMap;
    }

    try {
        std::string receivedData = MessageProtocolHelper::ReceiveMessage(clientSocket);

        if (receivedData.empty()) {
            return emptyMap;
        }

        return MessageSerializer::Deserialize(receivedData);
    }
    catch (const std::exception& e) {
        return emptyMap;
    }
}

bool SignUpDlg::IsPasswordStrong(CString password)
{
    if (password.GetLength() < 8) {
        return false;
    }

    bool hasLower = false;
    bool hasUpper = false;
    bool hasDigit = false;
    bool hasSpecial = false;

    for (int i = 0; i < password.GetLength(); i++) {
        TCHAR ch = password.GetAt(i);

        if (ch >= _T('a') && ch <= _T('z')) {
            hasLower = true;
        }
        else if (ch >= _T('A') && ch <= _T('Z')) {
            hasUpper = true;
        }
        else if (ch >= _T('0') && ch <= _T('9')) {
            hasDigit = true;
        }
        else if (_tcschr(_T("!@#$%^&*()_+-=[]{}|;':\",./<>?"), ch)) {
            hasSpecial = true;
        }
    }

    return hasLower && hasUpper && hasDigit && hasSpecial;
}

void SignUpDlg::OnBnClickedSignup()
{
    CString username, password, cfpassword;
    ctr_edt_name.GetWindowText(username);
    ctr_edt_pass.GetWindowText(password);
    ctr_edt_cfpass.GetWindowText(cfpassword);

    if (username.IsEmpty() || password.IsEmpty() || cfpassword.IsEmpty()) {
        AfxMessageBox(_T("Please fill in all fields"));
        return;
    }

    if (password != cfpassword) {
        AfxMessageBox(_T("Passwords do not match"));
        return;
    }

    if (username.GetLength() < 3) {
        AfxMessageBox(_T("Username must be at least 3 characters long"));
        return;
    }

    if (username.GetLength() > 50) {
        AfxMessageBox(_T("Username must be less than 50 characters"));
        return;
    }

    for (int i = 0; i < username.GetLength(); i++) {
        TCHAR ch = username.GetAt(i);
        if (!((ch >= _T('a') && ch <= _T('z')) ||
            (ch >= _T('A') && ch <= _T('Z')) ||
            (ch >= _T('0') && ch <= _T('9')) ||
            ch == _T('_') || ch == _T('-'))) {
            AfxMessageBox(_T("Username can only contain letters, numbers, underscore and hyphen"));
            return;
        }
    }

    if (!IsPasswordStrong(password)) {
        AfxMessageBox(_T("Password must be at least 8 characters long and contain lowercase, uppercase, digit and special characters"));
        return;
    }

    if (clientSocket == INVALID_SOCKET) {
        AfxMessageBox(_T("Not connected to server!"));
        return;
    }

    std::map<std::string, std::string> registerRequest;
    registerRequest["type"] = std::to_string(static_cast<int>(MessageType::REGISTER_REQUEST));
    registerRequest["username"] = CT2A(username);
    registerRequest["password"] = CT2A(password);

    if (!SendMessage(registerRequest)) {
        AfxMessageBox(_T("Failed to send registration request to server!"));
        return;
    }

    std::map<std::string, std::string> response = ReceiveMessage();

    if (response.empty()) {
        AfxMessageBox(_T("No response from server!"));
        return;
    }

    if (response.find("type") == response.end()) {
        AfxMessageBox(_T("Invalid response format from server!"));
        return;
    }

    int responseType = 0;
    try {
        responseType = std::stoi(response["type"]);
    }
    catch (const std::exception& e) {
        AfxMessageBox(_T("Invalid response format from server!"));
        return;
    }

    if (responseType != static_cast<int>(MessageType::REGISTER_RESPONSE)) {
        AfxMessageBox(_T("Unexpected response type from server!"));
        return;
    }

    if (response.find("success") == response.end()) {
        AfxMessageBox(_T("Invalid response format from server!"));
        return;
    }

    bool success = (response["success"] == "1");

    if (success) {
        CString successMsg;
        if (response.find("message") != response.end()) {
            successMsg = CString(response["message"].c_str());
        }
        else {
            successMsg = _T("Account created successfully!");
        }

        AfxMessageBox(_T("Registration successful: ") + successMsg);
        EndDialog(IDOK);
    }
    else {
        CString errorMsg;
        if (response.find("error") != response.end()) {
            errorMsg = CString(response["error"].c_str());
        }
        else {
            errorMsg = _T("Unknown registration error");
        }

        AfxMessageBox(_T("Registration failed: ") + errorMsg);
    }
}