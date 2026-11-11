#include "rosmini/Node.h"

#include <utility>

#include "rosmini/Exception.h"
#include "rosmini/Logger.h"
#include "rosmini/ROS.h"
#include "rosmini/Util.h"

namespace rosmini {

Node::Node(std::string name)
    : name_(trim(name)) {
    // 名称必须非空（对应 EX-01）
    if (name_.empty()) {
        throw NodeException("节点名不能为空：节点名去除空白后长度为 0");
    }
}

Node::~Node() {
    // 关键：从核心模块注销，避免核心的映射表里留下悬空指针。
    // unregister 声明为 noexcept 且幂等，放在析构里是安全的。
    ROS::instance().unregister(this);
}

void Node::ensureActive(const char* action) const {
    if (!initialized_) {
        throw NodeException("节点尚未初始化，无法执行 " + std::string(action) +
                            "：请先调用 init()");
    }
    if (closed_) {
        Logger::warn(name_, std::string("节点已关闭，拒绝执行 ") + action);
        throw NodeException("节点已关闭，无法执行 " + std::string(action));
    }
}

} // namespace rosmini
