#include <windows.h>
#include <shellapi.h>
#include <fstream>
#include <string>
#include <ctime>
#include <sstream>

HINSTANCE hInstance;
HWND hwnd;
HFONT font;
NOTIFYICONDATA nid;
HKL previousKeyboardLayout = 0;
HHOOK hKeyboardLayoutHook = NULL;

class LanguageIndicator;
static void LogMessage(const std::string& message);
static void UpdateLanguageText(HWND hWnd);
static void ShowWindowWithTimer(HWND hWnd);
static void CenterWindow(HWND hWnd);
static void FadeInWindow(HWND hWnd);
static void FadeOutWindow(HWND hWnd);

std::string GetTimestamp() {
    auto now = std::time(nullptr);
    auto localTime = *std::localtime(&now);
    std::ostringstream oss;
    oss << '[' << (localTime.tm_year + 1900) << '-'
        << (localTime.tm_mon + 1) << '-'
        << localTime.tm_mday << ' '
        << localTime.tm_hour << ':'
        << localTime.tm_min << ':'
        << localTime.tm_sec << "] ";
    return oss.str();
}

std::string GetLastErrorAsString() {
    DWORD error = GetLastError();
    if (error == 0) return "No error";

    LPVOID lpMsgBuf;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf,
        0,
        NULL
    );

    std::string message((char*)lpMsgBuf);
    LocalFree(lpMsgBuf);
    return "Error " + std::to_string(error) + ": " + message;
}

static void LogMessage(const std::string& message) {
    std::ofstream logFile("lang_indicator_debug.log", std::ios::app);
    if (logFile.is_open()) {
        logFile << GetTimestamp() << message << std::endl;
        logFile.close();
    }
}

LRESULT CALLBACK KeyboardLayoutProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;

        static HKL currentLayout = GetKeyboardLayout(0);
        HKL newLayout = GetKeyboardLayout(0);

        std::ostringstream oss;
        oss << "Hook - Current layout: 0x" << std::hex << (UINT_PTR)currentLayout
            << ", New layout: 0x" << (UINT_PTR)newLayout
            << ", Active window handle: 0x" << std::hex << (UINT_PTR)GetForegroundWindow();
        LogMessage(oss.str());

        if (newLayout != currentLayout) {
            currentLayout = newLayout;
            LogMessage("Layout change detected - Posting WM_INPUTLANGCHANGE");
            PostMessage(hwnd, WM_INPUTLANGCHANGE, 0, (LPARAM)newLayout);
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
        LogMessage("Initializing Language Indicator");
        hInstance = hInst;
        WNDCLASS wc = { 0 };
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = CLASS_NAME;
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

        if (!RegisterClass(&wc)) {
            LogMessage("Failed to register window class: " + GetLastErrorAsString());
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
            LogMessage("Failed to create window: " + GetLastErrorAsString());
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
            LogMessage("Failed to set keyboard layout hook: " + GetLastErrorAsString());
            return false;
        }

        LogMessage("Initialization complete");
        return true;
    }

    void Run() {
        LogMessage("Starting message loop");
        MSG msg = { 0 };
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        LogMessage("Message loop ended");
    }

    void Cleanup() {
        LogMessage("Starting cleanup");
        if (hKeyboardLayoutHook) {
            UnhookWindowsHookEx(hKeyboardLayoutHook);
            hKeyboardLayoutHook = NULL;
        }
        RemoveTrayIcon();
        LogMessage("Cleanup complete");
    }

private:
    LanguageIndicator() {
        LogMessage("LanguageIndicator constructor called");
    }

    ~LanguageIndicator() {
        LogMessage("LanguageIndicator destructor called");
        if (font) DeleteObject(font);
    }

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        std::ostringstream oss;
        oss << "WindowProc - Message: 0x" << std::hex << uMsg
            << ", wParam: 0x" << wParam
            << ", lParam: 0x" << lParam;
        LogMessage(oss.str());

        switch (uMsg) {
        case WM_CREATE:
            UpdateLanguageText(hWnd);
            break;

        case WM_INPUTLANGCHANGE: {
            LogMessage("Processing WM_INPUTLANGCHANGE");
            HWND activeWindow = GetForegroundWindow();
            oss.str("");
            oss << "Active window handle: 0x" << std::hex << (UINT_PTR)activeWindow;
            LogMessage(oss.str());

            char className[256];
            GetClassNameA(activeWindow, className, sizeof(className));
            oss.str("");
            oss << "Active window class: " << className;
            LogMessage(oss.str());

            UpdateLanguageText(hWnd);
            ShowWindowWithTimer(hWnd);
            break;
        }

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
            break;
        }

        case WM_TIMER:
            if (wParam == 1) {
                LogMessage("Timer triggered - hiding window");
                FadeOutWindow(hWnd);
                KillTimer(hWnd, 1);
            }
            break;

        case WM_APP + 1:
            if (LOWORD(lParam) == WM_RBUTTONDOWN) {
                LogMessage("Tray icon right-clicked - showing menu");
                HMENU hMenu = CreatePopupMenu();
                AppendMenu(hMenu, MF_STRING, 1, "Exit");

                POINT pt;
                GetCursorPos(&pt);
                SetForegroundWindow(hWnd);

                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hWnd, NULL);
                DestroyMenu(hMenu);

                if (cmd == 1) {
                    LogMessage("Exit menu item selected");
                    PostQuitMessage(0);
                }
            }
            break;

        case WM_DESTROY:
            LogMessage("WM_DESTROY received");
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
        }

        return 0;
    }

    static void UpdateLanguageText(HWND hWnd) {
        LogMessage("UpdateLanguageText - Start");

        HKL hkl = GetKeyboardLayout(0);
        std::ostringstream oss;
        oss << "Current keyboard layout: 0x" << std::hex << (UINT_PTR)hkl;
        LogMessage(oss.str());

        if (hkl != previousKeyboardLayout) {
            LANGID langId = LOWORD(hkl);
            TCHAR langName[10] = { 0 };

            if (GetLocaleInfo(langId, LOCALE_SISO639LANGNAME, langName, sizeof(langName) / sizeof(TCHAR))) {
                oss.str("");
                oss << "Language name: " << langName;
                LogMessage(oss.str());

                SetWindowText(hWnd, langName);
                InvalidateRect(hWnd, NULL, TRUE);
                UpdateWindow(hWnd);
                LogMessage("Window text and display updated");
            }
            else {
                LogMessage("Failed to get language info: " + GetLastErrorAsString());
            }

            previousKeyboardLayout = hkl;
        }

        LogMessage("UpdateLanguageText - End");
    }

    static void AddTrayIcon(HWND hWnd) {
        LogMessage("Adding tray icon");
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = hWnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_APP + 1;
        nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        lstrcpyn(nid.szTip, "Language Indicator", sizeof(nid.szTip) / sizeof(TCHAR));
        if (Shell_NotifyIcon(NIM_ADD, &nid)) {
            LogMessage("Tray icon added successfully");
        }
        else {
            LogMessage("Failed to add tray icon: " + GetLastErrorAsString());
        }
    }

    static void RemoveTrayIcon() {
        LogMessage("Removing tray icon");
        if (Shell_NotifyIcon(NIM_DELETE, &nid)) {
            LogMessage("Tray icon removed successfully");
        }
        else {
            LogMessage("Failed to remove tray icon: " + GetLastErrorAsString());
        }
    }

    static void CenterWindow(HWND hWnd) {
        LogMessage("Centering window");
        RECT rect;
        GetWindowRect(hWnd, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        int x = (screenWidth - width) / 2;
        int y = (screenHeight - height) / 2;

        std::ostringstream oss;
        oss << "Window position - X: " << x << ", Y: " << y
            << ", Width: " << width << ", Height: " << height;
        LogMessage(oss.str());

        SetWindowPos(hWnd, HWND_TOPMOST, x, y, width, height, SWP_NOZORDER);
    }

    static void ShowWindowWithTimer(HWND hWnd) {
        LogMessage("ShowWindowWithTimer - Starting fade in");
        FadeInWindow(hWnd);
        SetTimer(hWnd, 1, 1000, NULL);
        LogMessage("Timer set for 1 second");
    }

    static void FadeInWindow(HWND hWnd) {
        LogMessage("FadeInWindow - Start");
        ShowWindow(hWnd, SW_SHOW);
        CenterWindow(hWnd);
        SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        for (int i = 0; i <= 255; i += 10) {
            SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), i, LWA_ALPHA);
            Sleep(20);
        }
        LogMessage("FadeInWindow - Complete");
    }

    static void FadeOutWindow(HWND hWnd) {
        LogMessage("FadeOutWindow - Start");
        for (int i = 255; i >= 0; i -= 10) {
            SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), i, LWA_ALPHA);
            Sleep(20);
        }
        ShowWindow(hWnd, SW_HIDE);
        LogMessage("FadeOutWindow - Complete");
    }

    static const TCHAR CLASS_NAME[];
};

const TCHAR LanguageIndicator::CLASS_NAME[] = TEXT("LanguageIndicatorClass");

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    LogMessage("Application starting");

    LanguageIndicator& app = LanguageIndicator::GetInstance();
    if (!app.Initialize(hInstance)) {
        LogMessage("Initialization failed - exiting");
        return 1;
    }

    app.Run();
    app.Cleanup();

    LogMessage("Application ending");
    return 0;
}
