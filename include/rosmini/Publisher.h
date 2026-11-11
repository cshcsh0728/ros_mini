#pragma once

#include <string>

#include "rosmini/Node.h"

namespace rosmini {

/// 发布者（对应需求 FR-03 / FR-05）。
///
/// 行为：
///   - init()      ：向核心模块注册自身（以话题名为键），并输出初始化日志；
///   - publish()   ：接收传感器字符串 → 创建消息 → 输出发布日志 → 交给核心转发；
///   - shutdown()  ：输出关闭日志。
///
/// 注意：发布者不知道也不需要知道有多少订阅者，这是“松耦合”的体现。
class Publisher : public Node {
public:
    Publisher(std::string name, std::string topic);

    /// 话题名称只读访问。
    const std::string& topic() const noexcept { return topic_; }

    void init() override;
    void shutdown() override;

    /// 发布一条传感器数据，如 "temperature=25.3"。
    /// 内容为空时抛 MessageException，节点状态非法时抛 NodeException。
    void publish(const std::string& sensorData);

private:
    std::string topic_;
};

} // namespace rosmini
