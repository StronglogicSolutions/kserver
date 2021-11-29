namespace kiq {
class SerializerDeserializer
{
public:
virtual ~SerializerDeserializer() {}
virtual std::vector<std::string> deserialize(std::shared_ptr<uint8_t*> s_buf_ptr) = 0;
};

class KSerDes : public SerializerDeserializer
{
public:
virtual ~KSerDes() override {}
virtual std::vector<std::string> deserialize(std::shared_ptr<uint8_t*> s_buf_ptr) override
{
  return {};
}
};
} // ns
