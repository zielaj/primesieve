///
/// @file   CpuInfo.cpp
/// @brief  Get the CPUs cache sizes in bytes.
///
/// Copyright (C) 2017 Kim Walisch, <kim.walisch@gmail.com>
///
/// This file is distributed under the BSD License. See the COPYING
/// file in the top level directory.
///

#include <primesieve/CpuInfo.hpp>
#include <cstddef>

#if defined(__APPLE__)
  #if !defined(__has_include)
    #define APPLE_SYSCTL
  #elif __has_include(<sys/types.h>) && \
        __has_include(<sys/sysctl.h>)
    #define APPLE_SYSCTL
  #endif
#endif

#if defined(_WIN32)

#include <windows.h>
#include <vector>

#elif defined(APPLE_SYSCTL)

#include <sys/types.h>
#include <sys/sysctl.h>

#else // all other OSes

#include <exception>
#include <fstream>
#include <sstream>
#include <string>

using namespace std;

namespace {

string getString(const string& filename)
{
  ifstream file(filename);
  string str;

  if (file)
  {
    ostringstream oss;
    oss << file.rdbuf();
    str = oss.str();
  }

  return str;
}

size_t getValue(const string& filename)
{
  try
  {
    // first character after number
    size_t idx = 0;
    string str = getString(filename);
    size_t val = stol(str, &idx);

    if (idx < str.size())
    {
      // Last character may be:
      // 'K' = kilobytes
      // 'M' = megabytes
      // 'G' = gigabytes
      if (str[idx] == 'K')
        val *= 1024;
      if (str[idx] == 'M')
        val *= 1024 * 1024;
      if (str[idx] == 'G')
        val *= 1024 * 1024 * 1024;
    }

    return val;
  }
  catch (exception&)
  {
    return 0;
  }
}

} // namespace

#endif

using namespace std;

namespace primesieve {

CpuInfo::CpuInfo()
  : l1CacheSize_(0),
    l2CacheSize_(0),
    l3CacheSize_(0)
{
  initCache();
}

size_t CpuInfo::l1CacheSize() const
{
  return l1CacheSize_;
}

size_t CpuInfo::l2CacheSize() const
{
  return l2CacheSize_;
}

size_t CpuInfo::l3CacheSize() const
{
  return l3CacheSize_;
}

bool CpuInfo::hasL1Cache() const
{
  return l1CacheSize_ >= (1 << 12) &&
         l1CacheSize_ <= (1 << 30);
}

bool CpuInfo::hasL2Cache() const
{
  return l2CacheSize_ >= (1 << 12) &&
         l2CacheSize_ <= (1 << 30);
}

bool CpuInfo::hasL3Cache() const
{
  return l3CacheSize_ >= (1 << 16) &&
         l3CacheSize_ <= (1ull << 40);
}

bool CpuInfo::isPrivateL2Cache() const
{
  return hasL2Cache() && (!hasL1Cache() || hasL3Cache());
}

#if defined(APPLE_SYSCTL)

void CpuInfo::initCache()
{
  size_t l1Length = sizeof(l1CacheSize_);
  size_t l2Length = sizeof(l2CacheSize_);
  size_t l3Length = sizeof(l3CacheSize_);

  sysctlbyname("hw.l1dcachesize", &l1CacheSize_, &l1Length, NULL, 0);
  sysctlbyname("hw.l2cachesize" , &l2CacheSize_, &l2Length, NULL, 0);
  sysctlbyname("hw.l3cachesize" , &l3CacheSize_, &l3Length, NULL, 0);
}

#elif defined(_WIN32)

void CpuInfo::initCache()
{
  typedef BOOL (WINAPI *LPFN_GLPI)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);

  LPFN_GLPI glpi = (LPFN_GLPI) GetProcAddress(GetModuleHandle(TEXT("kernel32")), "GetLogicalProcessorInformation");

  if (glpi)
  {
    DWORD bytes = 0;
    glpi(0, &bytes);
    size_t size = bytes / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> info(size);
    glpi(info.data(), &bytes);

    for (size_t i = 0; i < size; i++)
    {
      if (info[i].Relationship == RelationCache)
      {
        if (info[i].Cache.Level == 1)
          l1CacheSize_ = info[i].Cache.Size;
        if (info[i].Cache.Level == 2)
          l2CacheSize_ = info[i].Cache.Size;
        if (info[i].Cache.Level == 3)
          l3CacheSize_ = info[i].Cache.Size;
      }
    }
  }
}

#else

/// This works on Linux, we also use this for all
/// unknown OSes, it might work.
///
void CpuInfo::initCache()
{
  for (int i = 0; i <= 4; i++)
  {
    string filename = "/sys/devices/system/cpu/cpu0/cache/index" + to_string(i);
    string cacheLevel = filename + "/level";
    string cacheSize = filename + "/size";

    switch (getValue(cacheLevel))
    {
      case 1: l1CacheSize_ = getValue(cacheSize); break;
      case 2: l2CacheSize_ = getValue(cacheSize); break;
      case 3: l3CacheSize_ = getValue(cacheSize); break;
      default:;
    }
  }
}

#endif

/// Singleton
const CpuInfo cpuInfo;

} // namespace
