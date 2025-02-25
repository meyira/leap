#include <coproto/Socket/AsioSocket.h>
#include <iostream>
#include <leap/psi/SpringNRPSIClient.h>
#include <leap/psi/SpringNRPSIServer.h>

int main(int argc, char **argv) {
  if (argc < 4) {
    std::cout << "usage: " << argv[0]
              << " {is_client} {ip} {port} {log2(num_inputs)} " << std::endl;
    return -1;
  }
  std::string ip;
  ip += argv[2];
  ip += ':';
  ip += argv[3];
  int exp = std::stoi(std::string(argv[4]));
  if (0 > exp || exp > 32) {
    std::cout << "log2(num_inputs) should be between 0 and 32" << std::endl;
    return -1;
  }
  if (atoi(argv[1]) == 1) {
    cp::AsioSocket client_socket = coproto::asioConnect(ip, true);
    size_t num_inputs_client = 1ULL << exp;
    LEAP::SpringNRPSIClient client(client_socket);
    client.Setup();
    std::vector<block> elements;
    PRNG prng(sysRandomSeed());
    elements.push_back(toBlock((const uint8_t *)"ffffffff88888888"));
    for (size_t i = 1; i < num_inputs_client; i++) {
      auto a = prng.get<uint64_t>();
      auto b = prng.get<uint64_t>();
      elements.push_back(toBlock(a, b));
    }
    client.Base(elements.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    client.Online(elements);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  else {
    // server
    size_t num_inputs_server = 1ULL << exp;
    cp::AsioSocket server_socket = coproto::asioConnect(ip, false);
    LEAP::SpringNRPSIServer server(server_socket, 1);
    std::vector<block> elements;
    elements.push_back(toBlock((const uint8_t *)"ffffffff88888888"));
    PRNG prng(sysRandomSeed());
    for (size_t i = 1; i < num_inputs_server; i++) {
      auto a = prng.get<uint64_t>();
      auto b = prng.get<uint64_t>();
      elements.push_back(toBlock(a, b));
    }
    server.Setup(elements);
    server.Base();
    server.Online();
  }
  return 0;
}
