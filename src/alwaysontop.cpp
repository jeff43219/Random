#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>

#define HOTKEY_ID 1
#define TIMER_UPDATE 1
#define WM_CLEANUP (WM_USER + 1)

// ------------------------------------------------------------------
// Global state
HWND g_hMainWnd    = NULL;    // Handle to our hidden main window
HWND g_hTargetWnd  = NULL;    // Currently pinned window
HWND g_hOverlay    = NULL;    // Overlay window (purple border)
HWINEVENTHOOK g_hHook = NULL; // WinEvent hook handle

// ------------------------------------------------------------------
// Forward declarations
LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK OverlayWndProc(HWND, UINT, WPARAM, LPARAM);
void TogglePin(HWND hForeground);
void CreateOverlay(HWND hTarget);
void DestroyOverlay();
void UpdateOverlayPos(HWND hTarget);
void InstallHook(HWND hTarget);
void RemoveHook();

// ------------------------------------------------------------------
// Window class names
const TCHAR MAIN_CLASS[]    = _T("AlwaysOnTopMainClass");
const TCHAR OVERLAY_CLASS[] = _T("AlwaysOnTopOverlayClass");

// ------------------------------------------------------------------
// Main entry point
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
                   _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nShowCmd);

    // 1. Register main window class (hidden)
    WNDCLASSEX wcMain = { sizeof(wcMain) };
    wcMain.lpfnWndProc   = MainWndProc;
    wcMain.hInstance     = hInstance;
    wcMain.lpszClassName = MAIN_CLASS;
    if (!RegisterClassEx(&wcMain)) return 1;

    // 2. Register overlay window class
    WNDCLASSEX wcOverlay = { sizeof(wcOverlay) };
    wcOverlay.style       = CS_HREDRAW | CS_VREDRAW;
    wcOverlay.lpfnWndProc = OverlayWndProc;
    wcOverlay.hInstance   = hInstance;
    wcOverlay.hCursor     = LoadCursor(NULL, IDC_ARROW);
    wcOverlay.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wcOverlay.lpszClassName = OVERLAY_CLASS;
    if (!RegisterClassEx(&wcOverlay)) return 1;

    // 3. Create the hidden main window (used for hotkey and timer)
    g_hMainWnd = CreateWindowEx(0, MAIN_CLASS, _T(""),
                                WS_OVERLAPPED, 0, 0, 0, 0,
                                NULL, NULL, hInstance, NULL);
    if (!g_hMainWnd) return 1;

    // 4. Register global hotkey with the main window
    if (!RegisterHotKey(g_hMainWnd, HOTKEY_ID,
                        MOD_CONTROL | MOD_SHIFT | MOD_ALT, 0x54)) {
        MessageBox(NULL,
                   _T("Could not register hotkey Ctrl+Shift+Alt+T.\n")
                   _T("It may be in use by another application."),
                   _T("Always-on-Top Utility"),
                   MB_OK | MB_ICONERROR);
        return 1;
    }

    // 5. Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterHotKey(g_hMainWnd, HOTKEY_ID);
    return 0;
}

// ------------------------------------------------------------------
// Main window procedure (hidden, processes hotkey and timer)
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_HOTKEY:
            if (wp == HOTKEY_ID) {
                HWND fg = GetForegroundWindow();
                if (fg) {
                    // Check class name for system elements
                    TCHAR className[256];
                    bool isSystem = false;
                    if (GetClassName(fg, className, _countof(className))) {
                        if (_tcscmp(className, _T("Shell_TrayWnd")) == 0 ||
                            _tcscmp(className, _T("Progman")) == 0 ||
                            _tcscmp(className, _T("WorkerW")) == 0) {
                            isSystem = true;
                        }
                    }

                    // Check process name for blocked apps
                    DWORD pid;
                    GetWindowThreadProcessId(fg, &pid);
                    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                    if (hProc) {
                        TCHAR procName[MAX_PATH];
                        DWORD len = MAX_PATH;
                        if (QueryFullProcessImageName(hProc, 0, procName, &len)) {
                            // Extract just the filename
                            TCHAR* fileName = _tcsrchr(procName, '\\');
                            if (fileName) fileName++; else fileName = procName;

                            // Block Roblox and Prism Launcher
                            if (_tcsicmp(fileName, _T("RobloxPlayerBeta.exe")) == 0 ||
                                _tcsicmp(fileName, _T("prismlauncher.exe")) == 0) {
                                isSystem = true;
                            }
                        }
                        CloseHandle(hProc);
                    }

                    if (isSystem) {
                        MessageBox(NULL,
                                   _T("Cannot make this window always-on-top."),
                                   _T("Always-on-Top"),
                                   MB_OK | MB_ICONERROR);
                        break;
                    }

                    TogglePin(fg);
                }
            }
            break;

        case WM_TIMER:
            if (wp == TIMER_UPDATE && g_hTargetWnd && g_hOverlay) {
                if (IsWindow(g_hTargetWnd)) {
                    UpdateOverlayPos(g_hTargetWnd);
                } else {
                    DestroyOverlay();
                    g_hTargetWnd = NULL;
                    KillTimer(hwnd, TIMER_UPDATE);
                }
            }
            break;

        case WM_CLEANUP:
            DestroyOverlay();
            RemoveHook();
            g_hTargetWnd = NULL;
            KillTimer(hwnd, TIMER_UPDATE);
            break;

        case WM_DESTROY:
            RemoveHook();
            KillTimer(hwnd, TIMER_UPDATE);
            DestroyOverlay();
            PostQuitMessage(0);
            break;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ------------------------------------------------------------------
// Core logic: toggle always-on-top for a single window
void TogglePin(HWND hForeground) {
    if (hForeground == g_hTargetWnd) {
        // Unpin current window
        SetWindowPos(g_hTargetWnd, HWND_NOTOPMOST,
                     0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        if (g_hMainWnd) KillTimer(g_hMainWnd, TIMER_UPDATE);
        DestroyOverlay();
        RemoveHook();
        g_hTargetWnd = NULL;
        MessageBeep(MB_OK);
        return;
    }

    // Unpin old window if different
    if (g_hTargetWnd != NULL) {
        SetWindowPos(g_hTargetWnd, HWND_NOTOPMOST,
                     0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        if (g_hMainWnd) KillTimer(g_hMainWnd, TIMER_UPDATE);
        DestroyOverlay();
        RemoveHook();
    }

    // Pin new window
    SetWindowPos(hForeground, HWND_TOPMOST,
                 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    g_hTargetWnd = hForeground;
    CreateOverlay(hForeground);
    InstallHook(hForeground);
    if (g_hMainWnd) SetTimer(g_hMainWnd, TIMER_UPDATE, 50, NULL);
    MessageBeep(MB_OK);
}

// ------------------------------------------------------------------
// Overlay window creation
void CreateOverlay(HWND hTarget) {
    if (g_hOverlay) {
        UpdateOverlayPos(hTarget);
        return;
    }

    RECT rc;
    GetWindowRect(hTarget, &rc);

    g_hOverlay = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        OVERLAY_CLASS,
        _T(""),
        WS_POPUP,
        rc.left, rc.top,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, GetModuleHandle(NULL), NULL);

    if (!g_hOverlay) return;

    SetLayeredWindowAttributes(g_hOverlay, RGB(255, 0, 255), 0, LWA_COLORKEY);
    ShowWindow(g_hOverlay, SW_SHOWNOACTIVATE);
}

void DestroyOverlay() {
    if (g_hOverlay) {
        DestroyWindow(g_hOverlay);
        g_hOverlay = NULL;
    }
}

void UpdateOverlayPos(HWND hTarget) {
    if (!g_hOverlay) return;
    if (!IsWindow(hTarget)) return;

    RECT rc;
    if (GetWindowRect(hTarget, &rc)) {
        SetWindowPos(g_hOverlay, HWND_TOPMOST,
                     rc.left, rc.top,
                     rc.right - rc.left, rc.bottom - rc.top,
                     SWP_NOACTIVATE | SWP_NOSENDCHANGING);
    }
}

// ------------------------------------------------------------------
// Event hook installation
void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event,
                           HWND hwnd, LONG idObject, LONG idChild,
                           DWORD dwEventThread, DWORD dwmsEventTime) {
    if (hwnd != g_hTargetWnd) return;
    if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF) return;

    if (event == EVENT_OBJECT_LOCATIONCHANGE) {
        if (g_hMainWnd) PostMessage(g_hMainWnd, WM_TIMER, TIMER_UPDATE, 0);
    }
    else if (event == EVENT_OBJECT_DESTROY) {
        if (g_hMainWnd) {
            KillTimer(g_hMainWnd, TIMER_UPDATE);
            PostMessage(g_hMainWnd, WM_CLEANUP, 0, 0);
        }
    }
}

void InstallHook(HWND hTarget) {
    RemoveHook();
    DWORD targetThreadId = GetWindowThreadProcessId(hTarget, NULL);

    g_hHook = SetWinEventHook(
        EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE,
        NULL, WinEventProc,
        GetCurrentProcessId(), targetThreadId,
        WINEVENT_OUTOFCONTEXT);

    SetWinEventHook(
        EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY,
        NULL, WinEventProc,
        GetCurrentProcessId(), targetThreadId,
        WINEVENT_OUTOFCONTEXT);
}

void RemoveHook() {
    if (g_hHook) {
        UnhookWinEvent(g_hHook);
        g_hHook = NULL;
    }
}

// ------------------------------------------------------------------
// Overlay window procedure: paints a purple border
LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);

            // Fill with colour key (magenta = transparent)
            HBRUSH hTrans = CreateSolidBrush(RGB(255, 0, 255));
            FillRect(hdc, &rc, hTrans);
            DeleteObject(hTrans);

            // Purple border (3px)
            HPEN hPen = CreatePen(PS_SOLID, 3, RGB(128, 0, 128));
            HGDIOBJ hOldPen = SelectObject(hdc, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, rc.left + 1, rc.top + 1,
                      rc.right - 1, rc.bottom - 1);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hPen);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}