#pragma once
#include <algorithm>
#include <mutex>
#include <cstdio>
#include <cstring>
#include <strings.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
using std::min;
using std::max;
using portMUX_TYPE = std::mutex;
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(mux) (mux)->lock()
#define portEXIT_CRITICAL(mux) (mux)->unlock()
