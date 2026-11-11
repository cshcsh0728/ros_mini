#pragma once

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "rosmini/Node.h"

namespace rosmini {

/// Win32 图形界面（不依赖任何第三方库，仅使用系统自带控件）。
///
/// 布局：
///   ┌──────────── 标题条（自绘）───────────────┐
///   │ 左：节点列表(ListView)  │ 右上：新建节点      │
///   │                        │ 右中：发布消息      │
///   │                        │ 右下：演示与工具    │
///   ├────────────────────────┴──────────────────┤
///   │ 消息记录(ListView，双击可重发)              │
///   ├───────────────────────────────────────────┤
///   │ 运行日志（RichEdit，按级别着色）            │
///   ├───────────────────────────────────────────┤
///   │ 状态栏：节点 / 话题 / 消息数 / 日志级别     │
///   └───────────────────────────────────────────┘
class GuiApp {
public:
    GuiApp();
    ~GuiApp();

    GuiApp(const GuiApp&) = delete;
    GuiApp& operator=(const GuiApp&) = delete;

    int run(HINSTANCE instance, int showCommand);

private:
    struct ManagedNode {
        std::unique_ptr<Node> node;
        std::string kind;   // "发布者" / "订阅者"
        std::string topic;
    };

    /// 界面层记录的一条消息（核心库不感知统计，保持职责单一）。
    struct MessageRecord {
        int index = 0;
        std::string time;
        std::string topic;
        std::string content;
        std::string publisher;
        std::size_t delivered = 0;
    };

    static LRESULT CALLBACK windowProcStatic(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT windowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

    void createControls();
    void applyFonts();
    void paint(HDC dc);
    int scale(int value) const;

    void refreshNodeList();
    void refreshTopics();
    void refreshMessageList();
    void refreshStatus();
    void appendLog(const std::string& utf8Line);
    void recordMessage(const std::string& topic, const std::string& content,
                       const std::string& publisher, std::size_t delivered);

    bool createManagedNode(const std::string& name, const std::string& topic, bool asPublisher);
    bool publishOnTopic(const std::string& topic, const std::string& content);
    void runExceptionCase(int index);

    void onCreateNode();
    void onRemoveNode();
    void onShutdownAll();
    void onPublish();
    void onRandomPublish();
    void onRunDemo();
    void onExceptionLab();
    void onLogLevelChanged();
    void onExportCsv();
    void onResendSelected();

    // ---- 自动演示：由定时器逐步推进，界面始终保持响应 ----
    void startAutoDemo();
    void stopAutoDemo();
    void runAutoDemoStep(int step);

    HWND createChild(const wchar_t* className, const wchar_t* text, DWORD style, DWORD exStyle,
                     int x, int y, int width, int height, int id);
    HWND control(int id) const;
    std::string textOf(HWND handle) const;
    void setText(HWND handle, const std::string& utf8Text) const;
    void selectTopic(const std::string& topic);

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    HWND nodeList_ = nullptr;
    HWND messageList_ = nullptr;
    HWND logEdit_ = nullptr;
    HWND topicsCombo_ = nullptr;
    HWND logLevelCombo_ = nullptr;
    HWND autoDemoButton_ = nullptr;
    bool richEdit_ = false;

    HFONT uiFont_ = nullptr;
    HFONT monoFont_ = nullptr;
    HFONT titleFont_ = nullptr;

    std::vector<ManagedNode> nodes_;
    std::vector<MessageRecord> messages_;
    int messageSeq_ = 0;
    int publishSeed_ = 0;
    bool autoDemoRunning_ = false;
    int autoDemoStep_ = 0;
    std::wstring statusText_;
    int dpi_ = 96;
};

} // namespace rosmini
