// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <cstdio>

#include <Uxtheme.h>
#include <map>
#pragma comment(lib, "uxtheme.lib")

//Win32 APIs. Credits to:
//https://github.com/ysc3839/win32-darkmode/blob/master/win32-darkmode/DarkMode.h
//https://gist.github.com/rounk-ctrl/b04e5622e30e0d62956870d5c22b7017
//https://github.com/notepad-plus-plus/notepad-plus-plus/labels/dark%20mode

#define TMT_BACKGROUND 0x0642
#define TMT_ACTIVECAPTION 0x0643
#define TMT_INACTIVECAPTION 0x0644
#define TMT_WINDOW 0x0646
#define TMT_WINDOWFRAME 0x0647

struct SubData
{
    WNDPROC origProc;

};

static std::map<HWND, SubData> g_subData;
static HWND g_tmp = 0;

static HINSTANCE g_hinstDLL = NULL;
static bool g_isDarkModeInited = false;
static bool g_isDarkModeSupported = false;
static bool g_isDarkModeEnabled = false;
static bool g_isExplorer = false;
static bool g_isWhitelisted = false; ///


using fnRtlGetNtVersionNumbers = void (WINAPI*)(LPDWORD major, LPDWORD minor, LPDWORD build);
using fnSetWindowCompositionAttribute = BOOL(WINAPI*)(HWND hWnd, WINDOWCOMPOSITIONATTRIBDATA*);
// 1809 17763
using fnShouldAppsUseDarkMode = bool (WINAPI*)(); // ordinal 132
using fnAllowDarkModeForWindow = bool (WINAPI*)(HWND hWnd, bool allow); // ordinal 133
using fnAllowDarkModeForApp = bool (WINAPI*)(bool allow); // ordinal 135, in 1809
using fnFlushMenuThemes = void (WINAPI*)(); // ordinal 136
using fnRefreshImmersiveColorPolicyState = void (WINAPI*)(); // ordinal 104
using fnIsDarkModeAllowedForWindow = bool (WINAPI*)(HWND hWnd); // ordinal 137
using fnGetIsImmersiveColorUsingHighContrast = bool (WINAPI*)(IMMERSIVE_HC_CACHE_MODE mode); // ordinal 106
using fnOpenNcThemeData = HTHEME(WINAPI*)(HWND hWnd, LPCWSTR pszClassList); // ordinal 49
// 1903 18362
using fnShouldSystemUseDarkMode = bool (WINAPI*)(); // ordinal 138
using fnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode appMode); // ordinal 135, in 1903
using fnIsDarkModeAllowedForApp = bool (WINAPI*)(); // ordinal 139


fnSetWindowCompositionAttribute _SetWindowCompositionAttribute = nullptr;
fnShouldAppsUseDarkMode _ShouldAppsUseDarkMode = nullptr;
fnAllowDarkModeForWindow _AllowDarkModeForWindow = nullptr;
fnAllowDarkModeForApp _AllowDarkModeForApp = nullptr;
fnFlushMenuThemes _FlushMenuThemes = nullptr;
fnRefreshImmersiveColorPolicyState _RefreshImmersiveColorPolicyState = nullptr;
fnIsDarkModeAllowedForWindow _IsDarkModeAllowedForWindow = nullptr;
fnGetIsImmersiveColorUsingHighContrast _GetIsImmersiveColorUsingHighContrast = nullptr;
fnOpenNcThemeData _OpenNcThemeData = nullptr;
fnSetPreferredAppMode _SetPreferredAppMode = nullptr;

void dbgLog(const TCHAR* str) {
    FILE* f = 0;
    fopen_s(&f, "c:\\temp\\hook.log", "a");
    if (!f) return;
    _ftprintf(f, _T("%s\n"), str);
    fclose(f);
}

bool IsColorSchemeChangeMessage(LPARAM lParam)
{
    bool is = false;
    if (lParam && CompareStringOrdinal(reinterpret_cast<LPCWCH>(lParam), -1, L"ImmersiveColorSet", -1, TRUE) == CSTR_EQUAL)
    {
        _RefreshImmersiveColorPolicyState();
        is = true;
    }
    _GetIsImmersiveColorUsingHighContrast(IHCM_REFRESH);
    return is;
}

void InitDarkSupport()
{
    HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    _OpenNcThemeData = reinterpret_cast<fnOpenNcThemeData>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(49)));
    _RefreshImmersiveColorPolicyState = reinterpret_cast<fnRefreshImmersiveColorPolicyState>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(104)));
    _GetIsImmersiveColorUsingHighContrast = reinterpret_cast<fnGetIsImmersiveColorUsingHighContrast>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(106)));
    _ShouldAppsUseDarkMode = reinterpret_cast<fnShouldAppsUseDarkMode>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(132)));
    _AllowDarkModeForWindow = reinterpret_cast<fnAllowDarkModeForWindow>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133)));
    auto ord135 = GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));
    _SetPreferredAppMode = reinterpret_cast<fnSetPreferredAppMode>(ord135);
    _IsDarkModeAllowedForWindow = reinterpret_cast<fnIsDarkModeAllowedForWindow>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(137)));
    _SetWindowCompositionAttribute = reinterpret_cast<fnSetWindowCompositionAttribute>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
    if (_OpenNcThemeData &&
        _RefreshImmersiveColorPolicyState &&
        _ShouldAppsUseDarkMode &&
        _AllowDarkModeForWindow &&
        _SetPreferredAppMode &&
        _IsDarkModeAllowedForWindow)
    {
        g_isDarkModeSupported = true;
        _SetPreferredAppMode(AllowDark);
        _RefreshImmersiveColorPolicyState();
        g_isDarkModeEnabled = _ShouldAppsUseDarkMode();
    }
}

void RefreshTitleBarThemeColor(HWND hWnd)
{
    BOOL dark = FALSE;
    if (_IsDarkModeAllowedForWindow(hWnd) && _ShouldAppsUseDarkMode())
    {
        dark = TRUE;
    }
    if (_SetWindowCompositionAttribute)
    {
        WINDOWCOMPOSITIONATTRIBDATA data = { WCA_USEDARKMODECOLORS, &dark, sizeof(dark) };
        _SetWindowCompositionAttribute(hWnd, &data);
    }
}

bool IsHighContrast()
{
    HIGHCONTRASTW highContrast = { sizeof(highContrast) };
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(highContrast), &highContrast, FALSE))
        return highContrast.dwFlags & HCF_HIGHCONTRASTON;
    return false;
}

void InitDarkModeForWindow(HWND hWnd)
{
    if (g_isDarkModeSupported)
    {
        WCHAR buf[256];
        RealGetWindowClass(hWnd, buf, sizeof(buf)/sizeof(*buf));
        //dbgLog(buf);
        LPCWSTR theme = L"Explorer";
        if (wcscmp(buf, L"Edit") == 0) //ComboBox
            theme = L"CFD";
        SetWindowTheme(hWnd, theme, NULL);
        _AllowDarkModeForWindow(hWnd, true);
        RefreshTitleBarThemeColor(hWnd);
        SendMessage(hWnd, WM_THEMECHANGED, 0, 0);
    }

}

#ifdef __cplusplus
extern "C" {
#endif

LRESULT CALLBACK DarkTabctlSubProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WNDPROC proc = g_subData[hWnd].origProc;
    switch (message)
    {
    case WM_INITDIALOG:
    {
    }
    break;
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSCROLLBAR:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORMSGBOX:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
    {
        if (g_isDarkModeSupported && g_isDarkModeEnabled)
        {
            COLORREF darkTextColor = RGB(255, 255, 255);
            COLORREF darkBkColor = RGB(1, 1, 1);
            HBRUSH hbrBkgnd = 0;

            bool groupbox = false;
            HWND target = (HWND)lParam;
            TCHAR claz[256], text[256];
            RealGetWindowClass(target, claz, 256);
            if (_tcscmp(claz, _T("Button")) == 0)
            {
                LONG_PTR style = GetWindowLongPtr(target, GWL_STYLE);
                if ((style & (BS_GROUPBOX)) == (BS_GROUPBOX))
                {
                    groupbox = true;
                    GetWindowText(target, text, 256);
                    int id = GetDlgCtrlID(target);
                    dbgLog(L"Found Groupbox:");
                    _itot_s(id, text, 16);
                    dbgLog(text);
                }
            }

            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdc, darkTextColor);
            SetBkColor(hdc, darkBkColor);
            SetBkMode(hdc, groupbox ? TRANSPARENT: OPAQUE);
            if (!hbrBkgnd)
                hbrBkgnd = (HBRUSH)GetStockObject(groupbox ? BLACK_BRUSH : HOLLOW_BRUSH );
            return reinterpret_cast<INT_PTR>(hbrBkgnd);
        }
    }
    break;
    case WM_PAINT: //fills tabs to black
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
        EndPaint(hWnd, &ps);
    }
        break;
    case WM_DESTROY:
        SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)proc);
        g_subData.erase(hWnd);
        break;
    default:
        break;
    }
    return proc(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK DarkDialogSubProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    SubData data = g_subData[hDlg];
    TCHAR buf[256];
    GetWindowText(hDlg, buf, 256);
    switch (message)
    {
    case WM_INITDIALOG:
    {
    }
        break;
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSCROLLBAR:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORMSGBOX:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
    {
        if (g_isDarkModeSupported && g_isDarkModeEnabled)
        {
            COLORREF darkTextColor = RGB(255, 255, 255);
            COLORREF darkBkColor = RGB(1, 1, 1);
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdc, darkTextColor);
            SetBkColor(hdc, darkBkColor);
            SetBkMode(hdc, (lParam == (LPARAM)g_tmp)? TRANSPARENT: OPAQUE);
            static HBRUSH hbrBkgnd = 0;
            if (!hbrBkgnd)
                hbrBkgnd = (HBRUSH)GetStockObject((lParam == (LPARAM)g_tmp) ? HOLLOW_BRUSH: BLACK_BRUSH); //BLACK_BRUSH
            return reinterpret_cast<INT_PTR>(hbrBkgnd);
        }
    }
    break;
    case WM_DESTROY:
        SetWindowLongPtr(hDlg, GWLP_WNDPROC, (LONG_PTR)data.origProc);
        g_subData.erase(hDlg);
        break;
    default:
        break;
    }
    return data.origProc(hDlg, message, wParam, lParam);
}


PAYLOADAPI LRESULT CALLBACK DarkHookProc(INT code, WPARAM wParam, LPARAM lParam)
{
    LRESULT nRet = 0;
    if (code >= 0)
    {
        if (code >= HC_ACTION && lParam != 0)
        {
            CWPSTRUCT* msg = (CWPSTRUCT*)lParam;
            switch (msg->message) {
            case WM_NCCREATE:
                if (!g_isDarkModeInited && !g_isExplorer && g_isWhitelisted)
                {
                    InitDarkSupport();
                    g_isDarkModeInited = true;
                }
                else if(!g_isDarkModeInited)
                {
                    g_isDarkModeSupported = false;
                    g_isDarkModeInited = true;
                }
                break;
            case WM_DRAWITEM:

            if (g_isDarkModeSupported)
            {
                DRAWITEMSTRUCT* draw = (DRAWITEMSTRUCT*)lParam;
                TCHAR buf[256], text[256];
                RealGetWindowClass(draw->hwndItem, buf, 256);
                GetWindowText(draw->hwndItem, text, 256);
                dbgLog(_T("WM_DRAWITEM:"));
                dbgLog(buf);
                dbgLog(text);
                if (draw /*&& _tcscmp(buf, _T("Button")) == 0*/) //SysTabControl32
                {
                    RECT rect;
                    GetClientRect(draw->hwndItem, &rect);
                    //FillRect(draw->hDC, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
                    //FillRect(draw->hDC, &draw->rcItem, (HBRUSH)GetStockObject(BLACK_BRUSH));
                    rect.left = 0; rect.top = 0; rect.bottom = 111; rect.right = 111;
                }
            }
                break;
            case WM_CREATE:

            if (g_isDarkModeSupported)
            {
                //ComboBox; SysListView32; SysTabControl32
                TCHAR claz[256], text[256];
                RealGetWindowClass(msg->hwnd, claz, 256);
                
                LONG_PTR style = GetWindowLongPtr(msg->hwnd, GWL_STYLE);
                if ((style & (WS_CHILDWINDOW | DS_CONTROL|DS_3DLOOK)) == (WS_CHILDWINDOW | DS_CONTROL | DS_3DLOOK))
                {
                    HWND hWnd = msg->hwnd;

                    SubData data;
                    data.origProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)&DarkTabctlSubProc);
                    g_subData[hWnd] = data;
                }

                if (_tcscmp(claz, _T("Button")) == 0)
                {
                    LONG_PTR style = GetWindowLongPtr(msg->hwnd, GWL_STYLE);
                }
                else if (_tcscmp(claz, _T("SysListView32")) == 0)
                {
                    /*dbgLog(_T("Found SysListView32:"));
                    HWND hHeader = ListView_GetHeader(msg->hwnd);
                    _AllowDarkModeForWindow(msg->hwnd, true);
                    _AllowDarkModeForWindow(hHeader, true);
                    ListView_SetBkColor(msg->hwnd, 0);
                    ListView_SetTextBkColor(msg->hwnd, 0);
                    ListView_SetTextColor(msg->hwnd, RGB(255,255,255));
                    SendMessageW(hHeader, WM_THEMECHANGED, 0, 0);
                    RedrawWindow(msg->hwnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE)
                    SetWindowTheme(hHeader, L"ItemsView", nullptr);
                    SetWindowTheme(msg->hwnd, L"ItemsView", nullptr);;*/
                }
                else if (_tcscmp(claz, _T("SysTabControl32")) == 0)
                {
                    dbgLog(_T("Found SysTabControl32:"));
                    GetWindowText(msg->hwnd, text, 256);
                    dbgLog(text);
                    LONG_PTR style = GetWindowLongPtr(msg->hwnd, GWL_STYLE);
                    if (g_subData.find(msg->hwnd) == g_subData.cend())
                    {
                        HWND hWnd = msg->hwnd;
                        SubData data;
                        data.origProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)&DarkTabctlSubProc);
                        g_subData[hWnd] = data;
                    }
                }
                //InitDarkModeForWindow(msg->hwnd);
            }
                break;
            case WM_INITDIALOG:
            {
                if (g_isDarkModeSupported)
                {
                    dbgLog(L"WM_INITDIALOG");
                    HWND deff = (HWND)msg->wParam;
                    HWND hWnd = msg->hwnd;
                    TCHAR claz[256], text[256];
                    RealGetWindowClass(msg->hwnd, claz, 256);

                    if (_tcscmp(claz, _T("Button")) == 0)
                    {
                        LONG_PTR style = GetWindowLongPtr(msg->hwnd, GWL_STYLE);
                        //SetWindowLongPtr(msg->hwnd, GWL_STYLE, style | BS_HATCHED);
                    }
                    else if (_tcscmp(claz, _T("SysTabControl32")) == 0)
                    {
                        dbgLog(_T("Found in initdialog SysTabControl32:"));
                        /*SetWindowTheme(hWnd, L"", L"");
                        GetWindowText(msg->hwnd, text, 256);
                        dbgLog(text);
                        LONG_PTR style = GetWindowLongPtr(msg->hwnd, GWL_STYLE);
                        SetWindowLongPtr(msg->hwnd, GWL_STYLE, style | TCS_OWNERDRAWFIXED);*/
                    }
                }
                if (GetAncestor(msg->hwnd, GA_ROOT) == msg->hwnd)
                {
                    if (g_subData.find(msg->hwnd) == g_subData.cend()) {
                        HWND hWnd = msg->hwnd;// (HWND)msg->wParam;
                        SubData data;
                        data.origProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)&DarkDialogSubProc);
                        g_subData[hWnd] = data;
                    }
                }

                break; // return (INT_PTR)TRUE;
            }

            case WM_SETTINGCHANGE:
            {
                if (g_isDarkModeSupported && IsColorSchemeChangeMessage(lParam))
                {
                    g_isDarkModeEnabled = _ShouldAppsUseDarkMode() && !IsHighContrast();

                    RefreshTitleBarThemeColor(msg->hwnd);
                    SendMessageW(msg->hwnd, WM_THEMECHANGED, 0, 0);
                }
            }
            break;
            case WM_THEMECHANGED:
            {
                if (g_isDarkModeSupported)
                {
                    _AllowDarkModeForWindow(msg->hwnd, g_isDarkModeEnabled);
                    RefreshTitleBarThemeColor(msg->hwnd);
                    UpdateWindow(msg->hwnd);
                    EnableThemeDialogTexture(msg->hwnd, ETDT_DISABLE);
                }
            }
            break;
            case WM_ERASEBKGND:
                if (g_isDarkModeSupported && GetAncestor(msg->hwnd, GA_ROOT) == msg->hwnd)
                {
                    BOOL dark = FALSE; 
                    if (_IsDarkModeAllowedForWindow(msg->hwnd) && _ShouldAppsUseDarkMode())
                    {   
                        dark = TRUE;
                    }
                    
                    dark = true;///

                    HBRUSH brush = 0;
                    if (dark) 
                        brush = CreateSolidBrush(RGB(0, 0, 0));
                    else
                        brush = CreateSolidBrush(RGB(255, 255, 255));

                    HDC hdc = (HDC)(wParam);
                    RECT rc;
                    GetClientRect(msg->hwnd, &rc);
                    /*SelectObject(hdc, brush);
                    FillRect(hdc, &rc, brush);
                    DeleteObject(brush);
                    return TRUE;
                    HTHEME hTheme = OpenThemeData(nullptr, L"WINDOW");
                    if (hTheme)
                    {
                        HBRUSH brush = GetThemeSysColorBrush(hTheme, TMT_WINDOW);
                        HBRUSH hOldBrush = (HBRUSH)SetClassLongPtr(msg->hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)brush);
                        DeleteObject(hOldBrush);
                    }
                    CloseThemeData(hTheme);*/
                    return true;
                }
            break;
            }
        }
    }

    nRet = CallNextHookEx(NULL, code, wParam, lParam);
    return nRet;
}

PAYLOADAPI LRESULT CALLBACK DarkHookProcRet(INT code, WPARAM wParam, LPARAM lParam)
{
    LRESULT nRet = 0;
    if (code >= 0)
    {
        if (code >= HC_ACTION && lParam != 0)
        {
            CWPRETSTRUCT* msg = (CWPRETSTRUCT*)lParam;
            switch (msg->message) 
            {
            case WM_CREATE:

                if (g_isDarkModeSupported)
                {

                    InitDarkModeForWindow(msg->hwnd);

                    //ComboBox; SysListView32; SysTabControl32
                    TCHAR claz[256], text[256];
                    RealGetWindowClass(msg->hwnd, claz, 256);
                    if (_tcscmp(claz, _T("Button")) == 0)
                    {
                        LONG_PTR style = GetWindowLongPtr(msg->hwnd, GWL_STYLE);
                        if ((style & ( BS_GROUPBOX)) == (  BS_GROUPBOX))
                        {
                            dbgLog(L"Found Groupbox in Hook of WM_CREATE:");
                            int id = GetDlgCtrlID(msg->hwnd);
                            _itot_s(id, text, 16);
                            dbgLog(text);
                            SetWindowTheme(msg->hwnd, L"", L"");
                            g_tmp = msg->hwnd;
                        }
                    }
                    else if (_tcscmp(claz, _T("SysListView32")) == 0)
                    {
                        ListView_SetBkColor(msg->hwnd, 0);
                        ListView_SetTextBkColor(msg->hwnd, 0);
                        ListView_SetTextColor(msg->hwnd, RGB(255, 255, 255));
                    }
                }
            }
        }
    }

    nRet = CallNextHookEx(NULL, code, wParam, lParam);
    return nRet;
}


// The DLL main function.
BOOL WINAPI DllMain(
    HINSTANCE hinstDLL,
    DWORD fdwReason,
    LPVOID lpReserved)
{
    TCHAR buf[MAX_PATH];
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        OutputDebugString(TEXT("DLL_PROCESS_ATTACH\n"));
        DisableThreadLibraryCalls(hinstDLL);
        g_hinstDLL = hinstDLL;
        GetModuleFileName(NULL, buf, sizeof(buf));
        for (int i = 0; i < sizeof(buf); ++i) {
            buf[i] = _totlower(buf[i]);
        }
        //dbgLog(buf);
        if (_tcsstr(buf, _T("cleanmgr.exe")))
        {
            dbgLog(buf);
            g_isWhitelisted = true;
        }
        else if(_tcscmp(buf, _T("c:\\windows\\system32\\explorer.exe")) == 0)
            g_isExplorer = true;
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugString(TEXT("DLL_PROCESS_DETACH\n"));
        break;
    case DLL_THREAD_ATTACH:
        OutputDebugString(TEXT("DLL_THREAD_ATTACH\n"));
        break;
    case DLL_THREAD_DETACH:
        OutputDebugString(TEXT("DLL_THREAD_DETACH\n"));
        break;
    }
    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif
