#include <algorithm>
#include <bitset>
#include <cstring>
#include <interface/socket_listener.hpp>
#include <iomanip>
#include <iostream>
#include <request/request_handler.hpp>
#include <string>
#include <types/types.hpp>

using namespace KData;

template <typename T>
static std::string toBinaryString(const T& x) {
  std::stringstream ss;
  ss << std::bitset<sizeof(T) * 8>(x);
  return ss.str();
}

const std::vector<int> range_values{1, 2, 3, 4, 5, 6};

bool hasNthBitSet(int value, int n) {
  std::cout << "Checking for " << n << " bit" << std::endl;
  auto result = value & (1 << (n - 1));
  if (result) {
    std::cout << std::hex << " has bit " << n << " set" << std::endl;
    return true;
  }
  return false;
}

bool isdigits(const std::string& s) {
  for (char c : s)
    if (!isdigit(c)) {
      return false;
    }
  return true;
}

size_t findNullIndex(char* data) {
  size_t index = 0;
  while (data) {
    if (strcmp(data, "\0") == 0) {
      break;
    }
    index++;
    data++;
  }
  return index;
}

class KServer : public SocketListener {
 public:
  KServer(int argc, char** argv) : SocketListener(argc, argv) {}
  ~KServer() {}

  void set_handler(const RequestHandler&& handler) {
    m_request_handler = handler;
  }

  virtual void onMessageReceived(int client_socket_fd,
                                 std::weak_ptr<char[]> w_buffer_ptr) override {
    std::shared_ptr<char[]> s_buffer_ptr = w_buffer_ptr.lock();
    size_t null_index = findNullIndex(s_buffer_ptr.get());

    std::string message_string =
        std::string(s_buffer_ptr.get(), s_buffer_ptr.get() + null_index);

    size_t br_pos = message_string.find("\r\n");
    if (br_pos != std::string::npos) {
      message_string.erase(message_string.find("\r\n"));
    }

    if (message_string.size() > 0 && isdigits(message_string)) {
      std::cout << "Numerical data discovered" << std::endl;
      std::vector<uint32_t> bits{};
      try {
        unsigned int message_code = stoi(message_string);
        std::string binary_string = toBinaryString<int>(+message_code);
        std::cout << binary_string << "\n"
                  << std::hex << +message_code << std::endl;
        for (const auto& i : range_values) {
          if (hasNthBitSet(stoi(message_string), i)) {
            bits.push_back(static_cast<uint32_t>(i));
          }
        }

        std::pair<uint8_t*, int> tuple_data = m_request_handler(bits);

        uint8_t* byte_buffer = tuple_data.first;
        int size = tuple_data.second;

        auto message = GetMessage(byte_buffer);

        auto id = message->id();
        const flatbuffers::Vector<uint8_t>* data = message->data();

        std::cout << "ID: " << id << std::endl;
        int index = 0;

        std::string message_string{};

        for (auto it = data->begin(); it != data->end(); it++) {
          message_string += (char)*it;
        }

        std::cout << "Message: " << message_string << std::endl;

        std::string placeholder{"Return string"};

        sendMessage(client_socket_fd, const_cast<char*>(placeholder.c_str()),
                    placeholder.size());
      } catch (std::out_of_range& e) {
        std::cout << e.what() << std::endl;
        const char* return_message{"Out of range\0"};
        sendMessage(client_socket_fd, return_message, 13);
      }
    } else {
      const char* return_message{"Value was not accepted\0"};
      sendMessage(client_socket_fd, return_message, static_cast<size_t>(24));
    }
  };

 private:
  RequestHandler m_request_handler;
};
