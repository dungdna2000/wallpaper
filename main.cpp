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
#define WINDOW_TITLE L"Wallpaper"
#define WINDOW_TITLE_FOREGROUND L"Hidden foreground window"


#define MUTEX_NAME L"Global\\HomeCredit.Wallpaper"

#define WM_KILL_MESSAGE (WM_USER + 100)
#define WM_DISPLAYCHANGE_FWD (WM_USER + 101)

void OnPaint(HWND);
void CreateWindows(HWND&, HWND&);


// HWND of the main wallpaper window
HWND hWndWallpaper = 0;

// HWND of the hidden foreground window, 
// the foreground window sole job is to forward WM_DISPLAYCHANGE to the wallpaper window
HWND hWndForeGround = 0;		

// The background image path set from command line
LPWSTR imgPathBackground = NULL;

// The background image loaded from the image file 
Bitmap* imgBackground = NULL;

// The background color specified by -c option from command line 
Color colorBackground(0, 0, 0);

// Mutex used to communicate between existing and new process 
HANDLE hMutex = 0;

// GDI+
GdiplusStartupInput gdiplusStartupInput;
ULONG_PTR gdiplusToken = 0;

void InitGdi() {
	if (!::gdiplusToken) {
		GdiplusStartup(&::gdiplusToken, &::gdiplusStartupInput, NULL);
	}
}


// SOME UTILITIES FUNCTIONS 
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

/*
*  Split a string into sub-strings separated by {delimeter}
*/
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


LRESULT CALLBACK WinProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_PAINT:
		PAINTSTRUCT ps; 		
		BeginPaint(hWnd, &ps);
		
		if (hWnd == ::hWndWallpaper) {
			OnPaint(::hWndWallpaper);
		}

		EndPaint(hWnd,&ps);
		break;
	case WM_KILL_MESSAGE:
		PostQuitMessage(0);
		break;

	case WM_DISPLAYCHANGE_FWD:
		if (hWnd == ::hWndWallpaper) {
			RECT vScreen = GetVirtualScreenRect();
			SetWindowPos(::hWndWallpaper, NULL, 
				vScreen.left, 
				vScreen.top,
				vScreen.right - vScreen.left,
				vScreen.bottom - vScreen.top, SWP_NOZORDER | SWP_NOSENDCHANGING);

			PostMessage(hWnd, WM_PAINT, 0, 0);
		}
		break;
	case WM_DISPLAYCHANGE:
		if (hWnd == ::hWndForeGround) {
			SendMessage(::hWndWallpaper, WM_DISPLAYCHANGE_FWD, 0, 0);
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
*  Find this wallpaper window under WorkerW windows
*/
HWND FindMe() {
	HWND hWndChild = 0;
	HWND hWndRes = 0;
	HWND hWndParent = GetDesktopWindow();
	do {
		hWndChild = FindWindowEx(hWndParent, hWndChild, L"WorkerW", 0);
		if (hWndChild) {
			hWndRes = FindWindowEx(hWndChild, 0, WINDOW_CLASS_NAME, 0);
			if (hWndRes) return hWndRes;
		}
	} while (hWndChild);

	return 0;
}

/*
*   Create the wallpaper window and the hidden foreground window
*/
void CreateWindows(HWND &hWndWallpaper, HWND &hWndForeground) {

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

	hWndWallpaper =
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

	if (!hWndWallpaper) {
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

	SetParent(hWndWallpaper, hwndWorkerW);

	ShowWindow(hWndWallpaper, SW_SHOW);
	UpdateWindow(hWndWallpaper);
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


/*
*  Cleanup before quitting
*/
void CleanUp() {

	if (::imgBackground != NULL) delete ::imgBackground;

	if (::hWndWallpaper) {
		SetParent(::hWndWallpaper, GetDesktopWindow());
		ShowWindow(::hWndWallpaper, SW_HIDE);
		UpdateWindow(::hWndWallpaper);
		DestroyWindow(::hWndWallpaper);
	}
	
	if (::hWndForeGround) DestroyWindow(::hWndForeGround);

	if (gdiplusToken) {
		Gdiplus::GdiplusShutdown(gdiplusToken);
	}
}

/*
*   Main application message loop
*/
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

/*
*   Handle WM_PAINT event from wallpaper window
*/
void OnPaint(HWND hWnd) {

	HDC hdc = GetDC(hWnd);
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
	// Draw background image or fill background color into each intersect rectangle with monitor, a.k.a draw on each monitor
	//
	RECT rIntersect;
	for (RECT rMonitor : *recMonitors) {
		
		if (IntersectRect(&rIntersect, &rMonitor, &vScreen)) {

			if (::imgBackground == NULL) {
				Gdiplus::SolidBrush brush(::colorBackground);
				
				Gdiplus::Rect rect(
					(INT)rIntersect.left, (INT)rIntersect.top,
					(INT)(rIntersect.right - rIntersect.left), (INT)(rIntersect.bottom - rIntersect.top));
				graphics->FillRectangle(&brush, rect);
			} 
			else {
				graphics->DrawImage(::imgBackground, (INT)rIntersect.left, (INT)rIntersect.top,
					(INT)(rIntersect.right - rIntersect.left), (INT)(rIntersect.bottom - rIntersect.top));
			}
			
		}
	}

	delete graphics;
}

void doDebug() {

}

void printUsage() {
	cout << "Usage: " << endl; 
	cout << "\n(A) Set solid background RGB color" << endl;
	cout << "Syntax: " << endl; 
	cout << "  wallpaper -c Red Greed Blue" << endl;
	cout << "Example: " << endl;
	cout << "  wallpaper -c 128 64 64" << endl;
	cout << "\n(B) Set an image as background" << endl;
	cout << "Syntax:  wallpaper path/to/your/image" << endl;
	cout << "Example: " << endl;
	cout << "  wallpaper c:\\images\\bg.jpg" << endl;

	cout << "\n(C) Kill existing wallpaper" << endl;
	cout << "Syntax: " << endl;
	cout << "  wallpaper -k" << endl;
}
void printAbout() {
	cout << "Small utility to set wallpaper in case you cannot change your wallpaper using Windows personalize settings" << endl;
	cout << "CREDIT TO: https://github.com/Francesco149/weebp" << endl << endl;
	printUsage();
}

void printError() {
	err(L"ERROR: Invalid number of arguments or invalid argument\n\n");
	printUsage();
}

/*
*   Kill previous wallpaper (i.e. -k option)
*/
void KillPrev() {

	HWND hWndWallpaper = FindMe();
	if (hWndWallpaper) {
		HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, MUTEX_NAME);
		if (!hMutex) {
			err(L"FATAL: Mutex %s is NULL!\n", MUTEX_NAME);
			return;
		}

		PostMessage(hWndWallpaper, WM_KILL_MESSAGE, 0, 0);

		CloseHandle(hMutex);

		//log(L"Killed!\n");
	}
	else
	{
		err(L"Wallpaper window not found.\n ");
	}
}

void HandleSetColorBG(int argc, char* argv[]) {
	if (FindMe()) {
		KillPrev();
	}

	InitGdi();
	::colorBackground = Gdiplus::Color(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
	::hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);

	CreateWindows(::hWndWallpaper, ::hWndForeGround);

	Run();
}

void HandleSetImageBG(int argc, char* argv[]) {

	if (FindMe()) {
		KillPrev();
	}

	::hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);
	
	char* imagePath = argv[1];
	std::filesystem::path filePath(imagePath);
	if (!std::filesystem::exists(imagePath)) {
		err(L"FATAL: Image file cannot be found: %s\n", ToWSTR(imagePath));
		return;
	}

	InitGdi();
	::imgPathBackground = ToWSTR(imagePath);

	::imgBackground = new Bitmap(::imgPathBackground);
	if (::imgBackground == NULL) {
		err(L"FATAL: Failed to load image file %s \n Is it a valid image?", ::imgPathBackground);
		return;
	}

	CreateWindows(::hWndWallpaper, ::hWndForeGround);

	Run();
}

int main(int argc, char* argv[]) {
	if (argc == 1) {
		printAbout();
		return 0;
	}

	SetProcessDPIAware();

	if (argc == 5) {
		if (strcmp(argv[1], "-c")==0) {

			HandleSetColorBG(argc, argv);
			return 0;
		}
		else
		{
			printError();
			return 1;
		}
	}

	if (argc == 2) {
		if (strcmp(argv[1], "-d") == 0) {
			doDebug();
			return 0;
		}

		if (strcmp(argv[1], "-k") == 0) {
			KillPrev();
			return 0;
		}

		HandleSetImageBG(argc, argv);
		return 0;
	}

	printError();
	return 1;

}