#pragma once

#include <string>

namespace rosmini {

/// 生成当前本地时间字符串，格式固定为 "YYYY-MM-DD HH:MM:SS"（各字段补零）。
/// 实现见 Util.cpp：使用 std::put_time，保证跨平台行为一致。
std::string nowTimestamp();

/// 去掉字符串首尾的空白字符（空格、制表符、回车、换行）。
std::string trim(const std::string& text);

/// 判断字符串是否为空或仅由空白字符组成。
bool isBlank(const std::string& text);

} // namespace rosmini
