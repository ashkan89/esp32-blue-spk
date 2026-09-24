#pragma once
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

// A bounded, allocation-free HTTP body reader. Client::read() may return -1
// between TLS records; that is not EOF while the connection is still alive.
// Clock supplies now() and idle() so delayed records can be tested on a host.
template <class Client, class Clock> class UpdateBody {
 public:
  UpdateBody(Client &client, Clock &clock, int length, bool chunked,
             size_t limit, uint32_t idleMs, uint32_t totalMs)
      : client_(client), clock_(clock), remaining_(length), chunked_(chunked),
        limit_(limit), idleMs_(idleMs), totalMs_(totalMs),
        started_(clock.now()), lastData_(started_) {}

  int read() {
    if (error_ || done_) return -1;
    if (chunked_ && chunkLeft_ == 0) {
      if (chunkEnd_ && (wire() != '\r' || wire() != '\n'))
        return fail("Invalid HTTP chunk ending");
      chunkEnd_ = false;
      char line[128];
      if (!readLine(line, sizeof(line))) return -1;
      size_t size = 0, digits = 0;
      for (const char *p = line; *p && *p != ';'; ++p) {
        const int digit = *p >= '0' && *p <= '9' ? *p - '0'
            : *p >= 'a' && *p <= 'f' ? *p - 'a' + 10
            : *p >= 'A' && *p <= 'F' ? *p - 'A' + 10 : -1;
        if (digit < 0 || size > (SIZE_MAX - digit) / 16)
          return fail("Invalid HTTP chunk size");
        size = size * 16 + digit;
        ++digits;
      }
      if (!digits) return fail("Missing HTTP chunk size");
      if (size > limit_ - received_) return fail("HTTP body exceeds size limit");
      if (!size) {
        // Consume bounded trailers before declaring the transfer complete.
        size_t trailers = 0;
        do {
          if (!readLine(line, sizeof(line))) return -1;
          if (++trailers > 32) return fail("Too many HTTP trailers");
        } while (line[0]);
        done_ = true;
        return -1;
      }
      chunkLeft_ = size;
    }
    if (!chunked_ && remaining_ == 0) { done_ = true; return -1; }
    const int value = wire();
    if (value < 0) {
      if (!error_ && !chunked_ && remaining_ < 0) done_ = true;
      else if (!error_) fail("HTTP body ended early");
      return -1;
    }
    if (received_ == limit_) return fail("HTTP body exceeds size limit");
    ++received_;
    if (chunked_) { if (--chunkLeft_ == 0) chunkEnd_ = true; }
    else if (remaining_ > 0) --remaining_;
    return value;
  }

  size_t readBytes(char *out, size_t count) {
    size_t n = 0;
    for (; n < count; ++n) {
      const int value = read();
      if (value < 0) break;
      out[n] = (char)value;
    }
    return n;
  }
  const char *error() const { return error_; }
  bool complete() const { return done_ && !error_; }
  size_t received() const { return received_; }

 private:
  int fail(const char *message) { if (!error_) error_ = message; return -1; }
  int wire() {
    for (;;) {
      if ((uint32_t)(clock_.now() - started_) >= totalMs_)
        return fail("HTTP transfer deadline exceeded");
      if (pos_ < used_) return buffer_[pos_++];
      const int available = client_.available();
      if (available > 0) {
        const size_t want = (size_t)available < sizeof(buffer_)
                                ? (size_t)available : sizeof(buffer_);
        const int n = client_.read(buffer_, want);
        if (n > 0) {
          pos_ = 0; used_ = (size_t)n; lastData_ = clock_.now();
          continue;
        }
      }
      if (!client_.connected()) return -1;
      if ((uint32_t)(clock_.now() - lastData_) >= idleMs_)
        return fail("HTTP transfer stalled");
      clock_.idle();
    }
  }
  bool readLine(char *line, size_t capacity) {
    for (size_t n = 0; n < capacity; ++n) {
      const int c = wire();
      if (c < 0) { fail("HTTP framing ended early"); return false; }
      if (c == '\r') {
        if (wire() != '\n') { fail("Invalid HTTP line ending"); return false; }
        line[n] = 0;
        return true;
      }
      if (c == '\n' || c == 0) { fail("Invalid HTTP header line"); return false; }
      line[n] = (char)c;
    }
    fail("HTTP header line too long");
    return false;
  }
  Client &client_;
  Clock &clock_;
  int remaining_;
  bool chunked_, chunkEnd_ = false, done_ = false;
  size_t limit_, received_ = 0, chunkLeft_ = 0;
  uint32_t idleMs_, totalMs_, started_, lastData_;
  const char *error_ = nullptr;
  uint8_t buffer_[512];
  size_t pos_ = 0, used_ = 0;
};
