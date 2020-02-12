#ifndef __DECODER_HPP__
#define __DECODER_HPP__

#include <log/logger.h>
#include <server/types.hpp>
#include <codec/util.hpp>

namespace Decoder {

KLogger *k_logger_ptr = KLogger::GetInstance();

auto KLOG = k_logger_ptr->get_logger();

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
  class File {
   public:
    uint8_t *b_ptr;
    uint32_t size;
    bool complete;
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
            std::function<void(uint8_t *, int, std::string)> file_callback)
        : index(0),
          file_buffer(nullptr),
          packet_buffer(nullptr),
          total_packets(0),
          packet_buffer_offset(0),
          file_buffer_offset(0),
          file_size(0),
          filename(name),
          m_fd(fd),
          m_file_cb(file_callback) {
            KLOG->info("FileHandler::Decoder::Decoder() - instantiated");
          }
  /**
   * @destructor
   */
    ~Decoder() {
      KLOG->info("FileHandler::Decoder::~Decoder() - destructor called");
      if (file_buffer != nullptr) {
        KLOG->info("FileHandler::Decoder::~Decoder() - Deleting file buffer and packet buffer");
        delete[] file_buffer;
        delete[] packet_buffer;
        file_buffer = nullptr;
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
     * Reset the decoder's state so that it made be ready to decode a new file
     */
    void reset() {
      index = 0;
      total_packets = 0;
      file_buffer_offset = 0;
      file_size = 0;
    }

    /**
     * processPacketBuffer
     *
     * @param[in] {uint8_t*} data
     * @param[in] {uint32_t} size
     * @param[in] {bool} last_packet
     */
    void processPacketBuffer(uint8_t* data, uint32_t size, bool last_packet = false) {
      KLOG->info("processPacketBuffer index {} of {}", index, total_packets);
      if (packet_buffer_offset == 0 && size == MAX_PACKET_SIZE) { // Complete, non-initial packet
          std::memcpy(file_buffer + file_buffer_offset, data, // no packet buffer needed
                    MAX_PACKET_SIZE);
          index++;
          KLOG->info("Incrementing packet index");
          return;
      }
      if (index == 0) { // First packet, but incomplete (complete single-packet files handled in `processPacket()`)
        KLOG->info("Incomplete first packet");
        std::memcpy(packet_buffer, data, size);
        packet_buffer_offset = packet_buffer_offset + size;
        return;
      }
      // Other packets
      uint32_t bytes_to_full_packet = MAX_PACKET_SIZE - packet_buffer_offset;
      if (!last_packet) {
        if (size >= bytes_to_full_packet) {
          uint32_t packet_offset = size - bytes_to_full_packet; // Bytes to read after finishing this packet
          std::memcpy(packet_buffer + packet_buffer_offset, data, bytes_to_full_packet); // complete current packet
          if (index == 0) {
            KLOG->info("Completed first packet");
          } else {
            KLOG->info("Completed packet {}", index);
          }
          std::memcpy(file_buffer + file_buffer_offset, packet_buffer, MAX_PACKET_SIZE); // copy complete packet into file buffer
          clearPacketBuffer(); // packet buffer ready to read next packet
          index++; // increment packet index
          KLOG->info("Incrementing packet index");
          if (size > bytes_to_full_packet) { // Start the next packet
            std::memcpy(packet_buffer, data + bytes_to_full_packet, packet_offset); // Copy remaining
            packet_buffer_offset = packet_buffer_offset + packet_offset;
          }
          return;
        }
        std::memcpy(packet_buffer + packet_buffer_offset, data, size); // Continue filling packet
        packet_buffer_offset = packet_buffer_offset + size;
      } else { // Last packet
        KLOG->info("Decoder::processPacketBuffer() - processing last packet");
        std::memcpy(packet_buffer + packet_buffer_offset, data, size);
        uint32_t last_packet_size = file_size - file_buffer_offset;
        uint32_t bytes_still_expected = last_packet_size - packet_buffer_offset;
        if (bytes_still_expected > size) { // Expecting more data to complete last packet
          packet_buffer_offset = packet_buffer_offset + size;
        } else { // We've received all of the data for this file
          std::memcpy(file_buffer + file_buffer_offset, packet_buffer, last_packet_size);
          m_files.push_back(
              File{.b_ptr = file_buffer, .size = file_size, .complete = true}); // push to received files
          m_file_cb(file_buffer, file_size, filename); // Invoke callback to notify client
          KLOG->info("Decoder::processPacketBuffer() - cleaning up");
          clearPacketBuffer();
          reset(); // Reset the decoder, it's now ready to decode a new packet stream
        }
      }
    }

    /**
     * processPacket
     *
     * @param[in] {uint8_t*} data
     * @param[in] {uint32_t} size
     */
    void processPacket(uint8_t *data, uint32_t size) {
      KLOG->info("processPacket() - index {} of {}", index, total_packets);
      bool is_first_packet = (index == 0);
      if (is_first_packet) {
        if (packet_buffer_offset > 0) { // We are still finishing the first packet
          processPacketBuffer(data, size);
          return;
        }
        KLOG->info("Decoder::processPacket() - processing first packet");
        file_size =
            int(data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]) -
            HEADER_SIZE;
        file_buffer = new uint8_t[file_size];
        if (file_size < size) { // The file is contained within this packet
          KLOG->info("File data contained within single packet");
          file_buffer = new uint8_t[file_size];
          std::memcpy(file_buffer, data + HEADER_SIZE, file_size);
          m_file_cb(file_buffer, file_size, filename);
          return;
        }
        // Received first packet and expecting subsequent packets
        total_packets = static_cast<uint32_t>(ceil(
            static_cast<double>(file_size / MAX_PACKET_SIZE)));
        packet_buffer = new uint8_t[MAX_PACKET_SIZE];
        file_buffer_offset = 0;
        processPacketBuffer(data + HEADER_SIZE, size - HEADER_SIZE);
      } else { // Subsequent packets
        KLOG->info("Subsequent packet");
        // offset safely assumes packets are MAX_PACKET_SIZE, because only the
        // last packet could be different
        file_buffer_offset = (index * MAX_PACKET_SIZE) - HEADER_SIZE; // offset for header from first packet
        bool is_last_packet = (index == (total_packets - 1));
        processPacketBuffer(data, size, is_last_packet);
      }
    }

   private:
    uint8_t *file_buffer;
    uint8_t *packet_buffer;
    uint32_t index;
    uint32_t packet_buffer_offset;
    uint32_t total_packets;
    uint32_t file_buffer_offset;
    uint32_t file_size;
    std::string filename;
    int m_fd;
    std::vector<File> m_files;
    std::function<void(int)> m_cb;
    std::function<void(uint8_t *data, int size, std::string)> m_file_cb;
  };

  /**
   * @constructor
   */
  FileHandler(int client_fd, std::string name, uint8_t *first_packet, uint32_t size,
              std::function<void(int, int, uint8_t *, size_t)> callback)
      : socket_fd(client_fd) {
        KLOG->info("FileHandler() - Instantiated. Creating new Decoder");
    m_decoder =
        new Decoder(client_fd, name,
                    [this, client_fd, callback](uint8_t *data, int size,
                                                std::string filename) {
                      if (size > 0) {
                        if (!filename.empty()) {
                          // Read to save TODO: Check to see if pointer is not
                          // null and size > 0?
                          FileUtils::saveFile(data, size, filename);
                        } else {
                          callback(client_fd, FILE_HANDLE__SUCCESS, data, size);
                        }
                      }
                    });
    m_decoder->processPacket(first_packet, size);
  }

  /**
   * Move constructor
   * @constructor
   */
  FileHandler(FileHandler &&f)
      : m_decoder(f.m_decoder), socket_fd(f.socket_fd) {
    f.m_decoder = nullptr;
  }

  /**
   * Copy constructor
   * @constructor
   */

  FileHandler(const FileHandler &f)
      : m_decoder(new Decoder{*(f.m_decoder)}), socket_fd(f.socket_fd) {}

  FileHandler &operator=(const FileHandler &f) {
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
   * @param[in] {uint8_t*} data
   * @param[in] {uint32_t} size
   */
  void processPacket(uint8_t *data, uint32_t size) { m_decoder->processPacket(data, size); }
  bool isHandlingSocket(int fd) { return fd == socket_fd; }

 private:
  Decoder *m_decoder;
  int socket_fd;
};
} // namespace
#endif // __DECODER_HPP__
