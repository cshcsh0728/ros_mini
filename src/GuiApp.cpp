#include "rosmini/GuiApp.h"

#include <algorithm>
#include <cstdio>
#include <cwchar>
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

namespace rosmini {
namespace {

// ---------- 设计尺寸（以 96 DPI 为基准，运行时按 DPI 缩放） ----------

constexpr int kDesignWidth = 1000;
constexpr int kDesignHeight = 700;

constexpr COLORREF kHeaderColor = RGB(30, 41, 66);
constexpr COLORREF kHeaderTextColor = RGB(255, 255, 255);

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
    kIdLogEdit = 501,
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
    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    const int x = (screenWidth - windowWidth) / 2;
    const int y = (screenHeight - windowHeight) / 2;

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
            refreshStatus();
            Logger::info("GUI", "界面已就绪。可从左侧创建节点，或在右侧发布消息。");
            return 0;

        case WM_COMMAND: {
            const int id = LOWORD(wparam);
            const int code = HIWORD(wparam);
            if (id == kIdCreateNode && code == BN_CLICKED) {
                onCreateNode();
                return 0;
            }
            if (id == kIdRemoveNode && code == BN_CLICKED) {
                onRemoveNode();
                return 0;
            }
            if (id == kIdShutdownAll && code == BN_CLICKED) {
                onShutdownAll();
                return 0;
            }
            if (id == kIdPublishButton && code == BN_CLICKED) {
                onPublish();
                return 0;
            }
            if (id == kIdRandomButton && code == BN_CLICKED) {
                onRandomPublish();
                return 0;
            }
            if (id == kIdDemoButton && code == BN_CLICKED) {
                onRunDemo();
                return 0;
            }
            if (id == kIdLabButton && code == BN_CLICKED) {
                onExceptionLab();
                return 0;
            }
            if (id == kIdLogLevel && code == CBN_SELCHANGE) {
                onLogLevelChanged();
                return 0;
            }
            break;
        }

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
    // 左侧：节点列表
    createChild(L"BUTTON", L" 节点列表", BS_GROUPBOX, 0, 16, 82, 300, 350, 0);
    nodeList_ = createChild(L"LISTBOX", L"", WS_VSCROLL | WS_BORDER | LBS_NOTIFY |
                                                  LBS_NOINTEGRALHEIGHT,
                            0, 30, 106, 272, 262, kIdNodeList);
    createChild(L"BUTTON", L"移除选中节点", BS_PUSHBUTTON, 0, 30, 386, 130, 28, kIdRemoveNode);
    createChild(L"BUTTON", L"关闭全部", BS_PUSHBUTTON, 0, 172, 386, 130, 28, kIdShutdownAll);

    // 右上：新建节点
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

    // 右中：发布消息
    createChild(L"BUTTON", L" 发布消息", BS_GROUPBOX, 0, 332, 206, 652, 96, 0);
    createChild(L"STATIC", L"话题", SS_LEFT, 0, 348, 228, 44, 22, 0);
    topicsCombo_ = createChild(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL,
                               0, 396, 224, 170, 220, kIdPublishTopic);
    createChild(L"STATIC", L"内容", SS_LEFT, 0, 582, 228, 44, 22, 0);
    createChild(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE,
                630, 224, 170, 26, kIdPublishContent);
    createChild(L"BUTTON", L"发布", BS_PUSHBUTTON, 0, 808, 222, 80, 30, kIdPublishButton);
    createChild(L"BUTTON", L"随机一条", BS_PUSHBUTTON, 0, 896, 222, 80, 30, kIdRandomButton);

    // 右下：演示与工具
    createChild(L"BUTTON", L" 演示与工具", BS_GROUPBOX, 0, 332, 310, 652, 92, 0);
    createChild(L"BUTTON", L"运行内置演示", BS_PUSHBUTTON, 0, 348, 330, 130, 30, kIdDemoButton);
    createChild(L"BUTTON", L"异常实验室", BS_PUSHBUTTON, 0, 490, 330, 120, 30, kIdLabButton);
    createChild(L"STATIC", L"日志级别", SS_LEFT, 0, 626, 336, 66, 22, 0);
    logLevelCombo_ = createChild(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL,
                                 0, 694, 330, 110, 160, kIdLogLevel);
    SendMessageW(logLevelCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"INFO"));
    SendMessageW(logLevelCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"WARN"));
    SendMessageW(logLevelCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"ERROR"));
    SendMessageW(logLevelCombo_, CB_SETCURSEL, 0, 0);
    createChild(L"STATIC", L"提示：异常实验室会依次触发 6 种非法操作，全部被捕获后回到正常状态。",
                SS_LEFT, 0, 348, 370, 620, 22, 0);

    // 底部：日志区
    createChild(L"BUTTON", L" 运行日志", BS_GROUPBOX, 0, 16, 440, 968, 212, 0);
    logEdit_ = createChild(L"EDIT", L"",
                           WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                           WS_EX_CLIENTEDGE, 30, 464, 940, 172, kIdLogEdit);
}

void GuiApp::applyFonts() {
    uiFont_ = CreateFontW(-MulDiv(9, dpi_, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                          L"Microsoft YaHei UI");
    monoFont_ = CreateFontW(-MulDiv(10, dpi_, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    titleFont_ = CreateFontW(-MulDiv(22, dpi_, 72), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                             L"Microsoft YaHei UI");

    EnumChildWindows(window_, &applyFontProc, reinterpret_cast<LPARAM>(uiFont_));
    // 日志区与节点列表用等宽字体：中文宽 2 列、英文宽 1 列，列才能对齐
    if (logEdit_ != nullptr) {
        SendMessageW(logEdit_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
    }
    if (nodeList_ != nullptr) {
        SendMessageW(nodeList_, WM_SETFONT, reinterpret_cast<WPARAM>(monoFont_), TRUE);
    }
}

void GuiApp::paint(HDC dc) {
    RECT client{};
    GetClientRect(window_, &client);

    // 顶部标题条
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
    DrawTextW(dc, L"节点 · 话题 · 发布/订阅通信模拟平台      C++17 / Win32", -1, &subtitleRect,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    // 底部状态条
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
    SendMessageW(nodeList_, LB_RESETCONTENT, 0, 0);
    for (const auto& managed : nodes_) {
        const std::wstring line = utf8ToWide(managed.node->name() + "    [" + managed.kind +
                                             "]    话题=" + managed.topic);
        SendMessageW(nodeList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
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

void GuiApp::refreshStatus() {
    const std::size_t topicCount = ROS::instance().topics().size();
    static const wchar_t* const kLevelNames[] = {L"INFO", L"WARN", L"ERROR"};
    const int levelIndex = static_cast<int>(SendMessageW(logLevelCombo_, CB_GETCURSEL, 0, 0));
    const wchar_t* levelName =
        (levelIndex >= 0 && levelIndex <= 2) ? kLevelNames[levelIndex] : L"INFO";

    wchar_t buffer[256];
    std::swprintf(buffer, 256, L"节点 %d 个  ·  话题 %d 个  ·  日志级别 %s",
                  static_cast<int>(nodes_.size()), static_cast<int>(topicCount), levelName);
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
    const std::wstring text = utf8ToWide(utf8Line) + L"\r\n";
    const int length = GetWindowTextLengthW(logEdit_);
    SendMessageW(logEdit_, EM_SETSEL, static_cast<WPARAM>(length), static_cast<LPARAM>(length));
    SendMessageW(logEdit_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
    SendMessageW(logEdit_, EM_SCROLLCARET, 0, 0);
}

// ---------------------------------------------------------------- 控件读写

HWND GuiApp::control(int id) const {
    return GetDlgItem(window_, id);
}

std::string GuiApp::textOf(HWND parent) const {
    if (parent == nullptr) {
        return std::string();
    }
    const int length = GetWindowTextLengthW(parent);
    if (length <= 0) {
        return std::string();
    }
    std::wstring buffer(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(parent, buffer.data(), length + 1);
    buffer.resize(static_cast<std::size_t>(length));
    return trim(wideToUtf8(buffer));
}

void GuiApp::setText(HWND target, const std::string& utf8Text) const {
    if (target == nullptr) {
        return;
    }
    SetWindowTextW(target, utf8ToWide(utf8Text).c_str());
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
        setText(control(kIdNameEdit), std::string());
        refreshNodeList();
        refreshTopics();
        refreshStatus();
    } catch (const RosException& error) {
        Logger::error("GUI", error.what());
    }
}

void GuiApp::onRemoveNode() {
    if (nodes_.empty()) {
        Logger::warn("GUI", "当前没有可移除的节点");
        return;
    }
    const int index = static_cast<int>(SendMessageW(nodeList_, LB_GETCURSEL, 0, 0));
    if (index == LB_ERR || index < 0 || index >= static_cast<int>(nodes_.size())) {
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
    const std::string topic = textOf(topicsCombo_);
    if (topic.empty()) {
        Logger::warn("GUI", "还没有任何话题：请先创建一个发布者");
        return;
    }
    const std::string content = textOf(control(kIdPublishContent));
    if (content.empty()) {
        Logger::warn("GUI", "消息内容为空，已取消发布");
        return;
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
        return;
    }

    try {
        publisher->publish(content);
    } catch (const RosException& error) {
        Logger::error("GUI", error.what());
        Logger::info("GUI", "提示：该话题还没有订阅者，先创建一个订阅者再发布。");
    }
}

void GuiApp::onRandomPublish() {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "temperature=%.1f", 20.0 + (publishSeed_ % 80) / 10.0);
    ++publishSeed_;
    setText(control(kIdPublishContent), std::string(buffer));
    onPublish();
}

void GuiApp::onRunDemo() {
    if (!nodes_.empty()) {
        Logger::warn("GUI", "内置演示会先关闭并清空当前节点");
        ROS::instance().shutdownAll();
        nodes_.clear();
        refreshNodeList();
        refreshTopics();
        refreshStatus();
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

        pubTemp.publish("temperature=25.3");
        pubHumi.publish("humidity=48%");
        pubTemp.publish("temperature=26.1");

        Logger::info("演示", "统计：sub_a=" + std::to_string(subA.receivedCount()) +
                                 " 条，sub_b=" + std::to_string(subB.receivedCount()) +
                                 " 条，sub_cmd=" + std::to_string(subCmd.receivedCount()) +
                                 " 条（cmd 话题没有发布者）");

        ROS::instance().shutdownAll();
    }
    Logger::info("演示", "===== 内置演示结束 =====");
    refreshNodeList();
    refreshTopics();
    refreshStatus();
}

void GuiApp::onExceptionLab() {
    Logger::info("实验", "===== 异常实验室：逐个触发非法操作，观察是否被捕获 =====");

    // 1. 空节点名
    try {
        const Publisher bad("", "sensor");
        (void)bad;
    } catch (const RosException& error) {
        Logger::error("实验", std::string("已捕获 ") + error.what());
    }

    // 2. 空话题名
    try {
        const Publisher bad("lab_bad_topic", "   ");
        (void)bad;
    } catch (const RosException& error) {
        Logger::error("实验", std::string("已捕获 ") + error.what());
    }

    // 3. 重复节点名注册
    try {
        Publisher first("lab_dup", "sensor");
        Publisher second("lab_dup", "sensor");
        first.init();
        second.init();
    } catch (const RosException& error) {
        Logger::error("实验", std::string("已捕获 ") + error.what());
    }

    // 4. 空消息内容
    try {
        Publisher publisher("lab_empty_pub", "sensor");
        Subscriber subscriber("lab_empty_sub", "sensor");
        publisher.init();
        subscriber.init();
        publisher.publish("   ");
    } catch (const RosException& error) {
        Logger::error("实验", std::string("已捕获 ") + error.what());
    }

    // 5. 向不存在的话题发布
    try {
        Publisher publisher("lab_ghost_pub", "ghost_topic");
        publisher.init();
        publisher.publish("temperature=20.0");
    } catch (const RosException& error) {
        Logger::error("实验", std::string("已捕获 ") + error.what());
    }

    // 6. 关闭后继续通信
    try {
        Publisher publisher("lab_closed_pub", "sensor");
        Subscriber subscriber("lab_closed_sub", "sensor");
        publisher.init();
        subscriber.init();
        publisher.publish("temperature=20.0");
        ROS::instance().shutdownAll();
        publisher.publish("temperature=21.0");
    } catch (const RosException& error) {
        Logger::error("实验", std::string("已捕获 ") + error.what());
    }

    Logger::info("实验", "===== 6 种非法操作全部被捕获，程序仍正常运行 =====");
    refreshNodeList();
    refreshTopics();
    refreshStatus();
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

} // namespace rosmini
