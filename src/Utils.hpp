#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace Utils {

bool matchesFilter(const nlohmann::json& event, const nlohmann::json& filter) {
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

  if (filter.contains("since") && event["created_at"].get<int>() < filter["since"].get<int>()) {
    return false;
  }

  if (filter.contains("until") && event["created_at"].get<int>() > filter["until"].get<int>()) {
    return false;
  }

  return true;
};

template<typename F>
void forEachMatchingEvent(const std::vector<std::string>& events, const nlohmann::json& filters, F&& f) {
  for (const auto& event : events) {
    auto json = nlohmann::json::parse(event);
    const bool match = std::all_of(
      filters.begin(), filters.end(),
      [&json](const auto& filter) { return matchesFilter(json, filter); });
    if (match) {
      f(event);
    }
  }
}

} // namespace Utils