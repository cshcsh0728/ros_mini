#pragma once

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "rosmini/Node.h"

namespace rosmini {

/// Win32 图形界面（不依赖任何第三方库）。
///
/// 界面结构：
///   ┌──────────── 标题栏（自绘）────────────┐
///   │ 左侧：节点列表 + 移除/全部关闭         │
///   │ 右侧：新建节点 / 发布消息 / 演示工具    │
///   ├──────────── 运行日志（只读，实时追加）──┤
///   │ 底部：状态栏（节点数/话题数/日志级别） │
///   └───────────────────────────────────────┘
class GuiApp {
public:
    GuiApp();
    ~GuiApp();

    GuiApp(const GuiApp&) = delete;
    GuiApp& operator=(const GuiApp&) = delete;

    /// 创建窗口并进入消息循环，返回进程退出码。
    int run(HINSTANCE instance, int showCommand);

private:
    /// 界面托管的节点：由界面负责创建与销毁，因此用 unique_ptr 持有。
    struct ManagedNode {
        std::unique_ptr<Node> node;
        std::string kind;   // "发布者" / "订阅者"
        std::string topic;
    };

    static LRESULT CALLBACK windowProcStatic(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT windowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

    void createControls();
    void applyFonts();
    void paint(HDC dc);
    int scale(int value) const;

    void refreshNodeList();
    void refreshTopics();
    void refreshStatus();
    void appendLog(const std::string& utf8Line);

    void onCreateNode();
    void onRemoveNode();
    void onShutdownAll();
    void onPublish();
    void onRandomPublish();
    void onRunDemo();
    void onExceptionLab();
    void onLogLevelChanged();

    HWND createChild(const wchar_t* className, const wchar_t* text, DWORD style,
                     DWORD exStyle, int x, int y, int width, int height, int id);

    std::string textOf(HWND control) const;
    void setText(HWND control, const std::string& utf8) const;
    HWND control(int id) const;

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    HWND nodeList_ = nullptr;
    HWND logEdit_ = nullptr;
    HWND topicsCombo_ = nullptr;
    HWND logLevelCombo_ = nullptr;

    HFONT uiFont_ = nullptr;
    HFONT monoFont_ = nullptr;
    HFONT titleFont_ = nullptr;

    std::vector<ManagedNode> nodes_;
    std::wstring statusText_;
    int dpi_ = 96;
    int publishSeed_ = 0;
};

} // namespace rosmini
