/**
 * Part of WOLF - WinAPI Object Lambda Framework
 * @author Rodrigo Cesar de Freitas Dias
 * @see https://github.com/rodrigocfd/wolf
 */

#include "WindowMain.h"
using namespace wolf;

WindowMain::SetupMain::SetupMain()
	: iconId(0)
{
}


WindowMain::~WindowMain()
{
}

WindowMain::WindowMain()
{
	this->WindowMsgHandler::onMessage(WM_DESTROY, [this](WPARAM wp, LPARAM lp)->LRESULT {
		// https://msdn.microsoft.com/en-us/library/windows/desktop/ff381396%28v=vs.85%29.aspx
		PostQuitMessage(0);
		return 0;
	});
}

int WindowMain::run(HINSTANCE hInst, int cmdShow)
{
	if (this->Window::hWnd()) {
		WindowMsgHandler::_errorShout(L"WindowMain::run called twice.");
		return -1;
	}

	InitCommonControls();
	DWORD style = 0, exStyle = 0;

	if (this->setup.dialogId) {
		if (!this->WindowTopLevel::_loadIfTemplate(hInst, this->setup)) return -1;
		if (this->setup.menu.hMenu()) this->setup.size.cy += GetSystemMetrics(SM_CYMENU); // add menu height
		style = this->_dialogTemplate.style;
		exStyle = this->_dialogTemplate.exStyle;
	} else {
		style = WindowTopLevel::_calcStyle(this->setup);
		exStyle = WindowTopLevel::_calcStyleEx(this->setup);
		if (!WindowTopLevel::_compensateBorders(WindowTopLevel::_calcStyle(this->setup),
			this->setup.menu.hMenu() != nullptr, this->setup)) return -1;
	}

	if (!CreateWindowEx(exStyle, MAKEINTATOM(this->_registerClass(hInst)),
		this->setup.title.c_str(), style,
		GetSystemMetrics(SM_CXSCREEN) / 2 - this->setup.size.cx / 2, // center on screen
		GetSystemMetrics(SM_CYSCREEN) / 2 - this->setup.size.cy / 2,
		this->setup.size.cx, this->setup.size.cy,
		nullptr, this->setup.menu.hMenu(), hInst,
		static_cast<LPVOID>(this)) ) // pass pointer to object, _hWnd is set on WM_NCCREATE
	{
		WindowMsgHandler::_errorShout(GetLastError(), L"WindowMain::run", L"CreateWindowEx");
		return -1;
	}

	ShowWindow(this->Window::hWnd(), cmdShow);
	UpdateWindow(this->Window::hWnd());
	return this->WindowTopLevel::_loop(this->setup);
}

ATOM WindowMain::_registerClass(HINSTANCE hInst)
{
	WNDCLASSEX wc = { 0 };
	wc.cbWndExtra = sizeof(WindowMain*);

	if (this->setup.iconId) {
		if (!( wc.hIcon = static_cast<HICON>(LoadImage(hInst, MAKEINTRESOURCE(this->setup.iconId), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR)) )) {
			WindowMsgHandler::_errorShout(GetLastError(), L"WindowMain::_registerClass", L"LoadImage 32");
			return 0;
		}
		if (!( wc.hIconSm = static_cast<HICON>(LoadImage(hInst, MAKEINTRESOURCE(this->setup.iconId), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR)) )) {
			WindowMsgHandler::_errorShout(GetLastError(), L"WindowMain::_registerClass", L"LoadImage 16");
			return 0;
		}
	}

	return WindowProc::_registerClass(hInst, wc, this->setup);
}