#pragma once


namespace elem
{

    // Returns the smallest power of two that is greater than or equal to
    // the given integer `n`
    inline int bitceil (int n) {
        if ((n & (n - 1)) == 0)
            return n;

        int o = 1;

        while (o < n) {
            o = o << 1;
        }

        return o;
    }

} // namespace elem
