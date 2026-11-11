#pragma once

#include <stdexcept>
#include <string>

namespace rosmini {

/// 项目异常基类。
/// 继承 std::runtime_error 的目的：
///   1. 可被 catch (const std::exception&) 统一兜底；
///   2. what() 直接返回可读的中文描述，日志里一行就能打印出来；
///   3. 统一前缀，便于在输出中一眼区分异常类型。
class RosException : public std::runtime_error {
public:
    explicit RosException(const std::string& message)
        : std::runtime_error(message) {}
};

/// 节点相关异常：节点名为空、重复注册、关闭后继续通信等。
class NodeException : public RosException {
public:
    explicit NodeException(const std::string& message)
        : RosException("[NodeException] " + message) {}
};

/// 话题相关异常：话题名为空、话题不存在等。
class TopicException : public RosException {
public:
    explicit TopicException(const std::string& message)
        : RosException("[TopicException] " + message) {}
};

/// 消息相关异常：消息内容为空等。
class MessageException : public RosException {
public:
    explicit MessageException(const std::string& message)
        : RosException("[MessageException] " + message) {}
};

} // namespace rosmini
