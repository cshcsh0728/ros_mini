#pragma once

#include <string>

namespace rosmini {

/// 消息（数据载体）。
///
/// 约束（对应需求 FR-04）：
///   1. 内容为字符串类型，构造时校验非空（去除首尾空白后非空）；
///   2. 时间戳在构造时自动生成，格式 "YYYY-MM-DD HH:MM:SS"；
///   3. 对外只提供只读接口，不提供任何写接口 —— 对象一旦创建即不可变。
class Message {
public:
    explicit Message(std::string content);

    /// 只读访问消息内容。
    const std::string& content() const noexcept { return content_; }

    /// 只读访问消息时间戳。
    const std::string& timestamp() const noexcept { return timestamp_; }

private:
    std::string content_;
    std::string timestamp_;
};

} // namespace rosmini
