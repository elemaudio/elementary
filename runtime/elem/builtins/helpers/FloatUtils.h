#pragma once


namespace elem
{

    template <typename FloatType>
    FloatType lerp (FloatType alpha, FloatType x, FloatType y) {
        return x + alpha * (y - x);
    }

    template <typename FloatType>
    FloatType fpEqual (FloatType x, FloatType y) {
        return std::abs(x - y) <= FloatType(1e-6);
    }

} // namespace elem
