#pragma once
#include <winsock2.h>
#include <map>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>

class Utf8Converter {
public:
	static std::string CStringToUtf8(const CString& cstr) {
		if (cstr.IsEmpty()) return "";

		CT2W wideStr(cstr);

		int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1,
			NULL, 0, NULL, NULL);
		if (utf8Size <= 0) return "";

		std::string utf8Result(utf8Size - 1, 0);
		WideCharToMultiByte(CP_UTF8, 0, wideStr, -1,
			&utf8Result[0], utf8Size, NULL, NULL);

		return utf8Result;
	}

	static CString Utf8ToCString(const std::string& utf8str) {
		if (utf8str.empty()) return CString();

		int wideSize = MultiByteToWideChar(CP_UTF8, 0, utf8str.c_str(), -1,
			NULL, 0);
		if (wideSize <= 0) return CString();

		WCHAR* wideBuffer = new WCHAR[wideSize];
		MultiByteToWideChar(CP_UTF8, 0, utf8str.c_str(), -1,
			wideBuffer, wideSize);

		CString result(wideBuffer);
		delete[] wideBuffer;

		return result;
	}
};

class main_window : public CDialogEx
{
	DECLARE_DYNAMIC(main_window)

public:
	main_window(CWnd* pParent = nullptr);
	virtual ~main_window();

	int m_userId;
	CString m_username;

	void SetCurrentUser(int userId, CString username);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG1 };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX); 
	virtual BOOL OnInitDialog() override;;

private:
	int m_currentUserId;
	CString m_currentUsername;
	int selectedUserId;

	SOCKET clientSocket;
	std::thread* networkThread;
	bool isNetworkActive;

	void StartNetworkThread();
	void StopNetworkThread();
	void NetworkThreadFunction();
	bool SendMessageToServer(std::map<std::string, std::string> message);
	void HandleServerMessage(std::map<std::string, std::string> message);

	void UpdateUserList(std::string userListData);
	void LoadChatHistory(int otherUserId);
	void AttemptReconnect();
	void ProcessMessageBuffer(std::string& buffer);

	std::string GetCurrentTimestamp();

	DECLARE_MESSAGE_MAP()

public:
	CListBox ctr_lst_user;
	CEdit ctr_edt_convers;
	CEdit ctr_edt_smsg;
	CButton ctr_btn_send;

	void SetClientSocket(SOCKET socket);

	afx_msg void OnLbnSelchangeUser();
	afx_msg void OnBnClickedSend();
	afx_msg void OnEnChangeSmsg();
	afx_msg void OnBnClickedLogout();
	afx_msg void OnClose();

	afx_msg LRESULT OnNewMessage(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnUserListUpdate(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnChatHistoryResponse(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnConnectionLost(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnSendMessageFailed(WPARAM wParam, LPARAM lParam);
};