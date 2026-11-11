#include "rosmini/GuiApp.h"

#include <commctrl.h>
#include <richedit.h>

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "rosmini/Exception.h"
#include "rosmini/Logger.h"
#include "rosmini/Message.h"
#include "rosmini/Publisher.h"
#include "rosmini/ROS.h"
#include "rosmini/Subscriber.h"
#include "rosmini/Util.h"

#pragma comment(lib, "comctl32.lib")

namespace rosmini {
namespace {

// ---------- 设计尺寸（96 DPI 基准，运行时按 DPI 缩放） ----------

constexpr int kDesignWidth = 1000;
constexpr int kDesignHeight = 890;

constexpr COLORREF kHeaderColor = RGB(30, 41, 66);
constexpr COLORREF kHeaderTextColor = RGB(255, 255, 255);

// 日志颜色：INFO 蓝、WARN 橙、ERROR 红
constexpr COLORREF kInfoColor = RGB(28, 84, 150);
constexpr COLORREF kWarnColor = RGB(176, 102, 0);
constexpr COLORREF kErrorColor = RGB(190, 40, 40);
constexpr COLORREF kPlainColor = RGB(60, 60, 60);

// 自动演示：每 600ms 推进一步，共 17 步（0~16）
constexpr UINT_PTR kTimerAutoDemo = 1;
constexpr UINT kAutoDemoStepMs = 600;
constexpr int kAutoDemoSteps = 17;

enum ControlId : int {
    kIdNodeList = 101,
    kIdRemoveNode = 102,
    kIdShutdownAll = 103,
    kIdNameEdit = 201,
    kIdTopicEdit = 202,
    kIdRadioPublisher = 203,
    kIdRadioSubscriber = 204,
    kIdCreateNode = 205,
    kIdPublishTopic = 301,
    kIdPublishContent = 302,
    kIdPublishButton = 303,
    kIdRandomButton = 304,
    kIdDemoButton = 401,
    kIdLabButton = 402,
    kIdLogLevel = 403,
    kIdExportButton = 404,
    kIdAutoDemoButton = 405,
    kIdLogEdit = 501,
    kIdMessageList = 601,
};

std::wstring utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return std::wstring();
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                         nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), size);
    return result;
}

std::string wideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return std::string();
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), size, nullptr, nullptr);
    return result;
}

BOOL CALLBACK applyFontProc(HWND child, LPARAM font) {
    SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
    return TRUE;
}

void addColumn(HWND list, int index, const wchar_t* title, int width) {
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.pszText = const_cast<wchar_t*>(title);
    column.cx = width;
    column.iSubItem = index;
    ListView_InsertColumn(list, index, &column);
}

void setItemText(HWND list, int row, int column, const std::wstring& text) {
    ListView_SetItemText(list, row, column, const_cast<wchar_t*>(text.c_str()));
}

/// 执行一段“故意出错”的操作，捕获并记录异常（异常实验室与自动演示共用）
void guardedCase(const std::function<void()>& body) {
    try {
        body();
    } catch (const RosException& error) {
        Logger::error("实验", std::string("已捕获 ") + error.what());
    }
}

} // namespace

// ---------------------------------------------------------------- 生命周期

GuiApp::GuiApp() = default;

GuiApp::~GuiApp() {
    Logger::setSink(nullptr);
    if (uiFont_ != nullptr) {
        DeleteObject(uiFont_);
    }
    if (monoFont_ != nullptr) {
        DeleteObject(monoFont_);
    }
    if (titleFont_ != nullptr) {
        DeleteObject(titleFont_);
    }
}

int GuiApp::run(HINSTANCE instance, int showCommand) {
    instance_ = instance;

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&controls);

    // RichEdit 用于日志着色；若系统缺少该组件则退回普通 EDIT（不影响功能）
    richEdit_ = LoadLibraryW(L"Msftedit.dll") != nullptr;

    HDC screen = GetDC(nullptr);
    dpi_ = GetDeviceCaps(screen, LOGPIXELSX);
    ReleaseDC(nullptr, screen);
    if (dpi_ <= 0) {
        dpi_ = 96;
    }

    const wchar_t* kClassName = L"RosMiniGuiWindow";

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = &GuiApp::windowProcStatic;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    windowClass.lpszClassName = kClassName;
    if (RegisterClassExW(&windowClass) == 0) {
        return 1;
    }

    RECT frame{0, 0, scale(kDesignWidth), scale(kDesignHeight)};
    AdjustWindowRect(&frame, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    const int windowWidth = frame.right - frame.left;
    const int windowHeight = frame.bottom - frame.top;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2;

    window_ = CreateWindowExW(0, kClassName, L"ROS 模拟节点系统 · 节点通信模拟平台",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
                                  WS_CLIPCHILDREN,
                              x, y, windowWidth, windowHeight,
                              nullptr, nullptr, instance, this);
    if (window_ == nullptr) {
        return 1;
    }

    ShowWindow(window_, showCommand);
    UpdateWindow(window_);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK GuiApp::windowProcStatic(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    GuiApp* self = reinterpret_cast<GuiApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        self = static_cast<GuiApp*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self != nullptr) {
            self->window_ = window;
        }
    }
    if (self != nullptr) {
        return self->windowProc(window, message, wparam, lparam);
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT GuiApp::windowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE:
            createControls();
            applyFonts();
            Logger::setSink([this](const std::string& line) { appendLog(line); });
            refreshNodeList();
            refreshTopics();
            refreshMessageList();
            refreshStatus();
            Logger::info("GUI", "界面已就绪。左侧创建节点，中间发布消息，双击消息记录可重发。");
            return 0;

        case WM_COMMAND: {
            const int id = LOWORD(wparam);
            const int code = HIWORD(wparam);
            if (code == BN_CLICKED) {
                switch (id) {
                    case kIdCreateNode: onCreateNode(); return 0;
                    case kIdRemoveNode: onRemoveNode(); return 0;
                    case kIdShutdownAll: onShutdownAll(); return 0;
                    case kIdPublishButton: onPublish(); return 0;
                    case kIdRandomButton: onRandomPublish(); return 0;
                    case kIdDemoButton: onRunDemo(); return 0;
                    case kIdLabButton: onExceptionLab(); return 0;
                    case kIdExportButton: onExportCsv(); return 0;
                    case kIdAutoDemoButton: startAutoDemo(); return 0;
                    default: break;
                }
            }
            if (id == kIdLogLevel && code == CBN_SELCHANGE) {
                onLogLevelChanged();
                return 0;
            }
            break;
        }

        case WM_NOTIFY: {
            const auto* header = reinterpret_cast<const NMHDR*>(lparam);
            if (header->idFrom == kIdMessageList && header->code == NM_DBLCLK) {
                onResendSelected();
                return 0;
            }
            break;
        }

        case WM_TIMER:
            if (wparam == kTimerAutoDemo) {
                if (autoDemoRunning_) {
                    runAutoDemoStep(autoDemoStep_++);
                } else {
                    KillTimer(window, kTimerAutoDemo);
                }
                return 0;
            }
            break;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT paintStruct{};
            HDC dc = BeginPaint(window, &paintStruct);
            RECT client{};
            GetClientRect(window, &client);
            HDC memory = CreateCompatibleDC(dc);
            HBITMAP bitmap = CreateCompatibleBitmap(dc, client.right, client.bottom);
            HGDIOBJ oldBitmap = SelectObject(memory, bitmap);
            FillRect(memory, &client, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
            paint(memory);
            BitBlt(dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);
            SelectObject(memory, oldBitmap);
            DeleteObject(bitmap);
            DeleteDC(memory);
            EndPaint(window, &paintStruct);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(window, kTimerAutoDemo);
            Logger::setSink(nullptr);
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

// ---------------------------------------------------------------- 界面构建

int GuiApp::scale(int value) const {
    return MulDiv(value, dpi_, 96);
}

HWND GuiApp::createChild(const wchar_t* className, const wchar_t* text, DWORD style,
                         DWORD exStyle, int x, int y, int width, int height, int id) {
    return CreateWindowExW(exStyle, className, text, WS_CHILD | WS_VISIBLE | style,
                           scale(x), scale(y), scale(width), scale(height),
                           window_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                           instance_, nullptr);
}

void GuiApp::createControls() {
    // ---- 左侧：节点列表（ListView 报表模式） ----
    createChild(L"BUTTON", L" 节点列表", BS_GROUPBOX, 0, 16, 82, 300, 300, 0);
    nodeList_ = createChild(L"SysListView32", L"",
                            WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                            WS_EX_CLIENTEDGE, 30, 106, 272, 240, kIdNodeList);
    ListView_SetExtendedListViewStyle(nodeList_, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES |
                                                    LVS_EX_DOUBLEBUFFER);
    addColumn(nodeList_, 0, L"名称", scale(94));
    addColumn(nodeList_, 1, L"类型", scale(54));
    addColumn(nodeList_, 2, L"话题", scale(76));
    addColumn(nodeList_, 3, L"收到", scale(38));

    createChild(L"BUTTON", L"移除选中节点", BS_PUSHBUTTON, 0, 30, 352, 130, 28, kIdRemoveNode);
    createChild(L"BUTTON", L"关闭全部", BS_PUSHBUTTON, 0, 172, 352, 130, 28, kIdShutdownAll);

    // ---- 右上：新建节点 ----
    createChild(L"BUTTON", L" 新建节点", BS_GROUPBOX, 0, 332, 82, 652, 116, 0);
    createChild(L"STATIC", L"名称", SS_LEFT, 0, 348, 102, 44, 22, 0);
    createChild(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE,
                396, 98, 150, 26, kIdNameEdit);
    createChild(L"STATIC", L"话题", SS_LEFT, 0, 562, 102, 44, 22, 0);
    createChild(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE,
                610, 98, 150, 26, kIdTopicEdit);
    HWND radioPublisher = createChild(L"BUTTON", L"发布者", BS_AUTORADIOBUTTON | WS_GROUP,
                                      0, 347, 144, 88, 24, kIdRadioPublisher);
    createChild(L"BUTTON", L"订阅者", BS_AUTORADIOBUTTON, 0, 445, 144, 88, 24, kIdRadioSubscriber);
    SendMessageW(radioPublisher, BM_SETCHECK, BST_CHECKED, 0);
    createChild(L"BUTTON", L"创建节点", BS_PUSHBUTTON, 0, 610, 138, 150, 30, kIdCreateNode);

    // ---- 右中：发布消息 ----
    createChild(L"BUTTON", L" 发布消息", BS_GROUPBOX, 0, 332, 206, 652, 96, 0);
    createChild(L"STATIC", L"话题", SS_LEFT, 0, 348, 228, 44, 22, 0);
    topicsCombo_ = createChild(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL,
                               0, 396, 224, 170, 220, kIdPublishTopic);
    createChild(L"STATIC", L"内容", SS_LEFT, 0, 582, 228, 44, 22, 0);
    createChild(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE,
                630, 224, 170, 26, kIdPublishContent);
    createChild(L"BUTTON", L"发布", BS_PUSHBUTTON, 0, 808, 222, 80, 30, kIdPublishButton);
    createChild(L"BUTTON", L"随机一条", BS_PUSHBUTTON, 0, 896, 222, 80, 30, kIdRandomButton);

    // ---- 右下：演示与工具 ----
    createChild(L"BUTTON", L" 演示与工具", BS_GROUPBOX, 0, 332, 310, 652, 92, 0);
    autoDemoButton_ = createChild(L"BUTTON", L"自动演示", BS_PUSHBUTTON, 0,
                                  348, 330, 110, 30, kIdAutoDemoButton);
    createChild(L"BUTTON", L"内置演示", BS_PUSHBUTTON, 0, 466, 330, 100, 30, kIdDemoButton);
    createChild(L"BUTTON", L"异常实验室", BS_PUSHBUTTON, 0, 574, 330, 110, 30, kIdLabButton);
    createChild(L"BUTTON", L"导出 CSV", BS_PUSHBUTTON, 0, 692, 330, 90, 30, kIdExportButton);
    createChild(L"STATIC", L"日志级别", SS_LEFT, 0, 792, 336, 58, 22, 0);
    logLevelCombo_ = createChild(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL,
                                 0, 852, 330, 110, 160, kIdLogLevel);
    SendMessageW(logLevelCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"INFO"));
    SendMessageW(logLevelCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"WARN"));
    SendMessageW(logLevelCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"ERROR"));
    SendMessageW(logLevelCombo_, CB_SETCURSEL, 0, 0);
    createChild(L"STATIC", L"提示：“自动演示”会依次创建 5 个节点、发布 3 条消息并触发 6 种异常；双击消息记录可重发。",
                SS_LEFT, 0, 348, 370, 620, 22, 0);

    // ---- 消息记录 ----
    createChild(L"BUTTON", L" 消息记录（双击某行可重发）", BS_GROUPBOX, 0, 16, 410, 968, 200, 0);
    messageList_ = createChild(L"SysListView32", L"",
                               WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                               WS_EX_CLIENTEDGE, 30, 434, 940, 166, kIdMessageList);
    ListView_SetExtendedListViewStyle(messageList_, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES |
                                                       LVS_EX_DOUBLEBUFFER);
    addColumn(messageList_, 0, L"#", scale(40));
    addColumn(messageList_, 1, L"时间", scale(162));
    addColumn(messageList_, 2, L"话题", scale(90));
    addColumn(messageList_, 3, L"内容", scale(458));
    addColumn(messageList_, 4, L"发布者", scale(120));
    addColumn(messageList_, 5, L"送达", scale(60));

    // ---- 运行日志 ----
    createChild(L"BUTTON", L" 运行日志", BS_GROUPBOX, 0, 16, 610, 968, 206, 0);
    logEdit_ = createChild(richEdit_ ? L"RICHEDIT50W" : L"EDIT", L"",
                           WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                           WS_EX_CLIENTEDGE, 30, 634, 940, 166, kIdLogEdit);
    if (richEdit_) {
        SendMessageW(logEdit_, EM_EXLIMITTEXT, 0, static_cast<LPARAM>(200000));
    }
}

void GuiApp::applyFonts() {
    uiFont_ = CreateFontW(-MulDiv(9, dpi_, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    monoFont_ = CreateFontW(-MulDiv(10, dpi_, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    titleFont_ = CreateFontW(-MulDiv(22, dpi_, 72), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");

    EnumChildWindows(window_, &applyFontProc, reinterpret_cast<LPARAM>(uiFont_));
    if (logEdit_ != nullptr) {
        SendMessageW(logEdit_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
    }
    if (nodeList_ != nullptr) {
        SendMessageW(nodeList_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
    }
    if (messageList_ != nullptr) {
        SendMessageW(messageList_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
    }
}

void GuiApp::paint(HDC dc) {
    RECT client{};
    GetClientRect(window_, &client);

    RECT header{0, 0, client.right, scale(66)};
    HBRUSH headerBrush = CreateSolidBrush(kHeaderColor);
    FillRect(dc, &header, headerBrush);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, kHeaderTextColor);

    HFONT oldFont = static_cast<HFONT>(SelectObject(dc, titleFont_));
    RECT titleRect{scale(24), scale(8), client.right - scale(24), scale(44)};
    DrawTextW(dc, L"ROS MINI", -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SelectObject(dc, uiFont_);
    RECT subtitleRect{scale(24), scale(42), client.right - scale(24), scale(62)};
    DrawTextW(dc, L"节点 · 话题 · 发布/订阅通信模拟平台      C++17 / Win32 / 零第三方依赖", -1,
              &subtitleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    RECT status{0, client.bottom - scale(34), client.right, client.bottom};
    FillRect(dc, &status, headerBrush);
    DeleteObject(headerBrush);

    SetTextColor(dc, kHeaderTextColor);
    RECT statusText{scale(20), status.top, client.right - scale(20), status.bottom};
    DrawTextW(dc, statusText_.c_str(), -1, &statusText, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SelectObject(dc, oldFont);
}

// ---------------------------------------------------------------- 数据刷新

void GuiApp::refreshNodeList() {
    if (nodeList_ == nullptr) {
        return;
    }
    ListView_DeleteAllItems(nodeList_);
    int row = 0;
    for (const auto& managed : nodes_) {
        const std::wstring name = utf8ToWide(managed.node->name());
        const std::wstring kind = utf8ToWide(managed.kind);
        const std::wstring topic = utf8ToWide(managed.topic);

        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.pszText = const_cast<wchar_t*>(name.c_str());
        const int inserted = ListView_InsertItem(nodeList_, &item);
        setItemText(nodeList_, inserted, 1, kind);
        setItemText(nodeList_, inserted, 2, topic);

        std::wstring received = L"-";
        if (managed.kind == "订阅者") {
            const auto* subscriber = dynamic_cast<const Subscriber*>(managed.node.get());
            if (subscriber != nullptr) {
                received = std::to_wstring(subscriber->receivedCount());
            }
        }
        setItemText(nodeList_, inserted, 3, received);
        ++row;
    }
}

void GuiApp::refreshTopics() {
    if (topicsCombo_ == nullptr) {
        return;
    }
    const std::string previous = textOf(topicsCombo_);
    SendMessageW(topicsCombo_, CB_RESETCONTENT, 0, 0);

    std::vector<std::string> topics;
    for (const auto& managed : nodes_) {
        if (std::find(topics.begin(), topics.end(), managed.topic) == topics.end()) {
            topics.push_back(managed.topic);
        }
    }
    std::sort(topics.begin(), topics.end());
    for (const auto& topic : topics) {
        const std::wstring wide = utf8ToWide(topic);
        SendMessageW(topicsCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str()));
    }

    int index = 0;
    const auto found = std::find(topics.begin(), topics.end(), previous);
    if (found != topics.end()) {
        index = static_cast<int>(std::distance(topics.begin(), found));
    }
    SendMessageW(topicsCombo_, CB_SETCURSEL, index, 0);
}

void GuiApp::refreshMessageList() {
    if (messageList_ == nullptr) {
        return;
    }
    ListView_DeleteAllItems(messageList_);

    int row = 0;
    for (int i = static_cast<int>(messages_.size()) - 1; i >= 0; --i) {
        const MessageRecord& record = messages_[static_cast<std::size_t>(i)];

        const std::wstring indexText = std::to_wstring(record.index);
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = row;
        item.pszText = const_cast<wchar_t*>(indexText.c_str());
        item.lParam = static_cast<LPARAM>(i);
        const int inserted = ListView_InsertItem(messageList_, &item);

        setItemText(messageList_, inserted, 1, utf8ToWide(record.time));
        setItemText(messageList_, inserted, 2, utf8ToWide(record.topic));
        setItemText(messageList_, inserted, 3, utf8ToWide(record.content));
        setItemText(messageList_, inserted, 4, utf8ToWide(record.publisher));
        setItemText(messageList_, inserted, 5, std::to_wstring(record.delivered) + L" 人");
        ++row;
    }
}

void GuiApp::refreshStatus() {
    const std::size_t topicCount = ROS::instance().topics().size();
    static const wchar_t* const kLevelNames[] = {L"INFO", L"WARN", L"ERROR"};
    const int levelIndex = static_cast<int>(SendMessageW(logLevelCombo_, CB_GETCURSEL, 0, 0));
    const wchar_t* levelName =
        (levelIndex >= 0 && levelIndex <= 2) ? kLevelNames[levelIndex] : L"INFO";

    wchar_t buffer[256];
    std::swprintf(buffer, 256, L"节点 %d 个   ·   话题 %d 个   ·   消息 %d 条   ·   日志级别 %s",
                  static_cast<int>(nodes_.size()), static_cast<int>(topicCount),
                  static_cast<int>(messages_.size()), levelName);
    statusText_ = buffer;

    RECT client{};
    GetClientRect(window_, &client);
    RECT statusRect{0, client.bottom - scale(34), client.right, client.bottom};
    InvalidateRect(window_, &statusRect, FALSE);
}

void GuiApp::appendLog(const std::string& utf8Line) {
    if (logEdit_ == nullptr || !IsWindow(logEdit_)) {
        return;
    }

    COLORREF color = kPlainColor;
    if (utf8Line.rfind("[ERROR]", 0) == 0) {
        color = kErrorColor;
    } else if (utf8Line.rfind("[WARN ]", 0) == 0) {
        color = kWarnColor;
    } else if (utf8Line.rfind("[INFO ]", 0) == 0) {
        color = kInfoColor;
    }

    const int length = GetWindowTextLengthW(logEdit_);
    SendMessageW(logEdit_, EM_SETSEL, static_cast<WPARAM>(length), static_cast<LPARAM>(length));

    if (richEdit_) {
        CHARFORMAT2W format{};
        format.cbSize = sizeof(format);
        format.dwMask = CFM_COLOR;
        format.crTextColor = color;
        SendMessageW(logEdit_, EM_SETCHARFORMAT, SCF_SELECTION,
                     reinterpret_cast<LPARAM>(&format));
    }

    const std::wstring text = utf8ToWide(utf8Line) + L"\r\n";
    SendMessageW(logEdit_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
    SendMessageW(logEdit_, EM_SCROLLCARET, 0, 0);
}

void GuiApp::recordMessage(const std::string& topic, const std::string& content,
                           const std::string& publisher, std::size_t delivered) {
    MessageRecord record;
    record.index = ++messageSeq_;
    record.time = nowTimestamp();
    record.topic = topic;
    record.content = content;
    record.publisher = publisher;
    record.delivered = delivered;
    messages_.push_back(std::move(record));

    // 只保留最近 500 条，避免长时间运行后占用过多内存
    constexpr std::size_t kMaxRecords = 500;
    if (messages_.size() > kMaxRecords) {
        messages_.erase(messages_.begin(), messages_.begin() +
                                               static_cast<std::ptrdiff_t>(messages_.size() -
                                                                           kMaxRecords));
    }
    refreshMessageList();
}

// ---------------------------------------------------------------- 控件读写

HWND GuiApp::control(int id) const {
    return GetDlgItem(window_, id);
}

std::string GuiApp::textOf(HWND handle) const {
    if (handle == nullptr) {
        return std::string();
    }
    const int length = GetWindowTextLengthW(handle);
    if (length <= 0) {
        return std::string();
    }
    std::wstring buffer(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(handle, buffer.data(), length + 1);
    buffer.resize(static_cast<std::size_t>(length));
    return trim(wideToUtf8(buffer));
}

void GuiApp::setText(HWND handle, const std::string& utf8Text) const {
    if (handle == nullptr) {
        return;
    }
    SetWindowTextW(handle, utf8ToWide(utf8Text).c_str());
}

void GuiApp::selectTopic(const std::string& topic) {
    if (topicsCombo_ == nullptr) {
        return;
    }
    const std::wstring wide = utf8ToWide(topic);
    const LRESULT index = SendMessageW(topicsCombo_, CB_FINDSTRINGEXACT,
                                       static_cast<WPARAM>(-1),
                                       reinterpret_cast<LPARAM>(wide.c_str()));
    if (index >= 0) {
        SendMessageW(topicsCombo_, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
    }
}

// ---------------------------------------------------------------- 交互动作

void GuiApp::onCreateNode() {
    const std::string name = textOf(control(kIdNameEdit));
    const std::string topic = textOf(control(kIdTopicEdit));

    if (name.empty()) {
        Logger::warn("GUI", "节点名不能为空");
        return;
    }
    if (topic.empty()) {
        Logger::warn("GUI", "话题名不能为空");
        return;
    }

    const bool asPublisher =
        SendMessageW(control(kIdRadioPublisher), BM_GETCHECK, 0, 0) == BST_CHECKED;

    if (createManagedNode(name, topic, asPublisher)) {
        setText(control(kIdNameEdit), std::string());
    }
}

void GuiApp::onRemoveNode() {
    if (nodes_.empty()) {
        Logger::warn("GUI", "当前没有可移除的节点");
        return;
    }
    const int index = ListView_GetNextItem(nodeList_, -1, LVNI_SELECTED);
    if (index < 0 || index >= static_cast<int>(nodes_.size())) {
        Logger::warn("GUI", "请先在左侧列表中选择要移除的节点");
        return;
    }

    ManagedNode& target = nodes_[static_cast<std::size_t>(index)];
    const std::string name = target.node->name();
    target.node->shutdown();
    ROS::instance().unregister(target.node.get());
    nodes_.erase(nodes_.begin() + index);

    Logger::info("GUI", "已移除节点：" + name + "（动态注销，对应需求 FR-16）");
    refreshNodeList();
    refreshTopics();
    refreshStatus();
}

void GuiApp::onShutdownAll() {
    if (nodes_.empty()) {
        Logger::warn("GUI", "当前没有已注册的节点");
        return;
    }
    ROS::instance().shutdownAll();
    nodes_.clear();
    refreshNodeList();
    refreshTopics();
    refreshStatus();
}

void GuiApp::onPublish() {
    publishOnTopic(textOf(topicsCombo_), textOf(control(kIdPublishContent)));
}

void GuiApp::onRandomPublish() {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "temperature=%.1f", 20.0 + (publishSeed_ % 80) / 10.0);
    ++publishSeed_;
    setText(control(kIdPublishContent), std::string(buffer));
    onPublish();
}

void GuiApp::onResendSelected() {
    if (messageList_ == nullptr) {
        return;
    }
    const int row = ListView_GetNextItem(messageList_, -1, LVNI_SELECTED);
    if (row < 0) {
        Logger::warn("GUI", "请先在消息记录中选中一行");
        return;
    }

    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = row;
    if (!ListView_GetItem(messageList_, &item)) {
        return;
    }
    const auto index = static_cast<std::size_t>(item.lParam);
    if (index >= messages_.size()) {
        return;
    }

    const MessageRecord record = messages_[index];
    selectTopic(record.topic);
    setText(control(kIdPublishContent), record.content);
    Logger::info("GUI", "重发消息：" + record.content + "（话题=" + record.topic + "）");
    onPublish();
}

void GuiApp::onRunDemo() {
    if (!nodes_.empty()) {
        Logger::warn("GUI", "内置演示会先关闭并清空当前节点");
        ROS::instance().shutdownAll();
        nodes_.clear();
    }

    Logger::info("演示", "===== 场景一：2 个发布者 + 2 个订阅者，同一话题多对多通信 =====");
    {
        Publisher pubTemp("demo_pub_temp", "sensor");
        Publisher pubHumi("demo_pub_humi", "sensor");
        Subscriber subA("demo_sub_a", "sensor");
        Subscriber subB("demo_sub_b", "sensor");
        Subscriber subCmd("demo_sub_cmd", "cmd");

        pubTemp.init();
        pubHumi.init();
        subA.init();
        subB.init();
        subCmd.init();

        const std::size_t delivered = ROS::instance().subscriberCount("sensor");

        pubTemp.publish("temperature=25.3");
        recordMessage("sensor", "temperature=25.3", "demo_pub_temp", delivered);

        pubHumi.publish("humidity=48%");
        recordMessage("sensor", "humidity=48%", "demo_pub_humi", delivered);

        pubTemp.publish("temperature=26.1");
        recordMessage("sensor", "temperature=26.1", "demo_pub_temp", delivered);

        Logger::info("演示", "统计：sub_a=" + std::to_string(subA.receivedCount()) +
                                 " 条，sub_b=" + std::to_string(subB.receivedCount()) +
                                 " 条，sub_cmd=" + std::to_string(subCmd.receivedCount()) +
                                 " 条（cmd 话题没有发布者）");

        ROS::instance().shutdownAll();
    }
    Logger::info("演示", "===== 内置演示结束，可在“消息记录”里双击任意一行重发 =====");
    refreshNodeList();
    refreshTopics();
    refreshMessageList();
    refreshStatus();
}

void GuiApp::runExceptionCase(int index) {
    static const char* const kTitles[] = {
        "空节点名（EX-01）",
        "空话题名（EX-05）",
        "重复节点名注册（EX-02）",
        "空消息内容（EX-03）",
        "向不存在的话题发布（EX-04）",
        "关闭后继续发布（EX-06）",
    };
    if (index < 0 || index >= 6) {
        return;
    }
    Logger::info("实验", std::string("触发非法操作：") + kTitles[index]);

    switch (index) {
        case 0:
            guardedCase([] {
                const Publisher bad("", "sensor");
                (void)bad;
            });
            break;
        case 1:
            guardedCase([] {
                const Publisher bad("lab_bad_topic", "   ");
                (void)bad;
            });
            break;
        case 2:
            guardedCase([] {
                Publisher first("lab_dup", "sensor");
                Publisher second("lab_dup", "sensor");
                first.init();
                second.init();
            });
            break;
        case 3:
            guardedCase([] {
                Publisher publisher("lab_empty_pub", "sensor");
                Subscriber subscriber("lab_empty_sub", "sensor");
                publisher.init();
                subscriber.init();
                publisher.publish("   ");
            });
            break;
        case 4:
            guardedCase([] {
                Publisher publisher("lab_ghost_pub", "ghost_topic");
                publisher.init();
                publisher.publish("temperature=20.0");
            });
            break;
        default:
            guardedCase([] {
                Publisher publisher("lab_closed_pub", "sensor");
                Subscriber subscriber("lab_closed_sub", "sensor");
                publisher.init();
                subscriber.init();
                publisher.publish("temperature=20.0");
                ROS::instance().shutdownAll();
                publisher.publish("temperature=21.0");
            });
            break;
    }
}

void GuiApp::onExceptionLab() {
    Logger::info("实验", "===== 异常实验室：逐个触发非法操作，观察是否被捕获 =====");
    for (int index = 0; index < 6; ++index) {
        runExceptionCase(index);
    }
    Logger::info("实验", "===== 6 种非法操作全部被捕获，程序仍正常运行 =====");
    refreshNodeList();
    refreshTopics();
    refreshStatus();
}

bool GuiApp::createManagedNode(const std::string& name, const std::string& topic, bool asPublisher) {
    try {
        if (asPublisher) {
            auto publisher = std::make_unique<Publisher>(name, topic);
            publisher->init();
            nodes_.push_back(ManagedNode{std::move(publisher), "发布者", topic});
        } else {
            auto subscriber = std::make_unique<Subscriber>(name, topic);
            subscriber->init();
            nodes_.push_back(ManagedNode{std::move(subscriber), "订阅者", topic});
        }
        refreshNodeList();
        refreshTopics();
        refreshStatus();
        return true;
    } catch (const RosException& error) {
        Logger::error("GUI", error.what());
        return false;
    }
}

bool GuiApp::publishOnTopic(const std::string& topic, const std::string& content) {
    if (topic.empty()) {
        Logger::warn("GUI", "还没有任何话题：请先创建一个发布者");
        return false;
    }
    if (content.empty()) {
        Logger::warn("GUI", "消息内容为空，已取消发布");
        return false;
    }

    Publisher* publisher = nullptr;
    for (auto& managed : nodes_) {
        if (managed.kind == "发布者" && managed.topic == topic) {
            publisher = dynamic_cast<Publisher*>(managed.node.get());
            break;
        }
    }
    if (publisher == nullptr) {
        Logger::error("GUI", "话题 \"" + topic + "\" 上没有发布者，无法发布");
        return false;
    }

    const std::size_t delivered = ROS::instance().subscriberCount(topic);
    try {
        publisher->publish(content);
        recordMessage(topic, content, publisher->name(), delivered);
        refreshNodeList();   // 更新订阅者的“收到”计数
        refreshStatus();
        return true;
    } catch (const RosException& error) {
        Logger::error("GUI", error.what());
        Logger::info("GUI", "提示：该话题还没有订阅者，先创建一个订阅者再发布。");
        return false;
    }
}

void GuiApp::startAutoDemo() {
    if (autoDemoRunning_) {
        return;
    }

    if (!nodes_.empty()) {
        ROS::instance().shutdownAll();
        nodes_.clear();
        refreshNodeList();
        refreshTopics();
        refreshStatus();
        Logger::warn("自动演示", "已清空原有节点");
    }

    autoDemoRunning_ = true;
    autoDemoStep_ = 0;
    if (autoDemoButton_ != nullptr) {
        EnableWindow(autoDemoButton_, FALSE);
    }
    Logger::info("自动演示", "开始：创建 5 个节点 → 发布 3 条消息 → 统一关闭 → 触发 6 种异常");
    SetTimer(window_, kTimerAutoDemo, kAutoDemoStepMs, nullptr);
}

void GuiApp::stopAutoDemo() {
    KillTimer(window_, kTimerAutoDemo);
    autoDemoRunning_ = false;
    if (autoDemoButton_ != nullptr) {
        SetWindowTextW(autoDemoButton_, L"自动演示");
        EnableWindow(autoDemoButton_, TRUE);
    }
}

void GuiApp::runAutoDemoStep(int step) {
    if (!autoDemoRunning_) {
        return;
    }

    if (autoDemoButton_ != nullptr && step < kAutoDemoSteps) {
        wchar_t text[32];
        std::swprintf(text, 32, L"演示中 %d/%d", step + 1, kAutoDemoSteps);
        SetWindowTextW(autoDemoButton_, text);
    }

    switch (step) {
        case 0:
            createManagedNode("pub_temp", "sensor", true);
            break;
        case 1:
            createManagedNode("pub_humi", "sensor", true);
            break;
        case 2:
            createManagedNode("sub_a", "sensor", false);
            break;
        case 3:
            createManagedNode("sub_b", "sensor", false);
            break;
        case 4:
            createManagedNode("sub_cmd", "cmd", false);
            break;
        case 5:
            Logger::info("自动演示", "发布第 1 条：temperature=25.3");
            publishOnTopic("sensor", "temperature=25.3");
            break;
        case 6:
            Logger::info("自动演示", "发布第 2 条：humidity=48 pct");
            publishOnTopic("sensor", "humidity=48 pct");
            break;
        case 7:
            Logger::info("自动演示", "发布第 3 条：temperature=26.1");
            publishOnTopic("sensor", "temperature=26.1");
            break;
        case 8: {
            std::size_t cmdCount = 0;
            for (auto& managed : nodes_) {
                if (managed.kind != "订阅者") {
                    continue;
                }
                const auto* subscriber = dynamic_cast<Subscriber*>(managed.node.get());
                if (subscriber != nullptr && subscriber->topic() == "cmd") {
                    cmdCount = subscriber->receivedCount();
                }
            }
            Logger::info("自动演示", "对照：订阅 cmd 话题的节点收到 " + std::to_string(cmdCount) +
                                         " 条（该话题没有发布者，验证话题隔离）");
            break;
        }
        case 9:
            Logger::info("自动演示", "统一关闭全部节点（观察 5 个节点的关闭日志）");
            onShutdownAll();
            break;
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
            Logger::info("自动演示", "异常用例 " + std::to_string(step - 9) + "/6");
            runExceptionCase(step - 10);
            break;
        default:
            Logger::info("自动演示", "===== 自动演示结束：3 条消息各送达 2 个订阅者，6 种异常全部被捕获 =====");
            stopAutoDemo();
            break;
    }
}

void GuiApp::onLogLevelChanged() {
    const int index = static_cast<int>(SendMessageW(logLevelCombo_, CB_GETCURSEL, 0, 0));
    LogLevel level = LogLevel::Info;
    if (index == 1) {
        level = LogLevel::Warn;
    } else if (index == 2) {
        level = LogLevel::Error;
    }
    Logger::setMinLevel(level);
    refreshStatus();
    Logger::info("GUI", "日志级别已切换");
}

void GuiApp::onExportCsv() {
    if (messages_.empty()) {
        Logger::warn("GUI", "还没有消息可导出，先发布一条试试");
        return;
    }

    std::ofstream out("messages.csv", std::ios::out | std::ios::binary);
    if (!out.is_open()) {
        Logger::error("GUI", "无法创建 messages.csv（工作目录可能不可写）");
        return;
    }

    // UTF-8 BOM：保证 Excel 打开中文不乱码
    out << "\xEF\xBB\xBF";
    out << "序号,时间,话题,内容,发布者,送达人数\r\n";
    for (const auto& record : messages_) {
        out << record.index << ',' << record.time << ',' << record.topic << ','
            << record.content << ',' << record.publisher << ',' << record.delivered << "\r\n";
    }
    out.close();

    Logger::info("GUI", "已导出 " + std::to_string(messages_.size()) +
                            " 条消息到 messages.csv（UTF-8，可直接用 Excel 打开）");
}

} // namespace rosmini
