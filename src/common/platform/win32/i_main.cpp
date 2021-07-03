/*
** i_main.cpp
** System-specific startup code. Eventually calls D_DoomMain.
**
**---------------------------------------------------------------------------
** Copyright 1998-2009 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>
#include <commctrl.h>
#include <richedit.h>

#include <processenv.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <wchar.h>

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

//#include <wtsapi32.h>
#define NOTIFY_FOR_THIS_SESSION 0

#ifdef _MSC_VER
#include <eh.h>
#include <new.h>
#include <crtdbg.h>
#endif
#include "resource.h"

#include "engineerrors.h"
#include "hardware.h"

#include "m_argv.h"
#include "i_module.h"
#include "c_console.h"
#include "version.h"
#include "i_input.h"
#include "filesystem.h"
#include "cmdlib.h"
#include "s_soundinternal.h"
#include "vm.h"
#include "i_system.h"
#include "gstrings.h"
#include "s_music.h"

#include "stats.h"
#include "st_start.h"
#include "i_interface.h"
#include "startupinfo.h"
#include "printf.h"

// MACROS ------------------------------------------------------------------

// The main window's title.
#ifdef _M_X64
#define X64 " 64-bit"
#else
#define X64 ""
#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

LRESULT CALLBACK WndProc (HWND, UINT, WPARAM, LPARAM);
void CreateCrashLog (const char *custominfo, DWORD customsize, HWND richedit);
void DisplayCrashLog ();
void I_FlushBufferedConsoleStuff();
void DestroyCustomCursor();
int GameMain();

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern EXCEPTION_POINTERS CrashPointers;
extern UINT TimerPeriod;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The command line arguments.
FArgs *Args;

HINSTANCE		g_hInst;
DWORD			SessionID;
HANDLE			MainThread;
DWORD			MainThreadID;
HANDLE			StdOut;
bool			FancyStdOut, AttachedStdOut;
bool			ConWindowHidden;

// The main window
HWND			Window;

// The subwindows used for startup and error output
HWND			ConWindow, GameTitleWindow;
HWND			ErrorPane, ProgressBar, NetStartPane, StartupScreen, ErrorIcon;

HFONT			GameTitleFont;
LONG			GameTitleFontHeight;
LONG			DefaultGUIFontHeight;
LONG			ErrorIconChar;

FModule Kernel32Module{"Kernel32"};
FModule Shell32Module{"Shell32"};
FModule User32Module{"User32"};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const WCHAR WinClassName[] = WGAMENAME "MainWindow";
static HMODULE hwtsapi32;		// handle to wtsapi32.dll

// CODE --------------------------------------------------------------------

//==========================================================================
//
// UnCOM
//
// Called by atexit if CoInitialize() succeeded.
//
//==========================================================================

static void UnCOM (void)
{
	CoUninitialize ();
}

//==========================================================================
//
// UnWTS
//
// Called by atexit if RegisterSessionNotification() succeeded.
//
//==========================================================================

static void UnWTS (void)
{
	if (hwtsapi32 != 0)
	{
		typedef BOOL (WINAPI *ursn)(HWND);
		ursn unreg = (ursn)GetProcAddress (hwtsapi32, "WTSUnRegisterSessionNotification");
		if (unreg != 0)
		{
			unreg (Window);
		}
		FreeLibrary (hwtsapi32);
		hwtsapi32 = 0;
	}
}

//==========================================================================
//
// LayoutErrorPane
//
// Lays out the error pane to the desired width, returning the required
// height.
//
//==========================================================================

static int LayoutErrorPane (HWND pane, int w)
{
	HWND ctl, ctl_two;
	RECT rectc, rectc_two;

	// Right-align the Quit button.
	ctl = GetDlgItem (pane, IDOK);
	GetClientRect (ctl, &rectc);	// Find out how big it is.
	MoveWindow (ctl, w - rectc.right - 1, 1, rectc.right, rectc.bottom, TRUE);

	// Second-right-align the Restart button
	ctl_two = GetDlgItem (pane, IDC_BUTTON1);
	GetClientRect (ctl_two, &rectc_two);	// Find out how big it is.
	MoveWindow (ctl_two, w - rectc.right - rectc_two.right - 2, 1, rectc.right, rectc.bottom, TRUE);

	InvalidateRect (ctl, NULL, TRUE);
	InvalidateRect (ctl_two, NULL, TRUE);

	// Return the needed height for this layout
	return rectc.bottom + 2;
}

//==========================================================================
//
// LayoutNetStartPane
//
// Lays out the network startup pane to the specified width, returning
// its required height.
//
//==========================================================================

int LayoutNetStartPane (HWND pane, int w)
{
	HWND ctl;
	RECT margin, rectc;
	int staticheight, barheight;

	// Determine margin sizes.
	SetRect (&margin, 7, 7, 0, 0);
	MapDialogRect (pane, &margin);

	// Stick the message text in the upper left corner.
	ctl = GetDlgItem (pane, IDC_NETSTARTMESSAGE);
	GetClientRect (ctl, &rectc);
	MoveWindow (ctl, margin.left, margin.top, rectc.right, rectc.bottom, TRUE);

	// Stick the count text in the upper right corner.
	ctl = GetDlgItem (pane, IDC_NETSTARTCOUNT);
	GetClientRect (ctl, &rectc);
	MoveWindow (ctl, w - rectc.right - margin.left, margin.top, rectc.right, rectc.bottom, TRUE);
	staticheight = rectc.bottom;

	// Stretch the progress bar to fill the entire width.
	ctl = GetDlgItem (pane, IDC_NETSTARTPROGRESS);
	barheight = GetSystemMetrics (SM_CYVSCROLL);
	MoveWindow (ctl, margin.left, margin.top*2 + staticheight, w - margin.left*2, barheight, TRUE);

	// Center the abort button underneath the progress bar.
	ctl = GetDlgItem (pane, IDCANCEL);
	GetClientRect (ctl, &rectc);
	MoveWindow (ctl, (w - rectc.right) / 2, margin.top*3 + staticheight + barheight, rectc.right, rectc.bottom, TRUE);

	return margin.top*4 + staticheight + barheight + rectc.bottom;
}

//==========================================================================
//
// LayoutMainWindow
//
// Lays out the main window with the game title and log controls and
// possibly the error pane and progress bar.
//
//==========================================================================

void LayoutMainWindow (HWND hWnd, HWND pane)
{
	RECT rect;
	int errorpaneheight = 0;
	int bannerheight = 0;
	int progressheight = 0;
	int netpaneheight = 0;
	int leftside = 0;
	int w, h;

	GetClientRect (hWnd, &rect);
	w = rect.right;
	h = rect.bottom;

	if (GameStartupInfo.Name.IsNotEmpty() && GameTitleWindow != NULL)
	{
		bannerheight = GameTitleFontHeight + 5;
		MoveWindow (GameTitleWindow, 0, 0, w, bannerheight, TRUE);
		InvalidateRect (GameTitleWindow, NULL, FALSE);
	}
	if (ProgressBar != NULL)
	{
		// Base the height of the progress bar on the height of a scroll bar
		// arrow, just as in the progress bar example.
		progressheight = GetSystemMetrics (SM_CYVSCROLL);
		MoveWindow (ProgressBar, 0, h - progressheight, w, progressheight, TRUE);
	}
	if (NetStartPane != NULL)
	{
		netpaneheight = LayoutNetStartPane (NetStartPane, w);
		SetWindowPos (NetStartPane, HWND_TOP, 0, h - progressheight - netpaneheight, w, netpaneheight, SWP_SHOWWINDOW);
	}
	if (pane != NULL)
	{
		errorpaneheight = LayoutErrorPane (pane, w);
		SetWindowPos (pane, HWND_TOP, 0, h - progressheight - netpaneheight - errorpaneheight, w, errorpaneheight, 0);
	}
	if (ErrorIcon != NULL)
	{
		leftside = GetSystemMetrics (SM_CXICON) + 6;
		MoveWindow (ErrorIcon, 0, bannerheight, leftside, h - bannerheight - errorpaneheight - progressheight - netpaneheight, TRUE);
	}
	// If there is a startup screen, it covers the log window
	if (StartupScreen != NULL)
	{
		SetWindowPos (StartupScreen, HWND_TOP, leftside, bannerheight, w - leftside,
			h - bannerheight - errorpaneheight - progressheight - netpaneheight, SWP_SHOWWINDOW);
		InvalidateRect (StartupScreen, NULL, FALSE);
		MoveWindow (ConWindow, 0, 0, 0, 0, TRUE);
	}
	else
	{
		// The log window uses whatever space is left.
		MoveWindow (ConWindow, leftside, bannerheight, w - leftside,
			h - bannerheight - errorpaneheight - progressheight - netpaneheight, TRUE);
	}
}


//==========================================================================
//
// I_SetIWADInfo
//
//==========================================================================

void I_SetIWADInfo()
{
	// Make the startup banner show itself
	LayoutMainWindow(Window, NULL);
}

//==========================================================================
//
// LConProc
//
// The main window's WndProc during startup. During gameplay, the WndProc
// in i_input.cpp is used instead.
//
//==========================================================================

LRESULT CALLBACK LConProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HWND view;
	HDC hdc;
	HBRUSH hbr;
	HGDIOBJ oldfont;
	RECT rect;
	SIZE size;
	LOGFONT lf;
	TEXTMETRIC tm;
	HINSTANCE inst = (HINSTANCE)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
	DRAWITEMSTRUCT *drawitem;
	CHARFORMAT2W format;

	switch (msg)
	{
	case WM_CREATE:
		// Create game title static control
		memset (&lf, 0, sizeof(lf));
		hdc = GetDC (hWnd);
		lf.lfHeight = -MulDiv(12, GetDeviceCaps(hdc, LOGPIXELSY), 72);
		lf.lfCharSet = ANSI_CHARSET;
		lf.lfWeight = FW_BOLD;
		lf.lfPitchAndFamily = VARIABLE_PITCH | FF_ROMAN;
		wcscpy (lf.lfFaceName, L"Trebuchet MS");
		GameTitleFont = CreateFontIndirect (&lf);

		oldfont = SelectObject (hdc, GetStockObject (DEFAULT_GUI_FONT));
		GetTextMetrics (hdc, &tm);
		DefaultGUIFontHeight = tm.tmHeight;
		if (GameTitleFont == NULL)
		{
			GameTitleFontHeight = DefaultGUIFontHeight;
		}
		else
		{
			SelectObject (hdc, GameTitleFont);
			GetTextMetrics (hdc, &tm);
			GameTitleFontHeight = tm.tmHeight;
		}
		SelectObject (hdc, oldfont);

		// Create log read-only edit control
		view = CreateWindowExW (WS_EX_NOPARENTNOTIFY, L"RichEdit20W", nullptr,
			WS_CHILD | WS_VISIBLE | WS_VSCROLL |
			ES_LEFT | ES_MULTILINE | WS_CLIPSIBLINGS,
			0, 0, 0, 0,
			hWnd, NULL, inst, NULL);
		HRESULT hr;
		hr = GetLastError();
		if (view == NULL)
		{
			ReleaseDC (hWnd, hdc);
			return -1;
		}
		SendMessage (view, EM_SETREADONLY, TRUE, 0);
		SendMessage (view, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
		SendMessage (view, EM_SETBKGNDCOLOR, 0, RGB(70,70,70));
		// Setup default font for the log.
		//SendMessage (view, WM_SETFONT, (WPARAM)GetStockObject (DEFAULT_GUI_FONT), FALSE);
		format.cbSize = sizeof(format);
		format.dwMask = CFM_BOLD | CFM_COLOR | CFM_FACE | CFM_SIZE | CFM_CHARSET;
		format.dwEffects = 0;
		format.yHeight = 200;
		format.crTextColor = RGB(223,223,223);
		format.bCharSet = ANSI_CHARSET;
		format.bPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
		wcscpy(format.szFaceName, L"DejaVu Sans");	// At least I have it. :p
		SendMessageW(view, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&format);

		ConWindow = view;
		ReleaseDC (hWnd, hdc);

		view = CreateWindowExW (WS_EX_NOPARENTNOTIFY, L"STATIC", NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_OWNERDRAW, 0, 0, 0, 0, hWnd, nullptr, inst, nullptr);
		if (view == nullptr)
		{
			return -1;
		}
		SetWindowLong (view, GWL_ID, IDC_STATIC_TITLE);
		GameTitleWindow = view;

		return 0;

	case WM_SIZE:
		if (wParam != SIZE_MAXHIDE && wParam != SIZE_MAXSHOW)
		{
			LayoutMainWindow (hWnd, ErrorPane);
		}
		return 0;

	case WM_DRAWITEM:
		// Draw title banner.
		if (wParam == IDC_STATIC_TITLE && GameStartupInfo.Name.IsNotEmpty())
		{
			const PalEntry *c;

			// Draw the game title strip at the top of the window.
			drawitem = (LPDRAWITEMSTRUCT)lParam;

			// Draw the background.
			rect = drawitem->rcItem;
			rect.bottom -= 1;
			c = (const PalEntry *)&GameStartupInfo.BkColor;
			hbr = CreateSolidBrush (RGB(c->r,c->g,c->b));
			FillRect (drawitem->hDC, &drawitem->rcItem, hbr);
			DeleteObject (hbr);

			// Calculate width of the title string.
			SetTextAlign (drawitem->hDC, TA_TOP);
			oldfont = SelectObject (drawitem->hDC, GameTitleFont != NULL ? GameTitleFont : (HFONT)GetStockObject (DEFAULT_GUI_FONT));
			auto widename = GameStartupInfo.Name.WideString();
			GetTextExtentPoint32W (drawitem->hDC, widename.c_str(), (int)widename.length(), &size);

			// Draw the title.
			c = (const PalEntry *)&GameStartupInfo.FgColor;
			SetTextColor (drawitem->hDC, RGB(c->r,c->g,c->b));
			SetBkMode (drawitem->hDC, TRANSPARENT);
			TextOutW (drawitem->hDC, rect.left + (rect.right - rect.left - size.cx) / 2, 2, widename.c_str(), (int)widename.length());
			SelectObject (drawitem->hDC, oldfont);
			return TRUE;
		}
		// Draw startup screen
		else if (wParam == IDC_STATIC_STARTUP)
		{
			if (StartupScreen != NULL)
			{
				drawitem = (LPDRAWITEMSTRUCT)lParam;

				rect = drawitem->rcItem;
				// Windows expects DIBs to be bottom-up but ours is top-down,
				// so flip it vertically while drawing it.
				StretchDIBits (drawitem->hDC, rect.left, rect.bottom - 1, rect.right - rect.left, rect.top - rect.bottom,
					0, 0, StartupBitmap->bmiHeader.biWidth, StartupBitmap->bmiHeader.biHeight,
					ST_Util_BitsForBitmap(StartupBitmap), reinterpret_cast<const BITMAPINFO*>(StartupBitmap), DIB_RGB_COLORS, SRCCOPY);

				// If the title banner is gone, then this is an ENDOOM screen, so draw a short prompt
				// where the command prompt would have been in DOS.
				if (GameTitleWindow == NULL)
				{
					auto quitmsg = WideString(GStrings("TXT_QUITENDOOM"));

					SetTextColor (drawitem->hDC, RGB(240,240,240));
					SetBkMode (drawitem->hDC, TRANSPARENT);
					oldfont = SelectObject (drawitem->hDC, (HFONT)GetStockObject (DEFAULT_GUI_FONT));
					TextOutW (drawitem->hDC, 3, drawitem->rcItem.bottom - DefaultGUIFontHeight - 3, quitmsg.c_str(), (int)quitmsg.length());
					SelectObject (drawitem->hDC, oldfont);
				}
				return TRUE;
			}
		}
		// Draw stop icon.
		else if (wParam == IDC_ICONPIC)
		{
			HICON icon;
			POINTL char_pos;
			drawitem = (LPDRAWITEMSTRUCT)lParam;

			// This background color should match the edit control's.
			hbr = CreateSolidBrush (RGB(70,70,70));
			FillRect (drawitem->hDC, &drawitem->rcItem, hbr);
			DeleteObject (hbr);

			// Draw the icon aligned with the first line of error text.
			SendMessage (ConWindow, EM_POSFROMCHAR, (WPARAM)&char_pos, ErrorIconChar);
			icon = (HICON)LoadImage (0, IDI_ERROR, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
			DrawIcon (drawitem->hDC, 6, char_pos.y, icon);
			return TRUE;
		}
		return FALSE;

	case WM_COMMAND:
		if (ErrorIcon != NULL && (HWND)lParam == ConWindow && HIWORD(wParam) == EN_UPDATE)
		{
			// Be sure to redraw the error icon if the edit control changes.
			InvalidateRect (ErrorIcon, NULL, TRUE);
			return 0;
		}
		break;

	case WM_CLOSE:
		PostQuitMessage (0);
		break;

	case WM_DESTROY:
		if (GameTitleFont != NULL)
		{
			DeleteObject (GameTitleFont);
		}
		break;
	}
	return DefWindowProc (hWnd, msg, wParam, lParam);
}

//==========================================================================
//
// ErrorPaneProc
//
// DialogProc for the error pane.
//
//==========================================================================

INT_PTR CALLBACK ErrorPaneProc (HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		// Appear in the main window.
		LayoutMainWindow (GetParent (hDlg), hDlg);
		return TRUE;

	case WM_COMMAND:
		if (HIWORD(wParam) == BN_CLICKED)
		{
			if (LOWORD(wParam) == IDC_BUTTON1) // we pressed the restart button, so run GZDoom again
			{
				HMODULE hModule = GetModuleHandleW(NULL);
				WCHAR path[MAX_PATH];

				// Remove the first argument.
				std::wstring cmdline = std::wstring(PathGetArgsW(GetCommandLineW()));
				if (cmdline.find(L"-waitforprocess", 0) != std::wstring::npos)
				{
					// Strip -waitforprocess argument if it is found.
					cmdline.erase(cmdline.find(L"-waitforprocess", 0));
				}
				cmdline += std::wstring(L" -waitforprocess ") + std::to_wstring(GetCurrentProcessId());
				// Finally restart the application.
				GetModuleFileNameW(hModule, path, MAX_PATH);				
				ShellExecuteW(NULL, L"open", path, cmdline.c_str(), NULL, SW_SHOWNORMAL);
			}
			PostQuitMessage (0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

//==========================================================================
//
// I_SetWndProc
//
// Sets the main WndProc, hides all the child windows, and starts up
// in-game input.
//
//==========================================================================

void I_SetWndProc()
{
	if (GetWindowLongPtr (Window, GWLP_USERDATA) == 0)
	{
		SetWindowLongPtr (Window, GWLP_USERDATA, 1);
		SetWindowLongPtr (Window, GWLP_WNDPROC, (WLONG_PTR)WndProc);
		ShowWindow (ConWindow, SW_HIDE);
		ShowWindow(ProgressBar, SW_HIDE);
		ConWindowHidden = true;
		ShowWindow (GameTitleWindow, SW_HIDE);
		I_InitInput (Window);
	}
}

//==========================================================================
//
// RestoreConView
//
// Returns the main window to its startup state.
//
//==========================================================================

void RestoreConView()
{
	HDC screenDC = GetDC(0);
	int dpi = GetDeviceCaps(screenDC, LOGPIXELSX);
	ReleaseDC(0, screenDC);
	int width = (512 * dpi + 96 / 2) / 96;
	int height = (384 * dpi + 96 / 2) / 96;

	// Make sure the window has a frame in case it was fullscreened.
	SetWindowLongPtr (Window, GWL_STYLE, WS_VISIBLE|WS_OVERLAPPEDWINDOW);
	if (GetWindowLong (Window, GWL_EXSTYLE) & WS_EX_TOPMOST)
	{
		SetWindowPos (Window, HWND_BOTTOM, 0, 0, width, height,
			SWP_DRAWFRAME | SWP_NOCOPYBITS | SWP_NOMOVE);
		SetWindowPos (Window, HWND_TOP, 0, 0, 0, 0, SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOSIZE);
	}
	else
	{
		SetWindowPos (Window, NULL, 0, 0, width, height,
			SWP_DRAWFRAME | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOZORDER);
	}

	SetWindowLongPtr (Window, GWLP_WNDPROC, (WLONG_PTR)LConProc);
	ShowWindow (ConWindow, SW_SHOW);
	ConWindowHidden = false;
	ShowWindow (GameTitleWindow, SW_SHOW);
	I_ShutdownInput ();		// Make sure the mouse pointer is available.
	// Make sure the progress bar isn't visible.
	DeleteStartupScreen();
}

//==========================================================================
//
// ShowErrorPane
//
// Shows an error message, preferably in the main window, but it can
// use a normal message box too.
//
//==========================================================================

void ShowErrorPane(const char *text)
{
	auto widetext = WideString(text);
	if (Window == nullptr || ConWindow == nullptr)
	{
		if (text != NULL)
		{
			MessageBoxW (Window, widetext.c_str(),
				WGAMENAME " Fatal Error", MB_OK|MB_ICONSTOP|MB_TASKMODAL);
		}
		return;
	}

	if (StartScreen != NULL)	// Ensure that the network pane is hidden.
	{
		StartScreen->NetDone();
	}
	if (text != NULL)
	{
		FStringf caption("Fatal Error - " GAMENAME " %s " X64 " (%s)", GetVersionString(), GetGitTime());
		auto wcaption = caption.WideString();
		SetWindowTextW (Window, wcaption.c_str());
		ErrorIcon = CreateWindowExW (WS_EX_NOPARENTNOTIFY, L"STATIC", NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_OWNERDRAW, 0, 0, 0, 0, Window, NULL, g_hInst, NULL);
		if (ErrorIcon != NULL)
		{
			SetWindowLong (ErrorIcon, GWL_ID, IDC_ICONPIC);
		}
	}
	ErrorPane = CreateDialogParam (g_hInst, MAKEINTRESOURCE(IDD_ERRORPANE), Window, ErrorPaneProc, (LONG_PTR)NULL);

	if (text != NULL)
	{
		CHARRANGE end;
		CHARFORMAT2 oldformat, newformat;
		PARAFORMAT2 paraformat;

		// Append the error message to the log.
		end.cpMax = end.cpMin = GetWindowTextLength (ConWindow);
		SendMessage (ConWindow, EM_EXSETSEL, 0, (LPARAM)&end);

		// Remember current charformat.
		oldformat.cbSize = sizeof(oldformat);
		SendMessage (ConWindow, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&oldformat);

		// Use bigger font and standout colors.
		newformat.cbSize = sizeof(newformat);
		newformat.dwMask = CFM_BOLD | CFM_COLOR | CFM_SIZE;
		newformat.dwEffects = CFE_BOLD;
		newformat.yHeight = oldformat.yHeight * 5 / 4;
		newformat.crTextColor = RGB(255,170,170);
		SendMessage (ConWindow, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&newformat);

		// Indent the rest of the text to make the error message stand out a little more.
		paraformat.cbSize = sizeof(paraformat);
		paraformat.dwMask = PFM_STARTINDENT | PFM_OFFSETINDENT | PFM_RIGHTINDENT;
		paraformat.dxStartIndent = paraformat.dxOffset = paraformat.dxRightIndent = 120;
		SendMessage (ConWindow, EM_SETPARAFORMAT, 0, (LPARAM)&paraformat);
		SendMessageW (ConWindow, EM_REPLACESEL, FALSE, (LPARAM)L"\n");

		// Find out where the error lines start for the error icon display control.
		SendMessage (ConWindow, EM_EXGETSEL, 0, (LPARAM)&end);
		ErrorIconChar = end.cpMax;

		// Now start adding the actual error message.
		SendMessageW (ConWindow, EM_REPLACESEL, FALSE, (LPARAM)L"Execution could not continue.\n\n");

		// Restore old charformat but with light yellow text.
		oldformat.crTextColor = RGB(255,255,170);
		SendMessage (ConWindow, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&oldformat);

		// Add the error text.
		SendMessageW (ConWindow, EM_REPLACESEL, FALSE, (LPARAM)widetext.c_str());

		// Make sure the error text is not scrolled below the window.
		SendMessage (ConWindow, EM_LINESCROLL, 0, SendMessage (ConWindow, EM_GETLINECOUNT, 0, 0));
		// The above line scrolled everything off the screen, so pretend to move the scroll
		// bar thumb, which clamps to not show any extra lines if it doesn't need to.
		SendMessage (ConWindow, EM_SCROLL, SB_PAGEDOWN, 0);
	}

	BOOL bRet;
	MSG msg;

	while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (bRet == -1)
		{
			MessageBoxW (Window, widetext.c_str(), WGAMENAME " Fatal Error", MB_OK|MB_ICONSTOP|MB_TASKMODAL);
			return;
		}
		else if (!IsDialogMessage (ErrorPane, &msg))
		{
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}
	}
}

void PeekThreadedErrorPane()
{
	// Allow SendMessage from another thread to call its message handler so that it can display the crash dialog
	MSG msg;
	PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE);
}

static void UnTbp()
{
	timeEndPeriod(TimerPeriod);
}

//==========================================================================
//
// DoMain
//
//==========================================================================

int DoMain (HINSTANCE hInstance)
{
	LONG WinWidth, WinHeight;
	int height, width, x, y;
	RECT cRect;
	TIMECAPS tc;
	DEVMODE displaysettings;
	
	// Do not use the multibyte __argv here because we want UTF-8 arguments
	// and those can only be done by converting the Unicode variants.
	Args = new FArgs();
	auto argc = __argc;
	auto wargv = __wargv;
	for (int i = 0; i < argc; i++)
	{
		Args->AppendArg(FString(wargv[i]));
	}
	
	// Load Win32 modules
	Kernel32Module.Load({"kernel32.dll"});
	Shell32Module.Load({"shell32.dll"});
	User32Module.Load({"user32.dll"});
	
	// Under XP, get our session ID so we can know when the user changes/locks sessions.
	// Since we need to remain binary compatible with older versions of Windows, we
	// need to extract the ProcessIdToSessionId function from kernel32.dll manually.
	HMODULE kernel = GetModuleHandleA ("kernel32.dll");
	
	if (Args->CheckParm("-waitforprocess"))
	{
		// Try opening the process by its PID.
		int pidOfProcess = atoi(Args->TakeValue("-waitforprocess"));
		if (pidOfProcess != 0)
		{
			HANDLE handle = OpenProcess(SYNCHRONIZE, FALSE, pidOfProcess);
			if (handle)
			{
				// Wait up to 5 seconds for it to close.
				WaitForSingleObject(handle, 5000);
			}
		}
	}

	if (Args->CheckParm("-stdout"))
	{
		// As a GUI application, we don't normally get a console when we start.
		// If we were run from the shell and are on XP+, we can attach to its
		// console. Otherwise, we can create a new one. If we already have a
		// stdout handle, then we have been redirected and should just use that
		// handle instead of creating a console window.
		
		StdOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (StdOut != NULL)
		{
			// It seems that running from a shell always creates a std output
			// for us, even if it doesn't go anywhere. (Running from Explorer
			// does not.) If we can get file information for this handle, it's
			// a file or pipe, so use it. Otherwise, pretend it wasn't there
			// and find a console to use instead.
			BY_HANDLE_FILE_INFORMATION info;
			if (!GetFileInformationByHandle(StdOut, &info))
			{
				StdOut = NULL;
			}
		}
		if (StdOut == NULL)
		{
			if (AttachConsole(ATTACH_PARENT_PROCESS))
			{
				StdOut = GetStdHandle(STD_OUTPUT_HANDLE);
				DWORD foo; WriteFile(StdOut, "\n", 1, &foo, NULL);
				AttachedStdOut = true;
			}
			if (StdOut == NULL && AllocConsole())
			{
				StdOut = GetStdHandle(STD_OUTPUT_HANDLE);
			}
			
			// These two functions do not exist in Windows XP.
			BOOL (WINAPI* p_GetCurrentConsoleFontEx)(HANDLE hConsoleOutput, BOOL bMaximumWindow, PCONSOLE_FONT_INFOEX lpConsoleCurrentFontEx);
			BOOL (WINAPI* p_SetCurrentConsoleFontEx)(HANDLE hConsoleOutput, BOOL bMaximumWindow, PCONSOLE_FONT_INFOEX lpConsoleCurrentFontEx);
			
			p_SetCurrentConsoleFontEx = (decltype(p_SetCurrentConsoleFontEx))GetProcAddress(kernel, "SetCurrentConsoleFontEx");
			p_GetCurrentConsoleFontEx = (decltype(p_GetCurrentConsoleFontEx))GetProcAddress(kernel, "GetCurrentConsoleFontEx");
			if (p_SetCurrentConsoleFontEx && p_GetCurrentConsoleFontEx)
			{
				CONSOLE_FONT_INFOEX cfi;
				cfi.cbSize = sizeof(cfi);
				
				if (p_GetCurrentConsoleFontEx(StdOut, false, &cfi))
				{
					if (*cfi.FaceName == 0)	// If the face name is empty, the default (useless) raster font is actoive.
					{
						//cfi.dwFontSize = { 8, 14 };
						wcscpy(cfi.FaceName, L"Lucida Console");
						cfi.FontFamily = FF_DONTCARE;
						p_SetCurrentConsoleFontEx(StdOut, false, &cfi);
					}
				}
			}
			FancyStdOut = true;
		}
	}
	
	// Set the timer to be as accurate as possible
	if (timeGetDevCaps (&tc, sizeof(tc)) != TIMERR_NOERROR)
		TimerPeriod = 1;	// Assume minimum resolution of 1 ms
	else
		TimerPeriod = tc.wPeriodMin;
	
	timeBeginPeriod (TimerPeriod);
	atexit(UnTbp);
	
	// Figure out what directory the program resides in.
	WCHAR progbuff[1024];
	if (GetModuleFileNameW(nullptr, progbuff, sizeof progbuff) == 0)
	{
		MessageBoxA(nullptr, "Fatal", "Could not determine program location.", MB_ICONEXCLAMATION|MB_OK);
		exit(-1);
	}
	
	progbuff[1023] = '\0';
	if (auto lastsep = wcsrchr(progbuff, '\\'))
	{
		lastsep[1] = '\0';
	}
	
	progdir = progbuff;
	FixPathSeperator(progdir);
	
	HDC screenDC = GetDC(0);
	int dpi = GetDeviceCaps(screenDC, LOGPIXELSX);
	ReleaseDC(0, screenDC);
	width = (512 * dpi + 96 / 2) / 96;
	height = (384 * dpi + 96 / 2) / 96;
	
	// Many Windows structures that specify their size do so with the first
	// element. DEVMODE is not one of those structures.
	memset (&displaysettings, 0, sizeof(displaysettings));
	displaysettings.dmSize = sizeof(displaysettings);
	EnumDisplaySettings (NULL, ENUM_CURRENT_SETTINGS, &displaysettings);
	x = (displaysettings.dmPelsWidth - width) / 2;
	y = (displaysettings.dmPelsHeight - height) / 2;
	
	if (Args->CheckParm ("-0"))
	{
		x = y = 0;
	}
	
	WNDCLASS WndClass;
	WndClass.style			= 0;
	WndClass.lpfnWndProc	= LConProc;
	WndClass.cbClsExtra		= 0;
	WndClass.cbWndExtra		= 0;
	WndClass.hInstance		= hInstance;
	WndClass.hIcon			= LoadIcon (hInstance, MAKEINTRESOURCE(IDI_ICON1));
	WndClass.hCursor		= LoadCursor (NULL, IDC_ARROW);
	WndClass.hbrBackground	= NULL;
	WndClass.lpszMenuName	= NULL;
	WndClass.lpszClassName	= WinClassName;
	
	/* register this new class with Windows */
	if (!RegisterClass((LPWNDCLASS)&WndClass))
	{
		MessageBoxA(nullptr, "Could not register window class", "Fatal", MB_ICONEXCLAMATION|MB_OK);
		exit(-1);
	}
	
	/* create window */
	FStringf caption("" GAMENAME " %s " X64 " (%s)", GetVersionString(), GetGitTime());
	std::wstring wcaption = caption.WideString();
	Window = CreateWindowExW(
							 WS_EX_APPWINDOW,
							 WinClassName,
							 wcaption.c_str(),
							 WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
							 x, y, width, height,
							 (HWND)   NULL,
							 (HMENU)  NULL,
							 hInstance,
							 NULL);
	
	if (!Window)
	{
		MessageBoxA(nullptr, "Unable to create main window", "Fatal", MB_ICONEXCLAMATION|MB_OK);
		exit(-1);
	}
	
	if (kernel != NULL)
	{
		typedef BOOL (WINAPI *pts)(DWORD, DWORD *);
		pts pidsid = (pts)GetProcAddress (kernel, "ProcessIdToSessionId");
		if (pidsid != 0)
		{
			if (!pidsid (GetCurrentProcessId(), &SessionID))
			{
				SessionID = 0;
			}
			hwtsapi32 = LoadLibraryA ("wtsapi32.dll");
			if (hwtsapi32 != 0)
			{
				FARPROC reg = GetProcAddress (hwtsapi32, "WTSRegisterSessionNotification");
				if (reg == 0 || !((BOOL(WINAPI *)(HWND, DWORD))reg) (Window, NOTIFY_FOR_THIS_SESSION))
				{
					FreeLibrary (hwtsapi32);
					hwtsapi32 = 0;
				}
				else
				{
					atexit (UnWTS);
				}
			}
		}
	}
	
	GetClientRect (Window, &cRect);
	
	WinWidth = cRect.right;
	WinHeight = cRect.bottom;
	
	CoInitialize (NULL);
	atexit (UnCOM);
	
	int ret = GameMain ();
	DestroyCustomCursor();
	if (ret == 1337) // special exit code for 'norun'.
	{
		if (!batchrun)
		{
			if (FancyStdOut && !AttachedStdOut)
			{ // Outputting to a new console window: Wait for a keypress before quitting.
				DWORD bytes;
				HANDLE stdinput = GetStdHandle(STD_INPUT_HANDLE);
				
				ShowWindow(Window, SW_HIDE);
				WriteFile(StdOut, "Press any key to exit...", 24, &bytes, NULL);
				FlushConsoleInputBuffer(stdinput);
				SetConsoleMode(stdinput, 0);
				ReadConsole(stdinput, &bytes, 1, &bytes, NULL);
			}
			else if (StdOut == NULL)
			{
				ShowErrorPane(NULL);
			}
		}
	}
	return ret;
}

void I_ShowFatalError(const char *msg)
{
	I_ShutdownGraphics ();
	RestoreConView ();
	S_StopMusic(true);
	I_FlushBufferedConsoleStuff();

	if (CVMAbortException::stacktrace.IsNotEmpty())
	{
		Printf("%s", CVMAbortException::stacktrace.GetChars());
	}
	
	if (!batchrun)
	{
		ShowErrorPane(msg);
	}
	else
	{
		Printf("%s\n", msg);
	}
}

// Here is how the error logging system works.
//
// To catch exceptions that occur in secondary threads, CatchAllExceptions is
// set as the UnhandledExceptionFilter for this process. It records the state
// of the thread at the time of the crash using CreateCrashLog and then queues
// an APC on the primary thread. When the APC executes, it raises a software
// exception that gets caught by the __try/__except block in WinMain.
// I_GetEvent calls SleepEx to put the primary thread in a waitable state
// periodically so that the APC has a chance to execute.
//
// Exceptions on the primary thread are caught by the __try/__except block in
// WinMain. Not only does it record the crash information, it also shuts
// everything down and displays a dialog with the information present. If a
// console log is being produced, the information will also be appended to it.
//
// If a debugger is running, CatchAllExceptions never executes, so secondary
// thread exceptions will always be caught by the debugger. For the primary
// thread, IsDebuggerPresent is called to determine if a debugger is present.
// Note that this function is not present on Windows 95, so we cannot
// statically link to it.
//
// To make this work with MinGW, you will need to use inline assembly
// because GCC offers no native support for Windows' SEH.

//==========================================================================
//
// SleepForever
//
//==========================================================================

void SleepForever ()
{
	Sleep (INFINITE);
}

//==========================================================================
//
// ExitMessedUp
//
// An exception occurred while exiting, so don't do any standard processing.
// Just die.
//
//==========================================================================

LONG WINAPI ExitMessedUp (LPEXCEPTION_POINTERS foo)
{
	ExitProcess (1000);
}

//==========================================================================
//
// ExitFatally
//
//==========================================================================

void CALLBACK ExitFatally (ULONG_PTR dummy)
{
	SetUnhandledExceptionFilter (ExitMessedUp);
	I_ShutdownGraphics ();
	RestoreConView ();
	DisplayCrashLog ();
	exit(-1);
}

#ifndef _M_ARM64
//==========================================================================
//
// CatchAllExceptions
//
//==========================================================================

namespace
{
	CONTEXT MainThreadContext;
}

LONG WINAPI CatchAllExceptions (LPEXCEPTION_POINTERS info)
{
#ifdef _DEBUG
	if (info->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}
#endif

	static bool caughtsomething = false;

	if (caughtsomething) return EXCEPTION_EXECUTE_HANDLER;
	caughtsomething = true;

	char *custominfo = (char *)HeapAlloc (GetProcessHeap(), 0, 16384);

	CrashPointers = *info;
	if (sysCallbacks.CrashInfo && custominfo) sysCallbacks.CrashInfo(custominfo, 16384, "\r\n");
	CreateCrashLog (custominfo, (DWORD)strlen(custominfo), ConWindow);

	// If the main thread crashed, then make it clean up after itself.
	// Otherwise, put the crashing thread to sleep and signal the main thread to clean up.
	if (GetCurrentThreadId() == MainThreadID)
	{
#ifdef _M_X64
		*info->ContextRecord = MainThreadContext;
#else
		info->ContextRecord->Eip = (DWORD_PTR)ExitFatally;
#endif // _M_X64
	}
	else
	{
#ifndef _M_X64
		info->ContextRecord->Eip = (DWORD_PTR)SleepForever;
#else
		info->ContextRecord->Rip = (DWORD_PTR)SleepForever;
#endif
		QueueUserAPC (ExitFatally, MainThread, 0);
	}
	return EXCEPTION_CONTINUE_EXECUTION;
}
#else // !_M_ARM64
// stub this function for ARM64
LONG WINAPI CatchAllExceptions (LPEXCEPTION_POINTERS info)
{
	return EXCEPTION_CONTINUE_EXECUTION;
}
#endif // !_M_ARM64

//==========================================================================
//
// infiniterecursion
//
// Debugging routine for testing the crash logger.
//
//==========================================================================

#ifdef _DEBUG
static void infiniterecursion(int foo)
{
	if (foo)
	{
		infiniterecursion(foo);
	}
}
#endif

// Setting this to 'true' allows getting the standard notification for a crash
// which offers the very important feature to open a debugger and see the crash in context right away.
CUSTOM_CVAR(Bool, disablecrashlog, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	SetUnhandledExceptionFilter(!*self ? CatchAllExceptions : nullptr);
}

//==========================================================================
//
// WinMain
//
//==========================================================================

int WINAPI wWinMain (HINSTANCE hInstance, HINSTANCE nothing, LPWSTR cmdline, int nCmdShow)
{
	g_hInst = hInstance;

	InitCommonControls ();			// Load some needed controls and be pretty under XP

	// We need to load riched20.dll so that we can create the control.
	if (NULL == LoadLibraryA ("riched20.dll"))
	{
		// This should only happen on basic Windows 95 installations, but since we
		// don't support Windows 95, we have no obligation to provide assistance in
		// getting it installed.
		MessageBoxA(NULL, "Could not load riched20.dll", GAMENAME " Error", MB_OK | MB_ICONSTOP);
		return 0;
	}

#if !defined(__GNUC__) && defined(_DEBUG)
	if (__argc == 2 && __wargv != nullptr && wcscmp (__wargv[1], L"TestCrash") == 0)
	{
		__try
		{
			*(int *)0 = 0;
		}
		__except(CrashPointers = *GetExceptionInformation(),
			CreateCrashLog ("TestCrash", 9, NULL), EXCEPTION_EXECUTE_HANDLER)
		{
		}
		DisplayCrashLog ();
		return 0;
	}
	if (__argc == 2 && __wargv != nullptr && wcscmp (__wargv[1], L"TestStackCrash") == 0)
	{
		__try
		{
			infiniterecursion(1);
		}
		__except(CrashPointers = *GetExceptionInformation(),
			CreateCrashLog ("TestStackCrash", 14, NULL), EXCEPTION_EXECUTE_HANDLER)
		{
		}
		DisplayCrashLog ();
		return 0;
	}
#endif

	MainThread = INVALID_HANDLE_VALUE;
	DuplicateHandle (GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &MainThread,
		0, FALSE, DUPLICATE_SAME_ACCESS);
	MainThreadID = GetCurrentThreadId();

#ifndef _DEBUG
	if (MainThread != INVALID_HANDLE_VALUE)
	{
#ifndef _M_ARM64
		SetUnhandledExceptionFilter (CatchAllExceptions);
#endif

#ifdef _M_X64
		static bool setJumpResult = false;
		RtlCaptureContext(&MainThreadContext);
		if (setJumpResult)
		{
			ExitFatally(0);
			return 0;
		}
		setJumpResult = true;
#endif // _M_X64
	}
#endif

#if defined(_DEBUG) && defined(_MSC_VER)
	// Uncomment this line to make the Visual C++ CRT check the heap before
	// every allocation and deallocation. This will be slow, but it can be a
	// great help in finding problem areas.
	//_CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);

	// Enable leak checking at exit.
	_CrtSetDbgFlag (_CrtSetDbgFlag(0) | _CRTDBG_LEAK_CHECK_DF);

	// Use this to break at a specific allocation number.
	//_crtBreakAlloc = 227524;
#endif

	int ret = DoMain (hInstance);

	CloseHandle (MainThread);
	MainThread = INVALID_HANDLE_VALUE;
	return ret;
}

// each platform has its own specific version of this function.
void I_SetWindowTitle(const char* caption)
{
	std::wstring widecaption;
	if (!caption)
	{
		FStringf default_caption("" GAMENAME " %s " X64 " (%s)", GetVersionString(), GetGitTime());
		widecaption = default_caption.WideString();
	}
	else
	{
		widecaption = WideString(caption);
	}
	SetWindowText(Window, widecaption.c_str());
}
