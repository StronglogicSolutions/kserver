// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_GENERICTASK_GENERICDATA_H_
#define FLATBUFFERS_GENERATED_GENERICTASK_GENERICDATA_H_

#include "flatbuffers/flatbuffers.h"

namespace GenericData {

struct GenericTask;
struct GenericTaskBuilder;

struct GenericTask FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  typedef GenericTaskBuilder Builder;
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_ID = 4,
    VT_FILE_INFO = 6,
    VT_TIME = 8,
    VT_DESCRIPTION = 10,
    VT_IS_VIDEO = 12,
    VT_MASK = 14,
    VT_HEADER = 16,
    VT_USER = 18,
    VT_RECURRING = 20,
    VT_FREQUENCY = 22
  };
  int32_t id() const {
    return GetField<int32_t>(VT_ID, 0);
  }
  const flatbuffers::String *file_info() const {
    return GetPointer<const flatbuffers::String *>(VT_FILE_INFO);
  }
  const flatbuffers::String *time() const {
    return GetPointer<const flatbuffers::String *>(VT_TIME);
  }
  const flatbuffers::String *description() const {
    return GetPointer<const flatbuffers::String *>(VT_DESCRIPTION);
  }
  bool is_video() const {
    return GetField<uint8_t>(VT_IS_VIDEO, 0) != 0;
  }
  int32_t mask() const {
    return GetField<int32_t>(VT_MASK, 0);
  }
  const flatbuffers::String *header() const {
    return GetPointer<const flatbuffers::String *>(VT_HEADER);
  }
  const flatbuffers::String *user() const {
    return GetPointer<const flatbuffers::String *>(VT_USER);
  }
  bool recurring() const {
    return GetField<uint8_t>(VT_RECURRING, 0) != 0;
  }
  int32_t frequency() const {
    return GetField<int32_t>(VT_FREQUENCY, 0);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyField<int32_t>(verifier, VT_ID) &&
           VerifyOffset(verifier, VT_FILE_INFO) &&
           verifier.VerifyString(file_info()) &&
           VerifyOffset(verifier, VT_TIME) &&
           verifier.VerifyString(time()) &&
           VerifyOffset(verifier, VT_DESCRIPTION) &&
           verifier.VerifyString(description()) &&
           VerifyField<uint8_t>(verifier, VT_IS_VIDEO) &&
           VerifyField<int32_t>(verifier, VT_MASK) &&
           VerifyOffset(verifier, VT_HEADER) &&
           verifier.VerifyString(header()) &&
           VerifyOffset(verifier, VT_USER) &&
           verifier.VerifyString(user()) &&
           VerifyField<uint8_t>(verifier, VT_RECURRING) &&
           VerifyField<int32_t>(verifier, VT_FREQUENCY) &&
           verifier.EndTable();
  }
};

struct GenericTaskBuilder {
  typedef GenericTask Table;
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_id(int32_t id) {
    fbb_.AddElement<int32_t>(GenericTask::VT_ID, id, 0);
  }
  void add_file_info(flatbuffers::Offset<flatbuffers::String> file_info) {
    fbb_.AddOffset(GenericTask::VT_FILE_INFO, file_info);
  }
  void add_time(flatbuffers::Offset<flatbuffers::String> time) {
    fbb_.AddOffset(GenericTask::VT_TIME, time);
  }
  void add_description(flatbuffers::Offset<flatbuffers::String> description) {
    fbb_.AddOffset(GenericTask::VT_DESCRIPTION, description);
  }
  void add_is_video(bool is_video) {
    fbb_.AddElement<uint8_t>(GenericTask::VT_IS_VIDEO, static_cast<uint8_t>(is_video), 0);
  }
  void add_mask(int32_t mask) {
    fbb_.AddElement<int32_t>(GenericTask::VT_MASK, mask, 0);
  }
  void add_header(flatbuffers::Offset<flatbuffers::String> header) {
    fbb_.AddOffset(GenericTask::VT_HEADER, header);
  }
  void add_user(flatbuffers::Offset<flatbuffers::String> user) {
    fbb_.AddOffset(GenericTask::VT_USER, user);
  }
  void add_recurring(bool recurring) {
    fbb_.AddElement<uint8_t>(GenericTask::VT_RECURRING, static_cast<uint8_t>(recurring), 0);
  }
  void add_frequency(int32_t frequency) {
    fbb_.AddElement<int32_t>(GenericTask::VT_FREQUENCY, frequency, 0);
  }
  explicit GenericTaskBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  GenericTaskBuilder &operator=(const GenericTaskBuilder &);
  flatbuffers::Offset<GenericTask> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<GenericTask>(end);
    return o;
  }
};

inline flatbuffers::Offset<GenericTask> CreateGenericTask(
    flatbuffers::FlatBufferBuilder &_fbb,
    int32_t id = 0,
    flatbuffers::Offset<flatbuffers::String> file_info = 0,
    flatbuffers::Offset<flatbuffers::String> time = 0,
    flatbuffers::Offset<flatbuffers::String> description = 0,
    bool is_video = false,
    int32_t mask = 0,
    flatbuffers::Offset<flatbuffers::String> header = 0,
    flatbuffers::Offset<flatbuffers::String> user = 0,
    bool recurring = false,
    int32_t frequency = 0) {
  GenericTaskBuilder builder_(_fbb);
  builder_.add_frequency(frequency);
  builder_.add_user(user);
  builder_.add_header(header);
  builder_.add_mask(mask);
  builder_.add_description(description);
  builder_.add_time(time);
  builder_.add_file_info(file_info);
  builder_.add_id(id);
  builder_.add_recurring(recurring);
  builder_.add_is_video(is_video);
  return builder_.Finish();
}

inline flatbuffers::Offset<GenericTask> CreateGenericTaskDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    int32_t id = 0,
    const char *file_info = nullptr,
    const char *time = nullptr,
    const char *description = nullptr,
    bool is_video = false,
    int32_t mask = 0,
    const char *header = nullptr,
    const char *user = nullptr,
    bool recurring = false,
    int32_t frequency = 0) {
  auto file_info__ = file_info ? _fbb.CreateString(file_info) : 0;
  auto time__ = time ? _fbb.CreateString(time) : 0;
  auto description__ = description ? _fbb.CreateString(description) : 0;
  auto header__ = header ? _fbb.CreateString(header) : 0;
  auto user__ = user ? _fbb.CreateString(user) : 0;
  return GenericData::CreateGenericTask(
      _fbb,
      id,
      file_info__,
      time__,
      description__,
      is_video,
      mask,
      header__,
      user__,
      recurring,
      frequency);
}

inline const GenericData::GenericTask *GetGenericTask(const void *buf) {
  return flatbuffers::GetRoot<GenericData::GenericTask>(buf);
}

inline const GenericData::GenericTask *GetSizePrefixedGenericTask(const void *buf) {
  return flatbuffers::GetSizePrefixedRoot<GenericData::GenericTask>(buf);
}

inline bool VerifyGenericTaskBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<GenericData::GenericTask>(nullptr);
}

inline bool VerifySizePrefixedGenericTaskBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifySizePrefixedBuffer<GenericData::GenericTask>(nullptr);
}

inline void FinishGenericTaskBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<GenericData::GenericTask> root) {
  fbb.Finish(root);
}

inline void FinishSizePrefixedGenericTaskBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<GenericData::GenericTask> root) {
  fbb.FinishSizePrefixed(root);
}

}  // namespace GenericData

#endif  // FLATBUFFERS_GENERATED_GENERICTASK_GENERICDATA_H_
