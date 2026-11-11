#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace rosmini {

class Node;
class Publisher;
class Subscriber;
class Message;

/// 核心管理模块（对应需求 FR-08 / FR-09 / FR-10 / FR-11）。
///
/// 采用单例模式：全局只有一份话题映射表，保证“谁在哪个话题上”的信息唯一。
/// 单例实现用 Meyers 写法（函数内局部静态对象）：
///   - C++11 起局部静态变量的初始化是线程安全的；
///   - 无需手动释放，不存在忘记 delete 的问题；
///   - 通过删除拷贝构造与赋值，杜绝被复制出第二个实例。
class ROS {
public:
    static ROS& instance();

    ROS(const ROS&) = delete;
    ROS& operator=(const ROS&) = delete;

    /// 注册发布者 / 订阅者。节点名重复时抛 NodeException。
    void registerPublisher(Publisher* publisher);
    void registerSubscriber(Subscriber* subscriber);

    /// 注销节点（由 Node 析构函数自动调用）。
    /// 该接口必须幂等且不抛异常 —— 它会在析构路径上被调用。
    void unregister(Node* node) noexcept;

    /// 把消息转发给指定话题下的全部订阅者。
    /// 话题不存在（没有任何订阅者）时抛 TopicException。
    void publish(const std::string& topic, const Message& message);

    /// 统一关闭全部已注册节点，随后清空映射表与节点表。
    void shutdownAll();

    /// 查询接口，主要用于测试与演示。
    std::size_t publisherCount(const std::string& topic) const;
    std::size_t subscriberCount(const std::string& topic) const;
    std::size_t nodeCount() const noexcept { return nodes_.size(); }

private:
    ROS() = default;

    /// 校验节点名在全局唯一（发布者与订阅者共用同一命名空间）。
    void ensureUniqueName(const std::string& name) const;

    std::unordered_map<std::string, std::vector<Publisher*>> publishers_;
    std::unordered_map<std::string, std::vector<Subscriber*>> subscribers_;

    /// 所有已注册节点的总表，仅用于统一关闭。
    /// 关掉同一节点只关一次的逻辑依赖它：遍历总表而不是遍历两张映射表。
    std::vector<Node*> nodes_;
};

} // namespace rosmini
