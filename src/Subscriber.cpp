#include "rosmini/Subscriber.h"

#include <utility>

#include "rosmini/Exception.h"
#include "rosmini/Logger.h"
#include "rosmini/ROS.h"
#include "rosmini/Util.h"

namespace rosmini {

Subscriber::Subscriber(std::string name, std::string topic)
    : Node(std::move(name)),
      topic_(trim(topic)) {
    if (topic_.empty()) {
        throw TopicException("话题名不能为空：订阅者 \"" + name_ + "\" 的话题名去除空白后长度为 0");
    }
}

void Subscriber::init() {
    if (isInitialized()) {
        throw NodeException("节点 \"" + name_ + "\" 重复初始化");
    }

    ROS::instance().registerSubscriber(this);
    markInitialized();
    Logger::info(name_, "订阅者已初始化，话题=" + topic_);
}

void Subscriber::shutdown() {
    if (isClosed()) {
        return;
    }
    markClosed();
    Logger::info(name_, "订阅者已关闭，话题=" + topic_);
}

void Subscriber::receive(const Message& message) {
    ensureActive("接收消息");

    ++receivedCount_;
    Logger::info(name_, "接收消息 ← 话题=" + topic_ + " 内容=\"" + message.content() +
                            "\" 时间戳=" + message.timestamp());
}

} // namespace rosmini
