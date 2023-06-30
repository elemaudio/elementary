#pragma once

#include <memory>


namespace elem
{

    // A simple object pool which uses ref counting for object availability.
    //
    // The goal here is primarily for situations where we need to allocate
    // objects of a certain type and then hand them over to the realtime thread
    // for realtime processing. With the RefCountedPool, the non-realtime thread
    // can grab a reference to a previously allocated object, manipulate it,
    // then pass it through a lock free queue to the realtime thread.
    //
    // While the realtime thread is using that object for processing, it will
    // retain a reference to it. The RefCountedPool uses std::shared_ptr's atomic
    // use count to identify objects which are in use, either on the realtime
    // thread or in the queue awaiting the real time thread.
    //
    // When the realtime is done with it, it simply drops its reference, which
    // is an atomic update (so long as it doesn't decrease the ref count to 0),
    // setting the use_count back to a value which indicates to RefCountedPool that
    // this object is now available for reallocation.
    //
    // See the SequenceNode for an example of this pattern.
    template <typename ElementType>
    class RefCountedPool
    {
    public:
        RefCountedPool(size_t capacity = 4)
        {
            // Fill the pool with default-constructed shared_ptr<T>s
            for (size_t i = 0; i < capacity; ++i)
            {
                internal.push_back(std::make_shared<ElementType>());
            }
        }

        std::shared_ptr<ElementType> allocate()
        {
            for (size_t i = 0; i < internal.size(); ++i)
            {
                // If we found a pointer that has only one reference, that ref
                // is our pool, so we konw that there are no other users of this
                // element and can hand it out.
                if (internal[i].use_count() == 1)
                {
                    return internal[i];
                }
            }

            // If we exceed the pool size, dynamically allocate another element
            auto next = std::make_shared<ElementType>();
            internal.push_back(next);

            return next;
        }

        std::shared_ptr<ElementType> allocateAvailableWithDefault(std::shared_ptr<ElementType>&& dv)
        {
            for (size_t i = 0; i < internal.size(); ++i)
            {
                // If we found a pointer that has only one reference, that ref
                // is our pool, so we konw that there are no other users of this
                // element and can hand it out.
                if (internal[i].use_count() == 1)
                {
                    return internal[i];
                }
            }

            // Else, we return the provided default value without dynamically allocating anything
            return std::move(dv);
        }

        void forEach (std::function<void(std::shared_ptr<ElementType>&)>&& fn)
        {
            for (size_t i = 0; i < internal.size(); ++i)
            {
                fn(internal[i]);
            }
        }

    private:
        std::vector<std::shared_ptr<ElementType>> internal;
    };

} // namespace elem
