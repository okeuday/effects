//-*-Mode:C++;coding:utf-8;tab-width:4;c-basic-offset:4;indent-tabs-mode:()-*-
// ex: set ft=cpp fenc=utf-8 sts=4 ts=4 sw=4 et nomod:
//
// MIT License
//
// Copyright (c) 2023 Michael Truog <mjtruog at protonmail dot com>
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//

#ifndef EFFECTS_HPP
#define EFFECTS_HPP

#include <utility>
#include <type_traits>
#include <cfenv>

namespace effects
{

enum struct context_type
{
    // Execution will always terminate in a finite amount of time,
    // no timeouts are set to infinity and no infinite loops are present
    terminating,
    // It is possible for execution to not terminate
    nonterminating
};

namespace kind
{
    // All possible effects in C++
    enum
    {
        pure                = 0x0000, // mathematical purity
        nonterminating      = 0x0001, // execution may not terminate
        exception           = 0x0002, // throw/(signal)/exit/abort
        reference           = 0x0004, // reference to global data not owned
        write               = 0x0008, // write to global (heap) data owned
        fpe                 = 0x0010, // Floating-Point Exceptions (FPE)
        variation_os        = 0x0020, // Operating System (OS) variation
        variation_hardware  = 0x0040, // hardware variation
        bitmask             = 0x00ff
    };
}

namespace kind_fpe
{
    // All possible cross-platform Floating-Point Exceptions (FPE)
    enum
    {
        none                = 0x0000,
        invalid             = 0x0100,
        divide_by_zero      = 0x0400,
        overflow            = 0x0800,
        underflow           = 0x1000,
        inexact             = 0x2000,
        bitmask             = 0xff00
    };
}

class context;

// container for kind::write effects
template <typename T> class region
{
    private:
        static_assert(! std::is_reference<T>::value,
                      "Do not use a reference type");

        friend class context;
        constexpr region(context & c, T && value) noexcept;
        constexpr region(context & c, T const & value) noexcept;

    public:
        constexpr region(region const & o) noexcept = default;

        [[nodiscard]] constexpr operator T const & () const & noexcept
        {
            return m_value;
        }
        [[nodiscard]] constexpr operator T && () && noexcept
        {
            return std::move(m_value);
        }
        constexpr region<T> & operator = (T && rhs) noexcept;
        constexpr region<T> & operator = (region<T> const & rhs) noexcept;

    private:
        context & m_context;
        T m_value;
};

// container for constant values
template <typename T> class region<T const &>
{
    private:
        friend class context;
        constexpr region(context & c, T const & value) noexcept;

    public:
        constexpr region(region const & o) noexcept = default;

        [[nodiscard]] constexpr operator T const & () const noexcept
        {
            return m_constant;
        }

    private:
        context & m_context;
        T const & m_constant;
};

// container for kind::reference effects
template <typename T> class region<T &>
{
    private:
        friend class context;
        constexpr region(context & c, T & value) noexcept;

    public:
        constexpr region(region const & o) noexcept = default;

        [[nodiscard]] constexpr operator T const & () const noexcept
        {
            return m_reference;
        }
        [[nodiscard]] constexpr operator T && () && noexcept
        {
            return std::move(m_reference);
        }
        constexpr region<T &> & operator = (T && rhs) noexcept;

    private:
        context & m_context;
        T & m_reference;
};

#if __cplusplus >= 202002L
#define CXX20
#endif
#if __cplusplus >= 201703L
#define CXX17
#endif
#if defined(CXX20)
template <typename T>
using identity = std::type_identity<T>;
#elif defined(CXX17)
template <typename T>
struct identity
{
    using type = T;
};
#else
#error "C++17 or higher is required"
#endif
template <typename T>
struct remove_all_pointers : std::conditional_t<
    std::is_pointer_v<T>,
    remove_all_pointers< std::remove_pointer_t<T> >,
    identity<T>
>
{};
template <typename T>
using remove_all_pointers_t = typename remove_all_pointers<T>::type;
template <typename T>
using is_floating_point = std::is_floating_point< remove_all_pointers_t<T> >;

class context
{
    public:
        constexpr context(unsigned int const kind_valid,
                          context_type const type) noexcept :
            m_kind_invalid((~kind_valid) & kind::bitmask),
            m_kind(kind::pure)
        {
            if (type == context_type::nonterminating)
            {
                m_kind |= kind::nonterminating;
            }
            std::feclearexcept(context::fpe_all);
        }
        constexpr context(context const & o) noexcept = delete;

        template <typename T>
        [[nodiscard]] constexpr region<T> operator ()(T && t) noexcept
        {
            return region<T>(*this, std::forward<T>(t));
        }

        constexpr void set_exception() noexcept
        {
            // An exception was thrown, an unignored signal was raised or
            // execution will terminate with a function call
            // (like exit or abort).
            m_kind |= kind::exception;
        }

        constexpr void set_variation_os() noexcept
        {
            // Execution will vary due to the Operating System (OS) used,
            // preventing the functionality from being mathematically pure.
            // A filepath function that uses '/' characters on UNIX and
            // '\\' on Windows for its return value is an example of
            // kind::variation_os .
            m_kind |= kind::variation_os;
        }

        constexpr void set_variation_hardware() noexcept
        {
            // Execution will vary due to the hardware used,
            // preventing the functionality from being mathematically pure.
            // Usage of a long type on both a 32-bit architecture and a
            // 64-bit architecture can cause a function return value
            // to have different possible return value limitations and
            // is an example of kind::variation_hardware .
            m_kind |= kind::variation_hardware;
        }

        void clear() noexcept
        {
            m_kind = kind::pure;
            std::feclearexcept(context::fpe_all);
        }

        [[nodiscard]] bool valid() noexcept
        {
            update();
            return (m_kind_invalid & m_kind) == 0;
        }

        [[nodiscard]] unsigned int kind() const noexcept
        {
            return m_kind;
        }

        [[nodiscard]] bool is_pure() noexcept
        {
            update();
            return m_kind == kind::pure;
        }
        [[nodiscard]] bool has_nonterminating() noexcept
        {
            update();
            return m_kind & kind::nonterminating;
        }
        [[nodiscard]] bool has_exception() noexcept
        {
            update();
            return m_kind & kind::exception;
        }
        [[nodiscard]] bool has_reference() noexcept
        {
            update();
            return m_kind & kind::reference;
        }
        [[nodiscard]] bool has_write() noexcept
        {
            update();
            return m_kind & kind::write;
        }
        [[nodiscard]] bool has_fpe() noexcept
        {
            update();
            return m_kind & kind::fpe;
        }
        [[nodiscard]] bool has_fpe(unsigned int & kind) noexcept
        {
            bool const result = has_fpe();
            kind = m_kind;
            return result;
        }
        [[nodiscard]] bool has_variation_os() noexcept
        {
            update();
            return m_kind & kind::variation_os;
        }
        [[nodiscard]] bool has_variation_hardware() noexcept
        {
            update();
            return m_kind & kind::variation_hardware;
        }

    private:
        template <typename> friend class region;

        template <typename T>
        void created_value(T const & value)
        {
            unsigned int kind = kind::pure;
            if (context::is_memory_owned(value))
            {
                kind |= kind::write;
            }
            if (is_floating_point<T>::value)
            {
                // floating-point use is a reference effect due to fesetround
                // (this code assumes a round occurred because a round
                //  often occurs when floating-point is used)
                kind |= kind::reference;
            }
            update(kind, is_floating_point<T>::value);
        }

        template <typename T>
        void created_constant(T const & constant)
        {
            unsigned int kind = kind::pure;
            if (context::is_memory_owned(constant))
            {
                kind |= kind::write;
            }
            update(kind, is_floating_point<T>::value);
        }

        template <typename T>
        void created_reference(T const & reference)
        {
            unsigned int kind = kind::reference;
            if (context::is_memory_owned(reference))
            {
                kind |= kind::write;
            }
            update(kind, is_floating_point<T>::value);
        }

        template <typename T>
        [[nodiscard]] static constexpr bool is_memory_owned(T const & value)
        {
            // if the value is a non-null pointer, assume it is owned memory
            // on the heap which implies a write effect
            // (a reference effect is usage of memory not owned)
            return std::is_pointer<T>::value && value != static_cast<T>(0);
        }

        void update(unsigned int kind, bool const floating_point) noexcept
        {
            if (floating_point && std::fetestexcept(context::fpe_all))
            {
#ifdef FE_INVALID
                if (std::fetestexcept(FE_INVALID))
                {
                    kind |= kind::fpe | kind_fpe::invalid;
                }
#endif
#ifdef FE_DIVBYZERO
                if (std::fetestexcept(FE_DIVBYZERO))
                {
                    kind |= kind::fpe | kind_fpe::divide_by_zero;
                }
#endif
#ifdef FE_OVERFLOW
                if (std::fetestexcept(FE_OVERFLOW))
                {
                    kind |= kind::fpe | kind_fpe::overflow;
                }
#endif
#ifdef FE_UNDERFLOW
                if (std::fetestexcept(FE_UNDERFLOW))
                {
                    kind |= kind::fpe | kind_fpe::underflow;
                }
#endif
#ifdef FE_INEXACT
                if (std::fetestexcept(FE_INEXACT))
                {
                    kind |= kind::fpe | kind_fpe::inexact;
                }
#endif
                std::feclearexcept(context::fpe_all);
            }
            m_kind |= kind;
        }

        void update() noexcept
        {
            update(kind::pure, true);
        }

        static constexpr unsigned int fpe_all =
#ifdef FE_INVALID
            FE_INVALID |
#endif
#ifdef FE_DIVBYZERO
            FE_DIVBYZERO |
#endif
#ifdef FE_OVERFLOW
            FE_OVERFLOW |
#endif
#ifdef FE_UNDERFLOW
            FE_UNDERFLOW |
#endif
#ifdef FE_INEXACT
            FE_INEXACT |
#endif
            0;

        unsigned int const m_kind_invalid;
        unsigned int m_kind;
};

template <typename T>
constexpr region<T>::region(context & c, T && value) noexcept :
    m_context(c),
    m_value(std::move(value))
{
    m_context.created_value(m_value);
}

template <typename T>
constexpr region<T>::region(context & c, T const & value) noexcept :
    m_context(c),
    m_value(value)
{
    m_context.created_value(m_value);
}

template <typename T>
constexpr region<T> & region<T>::operator = (T && rhs) noexcept
{
    m_value = rhs;
    m_context.created_value(m_value);
    return *this;
}

template <typename T>
constexpr region<T> & region<T>::operator = (region<T> const & rhs) noexcept
{
    m_value = rhs.m_value;
    m_context.created_value(m_value);
    return *this;
}

template <typename T>
constexpr region<T const &>::region(context & c, T const & value) noexcept :
    m_context(c),
    m_constant(value)
{
    m_context.created_constant(m_constant);
}

template <typename T>
constexpr region<T &>::region(context & c, T & value) noexcept :
    m_context(c),
    m_reference(value)
{
    m_context.created_reference(m_reference);
}

template <typename T>
constexpr region<T &> & region<T &>::operator = (T && rhs) noexcept
{
    m_reference = rhs;
    m_context.created_reference(m_reference);
    return *this;
}

} // namespace effects

#endif // EFFECTS_HPP
