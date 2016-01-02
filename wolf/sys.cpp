/**
 * Part of WOLF - WinAPI Object Lambda Framework
 * @author Rodrigo Cesar de Freitas Dias
 * @see https://github.com/rodrigocfd/wolf
 */

#include <algorithm>
#include "Sys.h"
#include <process.h>
#include <Shlobj.h>
using namespace wolf;
using std::function;
using std::vector;
using std::wstring;

void Sys::thread(function<void()> callback)
{
	// Cheap alternative to std::thread([](){}).detach().

	struct CbPack { function<void()> cb; };
	CbPack *pack = new CbPack{ std::move(callback) };

	HANDLE thandle = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, [](void *ptr)->unsigned int {
		CbPack *pPack = reinterpret_cast<CbPack*>(ptr);
		pPack->cb(); // invoke user callback
		delete pPack;
		_endthreadex(0); // http://www.codeproject.com/Articles/7732/A-class-to-synchronise-thread-completions/
		return 0;
	}, pack, 0, nullptr));

	CloseHandle(thandle);
}

DWORD Sys::exec(wstring cmdLine)
{
	SECURITY_ATTRIBUTES sa = { 0 };
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	STARTUPINFO si = { 0 };
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOW;

	PROCESS_INFORMATION pi = { 0 };
	DWORD dwExitCode = 1; // returned by executed program

	// http://blogs.msdn.com/b/oldnewthing/archive/2009/06/01/9673254.aspx
	if (CreateProcess(nullptr, &cmdLine[0], &sa, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
		WaitForSingleObject(pi.hProcess, INFINITE); // the program flow is stopped here to wait
		GetExitCodeProcess(pi.hProcess, &dwExitCode);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
	return dwExitCode;
}

wstring Sys::pathOfExe()
{
	wchar_t buf[MAX_PATH] = { L'\0' };
	GetModuleFileName(nullptr, buf, ARRAYSIZE(buf)); // retrieves EXE itself directory

	wstring ret = buf;
	ret.resize(ret.find_last_of(L'\\')); // truncate removing EXE filename and trailing backslash
#ifdef _DEBUG
	ret.resize(ret.find_last_of(L'\\')); // bypass "Debug" folder, remove trailing backslash too
#endif
	return ret;
}

wstring Sys::pathOfDesktop()
{
	wchar_t buf[MAX_PATH] = { L'\0' };
	SHGetFolderPath(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, buf); // won't have trailing backslash
	return buf;
}

bool Sys::hasCtrl()
{
	return (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
}

bool Sys::hasShift()
{
	return (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
}

static HHOOK _hHookMsgBox = nullptr;
static HWND  _hWndParent = nullptr;
static LRESULT CALLBACK _msgBoxHookProc(int code, WPARAM wp, LPARAM lp)
{
	// http://www.codeguru.com/cpp/w-p/win32/messagebox/print.php/c4541
	if (code == HCBT_ACTIVATE) {
		HWND hMsgbox = reinterpret_cast<HWND>(wp);
		RECT rcMsgbox = { 0 }, rcParent = { 0 };

		if (hMsgbox && _hWndParent && GetWindowRect(hMsgbox, &rcMsgbox) && GetWindowRect(_hWndParent, &rcParent)) {
			RECT  rcScreen = { 0 };
			POINT pos = { 0 };
			SystemParametersInfo(SPI_GETWORKAREA, 0, static_cast<PVOID>(&rcScreen), 0); // size of desktop

			// Adjusted x,y coordinates to message box window.
			pos.x = rcParent.left + (rcParent.right - rcParent.left) / 2 - (rcMsgbox.right - rcMsgbox.left) / 2;
			pos.y = rcParent.top + (rcParent.bottom - rcParent.top) / 2 - (rcMsgbox.bottom - rcMsgbox.top) / 2;

			// Screen out-of-bounds corrections.
			if (pos.x < 0) {
				pos.x = 0;
			} else if (pos.x + (rcMsgbox.right - rcMsgbox.left) > rcScreen.right) {
				pos.x = rcScreen.right - (rcMsgbox.right - rcMsgbox.left);
			}
			if (pos.y < 0) {
				pos.y = 0;
			} else if (pos.y + (rcMsgbox.bottom - rcMsgbox.top) > rcScreen.bottom) {
				pos.y = rcScreen.bottom - (rcMsgbox.bottom - rcMsgbox.top);
			}
			MoveWindow(hMsgbox, pos.x, pos.y,
				rcMsgbox.right - rcMsgbox.left, rcMsgbox.bottom - rcMsgbox.top,
				FALSE);
		}
		UnhookWindowsHookEx(_hHookMsgBox); // release hook
	}
	return CallNextHookEx(nullptr, code, wp, lp);
}

int Sys::msgBox(const Window *parent, wstring title, wstring text, UINT uType)
{
	return msgBox(parent->hWnd(), title, text, uType);
}

int Sys::msgBox(HWND hParent, wstring title, wstring text, UINT uType)
{
	if (hParent) { // the hook is set to center the message box window on parent
		_hWndParent = hParent;
		_hHookMsgBox = SetWindowsHookEx(WH_CBT, _msgBoxHookProc, nullptr, GetCurrentThreadId());
	}
	return MessageBox(hParent, text.c_str(), title.c_str(), uType);
}

vector<wstring> Sys::getDroppedFiles(HDROP hDrop)
{
	vector<wstring> files(DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0)); // alloc return vector
	for (size_t i = 0; i < files.size(); ++i) {
		files[i].resize(DragQueryFile(hDrop, static_cast<UINT>(i), nullptr, 0) + 1, L'\0'); // alloc path string
		DragQueryFile(hDrop, static_cast<UINT>(i), &files[i][0], static_cast<UINT>(files[i].size()));
		files[i].resize(files[i].size() - 1); // trim null
	}
	DragFinish(hDrop);
	std::sort(files.begin(), files.end(), [](const wstring& a, const wstring& b)->bool {
		return lstrcmpi(a.c_str(), b.c_str()) < 0;
	});
	return files;
}