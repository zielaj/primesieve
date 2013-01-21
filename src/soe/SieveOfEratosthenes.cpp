///
/// @file   SieveOfEratosthenes.cpp
/// @brief  Implementation of the segmented sieve of Eratosthenes.
///
/// Copyright (C) 2013 Kim Walisch, <kim.walisch@gmail.com>
///
/// This file is distributed under the New BSD License. See the
/// LICENSE file in the top level directory.
///

#include "config.h"
#include "SieveOfEratosthenes.h"
#include "PreSieve.h"
#include "EratSmall.h"
#include "EratMedium.h"
#include "EratBig.h"
#include "imath.h"
#include "primesieve_error.h"

#include <stdint.h>
#include <exception>
#include <string>
#include <cstdlib>

namespace soe {

const uint_t SieveOfEratosthenes::bitValues_[8] = { 7, 11, 13, 17, 19, 23, 29, 31 };

/// De Bruijn bitscan table
const uint_t SieveOfEratosthenes::bruijnBitValues_[64] =
{
    7,  47,  11,  49,  67, 113,  13,  53,
   89,  71, 161, 101, 119, 187,  17, 233,
   59,  79,  91,  73, 133, 139, 163, 103,
  149, 121, 203, 169, 191, 217,  19, 239,
   43,  61, 109,  83, 157,  97, 181, 229,
   77, 131, 137, 143, 199, 167, 211,  41,
  107, 151, 179, 227, 127, 197, 209,  37,
  173, 223, 193,  31, 221,  29,  23, 241
};

/// @param start      Sieve primes >= start.
/// @param stop       Sieve primes <= stop.
/// @param sieveSize  A sieve size in kilobytes.
/// @param preSieve   Pre-sieve multiples of small primes <= preSieve
///                   to speed up the sieve of Eratosthenes.
/// @pre   start      >= 7
/// @pre   stop       <= 2^64 - 2^32 * 10
/// @pre   sieveSize  >= 1 && <= 4096
/// @pre   preSieve   >= 13 && <= 23
///
SieveOfEratosthenes::SieveOfEratosthenes(uint64_t start,
                                         uint64_t stop,
                                         uint_t sieveSize,
                                         uint_t preSieve) :
  start_(start),
  stop_(stop),
  limitPreSieve_(preSieve),
  sieve_(NULL),
  preSieve_(NULL),
  eratSmall_(NULL),
  eratMedium_(NULL),
  eratBig_(NULL)
{
  if (start_ < 7)
    throw primesieve_error("SieveOfEratosthenes: start must be >= 7");
  if (start_ > stop_)
    throw primesieve_error("SieveOfEratosthenes: start must be <= stop");
  sqrtStop_ = static_cast<uint_t>(isqrt(stop_));
  // sieveSize_ must be a power of 2
  sieveSize_ = getInBetween(1u, floorPowerOf2(sieveSize), 4096u);
  sieveSize_ *= 1024; // convert to bytes
  segmentLow_ = start_ - getByteRemainder(start_);
  segmentHigh_ = segmentLow_ + sieveSize_ * NUMBERS_PER_BYTE + 1;
  // allocate the sieve of Eratosthenes array
  sieve_ = new byte_t[sieveSize_];
  init();
}

SieveOfEratosthenes::~SieveOfEratosthenes()
{
  cleanUp();
}

void SieveOfEratosthenes::cleanUp()
{
  delete[] sieve_;
  delete preSieve_;
  delete eratSmall_;
  delete eratMedium_;
  delete eratBig_;
}

void SieveOfEratosthenes::init()
{
  limitEratSmall_  = static_cast<uint_t>(sieveSize_ * config::FACTOR_ERATSMALL);
  limitEratMedium_ = static_cast<uint_t>(sieveSize_ * config::FACTOR_ERATMEDIUM);
  try {
    preSieve_ = new PreSieve(limitPreSieve_);

    if (sqrtStop_ > limitPreSieve_)   eratSmall_  = new EratSmall (stop_, sieveSize_, limitEratSmall_);
    if (sqrtStop_ > limitEratSmall_)  eratMedium_ = new EratMedium(stop_, sieveSize_, limitEratMedium_);
    if (sqrtStop_ > limitEratMedium_) eratBig_    = new EratBig   (stop_, sieveSize_, sqrtStop_);
  }
  catch (const std::exception& e) {
    cleanUp();
    throw e;
  }
}

uint_t SieveOfEratosthenes::getSqrtStop() const
{
  return sqrtStop_;
}

uint_t SieveOfEratosthenes::getPreSieve() const
{
  return limitPreSieve_;
}

std::string SieveOfEratosthenes::getMaxStopString()
{
  return EratBig::getMaxStopString();
}

uint64_t SieveOfEratosthenes::getMaxStop()
{
  return EratBig::getMaxStop();
}

uint64_t SieveOfEratosthenes::getByteRemainder(uint64_t n)
{
  uint64_t r = n % NUMBERS_PER_BYTE;
  if (r <= 1)
    r += NUMBERS_PER_BYTE;
  return r;
}

void SieveOfEratosthenes::sieveSegment()
{
  preSieve();
  crossOffMultiples();
  segmentProcessed(sieve_, sieveSize_);
}

void SieveOfEratosthenes::crossOffMultiples()
{
  if (eratSmall_)   eratSmall_->crossOff(sieve_, &sieve_[sieveSize_]);
  if (eratMedium_) eratMedium_->crossOff(sieve_, sieveSize_);
  if (eratBig_)       eratBig_->crossOff(sieve_);
}

/// Pre-sieve multiples of small primes e.g. <= 19
/// to speed up the sieve of Eratosthenes.
///
void SieveOfEratosthenes::preSieve()
{
  preSieve_->doIt(sieve_, sieveSize_, segmentLow_);

  // unset bits (numbers) < start_
  if (segmentLow_ <= start_) {
    if (start_ <= limitPreSieve_)
      sieve_[0] = 0xff;
    int i = 0;
    uint64_t remainder = getByteRemainder(start_);
    while (bitValues_[i] < remainder) i++;
    sieve_[0] &= 0xff << i;
  }
}

/// Sieve the last segments remaining after that sieve(uint_t)
/// has been called for all primes up to sqrt(stop).
///
void SieveOfEratosthenes::finish()
{
  // sieve all segments left except the last one
  while (segmentHigh_ < stop_) {
    sieveSegment();
    segmentLow_  += sieveSize_ * NUMBERS_PER_BYTE;
    segmentHigh_ += sieveSize_ * NUMBERS_PER_BYTE;
  }
  // sieve the last segment
  uint64_t remainder = getByteRemainder(stop_);
  sieveSize_ = static_cast<uint_t>((stop_ - remainder) - segmentLow_) / NUMBERS_PER_BYTE + 1;
  segmentHigh_ = segmentLow_ + sieveSize_ * NUMBERS_PER_BYTE + 1;
  preSieve();
  crossOffMultiples();
  int i;
  // unset bits (numbers) > stop_
  for (i = 0; i < 8; i++)
    if (bitValues_[i] > remainder)
      break;
  sieve_[sieveSize_ - 1] &= ~(0xff << i);
  for (uint_t j = sieveSize_; j % 8 != 0; j++)
    sieve_[j] = 0;
  segmentProcessed(sieve_, sieveSize_);
}

} // namespace soe
