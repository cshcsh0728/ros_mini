#include "rosmini/Message.h"

#include <utility>

#include "rosmini/Exception.h"
#include "rosmini/Util.h"

namespace rosmini {

Message::Message(std::string content)
    : content_(trim(content)),
      timestamp_(nowTimestamp()) {
    // 内容为空或纯空白视为非法输入（对应 EX-03）
    if (content_.empty()) {
        throw MessageException("消息内容不能为空：内容长度为 0");
    }
}

} // namespace rosmini
