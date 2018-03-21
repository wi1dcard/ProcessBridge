#pragma once
#include "stdafx.h"
#include <Windows.h>
#include <queue>

#pragma comment(lib, "User32.lib")
#pragma comment(lib,"gdi32.lib")



typedef struct _putty_msg_header{
	HWND hSrcRoute;
	HANDLE hThread;
	BYTE **lpBuffer;
	DWORD *pdwBytesTransfered;
	UINT_PTR uTimerId;
}PUTTY_MSG_HEADER;

typedef void(CALLBACK *LPFN_NTRECV)(HWND hRouteSelf, HWND hRouteRemote, DWORD dwCmd, PUTTY_MSG_HEADER *lpMessage, BYTE *lpBuffer, int nSize);

typedef struct _putty_msg_info{
	PUTTY_MSG_HEADER Message;
	LPFN_NTRECV lpfnCallback;
	HWND hRoute;
	DWORD dwCmd;
	BYTE *lpBuffer;
	int nSize;
}PUTTY_MSG_INFO;

#define PUTTY_NO_DATA				(WM_APP + 0x1701)

void APIENTRY NtInitialzePutty(void);

//创建窗口
HWND APIENTRY NtCreatePutty(LPCSTR szRouteName);

//消息循环
void APIENTRY NtPumpMessage(void);

//发送数据
BOOL APIENTRY NtSend(HWND hSelf,HWND hTarget,DWORD dwCmd, BYTE *lpSendBuffer = NULL, int nSize = 0,BYTE **lpReceiveBuffer = NULL,DWORD *pdwBytesTransfered = NULL);

//设置回调接口
void APIENTRY NtSetRecv(HWND hRoute,LPFN_NTRECV lpfnCallback);

//回应数据
BOOL APIENTRY NtResponse(PUTTY_MSG_HEADER *lpMessage, BYTE *lpSrc, int nSize);

//释放资源
void APIENTRY NtFree(BYTE *lpBuffer);

namespace Route{
	LRESULT CALLBACK OnDispatch(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	void CALLBACK OnTasker(LPVOID lParameter);
}


