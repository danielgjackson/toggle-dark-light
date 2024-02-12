// Toggle Dark/Light Mode -- icon in the taskbar notification area
// Dan Jackson, 2021-2024.

#define _WIN32_WINNT 0x0601
#define _CRT_SECURE_NO_WARNINGS  // TODO: Use more secure versions
#include <windows.h>
#include <tchar.h>

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include <io.h>

#include <rpc.h>
#include <dbt.h>

// commctrl v6 for LoadIconMetric()
#include <commctrl.h>

// Notify icons
#include <shellapi.h>

// MSC-Specific Pragmas
#ifdef _MSC_VER
// Moved to external .manifest file linked through .rc file
//#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib") 	// InitCommonControlsEx(), LoadIconMetric()
#pragma comment(lib, "shell32.lib")  	// Shell_NotifyIcon()
#pragma comment(lib, "advapi32.lib") 	// RegOpenKeyEx(), RegSetValueEx(), RegGetValue(), RegDeleteValue(), RegCloseKey()
#pragma comment(lib, "Comdlg32.lib") 	// GetSaveFileName()
#pragma comment(lib, "Version.lib")		// GetFileVersionInfoSize(), GetFileVersionInfo(), VerQueryValue()
#pragma comment(lib, "gdi32.lib")		// CreateFontIndirect()
#pragma comment(lib, "User32.lib")		// Windows
#pragma comment(lib, "ole32.lib")		// CoInitialize(), etc.
// #pragma comment(lib, "psapi.lib")		// GetModuleFileName() // -lpsapi
#endif

// Defines
#define TITLE TEXT("Toggle Dark/Light Mode")
#define TITLE_SAFE TEXT("Toggle Dark-Light Mode")
#define HOT_KEY_ID		0x0000
#define WMAPP_NOTIFYCALLBACK (WM_APP + 1)
#define IDM_TOGGLE		101
#define IDM_AUTOSTART	102
#define IDM_DEBUG		103
#define IDM_ABOUT		104
#define IDM_EXIT		105

// Hacky global state
HINSTANCE ghInstance = NULL;
HWND ghWndMain = NULL;
UINT idMinimizeIcon = 1;
BOOL gbHasNotifyIcon = FALSE;
BOOL gbAutoStart = FALSE;
BOOL gbNotify = TRUE;
BOOL gbPortable = FALSE;
BOOL gbAllowDuplicate = FALSE;
BOOL gbSubsystemWindows = FALSE;
BOOL gbHasConsole = FALSE;
BOOL gbImmediatelyExit = FALSE;	// Quit right away (mainly for testing)
BOOL gbSetDark = FALSE;
BOOL gbSetLight = FALSE;
BOOL gbQuery = FALSE;
BOOL gbExiting = FALSE;
HANDLE ghStartEvent = NULL;		// Event for single instance
int gVersion[4] = { 0, 0, 0, 0 };

NOTIFYICONDATA nid = {0};

// Redirect standard I/O to a console
static BOOL RedirectIOToConsole(BOOL tryAttach, BOOL createIfRequired)
{
	BOOL hasConsole = FALSE;
	if (tryAttach && AttachConsole(ATTACH_PARENT_PROCESS))
	{
		hasConsole = TRUE;
	}
	if (createIfRequired && !hasConsole)
	{
		AllocConsole();
		AttachConsole(GetCurrentProcessId());
		hasConsole = TRUE;
	}
	if (hasConsole)
	{
		freopen("CONIN$", "r", stdin);
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
	}
	return hasConsole;
}

void DeleteNotificationIcon(void)
{
	if (gbHasNotifyIcon)
	{
		nid.uFlags = 0;
		memset(&nid.guidItem, 0, sizeof(nid.guidItem));
		nid.uID = idMinimizeIcon;
		Shell_NotifyIcon(NIM_DELETE, &nid);
		gbHasNotifyIcon = FALSE;
	}
}

BOOL AddNotificationIcon(HWND hwnd)
{
	DeleteNotificationIcon();
	
	memset(&nid, 0, sizeof(nid));
	nid.cbSize = sizeof(nid);
	nid.hWnd = hwnd;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
	nid.uFlags |= NIS_HIDDEN;
	nid.dwStateMask = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
	if (gbNotify)
	{
		nid.uFlags |= NIF_INFO | NIF_REALTIME;
		_tcscpy(nid.szInfoTitle, TITLE);
		_tcscpy(nid.szInfo, TEXT("Running in notification area."));
		nid.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
		nid.hBalloonIcon = LoadIcon(ghInstance, TEXT("MAINICON"));
		nid.dwInfoFlags |= NIIF_LARGE_ICON | NIIF_USER;
	}
	memset(&nid.guidItem, 0, sizeof(nid.guidItem));
	nid.uID = idMinimizeIcon;
	nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;
	nid.hIcon = NULL;
	LoadIconMetric(ghInstance, L"MAINICON", LIM_SMALL, &nid.hIcon);	// MAKEINTRESOURCE(IDI_APPLICATION)
	_tcscpy(nid.szTip, TITLE);
	Shell_NotifyIcon(NIM_ADD, &nid);
	nid.uVersion = NOTIFYICON_VERSION_4;
	Shell_NotifyIcon(NIM_SETVERSION, &nid);
	gbHasNotifyIcon = TRUE;
	return gbHasNotifyIcon;
}

BOOL Notify(TCHAR *message)
{
	if (!gbHasNotifyIcon) return FALSE;
	nid.uFlags |= NIF_INFO | NIF_REALTIME;
	_tcscpy(nid.szInfoTitle, TITLE);
	_tcscpy(nid.szInfo, message);
	nid.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
	nid.hBalloonIcon = LoadIcon(ghInstance, TEXT("MAINICON"));
	nid.dwInfoFlags |= NIIF_LARGE_ICON | NIIF_USER;
	Shell_NotifyIcon(NIM_MODIFY, &nid);
	return TRUE;
}

bool AutoStart(bool change, bool startup)
{
	bool retVal = false;
	HKEY hKeyMain = HKEY_CURRENT_USER;
	TCHAR *subKey = TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");
	TCHAR *value = TITLE_SAFE;

	TCHAR szModuleFileName[MAX_PATH];
	GetModuleFileName(NULL, szModuleFileName, sizeof(szModuleFileName) / sizeof(szModuleFileName[0]));

	TCHAR szQuotedFileName[MAX_PATH + 2];
	_sntprintf(szQuotedFileName, sizeof(szQuotedFileName) / sizeof(szQuotedFileName[0]), TEXT("\"%Ts\""), szModuleFileName);

	TCHAR szAutoStartValue[MAX_PATH + 32];
	_sntprintf(szAutoStartValue, sizeof(szAutoStartValue) / sizeof(szAutoStartValue[0]), TEXT("%Ts /AUTOSTART"), szQuotedFileName);

	if (change)
	{
		// Set the key
		HKEY hKey = NULL;
		LSTATUS lErrorCode = RegOpenKeyEx(hKeyMain, subKey, 0, KEY_SET_VALUE, &hKey);
		if (lErrorCode == ERROR_SUCCESS)
		{
			if (startup)
			{
				// Setting to auto-start
				lErrorCode = RegSetValueEx(hKey, value, 0, REG_SZ, (const BYTE *)szAutoStartValue, (DWORD)((_tcslen(szAutoStartValue) + 1) * sizeof(TCHAR)));
				if (lErrorCode == ERROR_SUCCESS)
				{
					retVal = true;
				}
			}
			else
			{
				// Setting to not auto-start
				lErrorCode = RegDeleteValue(hKey, value);
				if (lErrorCode == ERROR_SUCCESS)
				{
					retVal = true;
				}
			}
			RegCloseKey(hKey);
		}
	}
	else
	{
		// Query that the key is set and has the correct value prefix
		TCHAR szData[MAX_PATH];
		DWORD cbData = sizeof(szData);
		LSTATUS lErrorCode = RegGetValue(hKeyMain, subKey, value, RRF_RT_REG_SZ, NULL, &szData, &cbData);
		if (lErrorCode == ERROR_SUCCESS && _tcsncmp(szData, szQuotedFileName, _tcslen(szQuotedFileName)) == 0) {
			retVal = true;
		}
	}
	return retVal;
}


void ShowContextMenu(HWND hwnd, POINT pt)
{
	UINT autoStartFlags;
	// Don't touch the registry if running as a portable app
	if (!gbPortable) {
		autoStartFlags = AutoStart(false, false) ? MF_CHECKED : MF_UNCHECKED;
	} else {
		autoStartFlags = MF_UNCHECKED | MF_GRAYED;
	}

	HMENU hMenu = CreatePopupMenu();
	AppendMenu(hMenu, MF_STRING, IDM_TOGGLE, TEXT("&Toggle Dark/Light Mode\tWin+Shift+D"));
	AppendMenu(hMenu, MF_SEPARATOR, 0, TEXT("Separator"));
	AppendMenu(hMenu, MF_STRING | autoStartFlags, IDM_AUTOSTART, TEXT("Auto-&Start"));
	AppendMenu(hMenu, MF_STRING, IDM_DEBUG, TEXT("Save &Debug Info..."));
	AppendMenu(hMenu, MF_STRING, IDM_ABOUT, TEXT("&About"));
	AppendMenu(hMenu, MF_SEPARATOR, 0, TEXT("Separator"));
	AppendMenu(hMenu, MF_STRING, IDM_EXIT, TEXT("E&xit"));
	SetMenuDefaultItem(hMenu, IDM_TOGGLE, FALSE);
	SetForegroundWindow(hwnd);
	UINT uFlags = TPM_RIGHTBUTTON;
	if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
	{
		uFlags |= TPM_RIGHTALIGN;
	}
	else
	{
		uFlags |= TPM_LEFTALIGN;
	}
	SetForegroundWindow(hwnd); 	// for correct popup tracking
	TrackPopupMenuEx(hMenu, uFlags, pt.x, pt.y, hwnd, NULL);
	DestroyMenu(hMenu);
}

BOOL HasExistingInstance(void)
{
	if (ghStartEvent)
	{
		CloseHandle(ghStartEvent);
		ghStartEvent = NULL;
	}
	ghStartEvent = CreateEvent(NULL, FALSE, FALSE, TEXT("Local\\toggle"));	// Session namespace is ok (does not need to be Global)
	DWORD lastError = GetLastError();
	if (ghStartEvent && !lastError)
	{
		// We are the first instance (hold on to the event, it will only be released when the process ends)
		_ftprintf(stderr, TEXT("INSTANCE: No other instance found.\n"));
		return FALSE;
	}
	else if (ghStartEvent && lastError == ERROR_ALREADY_EXISTS)
	{
		// There is another instance running
		_ftprintf(stderr, TEXT("INSTANCE: Another instance found.\n"));
		//CloseHandle(ghStartEvent);
		//ghStartEvent = NULL;
		return TRUE;
	}
	else
	{
		// Otherwise, fail safe and assume not already running
		_ftprintf(stderr, TEXT("INSTANCE: Problem finding instance.\n"));
		return FALSE;
	}
}

HFONT hDlgFont = NULL;
bool windowOpen = false;

void PositionWindow(int x, int y)
{
	RECT rect = {0};
	GetWindowRect(ghWndMain, &rect);
	SIZE windowSize;
	windowSize.cx = rect.right - rect.left;
	windowSize.cy = rect.bottom - rect.top;

	// Fallback to (0,0)
	RECT rectSpace = {0};

	// Prefer desktop window
	GetWindowRect(GetDesktopWindow(), &rectSpace);

	// Prefer the work area
	SystemParametersInfo(SPI_GETWORKAREA, 0, &rectSpace, 0);

	// Default to the lower-right corner
	rect.left = 0;
	rect.top = 0;
	if (rectSpace.right - rectSpace.left > 0) rect.left = rectSpace.right - windowSize.cx;
	if (rectSpace.bottom - rectSpace.top > 0) rect.top = rectSpace.bottom - windowSize.cy;
	rect.top = rectSpace.bottom - windowSize.cy;
	rect.right = rect.left + windowSize.cx;
	rect.bottom = rect.top + windowSize.cy;

	// Prefer a calculated position
	POINT anchorPoint;
	anchorPoint.x = x;
	anchorPoint.y = y;
	CalculatePopupWindowPosition(&anchorPoint, &windowSize, TPM_CENTERALIGN | TPM_BOTTOMALIGN | TPM_VERTICAL | TPM_WORKAREA, NULL, &rect);

	AdjustWindowRect(&rect, GetWindowLong(ghWndMain, GWL_STYLE), FALSE);
	SetWindowPos(ghWndMain, 0, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

void OpenWindow(int x, int y)
{
	// PositionWindow(x, y);
	// ShowWindow(ghWndMain, SW_SHOW);
	// SetForegroundWindow(ghWndMain);
	// windowOpen = true;
}

void HideWindow(void)
{
	ShowWindow(ghWndMain, SW_HIDE);
	windowOpen = false;
}

void StartExit(void)
{
	gbExiting = TRUE;
	if (ghStartEvent)
	{
		CloseHandle(ghStartEvent);
		ghStartEvent = NULL;
	}
	PostMessage(ghWndMain, WM_CLOSE, 0, 0);
}

void Startup(HWND hWnd)
{
	_tprintf(TEXT("Startup...\n"));

	ghWndMain = hWnd;

	BOOL duplicateInstance = FALSE;
	int response = 0;
	if (!gbAllowDuplicate)
	{
		do
		{
			if (gbImmediatelyExit) break;	// Allow duplicates if immediately exiting
			duplicateInstance = HasExistingInstance();
			if (!duplicateInstance) break;
			response = MessageBox(NULL, TEXT("Toggle Dark/Light Mode is already running in the notification area.\r\nContinue with a new instance?"), TITLE, MB_ICONWARNING | MB_CANCELTRYCONTINUE | MB_DEFBUTTON1);
			if (response == IDCANCEL)
			{
				gbNotify = FALSE;
				gbImmediatelyExit = TRUE;
			}

			if (response == IDCONTINUE)
			{
				gbAllowDuplicate = TRUE;
				break;
			}
		} while (response == IDTRYAGAIN || response == IDCONTINUE);
	}

	if (response != IDCANCEL)
	{
		if (!gbImmediatelyExit)
		{
			AddNotificationIcon(ghWndMain);
		}
	}

	if (gbImmediatelyExit) StartExit();
}

void Shutdown(void)
{
	_tprintf(TEXT("Shutdown()...\n"));
	DeleteNotificationIcon();
	_tprintf(TEXT("...END: Shutdown()\n"));
}

int IsLight(void)
{
	HKEY hKeyMain = HKEY_CURRENT_USER;
	const TCHAR *subKey = TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize");
	const TCHAR *value = TEXT("SystemUsesLightTheme");
	
	DWORD dwData = (DWORD)-1;
	
	HKEY hKey = NULL;
	LSTATUS lErrorCode = RegOpenKeyEx(hKeyMain, subKey, 0, KEY_NOTIFY, &hKey);
	if (lErrorCode != ERROR_SUCCESS)
	{
		_tprintf(TEXT("ERROR: RegOpenKeyEx() failed (%d): %ls\n"), lErrorCode, subKey);
		return -1;
	}
	
	// Query current value
	DWORD dwFlags = RRF_RT_REG_DWORD;
	DWORD cbData = sizeof(dwData);
	lErrorCode = RegGetValue(hKeyMain, subKey, value, dwFlags, NULL, &dwData, &cbData);
	RegCloseKey(hKey);
	if (lErrorCode != ERROR_SUCCESS)
	{
		_tprintf(TEXT("ERROR: RegGetValue() failed (%d): %ls\n"), lErrorCode, subKey);
		return (DWORD)-1;
	}
	
	if (dwData == 0 || dwData == 1) return dwData;	// 1=light, 0=dark
	return -1;	// error
}

// Data for sending messages to specific windows
typedef struct {
	UINT msg;
	WPARAM wParam;
	LPARAM lParam;
	TCHAR *className;
	TCHAR *classNameAlt;
	UINT count;
} send_message_t;

BOOL CALLBACK SendEnumWindowsProc(HWND hWnd, LPARAM lParam) 
{
	send_message_t *msgData = (send_message_t *)lParam;
	if (!hWnd) return TRUE;

	// Filter by class name
	if (msgData->className != NULL || msgData->classNameAlt != NULL)
	{
		TCHAR szClassName[256];
		GetClassName(hWnd, szClassName, sizeof(szClassName) / sizeof(szClassName[0]));

		BOOL match = false;
		if (msgData->className != NULL && _tcscmp(szClassName, msgData->className) == 0) match = true;
		if (msgData->classNameAlt != NULL && _tcscmp(szClassName, msgData->classNameAlt) == 0) match = true;

		if (!match) return TRUE;
	}

	SendMessage(hWnd, msgData->msg, msgData->wParam, msgData->lParam);
	//PostMessage(hWnd, msgData->msg, msgData->wParam, msgData->lParam);
	msgData->count++;
	return TRUE;
}

// Send a message just to non-primary display taskbars (which seem a bit troublesome...)
UINT SendToTaskbar(UINT msg, WPARAM wParam, LPARAM lParam)
{
	send_message_t msgData = {0};
	msgData.msg = msg;
	msgData.wParam = wParam;
	msgData.lParam = lParam;
	msgData.className = (TCHAR *)&TEXT("Shell_TrayWnd");
	msgData.classNameAlt = (TCHAR *)&TEXT("Shell_SecondaryTrayWnd");
	msgData.count = 0;
	BOOL result = EnumWindows(SendEnumWindowsProc, (LPARAM)&msgData);
	if (!result) return 0;
	return (UINT)msgData.count;
}

BOOL SetLightDark(DWORD light)
{
	HKEY hKeyMain = HKEY_CURRENT_USER;
	const TCHAR *subKey = TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize");
	const TCHAR *valueSystem = TEXT("SystemUsesLightTheme");
	const TCHAR *valueApps = TEXT("AppsUseLightTheme");
	const TCHAR *valueColorPrevalence = TEXT("ColorPrevalence");
	
	HKEY hKey = NULL;
	LSTATUS lErrorCode = RegOpenKeyEx(hKeyMain, subKey, 0, KEY_SET_VALUE, &hKey);
	if (lErrorCode != ERROR_SUCCESS)
	{
		_tprintf(TEXT("ERROR: RegOpenKeyEx() failed (%d): %ls\n"), lErrorCode, subKey);
		return FALSE;
	}

	// Set the system key
	lErrorCode = RegSetValueEx(hKey, valueSystem, 0, REG_DWORD, (const BYTE *)&light, sizeof(light));
	if (lErrorCode != ERROR_SUCCESS)
	{
		_tprintf(TEXT("ERROR: RegSetValueEx() failed (%d): %ls - %ls\n"), lErrorCode, subKey, valueSystem);
		RegCloseKey(hKey);
		return FALSE;
	}
	
	// Set the apps key
	lErrorCode = RegSetValueEx(hKey, valueApps, 0, REG_DWORD, (const BYTE *)&light, sizeof(light));
	if (lErrorCode != ERROR_SUCCESS)
	{
		_tprintf(TEXT("ERROR: RegSetValueEx() failed (%d): %ls - %ls\n"), lErrorCode, subKey, valueApps);
		RegCloseKey(hKey);
		return FALSE;
	}

	// Set the ColorPrevalence key
	DWORD colorPrevalence = 0;		// HACK: Force off for now as we don't know the previous state (TODO: Option to set this value in dark mode)
	if (light) colorPrevalence = 0;	// Always off in light mode?
	lErrorCode = RegSetValueEx(hKey, valueColorPrevalence, 0, REG_DWORD, (const BYTE *)&colorPrevalence, sizeof(colorPrevalence));
	if (lErrorCode != ERROR_SUCCESS)
	{
		_tprintf(TEXT("ERROR: RegSetValueEx() failed (%d): %ls - %ls\n"), lErrorCode, subKey, valueColorPrevalence);
		RegCloseKey(hKey);
		return FALSE;
	}

	RegCloseKey(hKey);

	// Delay -- this may to help update taskbars on other displays?
	//Sleep(500);
	
	// Send WM_SETTINGCHANGE message directly to primary and secondary taskbars
	UINT countSC = SendToTaskbar(WM_SETTINGCHANGE, 0, (LPARAM)TEXT("ImmersiveColorSet"));
	_tprintf(TEXT("SendToTaskbar(WM_SETTINGCHANGE): %d\n"), countSC);

	// Broadcast WM_SETTINGCHANGE using SendMessageTimeout with lParam set to "ImmersiveColorSet" -- required by some apps such as explorer.exe (File Explorer)
	{
		HWND hWnd = HWND_BROADCAST;
		UINT msg = WM_SETTINGCHANGE;
		WPARAM wParam = 0;
		LPARAM lParam = (LPARAM)TEXT("ImmersiveColorSet");
		SendMessageTimeout(hWnd, msg, wParam, lParam, SMTO_ABORTIFHUNG, 5000, NULL);
	}

	// Broadcast a second WM_SETTINGCHANGE using SendMessageTimeout with lParam set to "ImmersiveColorSet" -- this second broadcast fixes Task Manager
	{
		HWND hWnd = HWND_BROADCAST;
		UINT msg = WM_SETTINGCHANGE;
		WPARAM wParam = 0;
		LPARAM lParam = (LPARAM)TEXT("ImmersiveColorSet");
		SendMessageTimeout(hWnd, msg, wParam, lParam, SMTO_ABORTIFHUNG, 5000, NULL);
	}

	// // Broadcast WM_THEMECHANGED
	// {
	// 	HWND hWnd = HWND_BROADCAST;
	// 	UINT msg = WM_THEMECHANGED;
	// 	WPARAM wParam = 0;
	// 	LPARAM lParam = 0;
	// 	SendMessageTimeout(hWnd, msg, wParam, lParam, SMTO_ABORTIFHUNG, 5000, NULL);
	// }

	// // Broadcast WM_SYSCOLORCHANGE
	// {
	// 	HWND hWnd = HWND_BROADCAST;
	// 	UINT msg = WM_SYSCOLORCHANGE;
	// 	WPARAM wParam = 0;
	// 	LPARAM lParam = 0;
	// 	SendMessageTimeout(hWnd, msg, wParam, lParam, SMTO_ABORTIFHUNG, 5000, NULL);
	// }

	// WM_DWMCOMPOSITIONCHANGED


	// TODO: If occasional File Explorer glitches remain, consider these:

	// #include <shlobj.h>
	
	// SHCNE_ASSOCCHANGED (SHCNF_IDLIST=0x00000000)
	//SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL); 

	// SHCNE_ALLEVENTS
	//SHChangeNotify(SHCNE_ALLEVENTS, 0, NULL, NULL); 

	// // Broadcast WM_PAINT
	// {
	// 	long res = BroadcastSystemMessageEx(BSF_POSTMESSAGE, NULL, WM_PAINT, 0, 0, NULL); // BSF_POSTMESSAGE / BSF_SENDNOTIFYMESSAGE
	// 	if (res <= 0) {
	// 		_tprintf(TEXT("ERROR: BroadcastSystemMessageEx() WM_PAINT failed (0x%x / 0x%x)\n"), res, GetLastError());
	// 	}
	// }

	return TRUE;
}

void ToggleDarkLight(BOOL notify)
{
	_tprintf(TEXT("ToggleDarkLight()...\n"));
	int isLight = IsLight();
	if (isLight == 0)		// dark
	{
		_tprintf(TEXT("...isLight=FALSE, setting: light\n"));
		if (notify) Notify(TEXT("Toggle to light mode."));
		SetLightDark(1);
	}
	else if (isLight == 1)	// light
	{
		_tprintf(TEXT("...isLight=TRUE, setting: dark\n"));
		if (notify) Notify(TEXT("Toggle to dark mode."));
		SetLightDark(0);
	}
	else
	{
		_tprintf(TEXT("...isLight=error\n"));
	}
	_tprintf(TEXT("...END: ToggleDarkLight()\n"));
}

void DumpDebug(FILE *file)
{
	 _ftprintf(file, TEXT("IsLight=%d\n"), IsLight());
}

// Open hyperlink from TaskDialog
HRESULT CALLBACK TaskDialogCallback(HWND hwnd, UINT uNotification, WPARAM wParam, LPARAM lParam, LONG_PTR dwRefData)
{
	if (uNotification == TDN_HYPERLINK_CLICKED)
	{
		ShellExecute(hwnd, TEXT("open"), (TCHAR *)lParam, NULL, NULL, SW_SHOW);
	}
	return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		Startup(hwnd);
		break;

	case WM_ACTIVATE:
		{
			if (wParam == WA_INACTIVE)
			{
				HideWindow();
			}
		}
		break;

	// Handled pre-translate message
	// case WM_KEYDOWN:
	// 	if (wParam == VK_ESCAPE)
	// 	{
	// 		HideWindow();
	// 	}
	// 	break;		

	case WM_CTLCOLORSTATIC:
	{
		//HDC hdcStatic = (HDC)wParam;
		//SetTextColor(hdcStatic, RGB(255,255,255));
		//SetBkColor(hdcStatic, RGB(0,0,0));
		return (INT_PTR)GetSysColorBrush(COLOR_WINDOW); // CreateSolidBrush(RGB(0,0,0));
	}
	break;

	case WM_HOTKEY:
	{
		if (wParam == HOT_KEY_ID)
		{
			ToggleDarkLight(TRUE);

			// Remove any repeated WM_HOTKEY messages from the window queue
			MSG m;
			while (PeekMessage(&m, hwnd, WM_HOTKEY, WM_HOTKEY, PM_REMOVE)) { ; }

			return 0;
		}
	}


	case WM_COMMAND:
		{
			int const wmId = LOWORD(wParam);
			switch (wmId)
			{
			case IDM_TOGGLE:
				{
					// POINT mouse;
					// GetCursorPos(&mouse);
					// OpenWindow(mouse.x, mouse.y);
					ToggleDarkLight(TRUE);
				}
				break;
			case IDM_AUTOSTART:
				if (AutoStart(false, false))
				{
					// Is auto-starting, remove
					AutoStart(true, false);
				}
				else
				{
					// Is not auto-starting, add
					AutoStart(true, true);
				}
				break;
			case IDM_DEBUG:
				{
					// Initial filename from local date/time
					TCHAR szFileName[MAX_PATH] = TEXT("");
					time_t now;
					time(&now);
					struct tm *local = localtime(&now);
					_sntprintf(szFileName, sizeof(szFileName) / sizeof(szFileName[0]), TEXT("" TITLE_SAFE "_%04d-%02d-%02d_%02d-%02d-%02d.txt"), local->tm_year + 1900, local->tm_mon + 1, local->tm_mday, local->tm_hour, local->tm_min, local->tm_sec);

					OPENFILENAME openFilename = {0};
					openFilename.lStructSize = sizeof(openFilename);
					openFilename.hwndOwner = hwnd;
					openFilename.lpstrFilter = TEXT("Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0");
					openFilename.lpstrFile = szFileName;
					openFilename.nMaxFile = sizeof(szFileName) / sizeof(*szFileName);
					openFilename.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;
					openFilename.lpstrDefExt = (LPCWSTR)L"txt";
					openFilename.lpstrTitle = TITLE;

					// Write file to temp folder
					TCHAR szInitialDir[MAX_PATH] = TEXT("");
					GetTempPath(sizeof(szInitialDir), szInitialDir);
					openFilename.lpstrInitialDir = szInitialDir[0] ? szInitialDir : NULL;

					if (GetSaveFileName(&openFilename))
					{
						FILE *file = _tfopen(openFilename.lpstrFile, TEXT("w"));
						if (file)
						{
							DumpDebug(file);
							fclose(file);
							ShellExecute(NULL, TEXT("open"), openFilename.lpstrFile, NULL, NULL, SW_SHOW);
						}
					}
				}
				break;
			case IDM_ABOUT:
				{
					TCHAR *szTitle = TEXT("About " TITLE);
					TCHAR szHeader[128] = TEXT("");
					_sntprintf(szHeader, sizeof(szHeader) / sizeof(szHeader[0]), TEXT("%s V%d.%d.%d"), TITLE, gVersion[0], gVersion[1], gVersion[2]);
					TCHAR *szContent = TEXT("Toggle dark/light mode.\nKeyboard shortcut: Win+Shift+D");
					//TCHAR *szExtraInfo = TEXT("...");
					TCHAR *szFooter = TEXT("Open source under <a href=\"https://github.com/danielgjackson/toggle-dark-light/blob/master/LICENSE.txt\">MIT License</a>, \u00A92021 Daniel Jackson.");
					TASKDIALOG_BUTTON aCustomButtons[] = {
						{ 1001, L"Project page\ngithub.com/danielgjackson/toggle-dark-light" },
						{ 1002, L"Check for updates\nSee the latest release" },
					};
					TASKDIALOGCONFIG tdc = {0};
					tdc.cbSize = sizeof(tdc);
					tdc.hwndParent = ghWndMain;
					tdc.dwFlags = TDF_USE_HICON_MAIN | TDF_USE_COMMAND_LINKS | TDF_ENABLE_HYPERLINKS | TDF_EXPANDED_BY_DEFAULT | TDF_EXPAND_FOOTER_AREA | TDF_ALLOW_DIALOG_CANCELLATION;
					tdc.pButtons = aCustomButtons;
					tdc.cButtons = sizeof(aCustomButtons) / sizeof(aCustomButtons[0]);
					tdc.pszWindowTitle = szTitle;
					tdc.nDefaultButton = IDOK;
					tdc.hMainIcon = LoadIcon(ghInstance, TEXT("MAINICON"));
					tdc.pszMainInstruction = szHeader;
					tdc.pszContent = szContent;
					//tdc.pszExpandedInformation = szExtraInfo;
					tdc.pszFooter = szFooter;
					tdc.pszFooterIcon = TD_INFORMATION_ICON;
					tdc.dwCommonButtons = TDCBF_OK_BUTTON;
					tdc.pfCallback = TaskDialogCallback;
					tdc.lpCallbackData = (LONG_PTR)NULL;	// no context needed
					int nClickedBtn;
					HRESULT hr = TaskDialogIndirect(&tdc, &nClickedBtn, NULL, NULL);	// nClickedBtn == IDOK
					if (SUCCEEDED(hr))
					{
						if (nClickedBtn == 1001)
						{
							ShellExecute(hwnd, TEXT("open"), TEXT("https://github.com/danielgjackson/toggle-dark-light/#readme"), NULL, NULL, SW_SHOW);
						}
						else if (nClickedBtn == 1002)
						{
							ShellExecute(hwnd, TEXT("open"), TEXT("https://github.com/danielgjackson/toggle-dark-light/releases/latest"), NULL, NULL, SW_SHOW);
						}
					}
				}
				break;
			case IDM_EXIT:
				StartExit();
				break;
			default:
				return DefWindowProc(hwnd, message, wParam, lParam);
			}
		}
		break;

	case WMAPP_NOTIFYCALLBACK:
		switch (LOWORD(lParam))
		{
		case NIN_SELECT:
			{
				// POINT mouse;
				// mouse.x = LOWORD(wParam);
				// mouse.y = HIWORD(wParam);
				// OpenWindow(mouse.x, mouse.y);
				ToggleDarkLight(TRUE);
			}
			break;

		case WM_RBUTTONUP:		// If not using NOTIFYICON_VERSION_4 ?
		case WM_CONTEXTMENU:
			{
				POINT const pt = { LOWORD(wParam), HIWORD(wParam) };
				ShowContextMenu(hwnd, pt);
			}
			break;
		}
		break;

	case WM_ENDSESSION:
		StartExit();
		break;

	case WM_CLOSE:
		if (gbExiting && hwnd == ghWndMain)
		{
			DestroyWindow(hwnd);
		}
		else
		{
			ShowWindow(hwnd, SW_HIDE);
		}
		break;

	case WM_DESTROY:
		Shutdown();
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}

void done(EXCEPTION_POINTERS *exceptionInfo)
{
	if (exceptionInfo)
	{
		TCHAR msg[512] = TEXT("");
		_sntprintf(msg, sizeof(msg) / sizeof(msg[0]), TEXT("ERROR: An unhandled error has occurred."));
		// [/CONSOLE:<ATTACH|CREATE|ATTACH-CREATE>]*  (* only as first parameter)
		if (gbHasConsole)
		{
			_tprintf(msg);
		}
		MessageBox(NULL, msg, TITLE, MB_OK | MB_ICONERROR);
	}
	_tprintf(TEXT("DONE.\n"));
}

void finish(void)
{
	_tprintf(TEXT("END: Application ended.\n"));
	done(NULL);
}

LONG WINAPI UnhandledException(EXCEPTION_POINTERS *exceptionInfo)
{
	_tprintf(TEXT("END: Unhandled exception.\n"));
	done(exceptionInfo);
	return EXCEPTION_EXECUTE_HANDLER; // EXCEPTION_EXECUTE_HANDLER; // EXCEPTION_CONTINUE_SEARCH;
}

void SignalHandler(int signal)
{
	_tprintf(TEXT("END: Received signal: %d\n"), signal);
	//if (signal == SIGABRT)
	done(NULL);
}

BOOL WINAPI consoleHandler(DWORD signal)
{
	_tprintf(TEXT("END: Received console control event: %u\n"), (unsigned int)signal);
	//if (signal == CTRL_C_EVENT)
	done(NULL);
	return FALSE;
}

int run(int argc, TCHAR *argv[], HINSTANCE hInstance, BOOL hasConsole)
{
	_ftprintf(stderr, TEXT("run()\n"));

	ghInstance = hInstance;
	gbHasConsole = hasConsole;

	if (gbHasConsole)
	{
		SetConsoleCtrlHandler(consoleHandler, TRUE);
	}
	signal(SIGABRT, SignalHandler);
	SetUnhandledExceptionFilter(UnhandledException);
	atexit(finish);

	ghInstance = hInstance;

	// Get module filename
	TCHAR szModuleFileName[MAX_PATH] = {0};
	GetModuleFileName(NULL, szModuleFileName, sizeof(szModuleFileName) / sizeof(szModuleFileName[0]));

	// Detect if the executable is named as a "PortableApps"-compatible ".paf.exe" -- ensure no registry changes, no check for updates, etc.
	if (_tcslen(szModuleFileName) > 8)
	{
		if (_tcsicmp(szModuleFileName + _tcslen(szModuleFileName) - 8, TEXT(".paf.exe")) == 0)
		{
			gbPortable = TRUE;
		}
	}

	// Get module version
	DWORD dwVerHandle = 0;
	DWORD dwVerLen = GetFileVersionInfoSize(szModuleFileName, &dwVerHandle);
	if (dwVerLen > 0)
	{
		LPVOID verData = malloc(dwVerLen);
		if (GetFileVersionInfo(szModuleFileName, dwVerHandle, dwVerLen, verData))
		{
			VS_FIXEDFILEINFO *pFileInfo = NULL;
			UINT uLenFileInfo = 0;
			if (VerQueryValue(verData, TEXT("\\"), (LPVOID*)&pFileInfo, &uLenFileInfo))
			{
				gVersion[0] = HIWORD(pFileInfo->dwProductVersionMS);	// Major
				gVersion[1] = LOWORD(pFileInfo->dwProductVersionMS);	// Minor
				gVersion[2] = HIWORD(pFileInfo->dwProductVersionLS);	// Patch ('build' in MS version order)
				gVersion[3] = LOWORD(pFileInfo->dwProductVersionLS);	// Build ('revision' in MS version order)
				//_ftprintf(stderr, TEXT("V%u.%u.%u.%u\n"), gVersion[0], gVersion[1], gVersion[2], gVersion[3]);
			}
		}
		free(verData);
	}

	BOOL bShowHelp = FALSE;
	int positional = 0;
	int errors = 0;

	for (int i = 0; i < argc; i++)  
	{
		if (_tcsicmp(argv[i], TEXT("/?")) == 0) { bShowHelp = TRUE; }
		else if (_tcsicmp(argv[i], TEXT("/HELP")) == 0) { bShowHelp = TRUE; }
		else if (_tcsicmp(argv[i], TEXT("--help")) == 0) { bShowHelp = TRUE; }
		else if (_tcsicmp(argv[i], TEXT("/AUTOSTART")) == 0) { gbAutoStart = TRUE; gbNotify = FALSE; }	// Don't notify when auto-starting
		else if (_tcsicmp(argv[i], TEXT("/NOTIFY")) == 0) { gbNotify = TRUE; }
		else if (_tcsicmp(argv[i], TEXT("/NONOTIFY")) == 0) { gbNotify = FALSE; }
		else if (_tcsicmp(argv[i], TEXT("/PORTABLE")) == 0) { gbPortable = TRUE; }
		else if (_tcsicmp(argv[i], TEXT("/NOPORTABLE")) == 0) { gbPortable = FALSE; }	// Allow override
		else if (_tcsicmp(argv[i], TEXT("/ALLOWDUPLICATE")) == 0) { gbAllowDuplicate = TRUE; }
		else if (_tcsicmp(argv[i], TEXT("/EXIT")) == 0) { gbImmediatelyExit = TRUE; }
		else if (_tcsicmp(argv[i], TEXT("/DARK")) == 0) { gbSetLight = FALSE; gbSetDark = TRUE; }
		else if (_tcsicmp(argv[i], TEXT("/LIGHT")) == 0) { gbSetLight = TRUE; gbSetDark = FALSE; }
		else if (_tcsicmp(argv[i], TEXT("/TOGGLE")) == 0) { gbSetLight = TRUE; gbSetDark = TRUE; }
		else if (_tcsicmp(argv[i], TEXT("/QUERY")) == 0) { gbQuery = TRUE; gbImmediatelyExit = TRUE; }
		
		else if (argv[i][0] == '/') 
		{
			_ftprintf(stderr, TEXT("ERROR: Unexpected parameter: %s\n"), argv[i]);
			errors++;
		} 
		else 
		{
			/*
			if (positional == 0)
			{
				argv[i];
			} 
			else
			*/
			{
				_ftprintf(stderr, TEXT("ERROR: Unexpected positional parameter: %s\n"), argv[i]);
				errors++;
			}
			positional++;
		}
	}

	if (gbPortable)
	{
		_ftprintf(stdout, TEXT("NOTE: Running as a portable app.\n"));
	}

	if (errors)
	{
		_ftprintf(stderr, TEXT("ERROR: %d parameter error(s).\n"), errors);
		bShowHelp = true;
	}

	if (bShowHelp) 
	{
		TCHAR msg[512] = TEXT("");
		_sntprintf(msg, sizeof(msg) / sizeof(msg[0]), TEXT("%s V%d.%d.%d  Daniel Jackson, 2021.\n\nParameters: [/LIGHT|/DARK|/TOGGLE] [/EXIT]\n\n"), TITLE, gVersion[0], gVersion[1], gVersion[2]);
		// [/CONSOLE:<ATTACH|CREATE|ATTACH-CREATE>]*  (* only as first parameter)
		if (gbHasConsole)
		{
			_tprintf(msg);
		}
		else
		{
			MessageBox(NULL, msg, TITLE, MB_OK | MB_ICONERROR);
		}
		return -1;
	}

	// Options to change mode at start (can combine with /EXIT)
	if (gbSetLight || gbSetDark)
	{
		if (gbSetLight && gbSetDark) ToggleDarkLight(FALSE);
		else if (gbSetLight) SetLightDark(1);
		else if (gbSetDark) SetLightDark(0);
	}

	// First opportunity to immediately exit
	if (gbImmediatelyExit) {
		return gbQuery ? IsLight() : 0;
	}

	// Initialize COM
	HRESULT hr;
	hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (FAILED(hr)) { fprintf(stderr, "ERROR: Failed CoInitializeEx().\n"); return 1; }
	hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
	if (FAILED(hr)) { fprintf(stderr, "ERROR: Failed CoInitializeSecurity().\n"); return 2; }

	// Initialize common controls
	INITCOMMONCONTROLSEX icce = {0};
	icce.dwSize = sizeof(icce);
	icce.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
	if (!InitCommonControlsEx(&icce))
	{
		fprintf(stderr, "WARNING: Failed InitCommonControlsEx().\n");
	}

	const TCHAR szWindowClass[] = TITLE;
	WNDCLASSEX wcex = {sizeof(wcex)};
	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.hInstance		= ghInstance;
	wcex.hIcon			= LoadIcon(ghInstance, TEXT("MAINICON"));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= szWindowClass;
	RegisterClassEx(&wcex);

	TCHAR *szTitle = TITLE;
	DWORD dwStyle = 0; // WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP | WS_VISIBLE | WS_THICKFRAME
	DWORD dwStyleEx = WS_EX_CONTROLPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST; // (WS_EX_TOOLWINDOW) & ~(WS_EX_APPWINDOW);

	HWND hWnd = CreateWindowEx(dwStyleEx, szWindowClass, szTitle, dwStyle, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, ghInstance, NULL);

	// Remove caption
	SetWindowLong(hWnd, GWL_STYLE, GetWindowLong(hWnd, GWL_STYLE) & ~(WS_CAPTION));

	// Fix font
	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof(ncm);
	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);
	hDlgFont = CreateFontIndirect(&(ncm.lfMessageFont));
	SendMessage(hWnd, WM_SETFONT, (WPARAM)hDlgFont, MAKELPARAM(FALSE, 0));

	ghWndMain = hWnd;
	if (!hWnd) { return -1; }

	HideWindow();  // nCmdShow
	
	// Shift+Win+D - Toggle Dark Mode
 	if (!gbImmediatelyExit && !RegisterHotKey(ghWndMain, HOT_KEY_ID, MOD_WIN | MOD_SHIFT | MOD_NOREPEAT, (UINT)'D'))
	{
		_ftprintf(stderr, TEXT("ERROR: Problem with RegisterHotKey(): 0x%08x\n"), GetLastError());
	}

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	UnregisterHotKey(hWnd, HOT_KEY_ID);

	CoUninitialize();

	return 0;
}

// Entry point when compiled with console subsystem
int _tmain(int argc, TCHAR *argv[]) 
{
	HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);
	return run(argc - 1, argv + 1, hInstance, TRUE);
}

// Entry point when compiled with Windows subsystem
int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	gbSubsystemWindows = TRUE;
	
	int argc = 0;
	TCHAR **argv = CommandLineToArgvW(GetCommandLine(), &argc);	// Must be UNICODE for CommandLineToArgvW()

	BOOL bConsoleAttach = FALSE;
	BOOL bConsoleCreate = FALSE;
	BOOL hasConsole = FALSE;
	int argOffset = 1;
	for (; argc > argOffset; argOffset++)
	{
		if (_tcsicmp(argv[argOffset], TEXT("/CONSOLE:ATTACH")) == 0)
		{
			bConsoleAttach = TRUE;
		}
		else if (_tcsicmp(argv[argOffset], TEXT("/CONSOLE:CREATE")) == 0)
		{
			bConsoleCreate = TRUE;
		}
		else if (_tcsicmp(argv[argOffset], TEXT("/CONSOLE:ATTACH-CREATE")) == 0)
		{
			bConsoleAttach = TRUE;
			bConsoleCreate = TRUE;
		}
		else if (_tcsicmp(argv[argOffset], TEXT("/CONSOLE:DEBUG")) == 0)	// Use existing console
		{
			_dup2(_fileno(stderr), _fileno(stdout));	// stdout to stderr (stops too much interleaving from buffering while debugging)
			fflush(stdout);
			hasConsole = TRUE;
		}
		else	// No more prefix arguments
		{
			//_ftprintf(stderr, TEXT("NOTE: Stopped finding prefix parameters at: %s\n"), argv[argOffset]);
			break;
		}
	}

	if (!hasConsole)
	{
		hasConsole = RedirectIOToConsole(bConsoleAttach, bConsoleCreate);
	}
	return run(argc - argOffset, argv + argOffset, hInstance, hasConsole);
}
