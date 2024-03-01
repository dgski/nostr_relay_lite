#pragma once

#include <functional>
#include <vector>
#include <string>
#include <iostream>
#include <nlohmann/json.hpp>

#include "Utils.hpp"

class RelayConnection {
  std::function<void(const std::string&)> _sendMessage;
  std::function<void()> _closeConnection;
  std::vector<std::string>& _events;
public:
  RelayConnection(
    std::function<void(const std::string&)>&& sendMessage,
    std::function<void()>&& closeConnection,
    std::vector<std::string>& events
  ) : _sendMessage(sendMessage), _closeConnection(closeConnection), _events(events) {}

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

    Utils::forEachMatchingEvent(_events, filters, [this, subId](const std::string& event) {
      _sendMessage(nlohmann::json::array({ "EVENT", subId, nlohmann::json::parse(event) }).dump());
    });

    _sendMessage(nlohmann::json::array({ "EOSE", subId }).dump());
  }

  void handleEvent(const nlohmann::json& json) {
    auto event = json[1];
    _events.push_back(event.dump());
  }
};