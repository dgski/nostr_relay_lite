#include <unordered_map>
#include <cxxopts.hpp>

#include "RelayConnection.hpp"
#include "RelayServer.hpp"

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

  std::cout << "Listening on port " << port << std::endl;
  RelayServer server(port);
  server.run();
  return 0;
}
