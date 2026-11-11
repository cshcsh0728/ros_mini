#include "rosmini/Logger.h"

#include <iostream>
#include <sstream>
#include <utility>

#include "rosmini/Util.h"

namespace rosmini {

LogLevel Logger::minLevel_ = LogLevel::Info;
std::ofstream Logger::file_;
std::function<void(const std::string&)> Logger::sink_;

void Logger::setSink(std::function<void(const std::string&)> sink) {
    sink_ = std::move(sink);
}

void Logger::setMinLevel(LogLevel level) {
    minLevel_ = level;
}

void Logger::setLogFile(const std::string& path) {
    closeLogFile();
    if (path.empty()) {
        return;
    }
    file_.open(path, std::ios::out | std::ios::app);
}

void Logger::closeLogFile() {
    if (file_.is_open()) {
        file_.close();
    }
}

void Logger::info(const std::string& module, const std::string& message) {
    write(LogLevel::Info, module, message);
}

void Logger::warn(const std::string& module, const std::string& message) {
    write(LogLevel::Warn, module, message);
}

void Logger::error(const std::string& module, const std::string& message) {
    write(LogLevel::Error, module, message);
}

void Logger::write(LogLevel level, const std::string& module, const std::string& message) {
    if (static_cast<int>(level) < static_cast<int>(minLevel_)) {
        return;
    }

    static const char* const kLevelNames[] = {"INFO ", "WARN ", "ERROR"};
    const auto index = static_cast<std::size_t>(static_cast<int>(level));

    std::ostringstream oss;
    oss << "[" << kLevelNames[index] << "] [" << nowTimestamp() << "] ["
        << module << "] " << message;
    const std::string line = oss.str();

    std::cout << line << std::endl;
    if (file_.is_open()) {
        file_ << line << std::endl;
    }
    if (sink_) {
        sink_(line);
    }
}

} // namespace rosmini
