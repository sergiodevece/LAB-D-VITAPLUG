// Cuenta reservas de memoria del montón para comprobar que process() no reserva.
#include "engine/AllocationGuard.h"

#include <cstdlib>
#include <new>

namespace labdtest {
std::atomic<long> allocationCount { 0 };
}

void* operator new (std::size_t size)
{
    labdtest::allocationCount.fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::malloc (size == 0 ? 1 : size))
        return p;
    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)
{
    labdtest::allocationCount.fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::malloc (size == 0 ? 1 : size))
        return p;
    throw std::bad_alloc();
}

void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }
