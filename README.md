# cpp-websocket-client

cpp-websocket-client is a simple dependency free, header only websocket client implementation for C++11 or greater.  

It was initially written for use as a testing utility library and is not suitable for use with a live system.

## Installation

Move `websocket_client.h` into your include path, then include it.

## Usage

cpp-websocket-client has one functional class, `websocket_client`.

```
// Connect to endpoint
try {
	websocket_client client("host", port, "resource");
} catch(websocket_exception & e) {
	std::cout << "Failed to get websocket client: [" << e.what() << "]" << std::endl;
}

...

// Send a text frame
try {
	client.send_frame("Hello!");
} catch(websocket_exception & e) {
	std::cout << "Failed to  send frame: [" << e.what() << "]" << std::endl;
}

...

// Send a binary frame (data is a std::basic_string<unsigned char>)
try {
	client.send_frame(BINARY, data);
} catch(websocket_exception & e) {
	std::cout << "Failed to send frame: [" << e.what() << "]" << std::endl;
}

...

// Send a pong frame
try {
	client.send_frame(PONG);
} catch(websocket_exception & e) {
	std::cout << "Failed to send frame: [" << e.what() << "]" << std::endl;
}

// Send a close frame (data is a std::basic_string<unsigned char>)
try {
	client.send_frame(CLOSE, data);
} catch(websocket_exception & e) {
	std::cout << "Failed to send frame: [" << e.what() << "]" << std::endl;
}

// Receive a frame from the client.  This will block until data is received
// websocket_client::byte_string is a typedef for std::basic_string<unsigned char>
try {
	client.receive_frame([](unsigned char opcode, const websocket_client::byte_string data) {
		if (opcode == TEXT) {
			std::cout << "Server sent TEXT frame " << std::string(data.begin(), data.end()) << std::endl;
		}
	});
} catch(websocket_exception & e) {
	std::cout << "Failed to receive frame: [" << e.what() << "]" << std::endl;
}

// Close the client, this is also called when the client goes out of scope
client.close_client();
```

## Improvements
  * Requires tests
  * Requires support for fragmented frames
  * Requires support for Windows

## Contributions and feedback

Feel free to raise issues or submit pull requests.  