#pragma once
#include <limits.h>
#include <stdlib.h>
#include <string.h>

// Commands are complete values: never interpret an empty/truncated value as 0.
inline bool mqtt_integer(const char *text, int low, int high, int *out) {
  if (!text || !*text) return false;
  const char *p = text;
  if (*p == '-') ++p;
  if (!*p) return false;
  for (; *p; ++p) if (*p < '0' || *p > '9') return false;
  char *end;
  const long value = strtol(text, &end, 10);
  if (*end || value < low || value > high) return false;
  *out = (int)value;
  return true;
}

inline bool mqtt_switch(const char *text, bool *out) {
  if (strcmp(text, "ON") == 0) { *out = true; return true; }
  if (strcmp(text, "OFF") == 0) { *out = false; return true; }
  return false;
}

inline const char *mqtt_command_suffix(const char *topic, const char *root) {
  const size_t n = strlen(root);
  if (strncmp(topic, root, n) || topic[n] != '/' || !topic[n + 1]) return nullptr;
  const char *suffix = topic + n + 1;
  return strchr(suffix, '/') ? nullptr : suffix;
}
