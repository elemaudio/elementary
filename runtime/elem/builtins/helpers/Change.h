#pragma once


namespace elem
{

    // A port of Max/MSP gen~'s change object
    //
    // Will report a value of 1 when the change from the most recent sample
    // to the current sample is increasing, a -1 whendecreasing, and a 0
    // when it holds the same.
    template <typename FloatType>
    struct Change {
        FloatType lastIn = 0;

        FloatType operator()(FloatType xn) {
            return tick(xn);
        }

        FloatType tick(FloatType xn) {
            FloatType dt = xn - lastIn;
            lastIn = xn;

            if (dt > FloatType(0))
                return FloatType(1);

            if (dt < FloatType(0))
                return FloatType(-1);

            return FloatType(0);
        }
    };

} // namespace elem
