#include "rosmini/Util.h"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace rosmini {

std::string nowTimestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _MSC_VER
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    std::ostringstream oss;
    // put_time 会自动补零，保证输出形如 2026-11-11 21:30:05
    oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return std::string();
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

bool isBlank(const std::string& text) {
    return trim(text).empty();
}

} // namespace rosmini
