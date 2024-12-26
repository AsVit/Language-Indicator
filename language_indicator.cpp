#include <windows.h>
#include <shellapi.h>
#include <fstream>
#include <string>

HINSTANCE hInstance;
HWND hwnd;
HFONT font;
NOTIFYICONDATA nid;
HKL previousKeyboardLayout = 0;
HHOOK hKeyboardLayoutHook = NULL;

LRESULT CALLBACK KeyboardLayoutProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        if (wParam == WM_INPUTLANGCHANGE) {
            HKL hkl = (HKL)lParam;
            if (hkl != previousKeyboardLayout) {
                previousKeyboardLayout = hkl;
                PostMessage(hwnd, WM_INPUTLANGCHANGE, 0, 0);
            }
        }
    }
    return CallNextHookEx(hKeyboardLayoutHook, nCode, wParam, lParam);
}

class LanguageIndicator {
public:
    static LanguageIndicator& GetInstance() {
        static LanguageIndicator instance;
        return instance;
    }

    bool Initialize(HINSTANCE hInst) {
        hInstance = hInst;
        WNDCLASS wc = { 0 };
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = CLASS_NAME;
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

        if (!RegisterClass(&wc)) {
            LogMessage("Failed to register window class.");
            return false;
        }

        hwnd = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
            CLASS_NAME,
            "Language Indicator",
            WS_POPUP,
            0, 0, 150, 50,
            NULL, NULL, hInstance, NULL);

        if (!hwnd) {
            LogMessage("Failed to create window.");
            return false;
        }

        font = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, "Arial");

        SetLayeredWindowAttributes(hwnd, RGB(255, 255, 255), 200, LWA_ALPHA);

        AddTrayIcon(hwnd);
        CenterWindow(hwnd);

        hKeyboardLayoutHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardLayoutProc, hInstance, 0);
        if (!hKeyboardLayoutHook) {
            LogMessage("Failed to set keyboard layout hook.");
            return false;
        }

        return true;
    }

    void Run() {
        MSG msg = { 0 };
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    void Cleanup() {
        if (hKeyboardLayoutHook) {
            UnhookWindowsHookEx(hKeyboardLayoutHook);
            hKeyboardLayoutHook = NULL;
        }
        RemoveTrayIcon();
    }

private:
    LanguageIndicator() {}
    ~LanguageIndicator() {
        if (font) DeleteObject(font);
    }

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
        case WM_CREATE:
            UpdateLanguageText(hWnd);
            break;

        case WM_INPUTLANGCHANGE:
            UpdateLanguageText(hWnd);
            ShowWindowWithTimer(hWnd);
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            SetTextColor(hdc, RGB(0, 0, 0));
            SetBkMode(hdc, TRANSPARENT);
            SelectObject(hdc, font);

            RECT rect;
            GetClientRect(hWnd, &rect);

            TCHAR text[10];
            GetWindowText(hWnd, text, 10);
            DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            EndPaint(hWnd, &ps);
        } break;

        case WM_TIMER:
            if (wParam == 1) {
                FadeOutWindow(hWnd);
                KillTimer(hWnd, 1);
                LogMessage("Window hidden by timer.");
            }
            break;

        case WM_APP + 1:
            if (LOWORD(lParam) == WM_RBUTTONDOWN) {
                HMENU hMenu = CreatePopupMenu();
                AppendMenu(hMenu, MF_STRING, 1, "Exit");

                POINT pt;
                GetCursorPos(&pt);
                SetForegroundWindow(hWnd);

                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hWnd, NULL);
                DestroyMenu(hMenu);

                if (cmd == 1) {
                    PostQuitMessage(0);
                }
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
        }

        return 0;
    }

    static void UpdateLanguageText(HWND hWnd) {
        HKL hkl = GetKeyboardLayout(0);
        if (hkl != previousKeyboardLayout) {
            LANGID langId = LOWORD(hkl);
            TCHAR langName[10] = { 0 };

            if (GetLocaleInfo(langId, LOCALE_SISO639LANGNAME, langName, sizeof(langName) / sizeof(TCHAR))) {
                SetWindowText(hWnd, langName);
                InvalidateRect(hWnd, NULL, TRUE);
                UpdateWindow(hWnd);
                LogMessage("Language updated.");
            }
            else {
                LogMessage("Failed to get language info.");
            }

            previousKeyboardLayout = hkl;
        }
    }

    static void AddTrayIcon(HWND hWnd) {
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = hWnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_APP + 1;
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        lstrcpyn(nid.szTip, "Language Indicator", sizeof(nid.szTip) / sizeof(TCHAR));
        Shell_NotifyIcon(NIM_ADD, &nid);
        LogMessage("Tray icon added.");
    }

    static void RemoveTrayIcon() {
        Shell_NotifyIcon(NIM_DELETE, &nid);
        LogMessage("Tray icon removed.");
    }

    static void CenterWindow(HWND hWnd) {
        RECT rect;
        GetWindowRect(hWnd, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        int x = (screenWidth - width) / 2;
        int y = (screenHeight - height) / 2;

        SetWindowPos(hWnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

    static void LogMessage(const std::string& message) {
        std::ofstream logFile("log.txt", std::ios::app);
        if (logFile.is_open()) {
            logFile << message << std::endl;
            logFile.close();
        }
    }

    static void ShowWindowWithTimer(HWND hWnd) {
        FadeInWindow(hWnd);
        SetTimer(hWnd, 1, 1000, NULL); // Timer ID 1, 1 second
        LogMessage("Window shown with timer.");
    }

    static void FadeInWindow(HWND hWnd) {
        ShowWindow(hWnd, SW_SHOW);
        CenterWindow(hWnd); // Ensure the window is centered
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        for (int i = 0; i <= 255; i += 10) {
            SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), i, LWA_ALPHA);
            Sleep(20);
        }
    }

    static void FadeOutWindow(HWND hWnd) {
        for (int i = 255; i >= 0; i -= 10) {
            SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), i, LWA_ALPHA);
            Sleep(20);
        }
        ShowWindow(hWnd, SW_HIDE);
    }

    static const TCHAR CLASS_NAME[];
};

const TCHAR LanguageIndicator::CLASS_NAME[] = TEXT("LanguageIndicatorClass");

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    LanguageIndicator& app = LanguageIndicator::GetInstance();
    if (!app.Initialize(hInstance)) {
        return 1;
    }

    app.Run();
    app.Cleanup();
    return 0;
}
