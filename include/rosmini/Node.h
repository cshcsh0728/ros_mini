#pragma once

#include <string>

namespace rosmini {

/// 节点抽象基类（对应需求 FR-01 / FR-02 / FR-13）。
///
/// 职责：
///   - 封装唯一的节点名称，对外只读；
///   - 定义初始化与关闭两个纯虚接口，强制子类实现（发布者、订阅者各有一份日志）；
///   - 记录节点状态（是否已初始化、是否已关闭），为“非法时序”检查提供依据；
///   - 析构时自动从核心模块注销，避免核心模块持有悬空指针。
class Node {
public:
    explicit Node(std::string name);
    virtual ~Node();

    /// 节点名称只读访问。
    const std::string& name() const noexcept { return name_; }

    /// 初始化：子类负责注册与日志。
    virtual void init() = 0;

    /// 关闭：子类负责日志。
    virtual void shutdown() = 0;

protected:
    bool isInitialized() const noexcept { return initialized_; }
    bool isClosed() const noexcept { return closed_; }

    void markInitialized() noexcept { initialized_ = true; }
    void markClosed() noexcept { closed_ = true; }

    /// 业务接口入口的统一前置校验：
    /// 未初始化、已关闭时抛出 NodeException（已关闭时额外记一条 WARN 日志）。
    void ensureActive(const char* action) const;

    std::string name_;

private:
    bool initialized_ = false;
    bool closed_ = false;
};

} // namespace rosmini
