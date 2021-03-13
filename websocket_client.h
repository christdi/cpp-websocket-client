#ifndef _UTILITY_WEBSOCKET_CLIENT_H_
#define _UTILITY_WEBSOCKET_CLIENT_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <unistd.h>

#include <vector>
#include <exception>
#include <sstream>
#include <string>
#include <cstdint>

enum opcode_type_t {
  CONTINUATION = 0x00,
  TEXT = 0x01,
  BINARY = 0x02,
  CLOSE = 0x08,
  PING = 0x09,
  PONG = 0x0A
};

class websocket_exception : public std::exception {
  public:
    websocket_exception() = delete;
    
    websocket_exception(const std::string & message) : 
      _message(message) {}

    virtual ~websocket_exception() = default;

    virtual const char * what() const noexcept {
      return _message.c_str();
    };

  private:
    std::string _message;
};

class websocket_client {
  public:
    typedef std::basic_string<unsigned char> byte_string;
    
    websocket_client(const std::string & host, const int port, const std::string & endpoint) :
      _fd(0),
      _host(host),
      _port(port),
      _endpoint(endpoint) {
      struct addrinfo hints, *res;

      memset(&hints, 0, sizeof hints);
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;

      getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res);

      _fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

      if (connect(_fd, res->ai_addr, res->ai_addrlen) == -1 ) {
        throw websocket_exception(strerror(errno));
      };

      perform_handshake();      
    }

    ~websocket_client() {
      close_client();
    }

    void send_frame(const std::string & data) {
      send_frame(TEXT, byte_string(data.begin(), data.end()));
    }

    void send_frame(opcode_type_t type, const byte_string data = byte_string()) {
      byte_string payload = encode_frame(type, data);

      int len = payload.length();
      int bytes_sent = 0;
      const unsigned char * request = payload.c_str();

      while (bytes_sent != len) {
        if ((bytes_sent += send(_fd, request + bytes_sent, len, 0)) == -1) {
          throw websocket_exception(strerror(errno));
        }
      }
    }

    template <typename func_t>
    void receive_frame(func_t && callback) {
      byte_string default_header = receive_partial_frame(2);

      unsigned char fin = (default_header[0] >> 7) & 0x01;  
      unsigned char opcode = default_header[0] & 0x0F;
      unsigned char masked = (default_header[1] >> 7) & 0x01;
      int64_t payload_length = default_header[1] & (~0x80);

      if (fin == 0 || opcode == CONTINUATION) {
        throw websocket_exception("received fragmented frame, not currently supported");
      }

      if (opcode == BINARY || opcode == TEXT) {
        receive_data_frame(opcode, masked, payload_length, callback);
      } else if (opcode == CLOSE || opcode == PING || opcode == PONG) {
        callback(opcode, byte_string());
      } else {
        throw websocket_exception("received unsupported opcode");
      }
    }

    void close_client() {
      if (_fd) {
        close(_fd);

        _fd = 0;
      }      
    }

  private:
    void perform_handshake() {
      std::stringstream ss;

      ss << "GET ws://" << _host << ":" << _port << _endpoint << " HTTP /1.1\r\n"
         << "Host:" << _host << ":" << _port << "\r\n"
         << "Connection: Upgrade\r\n"
         << "Upgrade: websocket\r\n"
         << "Origin: null\r\n"
         << "Sec-WebSocket-Version: 13\r\n"
         << "Sec-WebSocket-Key: l3ghee7Qd0GV/SLU1K6P7g==\r\n\r\n";


      std::string ss_string = ss.str();
      const char * request = ss_string.c_str();
      int len = strlen(request);
      int bytes_sent = 0;

      while (bytes_sent != len) {
        if ((bytes_sent += send(_fd, request + bytes_sent, len, 0)) == -1) {
          throw websocket_exception(strerror(errno));
        }
      }
    
      std::string http_response;
      
      while (http_response.find("\r\n\r\n") == std::string::npos) {
        int bytes_read = 0;
        char buffer[1024];

        bytes_read = recv(_fd, buffer, 1024, 0);

        if (bytes_read <= 0) {
          if (bytes_read == 0) {
            throw websocket_exception("server hungup during handshake");
          }          

          throw websocket_exception(strerror(errno));
        }

        buffer[bytes_read] = '\0';
        http_response.append(buffer);
      }

      if (http_response.find("101 Switching Protocols") == std::string::npos) {
        throw websocket_exception("websocket handshake failed, HTTP status 101 was not returned from handshake");
      }
    }

    template <typename func_t>
    void receive_data_frame(unsigned char opcode, unsigned char masked, int64_t payload_length, func_t && callback) {
      if (payload_length == 126) {
        byte_string length_header = receive_partial_frame(2);

        payload_length = (length_header[0] << 8) | (length_header[1]);
      } else if (payload_length == 127) {
        byte_string length_header = receive_partial_frame(8);

        payload_length = ((uint64_t)length_header[0] << 56) | 
          ((uint64_t)length_header[1] << 48) | 
          ((uint64_t)length_header[2] << 40) |
          ((uint64_t)length_header[3] << 32) |
          ((uint64_t)length_header[4] << 24) |
          ((uint64_t)length_header[5] << 16) |
          ((uint64_t)length_header[6] << 8) |
          ((uint64_t)length_header[7]);  
      }

      byte_string mask_header;
      if (masked) {
        mask_header = receive_partial_frame(4);
      }

      byte_string payload = receive_partial_frame(payload_length);

      if (masked) {
        for (size_t i = 0; i < payload.length(); i++ ) {
          payload[i] = payload[i] ^ mask_header[i % 4];
        }        
      }

      callback(opcode, payload);
    }    

    byte_string encode_frame(opcode_type_t opcode, const byte_string & data) {
      std::vector<unsigned char> buffer;

      buffer.push_back((unsigned char) opcode);
      buffer[0] = buffer[0] | 0x80;

      // Payload length
      if (data.empty()) {
        buffer.push_back(0);
      } else {
        size_t length = data.length();

        if(length <= 125) {
          buffer.push_back(length);
        } else if(length <= 65535) {
          buffer.push_back(126);
          buffer.push_back((length >> 8) & 0xFF);
          buffer.push_back((length & 0xFF));
        } else {
          buffer.push_back(127);

          for(int i = 3; i >= 0; i--) {
            buffer.push_back(0);
          }

          for(int i = 3; i >= 0; i--) {
            buffer.push_back(((length >> 8 * i) & 0xFF));
          }
        }        
      }

      // Toggle mask bit
      buffer[1] = buffer[1] | 0x80;

      // Masks
      std::vector<unsigned char> masks;

      for (int i = 3; i >= 0; i--) {
        masks.push_back(rand() % 256 + 1);
      }

      buffer.insert(buffer.end(), masks.begin(), masks.end());

      if (!data.empty()) {
        // Payload
        byte_string payload(data.begin(), data.end());

        for (size_t i = 0; i < payload.length(); i++ ) {
          payload[i] = payload[i] ^ masks[i % 4];
        }

        buffer.insert(buffer.end(), payload.begin(), payload.end());        
      }

      return byte_string(buffer.begin(), buffer.end());      
    }

    byte_string receive_partial_frame(int64_t receive_size) {
      unsigned char * buffer = new unsigned char[receive_size];

      int total_bytes_read = 0;

      while (total_bytes_read < receive_size) {
        int bytes_read = 0;

        if ((bytes_read = recv(_fd, buffer + total_bytes_read, receive_size, 0)) <= 0) {      
          if (bytes_read == 0) {
            throw websocket_exception("server hungup while receiving partial frame");
          }

          throw websocket_exception(strerror(errno));
        }

        total_bytes_read += bytes_read;

        buffer[total_bytes_read] = '\0';
      }


      return byte_string(buffer);
    }

    int _fd;
    std::string _host;
    int _port;
    std::string _endpoint;
};

#endif