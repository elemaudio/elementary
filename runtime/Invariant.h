#pragma once

#include <stdexcept>


namespace elem
{

    struct InvariantViolation : public std::runtime_error
    {
        InvariantViolation (std::string const& error)
            : std::runtime_error (error) {}
    };

    inline static void invariant(bool condition, std::string const& msg)
    {
        if (!condition)
        {
            throw InvariantViolation(msg);
        }
    }

} // namespace elem
