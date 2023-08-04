#include "effects.hpp"
#include <limits>
#include <iostream>
#include <cmath>
#include <cassert>

namespace
{
    int i = 1;
    int const j = 2;
}

using namespace effects;

void test_integers()
{
    context c(kind::reference, context_type::terminating);
    // any reference is assumed to be a reference to global data
    region<int &> i_reference = c(i);
    assert(c.kind() == 0x0004);
    assert(c.has_reference());
    assert(c.kind() == kind::reference);
    assert(i == i_reference);
    i_reference = i_reference + 1;
    assert(i == i_reference);
    assert(i == 2);
    assert(c.valid());
    c.clear();
    region<int const &> j_constant = c(j);
    assert(i_reference + j_constant == 4);
    region<int> value = c(3);
    assert(value + j_constant == 5);
    assert(c.is_pure());
    assert(c.valid());
    c.clear();
    // 1 / 0 will raise SIGFPE with the result undefined
}

void test_fpe()
{
    context c(kind::reference | kind::fpe, context_type::terminating);
    region<double> value_rounded = c(2.0 / 3);
#if defined(__clang__)
    assert(c.kind() == 0x2014);
    assert(c.kind() ==
           (kind_fpe::inexact | kind::fpe | kind::reference));
#elif defined(__GNUC__)
    // g++ does not provide the inexact floating-point exception in this
    // situation, probably because it is seen as a performance problem
    // when the inexact exception is used for any round that occurs,
    // though there doesn't appear to be a way to enable it either with
    // a cross-platform gcc command-line flag.
    // https://gcc.gnu.org/c99status.html
    // says "required exceptions may not be generated"
    assert(c.kind() == 0x0004);
    assert(c.kind() == kind::reference);
#else
#error "unknown compiler!"
#endif
    assert(c.has_reference());
    assert(c.valid());
    c.clear();
    region<double> value_invalid = c(0.0 / 0.0);
    assert(c.kind() == 0x0114);
    assert(c.kind() ==
           (kind_fpe::invalid | kind::fpe | kind::reference));
    assert(! c.is_pure());
    assert(c.kind() == 0x0114);
    assert(c.valid());
    c.clear();
    region<double> value_divide_by_zero = c(1.0 / 0.0);
    assert(c.kind() == 0x0414);
    assert(c.kind() ==
           (kind_fpe::divide_by_zero | kind::fpe | kind::reference));
    assert(! c.is_pure());
    assert(c.kind() == 0x0414);
    assert(c.valid());
    c.clear();
    region<double> value_overflow =
        c(std::numeric_limits<double>::max() * 2.0);
    assert(c.kind() == 0x2814);
    assert(c.kind() ==
           (kind_fpe::overflow | kind_fpe::inexact |
            kind::fpe | kind::reference));
    assert(! c.is_pure());
    assert(c.kind() == 0x2814);
    c.clear();
    region<double> value_underflow =
        c(std::numeric_limits<double>::min() / 3.0);
    assert(c.kind() == 0x3014);
    assert(c.kind() ==
           (kind_fpe::underflow | kind_fpe::inexact |
            kind::fpe | kind::reference));
    assert(! c.is_pure());
    assert(c.kind() == 0x3014);
    assert(c.valid());
    c.clear();
    region<double> value_inexact = c(std::sqrt(2));
    assert(c.kind() == 0x2014);
    assert(! c.is_pure());
    assert(c.kind() ==
           (kind_fpe::inexact | kind::fpe | kind::reference));
    assert(c.valid());
    unsigned int fpe_inexact = 0;
    assert(c.has_fpe(fpe_inexact));
    assert(fpe_inexact & kind_fpe::inexact);
    assert((fpe_inexact & (~kind_fpe::inexact & kind_fpe::bitmask)) == 0);
    c.clear();
}

void test_pointers()
{
#if defined(__clang__)
    context c(kind::fpe |
              kind::reference | kind::write, context_type::terminating);
#elif defined(__GNUC__)
    context c(kind::reference | kind::write, context_type::terminating);
#else
#error "unknown compiler!"
#endif
    region<double *> p1_value = c(new double(2.0 / 3));
#if defined(__clang__)
    assert(c.kind() == 0x201c);
    assert(c.kind() ==
           (kind_fpe::inexact | kind::fpe | kind::reference | kind::write));
#elif defined(__GNUC__)
    assert(c.kind() == 0x000c);
    assert(c.kind() == (kind::reference | kind::write));
#else
#error "unknown compiler!"
#endif
    assert(c.valid());
    delete p1_value;
    p1_value = 0;
    c.clear();
    region<int *> p2_value = c(new int(1));
    assert(c.kind() == 0x0008);
    assert(c.kind() == kind::write);
    assert(c.valid());
    delete p2_value;
    p2_value = 0;
    c.clear();
    // string literals look like they are allocated on the heap
    // because there isn't a portable way of checking whether a pointer
    // is not on the stack or is in a read-only data (or text) section,
    // so this is invalid and expected
    region<char const *> p3_value =
        c(static_cast<char const *>("invalid write effect"));
    assert(c.kind() == 0x0008);
    assert(c.kind() == kind::write);
    assert(c.valid());
    c.clear();
    region<int *> p4_value = c(static_cast<int *>(0));
    assert(c.is_pure());
    assert(c.valid());
}

int main(int, char **)
{
    test_integers();
    test_fpe();
    test_pointers();

    std::cout << "ALL TESTS PASSED" << std::endl;
    return 0;
}
