// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <cstdio>

#include <Uxtheme.h>
#include <map>
#pragma comment(lib, "uxtheme.lib")

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
    //if (_OpenNcThemeData &&
    //    _RefreshImmersiveColorPolicyState &&
    //    _ShouldAppsUseDarkMode &&
    //    _AllowDarkModeForWindow &&
    //    _SetPreferredAppMode &&
    //    _IsDarkModeAllowedForWindow)
    {
        g_isDarkModeSupported = true;
        //_AllowDarkModeForApp(true);
        _SetPreferredAppMode(AllowDark);
        _RefreshImmersiveColorPolicyState();
        g_isDarkModeEnabled = _ShouldAppsUseDarkMode();
        //FixDarkScrollBar();
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
        if (wcscmp(buf, L"Edit") == 0)
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

LRESULT CALLBACK DarkDialogSubProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    SubData data = g_subData[hDlg];
    TCHAR buf[256];
    GetWindowText(hDlg, buf, 256);
    switch (message)
    {
    case WM_CTLCOLORDLG:
        dbgLog(L"WM_CTLCOLORDLG SUBC");
    case WM_CTLCOLORSCROLLBAR:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORMSGBOX:
    case WM_CTLCOLORSTATIC:
    {
        if (g_isDarkModeSupported && g_isDarkModeEnabled)
        {
            dbgLog(L"WM_CTLCO* SUBC");
            COLORREF darkTextColor = RGB(255, 255, 255);
            COLORREF darkBkColor = RGB(1, 1, 1);
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdc, darkTextColor);
            SetBkColor(hdc, darkBkColor);
            static HBRUSH hbrBkgnd = 0;
            if (!hbrBkgnd)
                hbrBkgnd = (HBRUSH)GetStockObject(BLACK_BRUSH); //hbrBkgnd = CreateSolidBrush(darkBkColor);
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
                    g_isDarkModeInited = false;
                }
                break;
            case WM_CREATE:
                InitDarkModeForWindow(msg->hwnd);
                break;
            case WM_INITDIALOG:
            {
                if (g_isDarkModeSupported)
                {
                    dbgLog(L"WM_INITDIALOG");
                    HWND deff = (HWND)msg->wParam; //def. focus control
                    HWND hWnd = msg->hwnd; //GetParent(deff); //msg->hwnd

                    SetWindowTheme(hWnd, L"Explorer", NULL);
                    SetWindowTheme(GetDlgItem(hWnd, IDOK), L"Explorer", NULL);

                    _AllowDarkModeForWindow(hWnd, true);
                    RefreshTitleBarThemeColor(hWnd);
                    EnableThemeDialogTexture(hWnd, ETDT_DISABLE);
                    SendMessageW(hWnd, WM_THEMECHANGED, 0, 0);
                }
                if (GetAncestor(msg->hwnd, GA_ROOT) == msg->hwnd)
                {
                    HWND hWnd = msg->hwnd;// (HWND)msg->wParam;
                    SubData data;
                    data.origProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)&DarkDialogSubProc);
                    g_subData[hWnd] = data;
                }
                break;// return (INT_PTR)TRUE;
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
            //case WM_PAINT:
            //    if (g_isDarkModeSupported && GetAncestor(msg->hwnd, GA_ROOT) == msg->hwnd)
            //    {
            //        dbgLog(L"WM_PAINT");
            //        PAINTSTRUCT ps;
            //        HDC hdc = BeginPaint(msg->hwnd, &ps);
            //        HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
            //        RECT rc;
            //        GetClientRect(msg->hwnd, &rc);
            //        FillRect(hdc, &rc, brush);
            //        EndPaint(msg->hwnd, &ps);
            //    }
            //break;
            case WM_ERASEBKGND:
                if (g_isDarkModeSupported && GetAncestor(msg->hwnd, GA_ROOT) == msg->hwnd)
                {
                    dbgLog(L"WM_ERASEBKGND");
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
                    //SelectObject(hdc, brush);
                    FillRect(hdc, &rc, brush);
                    ////DeleteObject(brush);
                    //return TRUE;

                    //HTHEME hTheme = OpenThemeData(nullptr, L"WINDOW");
                    //if (hTheme)
                    //{
                    //    HBRUSH brush = GetThemeSysColorBrush(hTheme, TMT_WINDOW);
                   //     HBRUSH hOldBrush = (HBRUSH)SetClassLongPtr(msg->hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)brush);
                   //     DeleteObject(hOldBrush);
                    //}
                    //CloseThemeData(hTheme);
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
        if (_tcsstr(buf, _T("cleanmgr.exe"))) //if (_tcscmp(buf, _T("c:\\windows\\system32\\psr.exe")) == 0)
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
