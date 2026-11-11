// 控制台演示程序（rosmini_batch.exe）。
//
// 一次性跑完“多对多通信 + 5 种非法输入”两个场景，输出完整日志，
// 适合截图、录屏或命令行验证；图形界面请运行 rosmini_demo.exe。

#include <iostream>
#include <string>

#include "rosmini/Exception.h"
#include "rosmini/Logger.h"
#include "rosmini/Message.h"
#include "rosmini/Publisher.h"
#include "rosmini/ROS.h"
#include "rosmini/Subscriber.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

/// 让 Windows 控制台按 UTF-8 显示中文，否则中文日志会变成乱码。
void enableUtf8Console() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
}

} // namespace

int main() {
    using rosmini::Logger;
    using rosmini::Message;
    using rosmini::Publisher;
    using rosmini::ROS;
    using rosmini::RosException;
    using rosmini::Subscriber;

    enableUtf8Console();
    Logger::setLogFile("rosmini.log");

    Logger::info("main", "===== ROS 模拟节点系统 演示开始 =====");

    // ------------------------------------------------------------------
    // 场景一：两个发布者 + 两个订阅者，同一话题多对多通信
    // ------------------------------------------------------------------
    Logger::info("main", "---- 场景一：多对多通信（话题 sensor） ----");
    {
        Publisher pubTemp("pub_temp", "sensor");
        Publisher pubHumi("pub_humi", "sensor");
        Subscriber subA("sub_a", "sensor");
        Subscriber subB("sub_b", "sensor");
        Subscriber subCmd("sub_cmd", "cmd");

        pubTemp.init();
        pubHumi.init();
        subA.init();
        subB.init();
        subCmd.init();

        pubTemp.publish("temperature=25.3");
        pubHumi.publish("humidity=48%");
        pubTemp.publish("temperature=26.1");

        Logger::info("main", "统计：sub_a=" + std::to_string(subA.receivedCount()) +
                                 " 条，sub_b=" + std::to_string(subB.receivedCount()) +
                                 " 条，sub_cmd=" + std::to_string(subCmd.receivedCount()) +
                                 " 条（cmd 话题没有发布者）");

        ROS::instance().shutdownAll();
    }

    // ------------------------------------------------------------------
    // 场景二：非法输入 —— 程序不崩溃，只抛出可读异常
    // ------------------------------------------------------------------
    Logger::info("main", "---- 场景二：非法输入处理 ----");

    // 2.1 空节点名
    try {
        const Publisher badName("", "sensor");
        (void)badName;
    } catch (const RosException& e) {
        Logger::error("main", std::string("已捕获：") + e.what());
    }

    // 2.2 空话题名
    try {
        const Publisher badTopic("pub_bad_topic", "   ");
        (void)badTopic;
    } catch (const RosException& e) {
        Logger::error("main", std::string("已捕获：") + e.what());
    }

    // 2.3 重复节点名注册
    try {
        Publisher first("dup_pub", "sensor");
        Publisher second("dup_pub", "sensor");
        first.init();
        second.init(); // 这里抛出 NodeException
        (void)first;
    } catch (const RosException& e) {
        Logger::error("main", std::string("已捕获：") + e.what());
    }

    // 2.4 空消息发布
    try {
        Publisher pub("pub_empty", "sensor");
        Subscriber sub("sub_empty", "sensor");
        pub.init();
        sub.init();
        pub.publish("   "); // 这里抛出 MessageException
    } catch (const RosException& e) {
        Logger::error("main", std::string("已捕获：") + e.what());
    }

    // 2.5 向不存在的话题发布
    try {
        Publisher pub("pub_ghost", "ghost_topic");
        pub.init();
        pub.publish("temperature=20.0"); // 这里抛出 TopicException
    } catch (const RosException& e) {
        Logger::error("main", std::string("已捕获：") + e.what());
    }

    // 2.6 关闭后继续发布
    try {
        Publisher pub("pub_closed", "sensor");
        Subscriber sub("sub_closed", "sensor");
        pub.init();
        sub.init();
        pub.publish("temperature=20.0");
        ROS::instance().shutdownAll();
        pub.publish("temperature=21.0"); // 这里抛出 NodeException
    } catch (const RosException& e) {
        Logger::error("main", std::string("已捕获：") + e.what());
    }

    Logger::info("main", "===== 演示结束，全部异常均已捕获，程序正常退出 =====");
    Logger::closeLogFile();
    return 0;
}
