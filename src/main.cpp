// 图形界面入口（Win32 子系统：运行时不弹出控制台窗口）。
//
// 若需要原来的“控制台刷屏演示”，请运行 rosmini_batch.exe（源码见 batch_main.cpp）。

#include <windows.h>

#include "rosmini/GuiApp.h"
#include "rosmini/Logger.h"

// 启用 Common Controls 6（让按钮、下拉框使用 Windows 现代主题，而不是 Win95 样式）
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previousInstance, PWSTR commandLine,
                    int showCommand) {
    (void)previousInstance;
    (void)commandLine;

    // 高 DPI 感知：避免界面在 125%/150% 缩放下被系统拉伸模糊
    SetProcessDPIAware();

    // 日志同时写入文件（工作目录下的 rosmini.log），便于事后查看
    rosmini::Logger::setLogFile("rosmini.log");
    rosmini::Logger::setMinLevel(rosmini::LogLevel::Info);

    rosmini::GuiApp app;
    return app.run(instance, showCommand);
}
