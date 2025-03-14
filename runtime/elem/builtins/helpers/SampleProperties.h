#pragma once

#import "../../Types.h"
#import "../../Value.h"

namespace elem
{
    namespace Sample
    {
        static constexpr size_t kLoopOffsetNull = -1;
        static constexpr const char* kLoopStartOffset = "loopStartOffset";
        static constexpr const char* kLoopStopOffset = "loopStopOffset";

        inline int updateLoopProperty(js::Value const& val, std::atomic<size_t>& property)
        {
            if (val.isNumber())
            {
                auto const v = (js::Number)val;
                auto const vi = static_cast<int>(v);

                if (vi < 0)
                    return ReturnCode::InvalidPropertyValue();

                property.store(static_cast<size_t>(vi));
                return ReturnCode::Ok();
            }

            if (val.isUndefined() || val.isNull())
            {
                property.store(kLoopOffsetNull);
                return ReturnCode::Ok();
            }

            return ReturnCode::InvalidPropertyValue();
        }
    }
}
