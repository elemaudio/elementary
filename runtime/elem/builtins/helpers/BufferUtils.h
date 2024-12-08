#pragma once


namespace elem
{

    namespace util
    {
        template <typename FromType, typename ToType>
        void copy_cast_n(FromType const* input, size_t numSamples, ToType* output)
        {
            for (size_t i = 0; i < numSamples; ++i) {
                output[i] = static_cast<ToType>(input[i]);
            }
        }
    }

}
