#include <windows.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <random>
#include <string>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winmm.lib")

// Control IDs
#define ID_EDIT_HOURS   101
#define ID_EDIT_MINS    102
#define ID_EDIT_SECS    103
#define ID_EDIT_MS      104
#define ID_COMBO_BTN    105
#define ID_RADIO_REP1   106
#define ID_RADIO_REP2   107
#define ID_EDIT_REP     108
#define ID_BTN_START    109
#define ID_BTN_STOP     110
#define ID_CHECK_OFFSET 111
#define ID_EDIT_OFFSET  112
#define ID_BTN_HOTKEY   113

std::atomic<bool> g_clicking(false);
std::atomic<double> g_totalDelayMs(100.0);
std::atomic<bool> g_useOffset(false);
std::atomic<double> g_offsetMs(40.0);
std::atomic<int> g_currentHotkey(VK_F6);

HWND hEditHours, hEditMins, hEditSecs, hEditMs, hComboBtn, hRadioRep1, hRadioRep2, hEditRep, hCheckOffset, hEditOffset, hBtnHotkey, hBtnStart, hBtnStop, hStatus;
HBRUSH g_hDeepBlackBrush = NULL;
HBRUSH g_hDarkGrayBrush = NULL;
HFONT g_hModernFont = NULL;
bool g_waitingForHotkey = false;

// High-precision worker thread with support for Left, Right, and Middle clicks
void ClickerThread() {
    timeBeginPeriod(1);
    std::random_device rd;
    std::mt19937 gen(rd());

    while (true) {
        if (g_clicking) {
            INPUT inputs[2] = {};
            ZeroMemory(inputs, sizeof(inputs));

            int selectedBtn = SendMessageW(hComboBtn, CB_GETCURSEL, 0, 0);

            inputs[0].type = INPUT_MOUSE;
            inputs[1].type = INPUT_MOUSE;

            if (selectedBtn == 1) { // Right Click
                inputs[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
                inputs[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
            } else if (selectedBtn == 2) { // Middle Click
                inputs[0].mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
                inputs[1].mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
            } else { // Left Click (Default)
                inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
            }

            SendInput(2, inputs, sizeof(INPUT));

            double baseDelay = g_totalDelayMs.load();
            double finalDelay = baseDelay;

            if (g_useOffset.load()) {
                double offset = g_offsetMs.load();
                std::uniform_real_distribution<double> dist(-offset, offset);
                finalDelay += dist(gen);
            }

            if (finalDelay < 0.05) finalDelay = 0.05;

            auto start = std::chrono::high_resolution_clock::now();
            while (true) {
                auto elapsed = std::chrono::high_resolution_clock::now() - start;
                double elapsedMs = std::chrono::duration<double, std::milli>(elapsed).count();
                if (elapsedMs >= finalDelay) break;
                
                if (finalDelay - elapsedMs > 1.0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(500));
                } else {
                    std::this_thread::yield();
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    timeEndPeriod(1);
}

void ApplyFontToChildren(HWND hwnd, HFONT hFont) {
    HWND hChild = GetWindow(hwnd, GW_CHILD);
    while (hChild != NULL) {
        SendMessage(hChild, WM_SETFONT, (WPARAM)hFont, TRUE);
        hChild = GetWindow(hChild, GW_HWNDNEXT);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        g_hDeepBlackBrush = CreateSolidBrush(RGB(18, 18, 18));
        g_hDarkGrayBrush = CreateSolidBrush(RGB(35, 35, 35));
        
        g_hModernFont = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
            OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

        HWND hTitle = CreateWindowW(L"STATIC", L"Edin's Auto Clicker", WS_VISIBLE | WS_CHILD, 20, 15, 300, 25, hwnd, NULL, NULL, NULL);

        HWND hLblInterval = CreateWindowW(L"STATIC", L"Click interval", WS_VISIBLE | WS_CHILD, 20, 48, 150, 20, hwnd, NULL, NULL, NULL);

        hEditHours = CreateWindowW(L"EDIT", L"0", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, 20, 72, 35, 22, hwnd, (HMENU)ID_EDIT_HOURS, NULL, NULL);
        HWND l1 = CreateWindowW(L"STATIC", L"hours", WS_VISIBLE | WS_CHILD, 60, 74, 40, 20, hwnd, NULL, NULL, NULL);

        hEditMins = CreateWindowW(L"EDIT", L"0", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, 110, 72, 35, 22, hwnd, (HMENU)ID_EDIT_MINS, NULL, NULL);
        HWND l2 = CreateWindowW(L"STATIC", L"mins", WS_VISIBLE | WS_CHILD, 150, 74, 35, 20, hwnd, NULL, NULL, NULL);

        hEditSecs = CreateWindowW(L"EDIT", L"0", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, 190, 72, 35, 22, hwnd, (HMENU)ID_EDIT_SECS, NULL, NULL);
        HWND l3 = CreateWindowW(L"STATIC", L"secs", WS_VISIBLE | WS_CHILD, 230, 74, 35, 20, hwnd, NULL, NULL, NULL);

        hEditMs = CreateWindowW(L"EDIT", L"0.1", WS_VISIBLE | WS_CHILD | WS_BORDER, 270, 72, 50, 22, hwnd, (HMENU)ID_EDIT_MS, NULL, NULL);
        HWND l4 = CreateWindowW(L"STATIC", L"milliseconds", WS_VISIBLE | WS_CHILD, 325, 74, 90, 20, hwnd, NULL, NULL, NULL);

        hCheckOffset = CreateWindowW(L"BUTTON", L"Random offset + -", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 20, 104, 135, 22, hwnd, (HMENU)ID_CHECK_OFFSET, NULL, NULL);
        hEditOffset = CreateWindowW(L"EDIT", L"40", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, 160, 104, 45, 22, hwnd, (HMENU)ID_EDIT_OFFSET, NULL, NULL);
        HWND l_off = CreateWindowW(L"STATIC", L"milliseconds", WS_VISIBLE | WS_CHILD, 215, 106, 90, 20, hwnd, NULL, NULL, NULL);

        HWND hGrpOptions = CreateWindowW(L"BUTTON", L"Click options", WS_VISIBLE | WS_CHILD | BS_GROUPBOX, 20, 134, 210, 75, hwnd, NULL, NULL, NULL);

        // Widen this label so "Mouse button:" doesn't get cut off
        HWND l5 = CreateWindowW(L"STATIC", L"Mouse button:", WS_VISIBLE | WS_CHILD, 28, 162, 95, 20, hwnd, NULL, NULL, NULL);
        
        hComboBtn = CreateWindowW(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | CBS_HASSTRINGS, 125, 160, 95, 120, hwnd, (HMENU)ID_COMBO_BTN, NULL, NULL);
        SendMessageW(hComboBtn, CB_ADDSTRING, 0, (LPARAM)L"Left");
        SendMessageW(hComboBtn, CB_ADDSTRING, 0, (LPARAM)L"Right");
        SendMessageW(hComboBtn, CB_ADDSTRING, 0, (LPARAM)L"Middle");
        SendMessageW(hComboBtn, CB_SETCURSEL, 0, 0);

        HWND hGrpRepeat = CreateWindowW(L"BUTTON", L"Click repeat", WS_VISIBLE | WS_CHILD | BS_GROUPBOX, 240, 134, 210, 75, hwnd, NULL, NULL, NULL);

        hRadioRep1 = CreateWindowW(L"BUTTON", L"Repeat", WS_VISIBLE | WS_CHILD | BS_RADIOBUTTON, 250, 155, 70, 20, hwnd, (HMENU)ID_RADIO_REP1, NULL, NULL);
        hEditRep = CreateWindowW(L"EDIT", L"1", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, 320, 155, 40, 20, hwnd, (HMENU)ID_EDIT_REP, NULL, NULL);
        HWND l6 = CreateWindowW(L"STATIC", L"times", WS_VISIBLE | WS_CHILD, 365, 157, 40, 20, hwnd, NULL, NULL, NULL);

        hRadioRep2 = CreateWindowW(L"BUTTON", L"Repeat until stopped", WS_VISIBLE | WS_CHILD | BS_RADIOBUTTON | WS_TABSTOP, 250, 180, 160, 20, hwnd, (HMENU)ID_RADIO_REP2, NULL, NULL);
        SendMessage(hRadioRep2, BM_SETCHECK, BST_CHECKED, 0);

        hBtnStart = CreateWindowW(L"BUTTON", L"Start (F6)", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 20, 220, 210, 38, hwnd, (HMENU)ID_BTN_START, NULL, NULL);
        hBtnStop  = CreateWindowW(L"BUTTON", L"Stop (F6)", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 240, 220, 210, 38, hwnd, (HMENU)ID_BTN_STOP, NULL, NULL);
        hBtnHotkey = CreateWindowW(L"BUTTON", L"Hotkey setting: F6 (Click to change)", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW, 20, 268, 430, 32, hwnd, (HMENU)ID_BTN_HOTKEY, NULL, NULL);

        hStatus = CreateWindowW(L"STATIC", L"Status: IDLE", WS_VISIBLE | WS_CHILD, 20, 312, 430, 20, hwnd, NULL, NULL, NULL);

        ApplyFontToChildren(hwnd, g_hModernFont);
        SendMessage(hTitle, WM_SETFONT, (WPARAM)g_hModernFont, TRUE);
        break;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pDrawItem = (LPDRAWITEMSTRUCT)lParam;
        if (pDrawItem->CtlID == ID_BTN_START || pDrawItem->CtlID == ID_BTN_STOP || pDrawItem->CtlID == ID_BTN_HOTKEY) {
            HDC hdc = pDrawItem->hDC;
            RECT rect = pDrawItem->rcItem;

            HBRUSH hBrush = NULL;
            if (pDrawItem->CtlID == ID_BTN_START) {
                hBrush = CreateSolidBrush(RGB(40, 167, 69)); // Vibrant Green
            } else if (pDrawItem->CtlID == ID_BTN_STOP) {
                hBrush = CreateSolidBrush(RGB(220, 53, 69)); // Vibrant Red
            } else {
                hBrush = CreateSolidBrush(RGB(45, 45, 45));  // Dark Charcoal
            }

            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);

            SetTextColor(hdc, RGB(255, 255, 255));
            SetBkMode(hdc, TRANSPARENT);
            SelectObject(hdc, g_hModernFont);
            
            wchar_t text[256];
            GetWindowTextW(pDrawItem->hwndItem, text, 256);
            DrawTextW(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, RGB(255, 105, 180)); // Hot Pink text
        SetBkColor(hdcStatic, RGB(18, 18, 18));
        return (INT_PTR)g_hDeepBlackBrush;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdcEdit = (HDC)wParam;
        SetTextColor(hdcEdit, RGB(255, 255, 255));
        SetBkColor(hdcEdit, RGB(12, 12, 12));
        return (INT_PTR)g_hDeepBlackBrush;
    }
    case WM_CTLCOLORDLG: {
        return (INT_PTR)g_hDeepBlackBrush;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == ID_BTN_START) {
            wchar_t bufH[10], bufM[10], bufS[10], bufMs[20], bufOff[20];
            GetWindowTextW(hEditHours, bufH, 10);
            GetWindowTextW(hEditMins, bufM, 10);
            GetWindowTextW(hEditSecs, bufS, 10);
            GetWindowTextW(hEditMs, bufMs, 20);
            GetWindowTextW(hEditOffset, bufOff, 20);

            double msVal = _wtof(bufMs);
            double totalMs = (_wtof(bufH) * 3600000.0) + (_wtof(bufM) * 60000.0) + (_wtof(bufS) * 1000.0) + msVal;
            if (totalMs < 0.05) totalMs = 0.05;
            g_totalDelayMs = totalMs;

            g_useOffset = (IsDlgButtonChecked(hwnd, ID_CHECK_OFFSET) == BST_CHECKED);
            g_offsetMs = _wtof(bufOff);

            g_clicking = true;
            SetWindowTextW(hStatus, L"Status: ACTIVE 🔴");
        }
        else if (id == ID_BTN_STOP) {
            g_clicking = false;
            SetWindowTextW(hStatus, L"Status: PAUSED 🟢");
        }
        else if (id == ID_BTN_HOTKEY) {
            g_waitingForHotkey = true;
            SetWindowTextW(hBtnHotkey, L"Press any key or mouse button...");
            InvalidateRect(hBtnHotkey, NULL, TRUE);
        }
        else if (id == ID_RADIO_REP1) {
            CheckDlgButton(hwnd, ID_RADIO_REP1, BST_CHECKED);
            CheckDlgButton(hwnd, ID_RADIO_REP2, BST_UNCHECKED);
        }
        else if (id == ID_RADIO_REP2) {
            CheckDlgButton(hwnd, ID_RADIO_REP2, BST_CHECKED);
            CheckDlgButton(hwnd, ID_RADIO_REP1, BST_UNCHECKED);
        }
        break;
    }
    case WM_DESTROY:
        if (g_hDeepBlackBrush) DeleteObject(g_hDeepBlackBrush);
        if (g_hDarkGrayBrush) DeleteObject(g_hDarkGrayBrush);
        if (g_hModernFont) DeleteObject(g_hModernFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// Global Hotkey Listener supporting both keyboard keys and mouse buttons (Middle click, X1, X2, etc.)
void HotkeyListener(HWND hwnd) {
    bool lastKeyState = false;
    while (true) {
        if (g_waitingForHotkey) {
            // Scan keys and mouse buttons (VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2, etc.)
            for (int key = 1; key <= 254; ++key) {
                if ((GetAsyncKeyState(key) & 0x8000) != 0) {
                    g_currentHotkey = key;
                    g_waitingForHotkey = false;

                    std::wstring keyName = L"Unknown";
                    if (key == VK_MBUTTON) keyName = L"Middle Click";
                    else if (key == VK_XBUTTON1) keyName = L"Mouse 4";
                    else if (key == VK_XBUTTON2) keyName = L"Mouse 5";
                    else {
                        wchar_t nameBuf[128];
                        if (GetKeyNameTextW(MapVirtualKeyW(key, MAPVK_VK_TO_VSC) << 16, nameBuf, 128) > 0) {
                            keyName = nameBuf;
                        } else {
                            keyName = L"Key " + std::to_wstring(key);
                        }
                    }

                    std::wstring btnText = L"Hotkey setting: " + keyName + L" (Click to change)";
                    SetWindowTextW(hBtnHotkey, btnText.c_str());
                    InvalidateRect(hBtnHotkey, NULL, TRUE);
                    
                    std::wstring startText = L"Start (" + keyName + L")";
                    std::wstring stopText = L"Stop (" + keyName + L")";
                    SetWindowTextW(hBtnStart, startText.c_str());
                    SetWindowTextW(hBtnStop, stopText.c_str());
                    InvalidateRect(hBtnStart, NULL, TRUE);
                    InvalidateRect(hBtnStop, NULL, TRUE);

                    std::this_thread::sleep_for(std::chrono::milliseconds(400));
                    break;
                }
            }
        } else {
            int hotkey = g_currentHotkey.load();
            bool currentKeyState = (GetAsyncKeyState(hotkey) & 0x8000) != 0;
            if (currentKeyState && !lastKeyState) {
                g_clicking = !g_clicking;
                if (g_clicking) {
                    SetWindowTextW(hStatus, L"Status: ACTIVE 🔴 (Toggled)");
                } else {
                    SetWindowTextW(hStatus, L"Status: PAUSED 🟢 (Toggled)");
                }
            }
            lastKeyState = currentKeyState;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"EdinsDemoAutoClicker";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(18, 18, 18));

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Edin's Auto Clicker",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 485, 390,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    std::thread clickThread(ClickerThread);
    clickThread.detach();

    std::thread hotkeyThread(HotkeyListener, hwnd);
    hotkeyThread.detach();

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}