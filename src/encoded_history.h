#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <algorithm>

// Audio-task-owned circular history. Absolute positions avoid wrap ambiguity.
// Control tasks submit commands to time_shift.cpp, never mutate this object.
class EncodedHistory {
 public:
  void begin(uint8_t *memory, size_t capacity) { data_ = memory; capacity_ = capacity; end_ = cursor_ = 0; overruns_ = 0; }
  bool active() const { return data_ && capacity_; }
  uint64_t oldest() const { return end_ > capacity_ ? end_ - capacity_ : 0; }
  uint64_t end() const { return end_; }
  uint64_t cursor() const { return cursor_; }
  size_t retained() const { return (size_t)(end_ - oldest()); }
  size_t available() const { return (size_t)(end_ - cursor_); }
  uint32_t overruns() const { return overruns_; }
  void append(const uint8_t *bytes, size_t size) {
    if (!active()) return;
    for (size_t offset = 0; offset < size;) {
      size_t run = std::min(size - offset, capacity_ - (size_t)(end_ % capacity_));
      memcpy(data_ + end_ % capacity_, bytes + offset, run); end_ += run; offset += run;
    }
    if (cursor_ < oldest()) { cursor_ = oldest(); ++overruns_; align(); }
  }
  size_t read(uint8_t *bytes, size_t size) {
    size = std::min(size, available()); size_t offset = 0;
    while (offset < size) {
      const size_t run = std::min(size - offset, capacity_ - (size_t)(cursor_ % capacity_));
      memcpy(bytes + offset, data_ + cursor_ % capacity_, run); cursor_ += run; offset += run;
    }
    return size;
  }
  bool seek(uint64_t position) {
    cursor_ = std::max(oldest(), std::min(position, end_));
    return align();
  }
 private:
  uint8_t at(uint64_t p) const { return data_[p % capacity_]; }
  size_t frameSize(uint64_t p) const {
    if (p + 4 > end_ || at(p) != 0xff || (at(p+1) & 0xe0) != 0xe0) return 0;
    const unsigned version = (at(p+1) >> 3) & 3, layer = (at(p+1) >> 1) & 3;
    const unsigned bitrate = at(p+2) >> 4, sample = (at(p+2) >> 2) & 3;
    if (version == 1 || layer != 1 || !bitrate || bitrate == 15 || sample == 3) return 0;
    static const unsigned mpeg1[] = {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320};
    static const unsigned mpeg2[] = {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160};
    static const unsigned rates[] = {44100,48000,32000};
    unsigned rate = rates[sample] / (version == 3 ? 1 : version == 2 ? 2 : 4);
    return (version == 3 ? 144000 : 72000) * (version == 3 ? mpeg1[bitrate] : mpeg2[bitrate]) / rate + ((at(p+2) >> 1) & 1);
  }
  bool align() {
    while (cursor_ + 4 <= end_) {
      size_t size = frameSize(cursor_);
      // Require two consecutive frame headers, not a false sync in payload.
      if (size && cursor_ + size + 4 <= end_ && frameSize(cursor_ + size)) return true;
      ++cursor_;
    }
    cursor_ = end_; return false;
  }
  uint8_t *data_ = nullptr;
  size_t capacity_ = 0;
  uint64_t end_ = 0, cursor_ = 0;
  uint32_t overruns_ = 0;
};
