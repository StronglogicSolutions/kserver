#pragma once

#include <log/logger.h>
#include <server/types.hpp>
#include <common/util.hpp>

namespace Decoder {
static const uint32_t MAX_PACKET_SIZE = 4096;
static const uint8_t  HEADER_SIZE     = 4;
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
struct File
{
  uint8_t*  byte_ptr;
  uint32_t  size;
  bool      complete;
};

/**
 * Decoder
 *
 * Does most of the heavy lifting
 */
using ReceiveFn      = std::function<void(uint8_t *data, int32_t size, const std::string&)>;
using FileCallbackFn = std::function<void(int, int, uint8_t *, size_t)>;
class Decoder
{
public:
  /**
  * @constructor
  */
  Decoder(int32_t            id,
          const std::string& file_name,
          ReceiveFn          file_callback_fn_ptr,
          bool               keep_header)
  : file_buffer         (nullptr),
    packet_buffer       (nullptr),
    index               (0),
    packet_buffer_offset(0),
    total_packets       (0),
    file_buffer_offset  (0),
    file_size           (0),
    filename            (file_name),
    m_id                (id),
    m_file_cb_ptr       (file_callback_fn_ptr),
    m_keep_header       (keep_header),
    m_header_size       (HEADER_SIZE)
    {}

/**
 * @destructor
 */
  ~Decoder()
  {
    if (file_buffer != nullptr)
    {
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

  void clearPacketBuffer()
  {
    memset(packet_buffer, 0, MAX_PACKET_SIZE);
    packet_buffer_offset = 0;
  }

  /**
   * reset
   *
   * Reset the decoder's state so it's ready to decode a new file
   */
  void reset()
  {
    index              = 0;
    total_packets      = 0;
    file_buffer_offset = 0;
    file_size          = 0;
    clearPacketBuffer();
  }

  /**
   * processPacketBuffer
   *
   * @param[in] {uint8_t*} `data` A pointer to the beginning of a byte sequence
   * @param[in] {uint32_t} `size` The number of bytes to process
   */

  void processPacketBuffer(uint8_t* data, uint32_t size)
  {
    uint32_t bytes_to_finish        {}; // for current packet
    uint32_t remaining              {}; // after this function completes
    uint32_t bytes_to_copy          {}; // into packet buffer
    uint32_t packet_size            {};
    bool     packet_received        {};
    bool     is_last_packet = index == (total_packets);

    if (!index && !packet_buffer_offset && file_size > (MAX_PACKET_SIZE - m_header_size)) // 1st chunk
      bytes_to_finish = packet_size = (m_keep_header) ?
        MAX_PACKET_SIZE : MAX_PACKET_SIZE - m_header_size;
    else
    if (is_last_packet)
    {
      packet_size     = file_size   - file_buffer_offset;
      bytes_to_finish = packet_size - packet_buffer_offset;
    }
    else
    { // All other chunks
      packet_size     = MAX_PACKET_SIZE;
      bytes_to_finish = MAX_PACKET_SIZE - packet_buffer_offset;
    }

    remaining               = (size - bytes_to_finish);
    packet_received         = (size >= bytes_to_finish);
    bytes_to_copy           = (packet_received) ? bytes_to_finish : size;
    std::memcpy(packet_buffer + packet_buffer_offset, data, bytes_to_copy);
    packet_buffer_offset    += bytes_to_copy;

    KLOG("Packet buffer offset is {}", packet_buffer_offset);

    assert((packet_buffer_offset <= MAX_PACKET_SIZE));

    if (packet_received)
    {
      std::memcpy((file_buffer + file_buffer_offset), packet_buffer, packet_size);
      file_buffer_offset = (file_buffer_offset + packet_size);
      clearPacketBuffer();
      index++;

      if (remaining)
      {
        std::memcpy(packet_buffer, (data + bytes_to_copy), remaining);
        packet_buffer_offset = packet_buffer_offset + remaining;
      }

      if (is_last_packet)
      {
        m_file_cb_ptr(std::move(file_buffer), file_size, filename);
        reset();
      }
    }
  }
  /**
   * processPacket
   *
   * @param[in] {uint8_t*} `data` A pointer to the beginning of a byte sequence
   * @param[in] {uint32_t} `size` The number of bytes to process
   */
  void processPacket(uint8_t* data, uint32_t size)
  {
    bool     is_first_packet = (index == 0);
    uint32_t process_index{0};

    while (size)
    {
      uint32_t size_to_read = size <= MAX_PACKET_SIZE ? size : MAX_PACKET_SIZE;

      if (is_first_packet && packet_buffer_offset == 0 && file_buffer_offset == 0)
      {
        file_size     = (m_keep_header) ?
          int(data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]) + HEADER_SIZE + 1 :
          int(data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]) - HEADER_SIZE;
        total_packets = static_cast<uint32_t>(ceil(static_cast<double>(file_size / MAX_PACKET_SIZE)));
        file_buffer   = new uint8_t[file_size];

        if (nullptr == packet_buffer)
          packet_buffer = new uint8_t[MAX_PACKET_SIZE];

        file_buffer_offset    = 0;
        if (m_keep_header)
          processPacketBuffer(data, size_to_read);
        else
          processPacketBuffer(data + HEADER_SIZE, size_to_read - HEADER_SIZE);
      }
      else
        processPacketBuffer((data + process_index), size_to_read);

      size          -= size_to_read;
      process_index += size_to_read;
    }
  }

   private:
    uint8_t*    file_buffer;
    uint8_t*    packet_buffer;
    uint32_t    index;
    uint32_t    packet_buffer_offset;
    uint32_t    total_packets;
    uint32_t    file_buffer_offset;
    uint32_t    file_size;
    std::string filename;
    int32_t     m_id;
    ReceiveFn   m_file_cb_ptr;
    bool        m_keep_header;
    uint8_t     m_header_size;
  };

  /**
   * @constructor
   */
  FileHandler(int32_t        id,
              std::string    name,
              uint8_t*       first_packet,
              uint32_t       size,
              FileCallbackFn callback_fn,
              bool           keep_header = false)
  : m_decoder(new Decoder(id, name,
    [this, id, callback_fn](uint8_t*&& data, int size, std::string filename)
    {
      if (size > 0) {
        if (!filename.empty())
          FileUtils::SaveFile(data, size, filename);
        else
          callback_fn(id, FILE_HANDLE__SUCCESS, std::move(data), size);
      }
    },
    keep_header))
  {
    m_decoder->processPacket(first_packet, size);
  }

  /**
   * Move constructor
   * @constructor
   */
  FileHandler(FileHandler&& f)
  : m_decoder(f.m_decoder)
  {
    f.m_decoder = nullptr;
  }

  /**
   * Copy constructor
   * @constructor
   */

  FileHandler(const FileHandler& f)
  : m_decoder(new Decoder{*(f.m_decoder)})
  {}

  FileHandler &operator=(const FileHandler& f)
  {
    if (&f != this)
    {
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
  FileHandler &operator=(FileHandler &&f)
  {
    if (&f != this)
    {
      delete m_decoder;
      m_decoder   = f.m_decoder;
      f.m_decoder = nullptr;
    }

    return *this;
  }

  /**
   * @destructor
   */
  ~FileHandler()
  {
    delete m_decoder;
  }

  /**
   * processPacket
   *
   * @param[in] {uint8_t*} `data` A pointer to the first memory address of a
   * @param[in] {uint32_t} `size` The size of the packet, in bytes
   */
  void processPacket(uint8_t *data, uint32_t size)
  {
    m_decoder->processPacket(data, size);
  }

 private:
  Decoder* m_decoder;
};
} // namespace Decoder
