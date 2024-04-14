#pragma once

namespace elem 
{
     // calculates the smallest power of two that is greater than or equal to integer `n`
    inline int bitceil (int n) {
        if ((n & (n - 1)) == 0)
            return n;

        int o = 1;

        while (o < n) {
            o = o << 1;
        }

        return o;
    }  
}