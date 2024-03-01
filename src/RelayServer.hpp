#pragma once

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>

using WsServer = websocketpp::server<websocketpp::config::asio>;

// Manages websocket server as less as logical relays
class RelayServer {
  WsServer _wsServer;
  int _nextConnId = 0;
  std::unordered_map<int, RelayConnection> _connIdToRelay;
  std::vector<std::string> _events;
public:
  RelayServer(int port) {
    _wsServer.set_access_channels(websocketpp::log::alevel::none);
    _wsServer.init_asio();
    _wsServer.listen(port);
    _wsServer.start_accept();
    _wsServer.set_open_handler([this](auto connHandle) {
      initConnection(connHandle);
    });
  }
  void run() {
    _wsServer.run();
  }

  void initConnection(websocketpp::connection_hdl connHandle) {
    std::cout << "Connection opened" << std::endl;
    const int connId = _nextConnId++;
    const auto sendMessage = [this, connHandle](const std::string& msg) {
      std::cout << "Sending message: " << msg << std::endl;
      _wsServer.send(connHandle, msg, websocketpp::frame::opcode::text);
    };
    const auto closeConnection = [this, connHandle, connId]() {
      _wsServer.close(connHandle, websocketpp::close::status::normal, "Done");
      _connIdToRelay.erase(connId);
    };
    auto& relay = _connIdToRelay.emplace(
      connId, RelayConnection(sendMessage, closeConnection, _events)).first->second;
    auto connection = _wsServer.get_con_from_hdl(connHandle);
    connection->set_message_handler([&relay](auto connHandle, auto msg) {
      relay.handleMessage(msg->get_payload());
    });
    connection->set_close_handler([this, connId](auto connHandle) {
      _connIdToRelay.erase(connId);
    });
  }
};