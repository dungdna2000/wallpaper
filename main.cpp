/* 
*  Alternative wallpaper option in case your wall paper cannot be changed, somehow! 
*/

#include <Windows.h>
#include <signal.h>
#include <string>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <vector>

#include <filesystem>
#include <thread>

#include <gdiplus.h>

using namespace std;
using namespace Gdiplus;

#pragma comment (lib,"Gdiplus.lib")


#define WINDOW_CLASS_NAME L"HomeCredit.Wallpaper"
#define WINDOW_TITLE L"Wall paper"
#define WINDOW_TITLE_FOREGROUND L"Hidden foreground window"


#define MUTEX_NAME L"Global\\HomeCredit.Wallpaper"

#define WM_KILL_MESSAGE (WM_USER + 100)
#define WM_DISPLAYCHANGE_FWD (WM_USER + 101)


void DrawBgImage(HWND);
void FillSolidColor(HWND, int, int, int);
void CreateWindows(HWND&, HWND&);


HWND hWnd = 0;
HWND hWndPrev = 0;

HWND hWndForeGround = 0;

Bitmap* imgBackground = NULL;
LPWSTR imgPathBackground;

BOOL bWPNeedRedraw = FALSE;

// GDI+
GdiplusStartupInput gdiplusStartupInput;
ULONG_PTR gdiplusToken = 0;

void InitGdi() {
	if (!::gdiplusToken) {
		GdiplusStartup(&::gdiplusToken, &::gdiplusStartupInput, NULL);
	}
}


// DEBUG SUPPORT FUNCTIONS //////////////
#define _W(x)  __W(x)
#define __W(x)  L##x

#define VA_PRINTS(s) {				\
		va_list argp;				\
		va_start(argp, fmt);		\
		vswprintf_s(s, fmt, argp);	\
		va_end(argp);				\
}		

void log(const wchar_t* fmt, ...)
{
	wchar_t s[4096];
	VA_PRINTS(s);
	wcout << s;
}

void err(const wchar_t* fmt, ...)
{
	wchar_t s[4096];
	VA_PRINTS(s);
	wcerr << s;
}

vector<string> split(string line, string delimeter)
{
	vector<string> tokens;
	size_t last = 0; size_t next = 0;
	while ((next = line.find(delimeter, last)) != string::npos)
	{
		tokens.push_back(line.substr(last, next - last));
		last = next + 1;
	}
	tokens.push_back(line.substr(last));

	return tokens;
}

RECT GetVirtualScreenRect() {

	RECT r;
	r.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
	r.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
	r.right = GetSystemMetrics(SM_CXVIRTUALSCREEN) - r.left;
	r.bottom = GetSystemMetrics(SM_CYVIRTUALSCREEN);

	return r;
}

void PrintRect(const RECT& r) {
	cout << r.left << " " << r.top << " " << r.right << " " << r.bottom << endl;
}

//////////////////////////////////////////

LRESULT CALLBACK WinProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_PAINT:
		PAINTSTRUCT ps; 		
		BeginPaint(hWnd, &ps);
		
		if (hWnd == ::hWnd) {
			//log(L"hWnd > WM_PAINT\n");
			if (::imgBackground != NULL) {
				DrawBgImage(::hWnd);
			}
		}

		EndPaint(hWnd,&ps);
		break;
	case WM_KILL_MESSAGE:
		PostQuitMessage(0);
		break;

	case WM_DISPLAYCHANGE_FWD:
		if (hWnd == ::hWnd) {
			RECT vScreen = GetVirtualScreenRect();
			SetWindowPos(::hWnd, NULL, 
				vScreen.left, 
				vScreen.top,
				vScreen.right - vScreen.left,
				vScreen.bottom - vScreen.top, SWP_NOZORDER | SWP_NOSENDCHANGING);

			PostMessage(hWnd, WM_PAINT, 0, 0);
		}
		break;
	case WM_DISPLAYCHANGE:
		if (hWnd == ::hWndForeGround) {
			SendMessage(::hWnd, WM_DISPLAYCHANGE_FWD, 0, 0);
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

HWND FindLastWorkerW(int level, HWND hwndParent) {
	HWND hwndChild = 0;
	HWND hwndRes = 0;
	do {	
		hwndChild = FindWindowEx(hwndParent, hwndChild, L"WorkerW", 0);
		if (hwndChild) {
			hwndRes = hwndChild;
		}

	} while (hwndChild);

	return hwndRes;
}

/*
*  Find this wall paper window under WorkerW 
*/
HWND FindMe(HWND hwndParent) {
	HWND hwndChild = 0;
	HWND hwndRes = 0;
	do {
		hwndChild = FindWindowEx(hwndParent, hwndChild, L"WorkerW", 0);
		if (hwndChild) {
			hwndRes = FindWindowEx(hwndChild, 0, WINDOW_CLASS_NAME, 0);
			if (hwndRes) return hwndRes;
		}
	} while (hwndChild);

	return 0;
}

void CreateWindows(HWND &hWnd, HWND &hWndForeground) {

	HINSTANCE hInstance = GetModuleHandle(NULL);

	WNDCLASSEX wc;
	wc.cbSize = sizeof(WNDCLASSEX);

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.hInstance = hInstance;

	wc.lpfnWndProc = (WNDPROC)WinProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hIcon = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

	wc.lpszMenuName = NULL;
	wc.lpszClassName = WINDOW_CLASS_NAME;
	wc.hIconSm = NULL;

	RegisterClassEx(&wc);


	RECT vScreen = GetVirtualScreenRect();

	hWnd =
		CreateWindow(
			WINDOW_CLASS_NAME,
			WINDOW_TITLE,
			WS_CHILD | WS_EX_NOACTIVATE | WS_POPUP | WS_EX_TRANSPARENT,
			vScreen.left,
			vScreen.top,
			vScreen.right - vScreen.left,
			vScreen.bottom - vScreen.top,
			NULL,
			NULL,
			hInstance,
			NULL);

	if (!hWnd) {
		DWORD ErrCode = GetLastError();
		err((wchar_t*)L"Failed to create wallpaper window ErrCode: %d\nAt: %s %d \n", ErrCode, _W(__FILE__), __LINE__);
		return;
	}


	hWndForeGround =
		CreateWindow(
			WINDOW_CLASS_NAME,
			WINDOW_TITLE_FOREGROUND,
			WS_CHILD | WS_EX_NOACTIVATE | WS_POPUP | WS_EX_TRANSPARENT,
			0,
			0,
			10,
			10,
			NULL,
			NULL,
			hInstance,
			NULL);

	if (!hWndForeGround) {
		DWORD ErrCode = GetLastError();
		err((wchar_t*)L"Failed to create foreground hidden window ErrCode: %d\nAt: %s %d \n", ErrCode, _W(__FILE__), __LINE__);
		return;
	}


	HWND hwndDesktop = GetDesktopWindow();

	//
	// CREDIT : this magic is from : https://github.com/Francesco149/weebp  
	//
	HWND hwndProgman = FindWindowEx(hwndDesktop, 0, L"Progman", 0);
	SendMessage(hwndProgman, 0x052C, 0xD, 0);
	SendMessage(hwndProgman, 0x052C, 0xD, 1);

	HWND hwndWorkerW = FindLastWorkerW(0, hwndDesktop);
	if (!hwndWorkerW) {
		err(L"FATAL: Failed to find the WorkerW window!\n");
		return;
	}

	SetParent(hWnd, hwndWorkerW);

	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);
}

HWND GetWallPaperWindow(HINSTANCE hInstance, HWND &hWndPrev)
{

	HWND hwndDesktop = GetDesktopWindow();
	HWND hWnd = FindMe(hwndDesktop);
	if (hWnd) {
		log(L"INFO: Found existing wallpaper!\n");
		hWndPrev = hWnd;
		return hWnd;
	}

	CreateMutex(NULL, FALSE, MUTEX_NAME);

	hWndPrev = 0;

	CreateWindows(hWnd,::hWndForeGround);

	return hWnd;
}



/*
	Convert char * string to wchar_t* string.
*/
wchar_t* ToWSTR(const char * st)
{
	const char* str = st;

	size_t newsize = strlen(str) + 1;
	wchar_t* wcstring = new wchar_t[newsize];
	size_t convertedChars = 0;
	mbstowcs_s(&convertedChars, wcstring, newsize, str, _TRUNCATE);

	return wcstring;
}


void CleanUp() {

	if (::imgBackground != NULL) delete ::imgBackground;

	SetParent(hWnd, GetDesktopWindow());
	ShowWindow(hWnd, SW_HIDE);
	CloseWindow(hWnd);

	if (gdiplusToken) {
		Gdiplus::GdiplusShutdown(gdiplusToken);
	}
}

void Run() {
	MSG msg;
	int done = 0;

	while (!done)
	{
		if (GetMessage(&msg, NULL, 0, 0))
		{
			if (msg.message == WM_KILL_MESSAGE) done = 1;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	CleanUp();
	log(L"Bye!\n");
}

void DrawBgImage(HWND hWnd) {

	HDC hdc = GetWindowDC(hWnd);
	Gdiplus::Graphics* graphics = new Gdiplus::Graphics(hdc);

	RECT vScreen = GetVirtualScreenRect();

	vector<RECT> * recMonitors = new vector<RECT>();

	EnumDisplayMonitors(NULL, NULL, [](HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) -> BOOL {
		
		vector<RECT>* recMonitors = (vector<RECT> *)dwData;
		RECT r;
		r.left = lprcMonitor->left;
		r.top = lprcMonitor->top;
		r.right = lprcMonitor->right;
		r.bottom = lprcMonitor->bottom;
		recMonitors->push_back(r);


		return TRUE;

	}, (LPARAM)recMonitors);

	//
	// Draw background image into each intersect rectangle with monitor, a.k.a draw on each monitor
	//
	RECT rIntersect;
	for (RECT rMonitor : *recMonitors) {
		
		if (IntersectRect(&rIntersect, &rMonitor, &vScreen)) {
			Gdiplus::Status s = graphics->DrawImage(::imgBackground, (INT)rIntersect.left, (INT)rIntersect.top, 
				(INT)(rIntersect.right - rIntersect.left), (INT)(rIntersect.bottom - rIntersect.top));
		}
	}
}

void FillSolidColor(HWND hWnd, int R, int G, int B) {

	HDC hdc = GetDC(hWnd);
	if (!hdc) {
		err(L"ERROR Failed to retrieve HDC hWnd = %s", hWnd);
		return;
	}

	RECT vScreen = GetVirtualScreenRect();

	Gdiplus::Graphics* graphics = new Gdiplus::Graphics(hdc);

	Gdiplus::SolidBrush * brush = new Gdiplus::SolidBrush(Color(255, R, G, B)); 

	Gdiplus::Rect rect(vScreen.left, vScreen.top, vScreen.right - vScreen.left, vScreen.bottom - vScreen.left);

	graphics->FillRectangle(brush, rect);

	// Release resources
	delete brush;
	delete graphics;
	ReleaseDC(hWnd, hdc);
}

void doDebug() {

}

void printUsage() {
	cout << "Usage: wallpaper [-c red green blue] | [path/to/your/image]" << endl;
	cout << "Examples: " << endl;
	cout << "wallpaper -c 64 64 64" << endl;
	cout << "wallpaper c:\\images\\bg.jpg" << endl;
}
void printAbout() {
	cout << "Small utility to set wallpaper in case you cannot change your wallpaper using Windows personalize settings" << endl;
	cout << "CREDIT TO: https://github.com/Francesco149/weebp" << endl << endl;
	printUsage();
}

void printError() {
	err(L"ERROR: Invalid number of arguments or invalid argument\n");
	printUsage();
}


int main(int argc, char* argv[]) {
	if (argc == 1) {
		printAbout();
		return 0;
	}

	SetProcessDPIAware();

	if (argc == 5) {
		if (strcmp(argv[1], "-c")==0) {
			::hWnd = GetWallPaperWindow(GetModuleHandle(NULL), hWndPrev);

			InitGdi();
			FillSolidColor(::hWnd,atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));

			if (!hWndPrev) {
				Run();
			}
			return 0;
		}
		else
		{
			printError();
			return 0;
		}

	}

	if (argc == 2) {
		if (strcmp(argv[1], "-d") == 0) {
			doDebug();
			return 0;
		}

		if (strcmp(argv[1], "-k") == 0) {
			hWnd = FindMe(GetDesktopWindow());
			if (hWnd) {
				HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, MUTEX_NAME);
				if (!hMutex) {
					cout << "ERROR: hMutex is NULL!" << endl;
					return 1;
				}

				PostMessage(hWnd, WM_KILL_MESSAGE, 0, 0);
				cout << "Wallpaper killed!" << endl;

				CloseHandle(hMutex);

				return 0;
			}
			else
			{
				cout << "No wallpaper window found. " << endl;
				return 1;
			}
		}

		hWnd = GetWallPaperWindow(GetModuleHandle(NULL), hWndPrev);

		char* imagePath = argv[1];

		std::filesystem::path filePath(imagePath);
		if (!std::filesystem::exists(imagePath)) {
			err(L"FATAL: Image file cannot be found: %s\n", ToWSTR(imagePath));
			return 1;
		}

		::imgPathBackground = ToWSTR(imagePath);
		if (imgBackground != NULL) {
			delete imgBackground;
		}

		InitGdi();
		::imgBackground = new Bitmap(::imgPathBackground);
		if (::imgBackground == NULL) {
			err(L"FATAL Failed to load image file %s", ::imgPathBackground);
			return 1;
		}

		if (!hWndPrev) {
			Run();
		}

		return 0;
	}

	printError();
	return 1;

}