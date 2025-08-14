#pragma once
#include <winsock2.h>
#include <map>
#include <string>

// Cweek2Dlg dialog
class Cweek2Dlg : public CDialogEx
{
	// Construction
public:
	Cweek2Dlg(CWnd* pParent = nullptr);	// standard constructor
	virtual ~Cweek2Dlg();
	// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_WEEK2_DIALOG };
#endif
protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	// Implementation
protected:
	HICON m_hIcon;
	SOCKET clientSocket;
	// Network functions
	bool InitializeWinsock();
	bool ConnectToServer(char* serverIP, int port);
	void DisconnectFromServer();
	bool SendNetworkMessage(std::map<std::string, std::string> message);
	std::map<std::string, std::string> ReceiveNetworkMessage(); 
	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	// Controls
	CEdit ed_name;
	CEdit ed_pass;
	CButton btn_login;
	CButton btn_signup;
	// Event handlers
	afx_msg void OnBnClickedLogin();
	afx_msg void OnBnClickedSignup();
};