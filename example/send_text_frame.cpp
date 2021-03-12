#include "../websocket_client.h"

#include <iostream>

int main(int argc, char **argv) {
  try {
    websocket_client client("localhost", 5000, "/");
    
    client.send_frame("Hello!");
    
    client.receive_frame([](const unsigned char opcode, const websocket_client::byte_string data) {
      if (opcode == TEXT) {
        std::cout << "Server responded with: " << std::string(data.begin(), data.end());
      }      
    });
  } catch (websocket_exception & e) {
    std::cout << "It all went horribly wrong: [" << e.what() << "]" << std::endl;
  }
}