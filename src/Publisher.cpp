#include "rosmini/Publisher.h"

#include <utility>

#include "rosmini/Exception.h"
#include "rosmini/Logger.h"
#include "rosmini/Message.h"
#include "rosmini/ROS.h"
#include "rosmini/Util.h"

namespace rosmini {

Publisher::Publisher(std::string name, std::string topic)
    : Node(std::move(name)),
      topic_(trim(topic)) {
    if (topic_.empty()) {
        throw TopicException("话题名不能为空：发布者 \"" + name_ + "\" 的话题名去除空白后长度为 0");
    }
}

void Publisher::init() {
    if (isInitialized()) {
        throw NodeException("节点 \"" + name_ + "\" 重复初始化");
    }

    // 先注册（可能因重名抛异常），成功后再置状态、打日志
    ROS::instance().registerPublisher(this);
    markInitialized();
    Logger::info(name_, "发布者已初始化，话题=" + topic_);
}

void Publisher::shutdown() {
    if (isClosed()) {
        return; // 幂等：重复关闭不产生副作用
    }
    markClosed();
    Logger::info(name_, "发布者已关闭，话题=" + topic_);
}

void Publisher::publish(const std::string& sensorData) {
    ensureActive("发布消息");

    // 创建消息对象：内容为空时由 Message 抛出 MessageException
    const Message message(sensorData);
    Logger::info(name_, "发布消息 → 话题=" + topic_ + " 内容=\"" + message.content() + "\"");

    // 交给核心转发：此处不等待、不关心有多少订阅者
    ROS::instance().publish(topic_, message);
}

} // namespace rosmini
