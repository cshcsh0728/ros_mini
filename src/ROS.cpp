#include "rosmini/ROS.h"

#include <algorithm>
#include <iterator>

#include "rosmini/Exception.h"
#include "rosmini/Logger.h"
#include "rosmini/Message.h"
#include "rosmini/Publisher.h"
#include "rosmini/Subscriber.h"

namespace rosmini {

ROS& ROS::instance() {
    // Meyers 单例：C++11 起局部静态变量的初始化是线程安全的，
    // 且无需手动释放（程序结束时自动析构）。
    static ROS instance;
    return instance;
}

void ROS::registerPublisher(Publisher* publisher) {
    if (publisher == nullptr) {
        throw NodeException("注册失败：发布者为空指针");
    }
    if (publisher->topic().empty()) {
        throw TopicException("注册失败：发布者 \"" + publisher->name() + "\" 的话题名为空");
    }
    ensureUniqueName(publisher->name());

    publishers_[publisher->topic()].push_back(publisher);
    nodes_.push_back(publisher);
}

void ROS::registerSubscriber(Subscriber* subscriber) {
    if (subscriber == nullptr) {
        throw NodeException("注册失败：订阅者为空指针");
    }
    if (subscriber->topic().empty()) {
        throw TopicException("注册失败：订阅者 \"" + subscriber->name() + "\" 的话题名为空");
    }
    ensureUniqueName(subscriber->name());

    subscribers_[subscriber->topic()].push_back(subscriber);
    nodes_.push_back(subscriber);
}

void ROS::unregister(Node* node) noexcept {
    if (node == nullptr) {
        return;
    }

    // 把指针从两张映射表中移除，并顺手删掉“已经空了”的话题条目，
    // 否则 topics() 与状态栏会残留已无人使用的话题名。
    // 注意比较的是“基类指针地址”，不能对 node 做向下转型（类型未知，行为未定义）。
    for (auto it = publishers_.begin(); it != publishers_.end();) {
        auto& list = it->second;
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [node](Publisher* item) {
                                      return static_cast<Node*>(item) == node;
                                  }),
                   list.end());
        it = list.empty() ? publishers_.erase(it) : std::next(it);
    }
    for (auto it = subscribers_.begin(); it != subscribers_.end();) {
        auto& list = it->second;
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [node](Subscriber* item) {
                                      return static_cast<Node*>(item) == node;
                                  }),
                   list.end());
        it = list.empty() ? subscribers_.erase(it) : std::next(it);
    }

    nodes_.erase(std::remove(nodes_.begin(), nodes_.end(), node), nodes_.end());
}

void ROS::publish(const std::string& topic, const Message& message) {
    const auto it = subscribers_.find(topic);
    if (it == subscribers_.end() || it->second.empty()) {
        throw TopicException("话题不存在：\"" + topic + "\" 尚未注册任何订阅者");
    }

    // 遍历该话题下的全部订阅者 —— 循环次数即订阅者数量，这就是“一对多”转发。
    for (Subscriber* subscriber : it->second) {
        if (subscriber != nullptr) {
            subscriber->receive(message);
        }
    }
}

void ROS::shutdownAll() {
    if (nodes_.empty()) {
        Logger::info("ROS", "当前没有已注册的节点，无需关闭");
        return;
    }

    Logger::info("ROS", "开始关闭全部节点（共 " + std::to_string(nodes_.size()) + " 个）");
    for (Node* node : nodes_) {
        if (node == nullptr) {
            continue;
        }
        try {
            node->shutdown();
        } catch (const std::exception& e) {
            // 单个节点关闭失败不应中断整体关闭流程
            Logger::error("ROS", std::string("关闭节点时发生异常：") + e.what());
        }
    }

    // 必须“先遍历后清空”，否则遍历过程中容器被修改会导致迭代器失效。
    publishers_.clear();
    subscribers_.clear();
    nodes_.clear();
    Logger::info("ROS", "全部节点已关闭，映射表已清空");
}

std::size_t ROS::publisherCount(const std::string& topic) const {
    const auto it = publishers_.find(topic);
    return it == publishers_.end() ? 0 : it->second.size();
}

std::size_t ROS::subscriberCount(const std::string& topic) const {
    const auto it = subscribers_.find(topic);
    return it == subscribers_.end() ? 0 : it->second.size();
}

std::vector<std::string> ROS::topics() const {
    std::vector<std::string> result;
    result.reserve(publishers_.size() + subscribers_.size());
    for (const auto& entry : publishers_) {
        result.push_back(entry.first);
    }
    for (const auto& entry : subscribers_) {
        if (std::find(result.begin(), result.end(), entry.first) == result.end()) {
            result.push_back(entry.first);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

void ROS::ensureUniqueName(const std::string& name) const {
    const auto it = std::find_if(nodes_.begin(), nodes_.end(),
                                 [&name](const Node* node) {
                                     return node != nullptr && node->name() == name;
                                 });
    if (it != nodes_.end()) {
        throw NodeException("节点注册失败：名称 \"" + name + "\" 已被占用");
    }
}

} // namespace rosmini
