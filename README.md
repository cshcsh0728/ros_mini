# ROS 模拟节点系统（ros_mini）

用 **C++17** 在单进程内实现的 ROS 核心通信模型：节点（Node）、话题（Topic）、消息（Message）、
发布者（Publisher）、订阅者（Subscriber）与单例核心管理模块（ROS）。

本项目不依赖真实 ROS 运行时，也不使用 roscpp 等官方 API，而是用标准 C++ 复现「节点—话题」
松耦合发布/订阅的通信逻辑，用于练习面向对象抽象、STL 容器、对象生命周期管理与异常处理。

---

## 功能特性

- 节点抽象基类：封装唯一名称（只读）、定义 `init()` / `shutdown()` 纯虚接口
- 发布者：持有话题名，接收传感器字符串 → 创建消息 → 输出日志 → 交给核心转发
- 订阅者：持有话题名，被动接收消息并打印「内容 + 时间戳」
- 消息：字符串内容 + 构造时自动生成的时间戳（`YYYY-MM-DD HH:MM:SS`），对外只读
- 核心模块：单例，维护「话题→发布者」「话题→订阅者」两张映射表，按话题一对多转发
- 生命周期：初始化 → 通信 → 统一关闭，全过程日志；析构自动从核心注销，杜绝悬空指针
- 异常体系：`NodeException` / `TopicException` / `MessageException`，非法输入不崩溃
- 分级日志：`INFO` / `WARN` / `ERROR`，同时输出到控制台与 `rosmini.log`

---

## 目录结构

```
ros_mini/
├── CMakeLists.txt
├── include/rosmini/          对外头文件
│   ├── Exception.h           异常体系
│   ├── Logger.h              分级日志
│   ├── Message.h             消息载体
│   ├── Node.h                节点抽象基类
│   ├── Publisher.h           发布者
│   ├── Subscriber.h          订阅者
│   ├── ROS.h                 核心管理单例
│   └── Util.h                时间戳与字符串工具
├── src/                      实现文件 + 演示程序 main.cpp
├── tests/                    结构化测试（可执行）+ 测试用例表
└── docs/                     需求、架构、技术文档、用户手册
```

---

## 快速开始

### 方式一：Visual Studio（推荐）

1. `文件 → 打开 → 文件夹`，选择本仓库根目录
2. 等待 CMake 自动配置完成
3. 顶部启动项选择 `rosmini_demo.exe`，按 **F5**

### 方式二：命令行

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/rosmini_demo        # Windows: build\rosmini_demo.exe
./build/rosmini_tests       # 运行测试
```

---

## 运行效果

```
[INFO ] [2026-11-11 21:44:22] [pub_temp] 发布者已初始化，话题=sensor
[INFO ] [2026-11-11 21:44:22] [sub_a] 订阅者已初始化，话题=sensor
[INFO ] [2026-11-11 21:44:22] [pub_temp] 发布消息 → 话题=sensor 内容="temperature=25.3"
[INFO ] [2026-11-11 21:44:22] [sub_a] 接收消息 ← 话题=sensor 内容="temperature=25.3" 时间戳=2026-11-11 21:44:22
[INFO ] [2026-11-11 21:44:22] [sub_b] 接收消息 ← 话题=sensor 内容="temperature=25.3" 时间戳=2026-11-11 21:44:22
[ERROR] [2026-11-11 21:44:22] [main] 已捕获：[TopicException] 话题不存在："ghost_topic" 尚未注册任何订阅者
[INFO ] [2026-11-11 21:44:22] [main] ===== 演示结束，全部异常均已捕获，程序正常退出 =====
```

测试结果：

```
检查项：21 个，失败：0 个
全部用例通过
```

---

## 文档

| 文档 | 内容 |
|---|---|
| [需求文档](docs/01-需求文档.md) | 功能/非功能需求、异常场景、验收标准 |
| [架构设计文档](docs/02-架构设计文档.md) | 分层、类图、时序图、关键设计决策 |
| [技术文档](docs/03-技术文档.md) | 实现细节、踩坑记录、取舍分析 |
| [用户手册](docs/04-用户手册.md) | 编译、运行、扩展与常见问题 |
| [测试用例表](tests/测试用例表.md) | 12 个用例，覆盖正常/边界/异常 |

---

## 开发环境

| 项 | 版本 |
|---|---|
| 操作系统 | Windows 11 |
| IDE | Visual Studio 2026（Community） |
| 编译器 | MSVC 14.51 |
| 构建 | CMake 4.3.1 + Ninja |
| 语言标准 | C++17 |
| 依赖 | 仅标准库 |

---

## 作者

蔡劭恒 · 2026-11-11
