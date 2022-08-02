#pragma once
#include <windows.h>
#include <tchar.h>
#ifdef PAYLOAD_BUILD
#define PAYLOADAPI    __declspec(dllexport)
#else
#define PAYLOADAPI    __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

PAYLOADAPI LRESULT CALLBACK DarkHookProc(INT code, WPARAM wParam, LPARAM lParam);
PAYLOADAPI LRESULT CALLBACK DarkHookProcRet(INT code, WPARAM wParam, LPARAM lParam);

#ifdef __cplusplus
} // extern "C"


//Win32 APIs. Credits to:
//https://github.com/ysc3839/win32-darkmode/blob/master/win32-darkmode/DarkMode.h
//https://gist.github.com/rounk-ctrl/b04e5622e30e0d62956870d5c22b7017
//https://github.com/notepad-plus-plus/notepad-plus-plus/labels/dark%20mode

enum IMMERSIVE_HC_CACHE_MODE
{
	IHCM_USE_CACHED_VALUE,
	IHCM_REFRESH
};

// 1903 18362
enum PreferredAppMode
{
	Default,
	AllowDark,
	ForceDark,
	ForceLight,
	Max
};

enum WINDOWCOMPOSITIONATTRIB
{
	WCA_UNDEFINED = 0,
	WCA_NCRENDERING_ENABLED = 1,
	WCA_NCRENDERING_POLICY = 2,
	WCA_TRANSITIONS_FORCEDISABLED = 3,
	WCA_ALLOW_NCPAINT = 4,
	WCA_CAPTION_BUTTON_BOUNDS = 5,
	WCA_NONCLIENT_RTL_LAYOUT = 6,
	WCA_FORCE_ICONIC_REPRESENTATION = 7,
	WCA_EXTENDED_FRAME_BOUNDS = 8,
	WCA_HAS_ICONIC_BITMAP = 9,
	WCA_THEME_ATTRIBUTES = 10,
	WCA_NCRENDERING_EXILED = 11,
	WCA_NCADORNMENTINFO = 12,
	WCA_EXCLUDED_FROM_LIVEPREVIEW = 13,
	WCA_VIDEO_OVERLAY_ACTIVE = 14,
	WCA_FORCE_ACTIVEWINDOW_APPEARANCE = 15,
	WCA_DISALLOW_PEEK = 16,
	WCA_CLOAK = 17,
	WCA_CLOAKED = 18,
	WCA_ACCENT_POLICY = 19,
	WCA_FREEZE_REPRESENTATION = 20,
	WCA_EVER_UNCLOAKED = 21,
	WCA_VISUAL_OWNER = 22,
	WCA_HOLOGRAPHIC = 23,
	WCA_EXCLUDED_FROM_DDA = 24,
	WCA_PASSIVEUPDATEMODE = 25,
	WCA_USEDARKMODECOLORS = 26,
	WCA_LAST = 27
};

struct WINDOWCOMPOSITIONATTRIBDATA
{
	WINDOWCOMPOSITIONATTRIB Attrib;
	PVOID pvData;
	SIZE_T cbData;
};

#endif
