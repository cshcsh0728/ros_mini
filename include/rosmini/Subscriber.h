#pragma once

#include <cstddef>
#include <string>

#include "rosmini/Message.h"
#include "rosmini/Node.h"

namespace rosmini {

/// 订阅者（对应需求 FR-06 / FR-07）。
///
/// 行为：
///   - init()     ：向核心模块注册自身（以话题名为键），并输出初始化日志；
///   - receive()  ：由核心模块在转发时调用，输出“内容 + 时间戳”接收日志；
///   - shutdown() ：输出关闭日志。
///
/// 注意：订阅者不主动拉取消息，接收是“被动”的 —— 由核心推送触发。
class Subscriber : public Node {
public:
    Subscriber(std::string name, std::string topic);

    /// 话题名称只读访问。
    const std::string& topic() const noexcept { return topic_; }

    void init() override;
    void shutdown() override;

    /// 接收一条消息（由核心模块调用）。
    void receive(const Message& message);

    /// 已接收的消息条数，主要用于测试与演示统计。
    std::size_t receivedCount() const noexcept { return receivedCount_; }

private:
    std::string topic_;
    std::size_t receivedCount_ = 0;
};

} // namespace rosmini
