#include <iostream>
#include <regex>
#include <string>

#include "rosmini/Exception.h"
#include "rosmini/Logger.h"
#include "rosmini/Message.h"
#include "rosmini/Publisher.h"
#include "rosmini/ROS.h"
#include "rosmini/Subscriber.h"
#include "rosmini/Util.h"

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const std::string& description, int line) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::cout << "  [FAIL] 第 " << line << " 行：" << description << std::endl;
    }
}

template <typename ExceptionType, typename Function>
bool throwsException(Function&& function) {
    try {
        function();
    } catch (const ExceptionType&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

#define CHECK(condition) check((condition), #condition, __LINE__)

#define CHECK_THROWS(statement, ExceptionType)                       \
    check(throwsException<ExceptionType>([&] { statement; }),         \
          "应抛出 " #ExceptionType,                                  \
          __LINE__)

void runTest(const char* name, void (*test)()) {
    std::cout << "[用例] " << name << std::endl;
    const int before = g_failures;
    test();
    if (g_failures == before) {
        std::cout << "  [PASS]" << std::endl;
    }
}

// 用例 1：消息时间戳格式与内容去空白
void testMessage() {
    const rosmini::Message message("  temperature=25.3  ");
    const std::regex pattern(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$)");
    CHECK(std::regex_match(message.timestamp(), pattern));
    CHECK(message.content() == "temperature=25.3");
    CHECK(rosmini::nowTimestamp().size() == 19);
}

// 用例 2：空内容 / 纯空白内容
void testMessageEmpty() {
    CHECK_THROWS(rosmini::Message bad(""), rosmini::MessageException);
    CHECK_THROWS(rosmini::Message bad("   "), rosmini::MessageException);
}

// 用例 3：空节点名 / 空话题名
void testInvalidNames() {
    CHECK_THROWS(rosmini::Publisher bad("", "sensor"), rosmini::NodeException);
    CHECK_THROWS(rosmini::Publisher bad("pub_x", "  "), rosmini::TopicException);
    CHECK_THROWS(rosmini::Subscriber bad("", "sensor"), rosmini::NodeException);
}

// 用例 4：重复节点名注册
void testDuplicateRegistration() {
    rosmini::Publisher first("dup_node", "sensor");
    first.init();
    rosmini::Publisher second("dup_node", "sensor");
    CHECK_THROWS(second.init(), rosmini::NodeException);
    rosmini::ROS::instance().shutdownAll();
}

// 用例 5：向不存在的话题发布
void testPublishToUnknownTopic() {
    rosmini::Publisher publisher("lonely_pub", "lonely_topic");
    publisher.init();
    CHECK_THROWS(publisher.publish("x"), rosmini::TopicException);
    rosmini::ROS::instance().shutdownAll();
}

// 用例 6：一条消息送达该话题下的全部订阅者
void testDeliverToAllSubscribers() {
    rosmini::Publisher publisher("fan_pub", "fan");
    rosmini::Subscriber sub1("fan_sub_1", "fan");
    rosmini::Subscriber sub2("fan_sub_2", "fan");

    publisher.init();
    sub1.init();
    sub2.init();

    CHECK(rosmini::ROS::instance().subscriberCount("fan") == 2);

    publisher.publish("a");
    publisher.publish("b");

    CHECK(sub1.receivedCount() == 2);
    CHECK(sub2.receivedCount() == 2);

    rosmini::ROS::instance().shutdownAll();
}

// 用例 7：话题隔离——订阅者不接收其他话题的消息
void testTopicIsolation() {
    rosmini::Publisher publisher("iso_pub", "topic_a");
    rosmini::Subscriber subscriber("iso_sub", "topic_b");

    publisher.init();
    subscriber.init();

    CHECK_THROWS(publisher.publish("x"), rosmini::TopicException);
    CHECK(subscriber.receivedCount() == 0);

    rosmini::ROS::instance().shutdownAll();
}

// 用例 8：未初始化、已关闭状态下禁止参与通信
void testStateGuards() {
    {
        rosmini::Publisher publisher("raw_pub", "sensor");
        CHECK_THROWS(publisher.publish("x"), rosmini::NodeException);
    }
    {
        rosmini::Publisher publisher("closed_pub", "sensor");
        rosmini::Subscriber subscriber("closed_sub", "sensor");
        publisher.init();
        subscriber.init();
        publisher.shutdown();
        CHECK_THROWS(publisher.publish("x"), rosmini::NodeException);
        rosmini::ROS::instance().shutdownAll();
    }
}

// 用例 9：统一关闭后映射表被清空
void testShutdownAll() {
    rosmini::Publisher publisher("shut_pub", "sensor");
    rosmini::Subscriber subscriber("shut_sub", "sensor");
    publisher.init();
    subscriber.init();

    CHECK(rosmini::ROS::instance().nodeCount() == 2);

    rosmini::ROS::instance().shutdownAll();

    CHECK(rosmini::ROS::instance().nodeCount() == 0);
    CHECK(rosmini::ROS::instance().subscriberCount("sensor") == 0);
    CHECK_THROWS(publisher.publish("x"), rosmini::NodeException);
}

} // namespace

int main() {
    // 测试期间只输出 WARN 及以上级别，保持控制台整洁
    rosmini::Logger::setMinLevel(rosmini::LogLevel::Warn);

    runTest("消息封装与时间戳格式", testMessage);
    runTest("空消息内容校验", testMessageEmpty);
    runTest("空节点名/空话题名校验", testInvalidNames);
    runTest("重复节点名注册", testDuplicateRegistration);
    runTest("向不存在的话题发布", testPublishToUnknownTopic);
    runTest("消息送达全部订阅者", testDeliverToAllSubscribers);
    runTest("话题隔离", testTopicIsolation);
    runTest("未初始化/已关闭状态保护", testStateGuards);
    runTest("统一关闭", testShutdownAll);

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "检查项：" << g_checks << " 个，失败：" << g_failures << " 个" << std::endl;
    if (g_failures == 0) {
        std::cout << "全部用例通过" << std::endl;
        return 0;
    }
    std::cout << "存在失败用例" << std::endl;
    return 1;
}
