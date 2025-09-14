// Enable Unicode to match wide-character API usage
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shellapi.h>
#include <iphlpapi.h>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "metrics.hpp"

// Configuration for multi-graph overlay
static const int GRAPH_WIDTH = 60; // width of each sparkline
static const int GRAPH_HEIGHT = 16;
static const int GRAPH_SPACING = 4; // space between graphs
static const int PADDING_X = 6;
static const int PADDING_Y = 4;
static const int UPDATE_INTERVAL_MS = 1000; // 1s sampling

struct Histories {
    std::vector<float> cpu;
    std::vector<float> mem;
    std::vector<float> net; // utilization 0..1
} histories;

static size_t historyIndex = 0;
static bool historyFilled = false;

// Window / state
static HWND g_hwnd = nullptr;
static bool g_clickThrough = true;
static bool g_manualPosition = false; // user dragged -> disable docking
static bool g_frozenWidth = false;
static int g_frozenWindowWidth = 0;
static NOTIFYICONDATA g_nid{};
static MetricsCollector g_metrics;
static MetricsSnapshot g_lastSnap{};

// Network interface selection
static std::vector<std::pair<std::wstring, DWORD>> g_availableInterfaces;
static int g_selectedInterfaceIndex = -1; // -1 = auto-select fastest

// Forward declarations
void UpdateClickThrough();
void PositionNearTaskbarClock();
void EnsureTopmost();
void RecomputeAndResize();
std::string BuildOverlayLine(const MetricsSnapshot& snap);
void EnumerateNetworkInterfaces();
void ShowContextMenu(HWND hwnd);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        SetTimer(hwnd, 1, UPDATE_INTERVAL_MS, nullptr);
        break; }
    case WM_TIMER: {
        g_lastSnap = g_metrics.sample();
        if (histories.cpu.empty()) {
            histories.cpu.assign(GRAPH_WIDTH, 0.f);
            histories.mem.assign(GRAPH_WIDTH, 0.f);
            histories.net.assign(GRAPH_WIDTH, 0.f);
            historyIndex = 0; historyFilled = false;
        }
        auto push = [](std::vector<float>& h, float v){ h[historyIndex] = std::clamp(v, 0.f, 1.f); };
        push(histories.cpu, g_lastSnap.cpu.usage);
        push(histories.mem, g_lastSnap.memory.usage);
        float netUtil = 0.f;
        if (g_lastSnap.net && g_lastSnap.net->linkSpeedBitsPerSec > 0) {
            double maxBytes = std::max(g_lastSnap.net->bytesRecvPerSec, g_lastSnap.net->bytesSentPerSec);
            double capBytesPerSec = (double)g_lastSnap.net->linkSpeedBitsPerSec / 8.0;
            if (capBytesPerSec > 0.0) netUtil = (float)(maxBytes / capBytesPerSec);
        }
        if (netUtil < 0.f) netUtil = 0.f; if (netUtil > 1.f) netUtil = 1.f;
        push(histories.net, netUtil);
        historyIndex++;
        if (historyIndex >= histories.cpu.size()) { historyIndex = 0; historyFilled = true; }
        InvalidateRect(hwnd, nullptr, FALSE);
        EnsureTopmost();
        break; }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        SetBkColor(hdc, RGB(0,0,0));
        SetTextColor(hdc, RGB(0,255,0));
        HFONT hFont = (HFONT)GetStockObject(ANSI_FIXED_FONT);
        HGDIOBJ oldFont = SelectObject(hdc, hFont);
        std::string line = BuildOverlayLine(g_lastSnap);
        SIZE sz{}; GetTextExtentPoint32A(hdc, line.c_str(), (int)line.size(), &sz);
        if (!g_frozenWidth) {
            g_frozenWidth = true;
            g_frozenWindowWidth = sz.cx + PADDING_X * 2 + (GRAPH_WIDTH*3) + (GRAPH_SPACING*2) + 8;
            RecomputeAndResize();
        }
        TextOutA(hdc, PADDING_X + (GRAPH_WIDTH*3) + GRAPH_SPACING*2 + 8, PADDING_Y, line.c_str(), (int)line.size());
        
        // Draw graphs with visible colors
        auto drawGraph = [&](const std::vector<float>& h, int offsetX, COLORREF color) {
            HPEN pen = CreatePen(PS_SOLID, 1, color);
            HPEN oldPen = (HPEN)SelectObject(hdc, pen);
            
            int baseY = PADDING_Y + GRAPH_HEIGHT;
            int count = (int)h.size();
            MoveToEx(hdc, offsetX, baseY - (int)std::round(h[(historyIndex)%count]* (GRAPH_HEIGHT-1)), nullptr);
            for (int i=1;i<count;i++) {
                int idx = (historyIndex + i) % count;
                int x = offsetX + i;
                int y = baseY - (int)std::round(h[idx]*(GRAPH_HEIGHT-1));
                LineTo(hdc, x, y);
            }
            
            SelectObject(hdc, oldPen);
            DeleteObject(pen);
        };
        
        if (!histories.cpu.empty()) {
            drawGraph(histories.cpu, PADDING_X, RGB(0, 255, 100));        // Green for CPU
            drawGraph(histories.mem, PADDING_X + GRAPH_WIDTH + GRAPH_SPACING, RGB(100, 150, 255)); // Blue for Memory  
            drawGraph(histories.net, PADDING_X + (GRAPH_WIDTH + GRAPH_SPACING)*2, RGB(255, 200, 0)); // Yellow for Network
        }
        SelectObject(hdc, oldFont);
        EndPaint(hwnd, &ps);
        break; }
    case WM_LBUTTONDOWN: {
        g_manualPosition = true; // disable docking after drag
        ReleaseCapture();
        SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        break; }
    case WM_RBUTTONUP: {
        ShowContextMenu(hwnd);
        break; }
    case WM_APP + 1: // tray icon messages
        if (lParam == WM_RBUTTONUP) {
            ShowContextMenu(hwnd);
        }
        break;
    case WM_HOTKEY:
        if (wParam == 1) { if (IsWindowVisible(hwnd)) ShowWindow(hwnd, SW_HIDE); else ShowWindow(hwnd, SW_SHOW); }
        break;
    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        break;
    default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

static void SetDpiAwareness() {
    HMODULE shcore = LoadLibraryW(L"Shcore.dll");
    if (shcore) {
        typedef HRESULT (WINAPI *SetProcessDpiAwarenessFunc)(int);
        auto fn = (SetProcessDpiAwarenessFunc)GetProcAddress(shcore, "SetProcessDpiAwareness");
        if (fn) fn(2);
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

void UpdateClickThrough() {
    LONG ex = GetWindowLong(g_hwnd, GWL_EXSTYLE);
    if (g_clickThrough) ex |= WS_EX_TRANSPARENT; else ex &= ~WS_EX_TRANSPARENT;
    SetWindowLong(g_hwnd, GWL_EXSTYLE, ex);
}

void EnsureTopmost() {
    if (!g_hwnd) return;
    SetWindowPos(g_hwnd, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOREDRAW|SWP_NOACTIVATE);
}

void PositionNearTaskbarClock() {
    if (g_manualPosition) return;
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!taskbar) return;
    RECT tb; GetWindowRect(taskbar, &tb);
    HMONITOR mon = MonitorFromWindow(taskbar, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi) }; GetMonitorInfo(mon, &mi);
    RECT wnd; GetWindowRect(g_hwnd, &wnd);
    int w = wnd.right - wnd.left;
    int h = wnd.bottom - wnd.top;
    int tbWidth = tb.right - tb.left;
    int tbHeight = tb.bottom - tb.top;
    bool vertical = tbHeight > tbWidth;
    int x=0,y=0;
    if (!vertical) {
        bool top = (tb.top <= mi.rcMonitor.top + 10);
        x = tb.right - w - 10;
        y = top ? tb.bottom + 5 : tb.top - h - 5;
    } else {
        bool left = (tb.left <= mi.rcMonitor.left + 10);
        x = left ? tb.right + 5 : tb.left - w - 5;
        y = tb.bottom - h - 10;
    }
    SetWindowPos(g_hwnd, nullptr, x, y, 0,0, SWP_NOZORDER|SWP_NOSIZE|SWP_NOACTIVATE);
}

void RecomputeAndResize() {
    int graphsWidth = (GRAPH_WIDTH * 3) + (GRAPH_SPACING * 2);
    int textExtra = 300; // initial guess until frozen
    int width = g_frozenWidth ? g_frozenWindowWidth : PADDING_X*2 + graphsWidth + 8 + textExtra;
    int height = PADDING_Y*2 + GRAPH_HEIGHT;
    SetWindowPos(g_hwnd, nullptr, 0,0, width, height, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    PositionNearTaskbarClock();
}

static std::string FormatBytesPerSec(double v) {
    const char* units[] = { "B/s", "KB/s", "MB/s", "GB/s", "TB/s" };
    int idx = 0; while (v >= 1024.0 && idx < 4) { v /= 1024.0; idx++; }
    char buf[32];
    if (v >= 100.0) snprintf(buf, sizeof(buf), "%3.0f %s", v, units[idx]);
    else if (v >= 10.0) snprintf(buf, sizeof(buf), "%3.1f %s", v, units[idx]);
    else snprintf(buf, sizeof(buf), "%3.2f %s", v, units[idx]);
    return buf;
}

void EnumerateNetworkInterfaces() {
    g_availableInterfaces.clear();
    ULONG size = 0;
    if (GetIfTable(nullptr, &size, FALSE) != ERROR_INSUFFICIENT_BUFFER) return;
    
    std::vector<unsigned char> buf(size);
    PMIB_IFTABLE table = (PMIB_IFTABLE)buf.data();
    if (GetIfTable(table, &size, FALSE) != NO_ERROR) return;
    
    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        auto& row = table->table[i];
        if (row.dwOperStatus != IF_OPER_STATUS_OPERATIONAL) continue;
        if (row.dwType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        
        // Convert description to wide string (simple approach)
        std::wstring desc;
        for (int j = 0; j < MAXLEN_IFDESCR && row.bDescr[j]; ++j) {
            desc += (wchar_t)row.bDescr[j];
        }
        g_availableInterfaces.push_back({desc, row.dwIndex});
    }
}

void ShowContextMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    
    HMENU menu = CreatePopupMenu();
    HMENU netMenu = CreatePopupMenu();
    
    // Main menu items
    AppendMenuW(menu, MF_STRING, 100, g_clickThrough ? L"Disable Click-Through" : L"Enable Click-Through");
    AppendMenuW(menu, MF_STRING, 101, g_manualPosition ? L"Auto-Dock to Taskbar" : L"Manual Position Mode");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    
    // Network submenu
    EnumerateNetworkInterfaces();
    AppendMenuW(netMenu, MF_STRING | (g_selectedInterfaceIndex == -1 ? MF_CHECKED : 0), 200, L"Auto-select fastest");
    
    for (size_t i = 0; i < g_availableInterfaces.size(); ++i) {
        UINT flags = MF_STRING;
        if ((int)i == g_selectedInterfaceIndex) flags |= MF_CHECKED;
        AppendMenuW(netMenu, flags, 201 + (UINT)i, g_availableInterfaces[i].first.c_str());
    }
    
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)netMenu, L"Network Interface");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 199, L"Exit");
    
    SetForegroundWindow(hwnd);
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
    
    if (cmd == 100) {
        g_clickThrough = !g_clickThrough;
        UpdateClickThrough();
    } else if (cmd == 101) {
        g_manualPosition = !g_manualPosition;
        if (!g_manualPosition) {
            PositionNearTaskbarClock(); // Re-dock
        }
    } else if (cmd == 199) {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
    } else if (cmd == 200) {
        g_selectedInterfaceIndex = -1; // Auto-select
        g_metrics.setSelectedNetworkInterface(-1);
    } else if (cmd >= 201 && cmd < 201 + (int)g_availableInterfaces.size()) {
        g_selectedInterfaceIndex = cmd - 201;
        g_metrics.setSelectedNetworkInterface(g_availableInterfaces[g_selectedInterfaceIndex].second);
    }
    
    DestroyMenu(menu);
}

std::string BuildOverlayLine(const MetricsSnapshot& snap) {
    int cpuPct = (int)std::round(snap.cpu.usage * 100.0f);
    int memPct = (int)std::round(snap.memory.usage * 100.0f);
    cpuPct = std::clamp(cpuPct, 0, 100);
    memPct = std::clamp(memPct, 0, 100);
    double netRecv = 0, netSend = 0; int netPct = 0;
    if (snap.net) {
        netRecv = snap.net->bytesRecvPerSec;
        netSend = snap.net->bytesSentPerSec;
        if (snap.net->linkSpeedBitsPerSec > 0) {
            double maxBytes = std::max(netRecv, netSend);
            double cap = snap.net->linkSpeedBitsPerSec / 8.0;
            if (cap > 0) netPct = (int)std::round(std::clamp(maxBytes / cap, 0.0, 1.0) * 100.0);
        }
    }
    double diskR = 0, diskW = 0;
    if (snap.disk) { diskR = snap.disk->readBytesPerSec; diskW = snap.disk->writeBytesPerSec; }
    std::string netRecvStr = FormatBytesPerSec(netRecv);
    std::string netSendStr = FormatBytesPerSec(netSend);
    std::string diskRStr = FormatBytesPerSec(diskR);
    std::string diskWStr = FormatBytesPerSec(diskW);
    char buf[256];
    snprintf(buf, sizeof(buf), "CPU %3d%%  MEM %3d%%  NET %3d%% (%s / %s)  DISK %s / %s",
        cpuPct, memPct, netPct, netRecvStr.c_str(), netSendStr.c_str(), diskRStr.c_str(), diskWStr.c_str());
    return buf;
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    SetDpiAwareness();
    g_metrics.initialize();
    WNDCLASSW wc{}; wc.lpfnWndProc = WndProc; wc.hInstance = hInst; wc.lpszClassName = L"wtop_overlay"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_TRANSPARENT,
        wc.lpszClassName, L"wtop", WS_POPUP, 0,0, 300, 50, nullptr, nullptr, hInst, nullptr);
    g_hwnd = hwnd;
    SetLayeredWindowAttributes(hwnd, RGB(0,0,0), 0, LWA_COLORKEY);
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd; g_nid.uID = 1; g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; g_nid.uCallbackMessage = WM_APP + 1; g_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    lstrcpyW(g_nid.szTip, L"wtop overlay");
    Shell_NotifyIcon(NIM_ADD, &g_nid);
    RegisterHotKey(hwnd, 1, MOD_CONTROL | MOD_SHIFT, 'O');
    RecomputeAndResize();
    UpdateClickThrough();
    ShowWindow(hwnd, SW_SHOW);
    EnsureTopmost();
    MSG msg; while (GetMessage(&msg, nullptr, 0,0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}
