#ifndef __DECODER_HPP__
#define __DECODER_HPP__

#include <log/logger.h>
#include <server/types.hpp>
#include <common/util.hpp>

namespace Decoder {
/**
 * FileHandler
 *
 * Responsible for receiving and decoding file data for a client
 */
class FileHandler {
 public:

 /**
  * File
  *
  * What we make here
  */
  struct File {
    uint8_t*  b_ptr;
    uint32_t  size;
    bool      complete;
  };

/**
 * Decoder
 *
 * Does most of the heavy lifting
 */
  class Decoder {
   public:
   /**
    * @constructor
    */
    Decoder(int fd, std::string name,
            std::function<void(uint8_t *, int, std::string)> file_callback,
            bool keep_header = false)
        : file_buffer(nullptr),
          packet_buffer(nullptr),
          index(0),
          packet_buffer_offset(0),
          total_packets(0),
          file_buffer_offset(0),
          file_size(0),
          filename(name),
          m_fd(fd),
          m_file_cb(file_callback),
          m_keep_header(keep_header)
          {
            KLOG("Decoder instantiated");
          }

  /**
   * @destructor
   */
    ~Decoder() {
      if (file_buffer != nullptr) {
        KLOG("Deleting file buffer and packet buffer");
        delete[] file_buffer;
        delete[] packet_buffer;
        file_buffer   = nullptr;
        packet_buffer = nullptr;
      }
    }

    /**
     * clearPacketBuffer
     *
     * Clear buffer before writing a new packet
     */

    void clearPacketBuffer() {
      memset(packet_buffer, 0, MAX_PACKET_SIZE);
      packet_buffer_offset = 0;
    }

    /**
     * reset
     *
     * Reset the decoder's state so it's ready to decode a new file
     */
    void reset() {
      index = 0;
      total_packets = 0;
      file_buffer_offset = 0;
      file_size = 0;
      clearPacketBuffer();
    }

    /**
     * processPacketBuffer
     *
     * @param[in] {uint8_t*} `data` A pointer to the beginning of a byte sequence
     * @param[in] {uint32_t} `size` The number of bytes to process
     */

    void processPacketBuffer(uint8_t* data, uint32_t size) {
      uint32_t bytes_to_finish{}; // bytes to complete the current packet
      uint32_t remaining{}; // bytes left remaining after using passed data to complete current packet
      uint32_t bytes_to_copy{}; // number of bytes that will be copied into the packet buffer
      uint32_t packet_size{}; // size of packet currently being completed

      bool current_packet_received{}; // indicates if the current packet has been completely received
      bool is_last_packet = index == (total_packets); // if the current packet is the last packet of the file
      if (index == 0 && packet_buffer_offset == 0 && file_size > (MAX_PACKET_SIZE - HEADER_SIZE)) {
        // This is the first chunk of data for the first packet
        bytes_to_finish = (m_keep_header) ? MAX_PACKET_SIZE : MAX_PACKET_SIZE - HEADER_SIZE;
        packet_size     = (m_keep_header) ? 4096 : 4092;
      } else if (is_last_packet) {
        // We are currently iterating to complete the last packet
        packet_size = file_size - file_buffer_offset;
        bytes_to_finish = packet_size - packet_buffer_offset;
      } else {
        // All other chunks for all other packets
        packet_size = MAX_PACKET_SIZE;
        bytes_to_finish = MAX_PACKET_SIZE - packet_buffer_offset;
      }
      remaining = size - bytes_to_finish; // The size passed minus the bytes to complete current packet
      current_packet_received = (size >= bytes_to_finish); // Whether all data has been received to complete current packet

      bytes_to_copy = current_packet_received ? bytes_to_finish : size; // Number of bytes being copied into the packet buffer
      std::memcpy(packet_buffer + packet_buffer_offset, data, bytes_to_copy); // Copy into packet buffer
      packet_buffer_offset = packet_buffer_offset + bytes_to_copy; // Adjust the packet buffer offset, important if packet is incomplete
      if (packet_buffer_offset > MAX_PACKET_SIZE) {
        KLOG("Packet buffer overflow imminent");
      }

      if (current_packet_received) { // All data was received for this packet
        std::memcpy(file_buffer + file_buffer_offset, packet_buffer, packet_size); // Copy into file buffer
        clearPacketBuffer(); // clear packet buffer
        index++; // increment file buffer index
        file_buffer_offset = file_buffer_offset + packet_size; // Adjust the file buffer offset for new index
        if (remaining > 0) { // If there are still more bytes to copy, start new packet buffer for next packet
          std::memcpy(packet_buffer, data + bytes_to_copy, remaining);
          packet_buffer_offset = packet_buffer_offset + remaining; // Adjust packet buffer for the new index
        }
        if (is_last_packet) { // If last packet is complete
          m_file_cb(std::move(file_buffer), file_size, filename); // Invoke callback to notify client
          reset(); // Reset the decoder so it's ready to decode a new file
          KLOG("Cleaning up packet buffer");
        }
      }
    }
    /**
     * processPacket
     *
     * @param[in] {uint8_t*} `data` A pointer to the beginning of a byte sequence
     * @param[in] {uint32_t} `size` The number of bytes to process
     */
    void processPacket(uint8_t* data, uint32_t size) {
      bool is_first_packet = (index == 0);
      uint32_t process_index{0};
      while (size > 0) {

        uint32_t size_to_read = size <= MAX_PACKET_SIZE ? size : MAX_PACKET_SIZE;

        if (is_first_packet && packet_buffer_offset == 0 && file_buffer_offset == 0) {
          KLOG("processing first packet");
          // Compute file size from the first packet's 4 byte header
          file_size = (m_keep_header) ?
              int(data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]) + HEADER_SIZE + 1 :
              int(data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]) - HEADER_SIZE;
          // Compute total number of packets expected to decode this file
          total_packets = static_cast<uint32_t>(ceil(
              static_cast<double>(file_size / MAX_PACKET_SIZE)));
          // Reserve memory for file buffer
          file_buffer = new uint8_t[file_size];
          // Reserve memory for packet buffer
          if (packet_buffer == nullptr)
            packet_buffer = new uint8_t[MAX_PACKET_SIZE];

          file_buffer_offset = 0; // begin file buffer offset at 0
          uint8_t* start_buffer = (m_keep_header) ? data : data + HEADER_SIZE;
          uint32_t start_size   = (m_keep_header) ? size_to_read : size_to_read - HEADER_SIZE;
          processPacketBuffer(start_buffer, start_size); // process
        } else {
          processPacketBuffer((data + process_index), size_to_read); // process other chunks
        }
        size          -= size_to_read;
        process_index += size_to_read;
      }
    }

   private:
    uint8_t*                                                    file_buffer;
    uint8_t*                                                    packet_buffer;
    uint32_t                                                    index;
    uint32_t                                                    packet_buffer_offset;
    uint32_t                                                    total_packets;
    uint32_t                                                    file_buffer_offset;
    uint32_t                                                    file_size;
    std::string                                                 filename;
    int                                                         m_fd;
    std::function<void(int)>                                    m_cb;
    std::function<void(uint8_t *data, int size, std::string)>   m_file_cb;
    bool                                                        m_keep_header;
  };

  /**
   * @constructor
   */
  FileHandler(int client_fd,
              std::string name,
              uint8_t *first_packet,
              uint32_t size,
              std::function<void(int, int, uint8_t *, size_t)> callback_fn,
              bool keep_header = false
              )
    : socket_fd(client_fd) {
      KLOG("Creating new Decoder");
      m_decoder = new Decoder(
        client_fd, name,
        [this, client_fd, callback_fn](uint8_t*&& data, int size, std::string filename) {
          if (size > 0) {
            if (!filename.empty()) {
              FileUtils::saveFile(data, size, filename);
            } else {
              callback_fn(
                client_fd,
                FILE_HANDLE__SUCCESS,
                std::move(data), size
              );
            }
          }
        },
        keep_header
      );
    m_decoder->processPacket(first_packet, size);
  }

  /**
   * Move constructor
   * @constructor
   */
  FileHandler(FileHandler&& f)
      : m_decoder(f.m_decoder), socket_fd(f.socket_fd) {
    f.m_decoder = nullptr;
  }

  /**
   * Copy constructor
   * @constructor
   */

  FileHandler(const FileHandler& f)
      : m_decoder(new Decoder{*(f.m_decoder)}), socket_fd(f.socket_fd) {}

  FileHandler &operator=(const FileHandler& f) {
    if (&f != this) {
      delete m_decoder;
      m_decoder = nullptr;
      m_decoder = new Decoder{*(f.m_decoder)};
    }
    return *this;
  }

  /**
   * Assignment operator
   * @operator
   */
  FileHandler &operator=(FileHandler &&f) {
    if (&f != this) {
      delete m_decoder;
      m_decoder = f.m_decoder;
      f.m_decoder = nullptr;
    }
    return *this;
  }

  /**
   * @destructor
   */
  ~FileHandler() { delete m_decoder; }

  /**
   * processPacket
   *
   * @param[in] {uint8_t*} `data` A pointer to the first memory address of a
   * @param[in] {uint32_t} `size` The size of the packet, in bytes
   */
  void processPacket(uint8_t *data, uint32_t size) { m_decoder->processPacket(data, size); }
  bool isHandlingSocket(int fd) { return fd == socket_fd; }

 private:
  Decoder   *m_decoder;
  int       socket_fd;
};
} // namespace
#endif // __DECODER_HPP__
