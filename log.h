
#include <vector>

#include "message.h"

namespace beacon {

class Log {
public:
  void add_message(const Message& message) { m_messages.emplace_back(message); }
  void remove_message(const std::string& message_id) {}
  void remove_message(int index) {}

private:
  std::vector<Message> m_messages;
};

}