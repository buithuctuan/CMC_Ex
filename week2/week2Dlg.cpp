// week2Dlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "week2.h"
#include "week2Dlg.h"
#include "afxdialogex.h"
#include "main_window.h"
#include "SignUpDlg.h"
#include "winuser.h"
#include "MessageProtocol.h"
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CAboutDlg dialog used for App About
class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

	// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// Cweek2Dlg dialog

Cweek2Dlg::Cweek2Dlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_WEEK2_DIALOG, pParent), clientSocket(INVALID_SOCKET)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

Cweek2Dlg::~Cweek2Dlg()
{
	DisconnectFromServer();
}

void Cweek2Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, EDT_NAME, ed_name);
	DDX_Control(pDX, EDT_PASSWORD, ed_pass);
	DDX_Control(pDX, BTN_LOGIN, btn_login);
	DDX_Control(pDX, BTN_SIGNUP, btn_signup);
}

BEGIN_MESSAGE_MAP(Cweek2Dlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(BTN_LOGIN, &Cweek2Dlg::OnBnClickedLogin)
	ON_BN_CLICKED(BTN_SIGNUP, &Cweek2Dlg::OnBnClickedSignup)
END_MESSAGE_MAP()

BOOL Cweek2Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	if (!InitializeWinsock()) {
		AfxMessageBox(_T("Failed to initialize Winsock!"));
		EndDialog(IDCANCEL);
		return FALSE;
	}

	if (!ConnectToServer("127.0.0.1", 12345)) {
		AfxMessageBox(_T("Failed to connect to server!"));
		EndDialog(IDCANCEL);
		return FALSE;
	}

	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	return TRUE;
}

bool Cweek2Dlg::InitializeWinsock()
{
	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	return (result == 0);
}

bool Cweek2Dlg::ConnectToServer(char* serverIP, int port)
{
	clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (clientSocket == INVALID_SOCKET) {
		return false;
	}

	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);

	int result = connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
	return (result != SOCKET_ERROR);
}

void Cweek2Dlg::DisconnectFromServer()
{
	if (clientSocket != INVALID_SOCKET) {
		closesocket(clientSocket);
		clientSocket = INVALID_SOCKET;
	}
	WSACleanup();
}

bool Cweek2Dlg::SendNetworkMessage(std::map<std::string, std::string> message)
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

std::map<std::string, std::string> Cweek2Dlg::ReceiveNetworkMessage()
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

void Cweek2Dlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

void Cweek2Dlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this);

		CWnd::SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

HCURSOR Cweek2Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void Cweek2Dlg::OnBnClickedLogin()
{
	CString username, password;
	ed_name.GetWindowText(username);
	ed_pass.GetWindowText(password);

	if (username.IsEmpty() || password.IsEmpty()) {
		AfxMessageBox(_T("Please enter both username and password!"));
		return;
	}

	std::map<std::string, std::string> loginRequest;
	loginRequest["type"] = std::to_string(static_cast<int>(MessageType::LOGIN_REQUEST));
	loginRequest["username"] = CT2A(username);
	loginRequest["password"] = CT2A(password);

	if (!SendNetworkMessage(loginRequest)) {
		AfxMessageBox(_T("Failed to send login request to server!"));
		return;
	}

	std::map<std::string, std::string> response = ReceiveNetworkMessage();

	if (response.empty()) {
		AfxMessageBox(_T("No response from server!"));
		return;
	}

	if (response.find("type") == response.end()) {
		AfxMessageBox(_T("Invalid response format from server!"));
		return;
	}

	try {
		int responseType = std::stoi(response["type"]);
		if (responseType != static_cast<int>(MessageType::LOGIN_RESPONSE)) {
			AfxMessageBox(_T("Invalid response type from server!"));
			return;
		}

		if (response.find("success") == response.end()) {
			AfxMessageBox(_T("Invalid response format from server!"));
			return;
		}

		bool success = (response["success"] == "1");

		if (success) {
			if (response.find("user_id") == response.end() || response.find("username") == response.end()) {
				AfxMessageBox(_T("Invalid response format from server!"));
				return;
			}

			int userId = std::stoi(response["user_id"]);
			CString usernameCStr = CString(response["username"].c_str());

			AfxMessageBox(_T("Login successful!"));

			ShowWindow(SW_HIDE);

			main_window mwd;
			mwd.m_userId = userId;
			mwd.m_username = usernameCStr;
			mwd.SetClientSocket(clientSocket);
			mwd.DoModal();

			clientSocket = INVALID_SOCKET;
			EndDialog(IDOK);
		}
		else {
			CString errorMsg = _T("Login failed");
			if (response.find("error") != response.end()) {
				errorMsg += _T(": ") + CString(response["error"].c_str());
			}
			AfxMessageBox(errorMsg);
		}
	}
	catch (const std::exception& e) {
		AfxMessageBox(_T("Error processing server response!"));
	}
}

void Cweek2Dlg::OnBnClickedSignup()
{
	ShowWindow(SW_HIDE);

	SignUpDlg sud;
	sud.SetClientSocket(clientSocket);
	INT_PTR result = sud.DoModal();

	if (result == IDOK) {
		ShowWindow(SW_SHOW);
	}
	else {
		EndDialog(IDCANCEL);
	}
}