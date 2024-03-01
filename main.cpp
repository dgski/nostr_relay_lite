#include <unordered_map>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>

using Server = websocketpp::server<websocketpp::config::asio>;

template<typename F>
void forEachMatchingEvent(const std::vector<std::string>& events, const nlohmann::json& filters, F&& f) {
  auto matchesFilter = [](const nlohmann::json& event, const nlohmann::json& filter) {
    // TODO: add real filtering
    return true;
  };

  auto matchesAnyFilter = [&filters, &matchesFilter](const nlohmann::json& event) {
    for (const auto& filter : filters) {
      if (matchesFilter(event, filter)) {
        return true;
      }
    }
    return false;
  };
  for (const auto& event : events) {
    auto json = nlohmann::json::parse(event);
    if (matchesAnyFilter(json)) {
      f(event);
    }
  }
}

class RelayToConnection {
  std::function<void(const std::string&)> _sendMessage;
  std::vector<std::string> _events;
public:
  RelayToConnection(){}

  void setSendMessage(const std::function<void(const std::string&)>& sendMessage) {
    _sendMessage = sendMessage;
  }

  void setEvents(const std::vector<std::string>& events) {
    _events = events;
  }

  void handleMessage(const std::string& msg) {
    std::cout << "Message: " << msg << std::endl;
    auto json = nlohmann::json::parse(msg);
    const auto& msgType = json[0].get<std::string>();
    if (msgType == "REQ") {
      handleRequest(json);
    } else if (msgType == "EVENT") {
      handleEvent(json);
    }
  }

  void handleRequest(const nlohmann::json& json) {
    auto subId = json[1].get<std::string>();
    auto filters = json[2];

    forEachMatchingEvent(_events, filters, [this, subId](const std::string& event) {
      auto json = nlohmann::json::parse(event);
      auto response = nlohmann::json::array();
      response.push_back("EVENT");
      response.push_back(subId);
      response.push_back(json);
      _sendMessage(response.dump());
    });

    auto response = nlohmann::json::array();
    response.push_back("EOSE");
    response.push_back(subId);
    _sendMessage(response.dump());
  }

  void handleEvent(const nlohmann::json& json) {
    auto event = json[1];
    _events.push_back(event.dump());
  }
};

int main() {
  Server server;
  server.set_access_channels(websocketpp::log::alevel::none);
  server.init_asio();

  int nextConnId = 0;
  std::unordered_map<int, RelayToConnection> connIdToRelay;
  std::vector<std::string> events;

  server.set_open_handler([&server, &connIdToRelay, &nextConnId, &events](auto connHandle) {
    std::cout << "Connection opened" << std::endl;
    const int connId = nextConnId++;
    auto& relay = connIdToRelay[connId];
    relay.setSendMessage([&server, connHandle](const std::string& msg) {
      std::cout << "Sending message: " << msg << std::endl;
      server.send(connHandle, msg, websocketpp::frame::opcode::text);
    });
    relay.setEvents(events);

    auto connection = server.get_con_from_hdl(connHandle);
    connection->set_message_handler([&relay](auto connHandle, auto msg) {
      relay.handleMessage(msg->get_payload());
    });
    connection->set_close_handler([&connIdToRelay, &connId](auto connHandle) {
      connIdToRelay.erase(connId);
    });
  });

  server.listen(4001);
  server.start_accept();
  server.run();

  return 0;
}
