#pragma once

#include <functional>
#include <vector>
#include <string>
#include <iostream>
#include <unordered_map>
#include <nlohmann/json.hpp>

#include "Utils.hpp"

// Manages a single relay relationship
class RelayConnection {
  std::function<void(const std::string&)> _sendMessage;
  std::vector<std::string>& _events;
  std::unordered_map<std::string, nlohmann::json> _subscriptions;
public:
  RelayConnection(
    std::function<void(const std::string&)>&& sendMessage,
    std::function<void()>&& closeConnection,
    std::vector<std::string>& events
  ) : _sendMessage(sendMessage), _events(events) {}

  void handleMessage(const std::string& msg) {
    std::cout << "Message: " << msg << std::endl;
    auto json = nlohmann::json::parse(msg);
    const auto& msgType = json[0].get<std::string>();
    if (msgType == "REQ") {
      handleRequest(json);
    } else if (msgType == "EVENT") {
      handleEvent(json);
    } else if (msgType == "CLOSE") {
      handleClose(json);
    }
  }

  void handleRequest(const nlohmann::json& json) {
    auto subId = json[1].get<std::string>();
    auto filters = json[2];
    addSubscription(subId, filters);
    Utils::forEachMatchingEvent(_events, filters, [this, subId](const std::string& event) {
      _sendMessage(nlohmann::json::array({ "EVENT", subId, nlohmann::json::parse(event) }).dump());
    });
    _sendMessage(nlohmann::json::array({ "EOSE", subId }).dump());
  }

  void handleEvent(const nlohmann::json& json) {
    auto event = json[1];
    _events.push_back(event.dump());
    for (const auto& [subId, filters] : _subscriptions) {
      if (Utils::matchesAnyFilter(event, filters)) {
        _sendMessage(nlohmann::json::array({ "EVENT", subId, event }).dump());
      }
    }
  }

  void handleClose(const nlohmann::json& json) {
    _subscriptions.erase(json[1].get<std::string>());
  }

  void addSubscription(const std::string& subId, const nlohmann::json& filters) {
    _subscriptions[subId] = filters;
  }
};