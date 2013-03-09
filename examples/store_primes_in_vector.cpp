////////////////////////////////////////////////////////////////////
// store_primes_in_vector.cpp
// Store the first 1000 primes in a vector.

#include <primesieve/soe/PrimeSieve.h>
#include <iostream>
#include <exception>
#include <vector>

class stop_primesieve : public std::exception { };

std::vector<int> primes;

void callback(unsigned int prime)
{
  if (primes.size() == 1000) throw stop_primesieve();
  primes.push_back(prime);
}

int main()
{
  PrimeSieve ps;
  try {
    ps.generatePrimes(0, 999999999, callback);
  }
  catch (stop_primesieve&) { }
  std::cout << primes.size() << " primes stored!" << std::endl;
  return 0;
}
