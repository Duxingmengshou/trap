#include "resource.h"
#include <windows.h>
#include <shellapi.h>
#include <tchar.h>
#include <string>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iostream>

// 链接 OpenGL 库
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

// 常量
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_TOGGLE_HOOK 1000
#define ID_TRAY_ABOUT 1002
#define ID_TRAY_EXIT  1001
#define ID_HOTKEY     2001

// 全局变量
HINSTANCE g_hInst;
HWND g_hWnd;
NOTIFYICONDATA g_nid;

HHOOK g_hKbHook = NULL;
bool g_hookEnabled = false;
volatile LONG g_processing = 0;

// ---------- OpenGL 绘制关于窗口 ----------
HGLRC hRC;
HDC hDC;
float angle = 0.0f;

void DrawCube() {
	glBegin(GL_QUADS);
	glColor3f(0.0f, 1.0f, 0.0f);
	// 前
	glVertex3f(-1.0f, -1.0f, 1.0f);
	glVertex3f(1.0f, -1.0f, 1.0f);
	glVertex3f(1.0f, 1.0f, 1.0f);
	glVertex3f(-1.0f, 1.0f, 1.0f);
	// 后
	glVertex3f(-1.0f, -1.0f, -1.0f);
	glVertex3f(-1.0f, 1.0f, -1.0f);
	glVertex3f(1.0f, 1.0f, -1.0f);
	glVertex3f(1.0f, -1.0f, -1.0f);
	// 左
	glVertex3f(-1.0f, -1.0f, -1.0f);
	glVertex3f(-1.0f, -1.0f, 1.0f);
	glVertex3f(-1.0f, 1.0f, 1.0f);
	glVertex3f(-1.0f, 1.0f, -1.0f);
	// 右
	glVertex3f(1.0f, -1.0f, -1.0f);
	glVertex3f(1.0f, 1.0f, -1.0f);
	glVertex3f(1.0f, 1.0f, 1.0f);
	glVertex3f(1.0f, -1.0f, 1.0f);
	// 上
	glVertex3f(-1.0f, 1.0f, -1.0f);
	glVertex3f(-1.0f, 1.0f, 1.0f);
	glVertex3f(1.0f, 1.0f, 1.0f);
	glVertex3f(1.0f, 1.0f, -1.0f);
	// 下
	glVertex3f(-1.0f, -1.0f, -1.0f);
	glVertex3f(1.0f, -1.0f, -1.0f);
	glVertex3f(1.0f, -1.0f, 1.0f);
	glVertex3f(-1.0f, -1.0f, 1.0f);
	glEnd();
}

void SetupPixelFormat(HDC hDC) {
	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR), 1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA, 32,
		0,0,0,0,0,0, 0,0,0,0,0,0,0,
		24, 0,0,
		PFD_MAIN_PLANE, 0,0,0,0
	};
	int pixelFormat = ChoosePixelFormat(hDC, &pfd);
	SetPixelFormat(hDC, pixelFormat, &pfd);
}

LRESULT CALLBACK AboutWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CREATE:
		hDC = GetDC(hWnd);
		SetupPixelFormat(hDC);
		hRC = wglCreateContext(hDC);
		wglMakeCurrent(hDC, hRC);
		glEnable(GL_DEPTH_TEST);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(45.0, 1.0, 1.0, 100.0);
		glMatrixMode(GL_MODELVIEW);
		SetTimer(hWnd, 1, 16, NULL);
		return 0;
	case WM_TIMER:
		angle += 1.0f;
		InvalidateRect(hWnd, NULL, FALSE);
		return 0;
	case WM_PAINT: {
		PAINTSTRUCT ps;
		BeginPaint(hWnd, &ps);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glLoadIdentity();
		glTranslatef(0.0f, 0.0f, -6.0f);
		glRotatef(angle, 1.0f, 1.0f, 0.0f);
		DrawCube();
		SwapBuffers(hDC);
		EndPaint(hWnd, &ps);
	} return 0;
	case WM_DESTROY:
		KillTimer(hWnd, 1);
		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(hRC);
		ReleaseDC(hWnd, hDC);
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ---------- 剪贴板工具 ----------
bool ReadTempFileToUtf8String(std::string& out) {
	TCHAR tempPath[MAX_PATH];
	if (!GetTempPath(MAX_PATH, tempPath)) return false;
	TCHAR filePath[MAX_PATH];
	wsprintf(filePath, TEXT("%s__TEMPCLIP.GlobalTYDecode"), tempPath);

	HANDLE hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return false;

	DWORD fileSize = GetFileSize(hFile, NULL);
	if (fileSize == INVALID_FILE_SIZE || fileSize == 0) {
		CloseHandle(hFile);
		return false;
	}
	char* buffer = new char[fileSize + 1];
	DWORD bytesRead = 0;
	BOOL ok = ReadFile(hFile, buffer, fileSize, &bytesRead, NULL);
	CloseHandle(hFile);
	if (!ok || bytesRead == 0) {
		delete[] buffer;
		return false;
	}
	buffer[bytesRead] = '\0';
	out.assign(buffer, bytesRead);
	delete[] buffer;
	return true;
}

bool WriteUtf8ToClipboard(const std::string& utf8) {
	int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
	if (wideLen == 0) return false;
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (wideLen + 1) * sizeof(WCHAR));
	if (!hMem) return false;
	LPWSTR pMem = (LPWSTR)GlobalLock(hMem);
	if (!pMem) { GlobalFree(hMem); return false; }
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), pMem, wideLen);
	pMem[wideLen] = L'\0';
	GlobalUnlock(hMem);
	if (!OpenClipboard(NULL)) {
		GlobalFree(hMem);
		return false;
	}
	EmptyClipboard();
	if (SetClipboardData(CF_UNICODETEXT, hMem) == NULL) {
		CloseClipboard();
		GlobalFree(hMem);
		return false;
	}
	CloseClipboard();
	return true;
}

// ---------- 通知 ----------
void ShowNotify(LPCTSTR title, LPCTSTR text) {
	g_nid.uFlags = NIF_INFO;
	lstrcpyn(g_nid.szInfoTitle, title, ARRAYSIZE(g_nid.szInfoTitle));
	lstrcpyn(g_nid.szInfo, text, ARRAYSIZE(g_nid.szInfo));
	g_nid.dwInfoFlags = NIIF_INFO;
	Shell_NotifyIcon(NIM_MODIFY, &g_nid);
}

// ---------- Ctrl+V 钩子处理 ----------
void HandleCtrlVAction() {
	LONG prev = InterlockedExchange(&g_processing, 1);
	if (prev == 1) return;
	// 调用 code.exe
	TCHAR exePath[MAX_PATH];
	GetModuleFileName(NULL, exePath, MAX_PATH);
	TCHAR dirPath[MAX_PATH]; lstrcpy(dirPath, exePath);
	WCHAR* p = wcsrchr(dirPath, L'\\'); if (p) *p = L'\0';
	TCHAR codePath[MAX_PATH]; wsprintf(codePath, TEXT("%s\\code.exe"), dirPath);

	STARTUPINFO si = { sizeof(si) };
	PROCESS_INFORMATION pi;
	BOOL created = CreateProcess(
		codePath, NULL, NULL, NULL, FALSE,
		CREATE_NO_WINDOW,
		NULL, dirPath, &si, &pi);
	if (created) {
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
	}
	else {
		ShowNotify(TEXT("错误"), TEXT("无法启动 code.exe"));
		InterlockedExchange(&g_processing, 0);
		return;
	}

	// 读取文件
	std::string utf8;
	if (!ReadTempFileToUtf8String(utf8)) {
		ShowNotify(TEXT("错误"), TEXT("读取临时文件失败"));
		InterlockedExchange(&g_processing, 0);
		return;
	}
	if (utf8 == "__NON_TEXT__") {
		ShowNotify(TEXT("提示"), TEXT("检测到非文本内容，跳过"));
		InterlockedExchange(&g_processing, 0);
		return;
	}
	if (utf8 == "__TOO_LARGE__") {
		ShowNotify(TEXT("提示"), TEXT("检测到过大文本，跳过"));
		InterlockedExchange(&g_processing, 0);
		return;
	}

	// 写入剪贴板
	if (WriteUtf8ToClipboard(utf8)) {
		ShowNotify(TEXT("成功"), TEXT("已写入剪贴板"));
	}
	else {
		ShowNotify(TEXT("错误"), TEXT("写入剪贴板失败"));
	}
	InterlockedExchange(&g_processing, 0);
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode == HC_ACTION) {
		KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
		if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) && p->vkCode == 'V') {
			bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
			bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
			bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
			bool win = (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0;

			if (ctrl && !alt && !shift && !win) {
				HandleCtrlVAction();
			}
		}
	}
	return CallNextHookEx(g_hKbHook, nCode, wParam, lParam);
}

// ---------- 托盘 ----------
void AddTrayIcon(HWND hWnd) {
	g_nid.cbSize = sizeof(NOTIFYICONDATA);
	g_nid.hWnd = hWnd;
	g_nid.uID = 1;
	g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	g_nid.uCallbackMessage = WM_TRAYICON;
	g_nid.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_TRAPICON));
	lstrcpy(g_nid.szTip, TEXT("Trap!"));
	Shell_NotifyIcon(NIM_ADD, &g_nid);
}
void RemoveTrayIcon() { Shell_NotifyIcon(NIM_DELETE, &g_nid); }

void ShowTrayMenu(HWND hWnd) {
	POINT pt; GetCursorPos(&pt);
	HMENU hMenu = CreatePopupMenu();
	AppendMenu(hMenu, MF_STRING | (g_hookEnabled ? MF_CHECKED : MF_UNCHECKED),
		ID_TRAY_TOGGLE_HOOK, TEXT("启用 Ctrl+V 钩子"));
	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
	AppendMenu(hMenu, MF_STRING, ID_TRAY_ABOUT, TEXT("关于Trap"));
	AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, TEXT("退出"));
	SetForegroundWindow(hWnd);
	TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN,
		pt.x, pt.y, 0, hWnd, NULL);
	DestroyMenu(hMenu);
}

// ---------- 主窗口过程 ----------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_CREATE:
		AddTrayIcon(hWnd);
		if (RegisterHotKey(hWnd, ID_HOTKEY, MOD_CONTROL | MOD_ALT | MOD_WIN, 'V')) {
			ShowNotify(TEXT("Trap"), TEXT("已注册 Ctrl+Win+Alt+V 热键"));
		}
		else {
			ShowNotify(TEXT("Trap"), TEXT("注册热键失败，可能被占用"));
		}
		break;
	case WM_HOTKEY:
		if (wParam == ID_HOTKEY) {
			HandleCtrlVAction();
			if (!g_hookEnabled) {
				// 模拟 Ctrl+V
				INPUT inputs[4] = {};
				inputs[0].type = INPUT_KEYBOARD;
				inputs[0].ki.wVk = VK_CONTROL;

				inputs[1].type = INPUT_KEYBOARD;
				inputs[1].ki.wVk = 'V';

				inputs[2].type = INPUT_KEYBOARD;
				inputs[2].ki.wVk = 'V';
				inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

				inputs[3].type = INPUT_KEYBOARD;
				inputs[3].ki.wVk = VK_CONTROL;
				inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

				SendInput(4, inputs, sizeof(INPUT));
			}
		}
		break;
	case WM_TRAYICON:
		if (lParam == WM_RBUTTONUP) ShowTrayMenu(hWnd);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case ID_TRAY_TOGGLE_HOOK:
			if (g_hookEnabled) {
				UnhookWindowsHookEx(g_hKbHook);
				g_hKbHook = NULL; g_hookEnabled = false;
				ShowNotify(TEXT("Trap"), TEXT("已禁用 Ctrl+V 钩子"));
			}
			else {
				g_hKbHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_hInst, 0);
				if (g_hKbHook) {
					g_hookEnabled = true;
					ShowNotify(TEXT("Trap"), TEXT("已启用 Ctrl+V 钩子"));
				}
				else {
					ShowNotify(TEXT("错误"), TEXT("安装钩子失败"));
				}
			}
			break;
		case ID_TRAY_ABOUT: {
			WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
			wc.lpfnWndProc = AboutWndProc;
			wc.hInstance = g_hInst;
			wc.lpszClassName = TEXT("AboutWin");
			wc.style = CS_OWNDC;
			wc.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_TRAPICON));
			wc.hIconSm = (HICON)LoadImage(g_hInst, MAKEINTRESOURCE(IDI_TRAPICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
			RegisterClassEx(&wc);
			HWND hAbout = CreateWindowEx(WS_EX_TOPMOST, TEXT("AboutWin"), TEXT("Trap: Ctrl+Win+Alt+V "),
				WS_OVERLAPPED | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 400, 400,
				NULL, NULL, g_hInst, NULL);
			ShowWindow(hAbout, SW_SHOW);
			UpdateWindow(hAbout);
		} break;
		case ID_TRAY_EXIT:
			PostMessage(hWnd, WM_CLOSE, 0, 0);
			break;
		}
		break;
	case WM_DESTROY:
		if (g_hKbHook) UnhookWindowsHookEx(g_hKbHook);
		RemoveTrayIcon();
		UnregisterHotKey(hWnd, ID_HOTKEY);
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

// ---------- WinMain ----------
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
	g_hInst = hInstance;
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = TEXT("TrayAppClass");
	RegisterClass(&wc);
	g_hWnd = CreateWindowEx(0, TEXT("TrayAppClass"), TEXT("TrayApp"),
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
		NULL, NULL, hInstance, NULL);
	ShowWindow(g_hWnd, SW_HIDE);
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (int)msg.wParam;
}
