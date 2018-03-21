#include "stdafx.h"
#include "Route.h"

namespace Route{
	std::queue<PUTTY_MSG_INFO> m_pQuMsgTransfered;
	CRITICAL_SECTION m_pCritical;
}

HWND APIENTRY NtCreatePutty(LPCSTR szRouteName)
{
	WNDCLASSA WndClass;
	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hInstance = GetModuleHandle(NULL);
	WndClass.lpfnWndProc = Route::OnDispatch;
	WndClass.lpszClassName = (LPCSTR)szRouteName;
	WndClass.lpszMenuName = NULL;
	WndClass.style = CS_HREDRAW | CS_VREDRAW;
	WndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	RegisterClassA(&WndClass);
	HWND hWnd = CreateWindowA((LPCSTR)szRouteName, NULL, WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_TABSTOP, 1, 1, 1, 1, NULL, NULL, GetModuleHandle(NULL), NULL);
	return hWnd;
}

LRESULT CALLBACK Route::OnDispatch(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_COPYDATA)
	{
		COPYDATASTRUCT *pTransferedInfo = (COPYDATASTRUCT*)lParam;
		PUTTY_MSG_INFO Info = { 0 };
		DWORD dwUserData = GetWindowLong(hWnd, GWL_USERDATA);
		Info.hRoute = hWnd;
		Info.dwCmd = pTransferedInfo->dwData;
		if (pTransferedInfo->cbData == sizeof(PUTTY_MSG_HEADER))
		{
			memcpy(&Info.Message, pTransferedInfo->lpData, sizeof(PUTTY_MSG_HEADER));
			Info.lpfnCallback = (LPFN_NTRECV)dwUserData;
			EnterCriticalSection(&Route::m_pCritical);
			Route::m_pQuMsgTransfered.push(Info);
			LeaveCriticalSection(&Route::m_pCritical);
		}
		else
		{
			memcpy(&Info.Message, pTransferedInfo->lpData, sizeof(PUTTY_MSG_HEADER));
			Info.lpfnCallback = (LPFN_NTRECV)dwUserData;
			Info.nSize = pTransferedInfo->cbData - sizeof(PUTTY_MSG_HEADER);
			Info.lpBuffer = new BYTE[Info.nSize];
			memcpy(Info.lpBuffer, &((BYTE*)pTransferedInfo->lpData)[sizeof(PUTTY_MSG_HEADER)], Info.nSize);
			EnterCriticalSection(&Route::m_pCritical);
			Route::m_pQuMsgTransfered.push(Info);
			LeaveCriticalSection(&Route::m_pCritical);
		}
		QueueUserWorkItem((LPTHREAD_START_ROUTINE)Route::OnTasker, NULL, WT_EXECUTEDEFAULT);
		return S_OK;
	}
	else if (message == WM_TIMER){
		PUTTY_MSG_HEADER* Header = (PUTTY_MSG_HEADER*)wParam;
		KillTimer(hWnd, Header->uTimerId);
		Header->uTimerId = 0;
		ResumeThread(Header->hThread);
		MessageBox(hWnd, L"timer", L"", 0);
	}
	return DefWindowProc(hWnd,message,wParam,lParam);
}

//消息循环
void APIENTRY NtPumpMessage(void)
{
	MSG Msg;
	while (GetMessage(&Msg, NULL,0,0))
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
}

//发送数据
BOOL APIENTRY NtSend(HWND hSelf, HWND hRoute, DWORD dwCmd, BYTE *lpSendBuffer, int nSize, BYTE **lpReceiveBuffer, DWORD *pdwBytesTransfered, UINT uRecvTimeout)
{
	PUTTY_MSG_HEADER Header = { 0 };
	Header.hSrcRoute = hSelf;
	if (pdwBytesTransfered != NULL && lpReceiveBuffer!=NULL)
	{
		Header.hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId());
		Header.pdwBytesTransfered = pdwBytesTransfered;
		Header.lpBuffer = lpReceiveBuffer;
	}
	if (pdwBytesTransfered != NULL && lpReceiveBuffer != NULL && uRecvTimeout)
	{
		Header.uTimerId = (UINT_PTR)&Header;
		BOOL bRes = SetTimer(Header.hSrcRoute, Header.uTimerId, uRecvTimeout, NULL);
		if (!bRes){
			Header.uTimerId = NULL;
		}
	}
	COPYDATASTRUCT Info = { 0 };
	Info.dwData = dwCmd;
	if (nSize <= 0 || lpSendBuffer == NULL)
	{
		//不发送数据
		Info.lpData = &Header;
		Info.cbData = sizeof(PUTTY_MSG_HEADER);
		::SendMessage(hRoute, WM_COPYDATA, 0, (LPARAM)&Info);
	}
	else
	{
		BYTE *lpAddr = new BYTE[sizeof(PUTTY_MSG_HEADER)+nSize];
		memcpy(lpAddr, &Header, sizeof(PUTTY_MSG_HEADER));
		memcpy(&lpAddr[sizeof(PUTTY_MSG_HEADER)], lpSendBuffer, nSize);
		Info.lpData = lpAddr;
		Info.cbData = sizeof(PUTTY_MSG_HEADER)+nSize;
		::SendMessage(hRoute, WM_COPYDATA, 0, (LPARAM)&Info);
		delete[]lpAddr;
	}
	if (pdwBytesTransfered != NULL && lpReceiveBuffer != NULL)
	{
		SuspendThread(Header.hThread);
	}
	CloseHandle(Header.hThread);
	return TRUE;
}

//设置回调接口
void APIENTRY NtSetRecv(HWND hRoute,LPFN_NTRECV lpfnCallback)
{
	SetWindowLong(hRoute, GWL_USERDATA, (DWORD)lpfnCallback);
}

void APIENTRY NtInitialzePutty(void)
{
	InitializeCriticalSection(&Route::m_pCritical);
}

void CALLBACK Route::OnTasker(LPVOID lParameter)
{
	bool bVoid = FALSE;
	PUTTY_MSG_INFO Info = { 0 };
	EnterCriticalSection(&Route::m_pCritical);
	bVoid = Route::m_pQuMsgTransfered.empty();
	if (!bVoid)
	{
		Info = Route::m_pQuMsgTransfered.front();
		Route::m_pQuMsgTransfered.pop();
	}
	LeaveCriticalSection(&Route::m_pCritical);
	if (bVoid) return;
	if (Info.dwCmd == 0)
	{
		if (Info.Message.lpBuffer != NULL && Info.Message.pdwBytesTransfered != NULL)
		{
			if (Info.Message.uTimerId == NULL || KillTimer(Info.Message.hSrcRoute, Info.Message.uTimerId))
			{
				*Info.Message.pdwBytesTransfered = Info.nSize;
				(*Info.Message.lpBuffer) = new BYTE[Info.nSize];
				memcpy((*Info.Message.lpBuffer), Info.lpBuffer, Info.nSize);
				ResumeThread(Info.Message.hThread);
			}
		}
	}
	else if (Info.lpfnCallback != NULL)
	{
		Info.lpfnCallback(Info.hRoute, Info.Message.hSrcRoute, Info.dwCmd, &Info.Message, Info.lpBuffer, Info.nSize);
	}
	delete[]Info.lpBuffer;
}

//回应数据
BOOL APIENTRY NtResponse(PUTTY_MSG_HEADER *lpMessage, BYTE *lpSrc, int nSize)
{
	if (lpMessage != NULL && lpSrc != NULL && nSize > 0)
	{
		BYTE *lpBuffer = new BYTE[sizeof(PUTTY_MSG_HEADER)+nSize];
		memcpy(lpBuffer, lpMessage, sizeof(PUTTY_MSG_HEADER));
		memcpy(&lpBuffer[sizeof(PUTTY_MSG_HEADER)], lpSrc, nSize);
		COPYDATASTRUCT Info = { 0 };
		Info.dwData = 0;
		Info.lpData = lpBuffer;
		Info.cbData = sizeof(PUTTY_MSG_HEADER)+nSize;
		::SendMessage(lpMessage->hSrcRoute, WM_COPYDATA, 0, (LPARAM)&Info);
		delete[]lpBuffer;
		return TRUE;
	}
	return FALSE;
}

//释放资源
void APIENTRY NtFree(BYTE *lpBuffer)
{
	if (lpBuffer != NULL)
	{
		delete[]lpBuffer;
	}
}