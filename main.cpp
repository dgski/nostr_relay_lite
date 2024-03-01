#include <unordered_map>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>
#include <cxxopts.hpp>

using Server = websocketpp::server<websocketpp::config::asio>;

template<typename F>
void forEachMatchingEvent(const std::vector<std::string>& events, const nlohmann::json& filters, F&& f) {
  auto matchesFilter = [](const nlohmann::json& event, const nlohmann::json& filter) {
    if (filter.contains("ids")) {
      const auto& ids = filter["ids"];
      if (ids.is_array() && std::find(ids.begin(), ids.end(), event["id"]) == ids.end()) {
        bool startsWithPrefix = std::none_of(ids.begin(), ids.end(), [&event](const auto& prefix) {
          return event["id"].get<std::string>().starts_with(prefix.template get<std::string>());
        });
        if (startsWithPrefix) {
          return false;
        }
      }
    }

    if (filter.contains("kinds")) {
      const auto& kinds = filter["kinds"];
      if (kinds.is_array() && std::find(kinds.begin(), kinds.end(), event["kind"]) == kinds.end()) {
        return false;
      }
    }

    if (filter.contains("authors")) {
      const auto& authors = filter["authors"];
      if (authors.is_array() && std::find(authors.begin(), authors.end(), event["pubkey"]) == authors.end()) {
        bool startsWithPrefix = std::none_of(authors.begin(), authors.end(), [&event](const auto& prefix) {
          return event["pubkey"].get<std::string>().starts_with(prefix.template get<std::string>());
        });
        if (startsWithPrefix) {
          return false;
        }
      }
    }

    for (const auto& [key, value] : filter.items()) {
      if (key[0] == '#') {
        std::string tagName = key.substr(1);
        const auto& values = filter["#" + tagName];
        if (values.is_array() && std::none_of(values.begin(), values.end(), [&event, &tagName](const auto& tagValue) {
          return event["tags"].contains({ tagName, tagValue }) && event["tags"][tagName] == tagValue;
        })) {
          return false;
        }
      }
    }

    if (filter.contains("since") && event["created_at"] < filter["since"]) {
      return false;
    }

    if (filter.contains("until") && event["created_at"] > filter["until"]) {
      return false;
    }

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
  std::function<void()> _closeConnection;
  std::vector<std::string> _events;
public:
  RelayToConnection(){}

  void setSendMessage(const std::function<void(const std::string&)>& sendMessage) {
    _sendMessage = sendMessage;
  }

  void setCloseConnection(const std::function<void()>& closeConnection) {
    _closeConnection = closeConnection;
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
    } else if (msgType == "CLOSE") {
      _closeConnection();
    }
  }

  void handleRequest(const nlohmann::json& json) {
    auto subId = json[1].get<std::string>();
    auto filters = json[2];

    forEachMatchingEvent(_events, filters, [this, subId](const std::string& event) {
      _sendMessage(nlohmann::json::array({ "EVENT", subId, nlohmann::json::parse(event) }).dump());
    });

    _sendMessage(nlohmann::json::array({ "EOSE", subId }).dump());
  }

  void handleEvent(const nlohmann::json& json) {
    auto event = json[1];
    _events.push_back(event.dump());
  }
};

int main(int argc, const char* argv[]) {
  cxxopts::Options options("nostr_relay_lite", "Simple relay server for Nostr");
  options.add_options()
    ("h,help", "Print help")
    ("p,port", "Port to listen on", cxxopts::value<int>()->default_value("4001"));
  const auto result = options.parse(argc, argv);
  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }
  const int port = result["port"].as<int>();

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
    relay.setCloseConnection([&server, connHandle, &connIdToRelay, connId]() {
      server.close(connHandle, websocketpp::close::status::normal, "Done");
      connIdToRelay.erase(connId);
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

  server.listen(port);
  server.start_accept();

  std::cout << "Listening on port " << port << std::endl;

  server.run();

  return 0;
}
