#include "framework.h"
#include "DarkEnforcer.h"
#include <commctrl.h>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(linker,"/manifestdependency:\"" \
    "type='win32' " \
    "name='Microsoft.Windows.Common-Controls' " \
    "version='6.0.0.0' " \
    "processorArchitecture='*' "  \
    "publicKeyToken='6595b64144ccf1df' " \
    "language='*'\"")

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    INITCOMMONCONTROLSEX InitCtrls;
    InitCtrls.dwSize = sizeof(InitCtrls);
    InitCtrls.dwICC = ICC_WIN95_CLASSES;

    HINSTANCE hinstDLL = LoadLibrary(_T("DarkDll.dll"));
    HOOKPROC DarkHookProc = (HOOKPROC)GetProcAddress(hinstDLL, "DarkHookProc");
    HOOKPROC DarkHookProcRet = (HOOKPROC)GetProcAddress(hinstDLL, "DarkHookProcRet");
    HHOOK hook = SetWindowsHookEx(WH_CALLWNDPROC, DarkHookProc, hinstDLL, 0);
    HHOOK hookRet = SetWindowsHookEx(WH_CALLWNDPROCRET, DarkHookProcRet, hinstDLL, 0);

    //local hooks
    //HHOOK hook = SetWindowsHookEx(WH_CALLWNDPROC, DarkHookProc, hInstance, GetCurrentThreadId());
    ////HHOOK hookRet = SetWindowsHookEx(WH_CALLWNDPROCRET, DarkHookProcRet, hInstance, GetCurrentThreadId());

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_DARKENFORCER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    nCmdShow = SW_MINIMIZE;

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DARKENFORCER));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    UnhookWindowsHookEx(hookRet);
    UnhookWindowsHookEx(hook);

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DARKENFORCER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_DARKENFORCER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        HWND btn = CreateWindow(L"Button", L"My button", WS_CHILD | WS_TABSTOP | WS_VISIBLE, 11, 11, 111, 55, hWnd, (HMENU)13, hInst, 0);
    }
        break;
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
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

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSCROLLBAR:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORMSGBOX:
    case WM_CTLCOLORSTATIC:
    {/*
        {
            COLORREF darkTextColor = RGB(0, 255, 255);
            COLORREF darkBkColor = RGB(255, 1, 1);
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdc, darkTextColor);
            SetBkColor(hdc, darkBkColor);
            static HBRUSH hbrBkgnd = 0;
            if (!hbrBkgnd)
                hbrBkgnd = (HBRUSH)GetStockObject(BLACK_BRUSH); //hbrBkgnd = CreateSolidBrush(darkBkColor);
            //UpdateWindow(msg->hwnd);
            return reinterpret_cast<INT_PTR>(hbrBkgnd);
        }*/
    }
    break;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
