#pragma once

#include <fstream>
#include <functional>
#include <string>

namespace rosmini {

/// 日志级别，数值越大越严重。
enum class LogLevel {
    Info = 0,
    Warn = 1,
    Error = 2,
};

/// 统一日志出口。
///
/// 设计意图：节点与核心模块一律不直接调用 std::cout，全部经由 Logger。
/// 这样后续要加“日志分级”“写入文件”等能力时，只改这一个类即可，
/// 不需要去动十几个业务文件（对应架构文档第 7 节与加分项 FR-15）。
class Logger {
public:
    /// 低于该级别的日志不输出，默认 Info（即全部输出）。
    static void setMinLevel(LogLevel level);

    /// 打开日志文件（追加写入）。传入空字符串表示关闭文件输出。
    static void setLogFile(const std::string& path);
    static void closeLogFile();

    /// 设置额外的日志接收器（图形界面用）。
    ///
    /// 每产生一行日志，除了原有的控制台与文件输出外，还会回调该函数。
    /// 传入空函数对象表示取消接收器。
    /// 注意：回调在产生日志的线程上同步执行，接收方应避免在回调里再写日志（会递归）。
    static void setSink(std::function<void(const std::string&)> sink);

    static void info(const std::string& module, const std::string& message);
    static void warn(const std::string& module, const std::string& message);
    static void error(const std::string& module, const std::string& message);

private:
    static void write(LogLevel level, const std::string& module, const std::string& message);

    static LogLevel minLevel_;
    static std::ofstream file_;
    static std::function<void(const std::string&)> sink_;
};

} // namespace rosmini
