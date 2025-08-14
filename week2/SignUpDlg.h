#pragma once
#include <winsock2.h>
#include <map>
#include <string>

// SignUpDlg dialog

class SignUpDlg : public CDialogEx
{
	DECLARE_DYNAMIC(SignUpDlg)

public:
	SignUpDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~SignUpDlg();

	// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SignUpDlg };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	// Network related
	SOCKET clientSocket;
	bool SendMessage(std::map<std::string, std::string> message);
	std::map<std::string, std::string> ReceiveMessage();

	// Validation helper
	bool IsPasswordStrong(CString password);

	DECLARE_MESSAGE_MAP()

public:
	// Controls
	CEdit ctr_edt_name;
	CEdit ctr_edt_pass;
	CEdit ctr_edt_cfpass;

	// Methods
	void SetClientSocket(SOCKET socket);

	// Event handlers
	afx_msg void OnBnClickedSignup();
};