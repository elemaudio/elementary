#pragma once

#include <memory>
#include <set>
#include <unordered_map>

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

#include <cmath>

#include <atomic>

#include <memory>
#include <sstream>
#include <unordered_map>

namespace elem
{

    //==============================================================================
    // Our graph node identifier type
    using NodeId = int32_t;

    // A simple helper for pretty printing NodeId types
    inline std::string nodeIdToHex (NodeId i) {
        std::stringstream ss;
        ss << std::hex << i;

        auto s = ss.str();

        if (s.size() <= 8) {
            s.insert(0, 8 - s.size(), '0');
        }

        return "0x" + s;
    }

    //==============================================================================
    // A static helper for enumerating and describing return codes used throughout
    // the codebase
    struct ReturnCode {
        static int Ok()                         { return 0; }
        static int UnknownNodeType()            { return 1; }
        static int NodeNotFound()               { return 2; }
        static int NodeAlreadyExists()          { return 3; }
        static int NodeTypeAlreadyExists()      { return 4; }
        static int InvalidPropertyType()        { return 5; }
        static int InvalidPropertyValue()       { return 6; }
        static int InvariantViolation()         { return 7; }
        static int InvalidInstructionFormat()   { return 8; }

        static std::string describe (int c) {
            switch (c) {
                case 0:
                    return "Ok";
                case 1:
                    return "Node type not recognized";
                case 2:
                    return "Node not found";
                case 3:
                    return "Attempting to create a node that already exists";
                case 4:
                    return "Attempting to create a node type that already exists";
                case 5:
                    return "Invalid value type for the given node property";
                case 6:
                    return "Invalid value for the given node property";
                case 7:
                    return "Invariant violation";
                case 8:
                    return "Invalid instruction format";
                default:
                    return "Return code not recognized";
            }
        }
    };

    //==============================================================================
    // A simple struct representing the inputs to a given GraphNode during the realtime
    // audio block processing step.
    template <typename FloatType>
    struct BlockContext
    {
        FloatType const** inputData;
        size_t numInputChannels;
        FloatType* outputData;
        size_t numSamples;
        void* userData;
    };

    //==============================================================================
    // A couple of quick helpers to provide an API similar to std::views::keys of C++20.
    //
    // This allows the Runtime to expose the keys of the shared resource map to the end
    // user without exposing the values.
    template<class MapType>
    class KeyIterator : public MapType::iterator
    {
    public:
        typedef typename MapType::iterator map_iterator;
        typedef typename map_iterator::value_type::first_type key_type;

        KeyIterator(const map_iterator& other) : MapType::iterator(other) {} ;

        key_type& operator *()
        {
            return MapType::iterator::operator*().first;
        }
    };

    template<class MapType>
    struct MapKeyView {
        public:
            MapKeyView(MapType& m)
                : other(m) {}

            auto begin()    { return KeyIterator<MapType>(other.begin()); }
            auto end()      { return KeyIterator<MapType>(other.end()); }

        private:
            MapType& other;
    };

    //==============================================================================
    template <typename FloatType>
    using SharedResourceBuffer = std::shared_ptr<std::vector<FloatType> const>;

    template <typename FloatType>
    using MutableSharedResourceBuffer = std::shared_ptr<std::vector<FloatType>>;

    template <typename FloatType>
    class SharedResourceMap {
    public:
        //==============================================================================
        using KeyViewType = MapKeyView<std::unordered_map<std::string, SharedResourceBuffer<FloatType>>>;

        //==============================================================================
        SharedResourceMap() = default;

        //==============================================================================
        // Accessor methods for immutable resources
        bool insert(std::string const& p, SharedResourceBuffer<FloatType>&& srb);
        bool has(std::string const& p);
        SharedResourceBuffer<FloatType> const& get(std::string const& p);

        void prune();
        KeyViewType keys();

        //==============================================================================
        // Accessor methods for mutable resources
        MutableSharedResourceBuffer<FloatType> const& getOrCreateMutable(std::string const& p, size_t blockSize);

    private:
        std::unordered_map<std::string, SharedResourceBuffer<FloatType>> imms;
        std::unordered_map<std::string, MutableSharedResourceBuffer<FloatType>> muts;
    };

    //==============================================================================
    // Details...
    template <typename FloatType>
    bool SharedResourceMap<FloatType>::insert (std::string const& p, SharedResourceBuffer<FloatType>&& srb) {
        // We only allow insertions here, not updates. This preserves immutability of existing
        // entries which we need in case any active graph nodes hold references to those entries.
        return imms.emplace(p, std::move(srb)).second;
    }

    template <typename FloatType>
    bool SharedResourceMap<FloatType>::has (std::string const& p) {
        return imms.count(p) > 0;
    }

    template <typename FloatType>
    SharedResourceBuffer<FloatType> const& SharedResourceMap<FloatType>::get (std::string const& p) {
        return imms.at(p);
    }

    template <typename FloatType>
    void SharedResourceMap<FloatType>::prune() {
        for (auto it = imms.cbegin(); it != imms.cend(); /* no increment */) {
            if (it->second.use_count() == 1) {
                imms.erase(it++);
            } else {
                it++;
            }
        }
    }

    template <typename FloatType>
    typename SharedResourceMap<FloatType>::KeyViewType SharedResourceMap<FloatType>::keys() {
        return MapKeyView<decltype(imms)>(imms);
    }

    template <typename FloatType>
    MutableSharedResourceBuffer<FloatType> const& SharedResourceMap<FloatType>::getOrCreateMutable (std::string const& p, size_t blockSize) {
        if (muts.count(p) > 0) {
            return muts.at(p);
        }

        muts.emplace(p, std::make_shared<std::vector<FloatType>>(blockSize));
        std::fill_n(muts.at(p)->data(), blockSize, FloatType(0));
        return muts.at(p);
    }

} // namespace elem

#include <map>
#include <sstream>
#include <variant>
#include <vector>
#include <functional>

namespace elem
{
namespace js
{

    //==============================================================================
    // Representations of primitive JavaScript values
    struct Undefined {};
    struct Null {};

    using Boolean = bool;
    using Number = double;
    using String = std::string;

    //==============================================================================
    // Forward declare the Value to allow recursive type definitions
    class Value;

    //==============================================================================
    // Representations of JavaScript Objects
    using Object = std::map<String, Value>;
    using Array = std::vector<Value>;
    using Float32Array = std::vector<float>;
    using Function = std::function<Value(Array)>;

    //==============================================================================
    // The Value class is a thin wrapper around a std::variant for dynamically representing
    // values present in the underlying JavaScript runtime.
    class Value {
    public:
        //==============================================================================
        // Default constructor creates an undefined value
        Value()
           : var(Undefined()) {}

        // Destructor
        ~Value() noexcept = default;

        Value (Undefined v)             : var(v) {}
        Value (Null v)                  : var(v) {}
        Value (Boolean v)               : var(v) {}
        Value (Number v)                : var(v) {}
        Value (char const* v)           : var(String(v)) {}
        Value (String const& v)         : var(v) {}
        Value (Array const& v)          : var(v) {}
        Value (Float32Array const& v)   : var(v) {}
        Value (Object const& v)         : var(v) {}
        Value (Function const& v)       : var(v) {}

        Value (Value const& valueToCopy) : var(valueToCopy.var) {}
        Value (Value && valueToMove) noexcept : var(std::move(valueToMove.var)) {}

        //==============================================================================
        // Assignment
        Value& operator= (Value const& valueToCopy)
        {
            var = valueToCopy.var;
            return *this;
        }

        Value& operator= (Value && valueToMove) noexcept
        {
            var = std::move(valueToMove.var);
            return *this;
        }

        //==============================================================================
        // Type checks
        bool isUndefined()      const { return std::holds_alternative<Undefined>(var); }
        bool isNull()           const { return std::holds_alternative<Null>(var); }
        bool isBool()           const { return std::holds_alternative<Boolean>(var); }
        bool isNumber()         const { return std::holds_alternative<Number>(var); }
        bool isString()         const { return std::holds_alternative<String>(var); }
        bool isArray()          const { return std::holds_alternative<Array>(var); }
        bool isFloat32Array()   const { return std::holds_alternative<Float32Array>(var); }
        bool isObject()         const { return std::holds_alternative<Object>(var); }
        bool isFunction()       const { return std::holds_alternative<Function>(var); }

        //==============================================================================
        // Primitive value casts
        operator Boolean()  const { return std::get<Boolean>(var); }
        operator Number()   const { return std::get<Number>(var); }
        operator String()   const { return std::get<String>(var); }
        operator Array()    const { return std::get<Array>(var); }

        // Object value getters
        Array const& getArray()                 const { return std::get<Array>(var); }
        Float32Array const& getFloat32Array()   const { return std::get<Float32Array>(var); }
        Object const& getObject()               const { return std::get<Object>(var); }
        Function const& getFunction()           const { return std::get<Function>(var); }

        Array& getArray()                   { return std::get<Array>(var); }
        Float32Array& getFloat32Array()     { return std::get<Float32Array>(var); }
        Object& getObject()                 { return std::get<Object>(var); }
        Function& getFunction()             { return std::get<Function>(var); }

        //==============================================================================
        // Object property access with a default return value
        template <typename T>
        T getWithDefault(std::string const& k, T const& v) const
        {
            auto o = getObject();

            if (o.count(k) > 0) {
                return T(o.at(k));
            }

            return v;
        }

        // String representation
        String toString() const
        {
            if (isUndefined()) { return "undefined"; }
            if (isNull()) { return "null"; }
            if (isBool()) { return String(std::to_string(std::get<Boolean>(var))); }
            if (isNumber()) { return String(std::to_string(std::get<Number>(var))); }
            if (isString()) { return std::get<String>(var); }
            if (isArray())
            {
                auto& a = getArray();
                std::stringstream ss;
                ss << "[";

                for (size_t i = 0; i < std::min((size_t) 3, a.size()); ++i)
                    ss << a[i].toString() << ", ";

                if (a.size() > 3)
                {
                    ss << "...]";
                    return ss.str();
                }

                auto s = ss.str();
                return s.substr(0, s.size() - 2) + "]";
            }
            if (isFloat32Array())
            {
                auto& fa = getFloat32Array();
                std::stringstream ss;
                ss << "[";

                for (size_t i = 0; i < std::min((size_t) 3, fa.size()); ++i)
                    ss << std::to_string(fa[i]) << ", ";

                if (fa.size() > 3)
                {
                    ss << "...]";
                    return ss.str();
                }

                auto s = ss.str();
                return s.substr(0, s.size() - 2) + "]";
            }
            if (isObject())
            {
                std::stringstream ss;
                ss << "{\n";

                for (auto const& [k, v] : getObject()) {
                    ss << "    " << k << ": " << v.toString() << "\n";
                }

                ss << "}\n";
                return ss.str();
            }

            // TODO
            if (isFunction()) { return "[Object Function]"; }

            return "undefined";
        }

    private:
        //==============================================================================
        // Internally we represent the Value's real value with a variant
        using VarType = std::variant<
            Undefined,
            Null,
            Boolean,
            Number,
            String,
            Object,
            Array,
            Float32Array,
            Function>;

        VarType var;
    };

    // We need moves to avoid allocations on the realtime thread if moving from
    // a lock free queue.
    static_assert(std::is_move_assignable<Value>::value);

    static inline std::ostream& operator<< (std::ostream& s, Value const& v)
    {
        s << v.toString();
        return s;
    }

} // namespace js
} // namespace elem

#include <unordered_map>
#include <functional>

namespace elem
{

    //==============================================================================
    // The GraphNode represents a single audio processing operation within the
    // larger audio graph.
    //
    // Users may implement custom operations by extending this class and registering
    // the new class with the GraphHost.
    template <typename FloatType>
    struct GraphNode {
        //==============================================================================
        // GraphNode constructor.
        //
        // The default implementation will hang onto the incoming NodeId and sample rate
        // in member variables. Users may extend this constructor to take the incoming
        // blockSize into account as well.
        GraphNode(NodeId id, double sr, size_t blockSize);

        //==============================================================================
        // GraphNode destructor.
        virtual ~GraphNode() = default;

        //==============================================================================
        // Returns the NodeId associated with this node
        NodeId getId() { return nodeId; }

        //==============================================================================
        double getSampleRate() { return sampleRate; }
        size_t getBlockSize() { return blockSize; }

        //==============================================================================
        // Sets a property onto the graph node.
        //
        // By default this does nothing other than store the property into `props`,
        // but users may override this method to make changes to the way the node processes
        // audio according to incoming property values.
        //
        // Thread safety must be managed by the user. This method will be called on
        // a non-realtime thread.
        virtual int setProperty(std::string const& key, js::Value const& val);
        virtual int setProperty(std::string const& key, js::Value const& val, SharedResourceMap<FloatType>& resources);

        // Retreives a property from the Node's props, falling back to the provided
        // default value if no property exists by the given name.
        //
        // Properties are held in a map<std::string, js::Value>, but the ValueType template
        // allows fetching a property by a given subtype such as js::Number. If an entry is
        // found in the props map, its value will be returned by casting to ValueType, therefore
        // if the cast fails, this method will throw.
        template <typename ValueType>
        ValueType getPropertyWithDefault(std::string const& key, ValueType const& df);

        // Returns a copy of the entire property object in its current state
        js::Object getProperties();

        // Process the next block of audio data.
        //
        // Users must override this method when creating a custom GraphNode.
        //
        // This method will be called from the realtime thread. Thread safety
        // for any custom operations must be managed by the user.
        virtual void process (BlockContext<FloatType> const&)  = 0;

        // Derived classes may override this method to relay events.
        //
        // This method will be called during GraphHost's `processQueuedEvents` on
        // the non-realtime thread. Any event to be relayed should be sent through the
        // event handler callback provided.
        //
        // Thread safety must be managed by the user.
        virtual void processEvents(std::function<void(std::string const&, js::Value)>& /* eventHandler */) {}

        // Derived classes may override this method to reset themselves.
        //
        // This method will be called by the end user through Runtime/GraphHost
        // on the non-realtime thread. When receiving the call.
        virtual void reset() {}

    private:
        //==============================================================================
        NodeId nodeId;
        std::unordered_map<std::string, js::Value> props;

        double sampleRate;
        size_t blockSize;
    };

    //==============================================================================
    // Details...
    template <typename FloatType>
    GraphNode<FloatType>::GraphNode(NodeId id, double sr, size_t bs)
        : nodeId(id)
        , sampleRate(sr)
        , blockSize(bs)
    {
    }

    template <typename FloatType>
    int GraphNode<FloatType>::setProperty(std::string const& key, js::Value const& val) {
        props.insert_or_assign(key, val);
        return ReturnCode::Ok();
    }

    template <typename FloatType>
    int GraphNode<FloatType>::setProperty(std::string const& key, js::Value const& val, SharedResourceMap<FloatType>&) {
        return setProperty(key, val);
    }

    template <typename FloatType>
    template <typename ValueType>
    ValueType GraphNode<FloatType>::getPropertyWithDefault(std::string const& key, ValueType const& df) {
        if (props.count(key) > 0) {
            return static_cast<ValueType>(props.at(key));
        }

        return df;
    }

    template <typename FloatType>
    js::Object GraphNode<FloatType>::getProperties() {
        return js::Object(props.begin(), props.end());
    }

} // namespace elem

#include <optional>

#ifndef ELEM_BLOCK
  #define ELEM_BLOCK(x) do { x } while (false)
#endif

#ifndef ELEM_ASSERT
  #include <cassert>
  #define ELEM_ASSERT(x) ELEM_BLOCK(assert(x);)
#endif

#ifndef ELEM_ASSERT_FALSE
  #define ELEM_ASSERT_FALSE ELEM_ASSERT(false)
#endif

#ifndef ELEM_RETURN_IF
  #define ELEM_RETURN_IF(cond, val) ELEM_BLOCK( if (cond) { return (val); } )
#endif

namespace elem
{

    //==============================================================================
    // This is a simple lock-free single producer, single consumer queue.
    template <typename T>
    class SingleWriterSingleReaderQueue
    {
    public:
        //==============================================================================
        SingleWriterSingleReaderQueue(size_t capacity = 32)
            : maxElements(capacity), indexMask(capacity - 1)
        {
            // We need the queue length to be non-zero and a power of two so
            // that our bit masking trick will work. Enforce that here with
            // an assert.
            ELEM_ASSERT(capacity > 0 && ((capacity & indexMask) == 0));
            queue.resize(capacity);
        }

        ~SingleWriterSingleReaderQueue() = default;

        //==============================================================================
        bool push (T && el)
        {
            auto const w = writePos.load();
            auto const r = readPos.load();

            if (numFreeSlots(r, w) > 0)
            {
                queue[w] = std::move(el);
                auto const desiredWritePosition = (w + 1u) & indexMask;
                writePos.store(desiredWritePosition);
                return true;
            }

            return false;
        }

        bool push (std::vector<T>& els)
        {
            auto const w = writePos.load();
            auto const r = readPos.load();

            if (els.size() >= numFreeSlots(r, w))
                return false;

            for (size_t i = 0; i < els.size(); ++i)
                queue[(w + i) & indexMask] = std::move(els[i]);

            auto const desiredWritePosition = (w + els.size()) & indexMask;
            writePos.store(desiredWritePosition);
            return true;
        }

        bool pop (T& el)
        {
            auto const r = readPos.load();
            auto const w = writePos.load();

            if (numFullSlots(r, w) > 0)
            {
                el = std::move(queue[r]);
                auto const desiredReadPosition = (r + 1u) & indexMask;
                readPos.store(desiredReadPosition);
                return true;
            }

            return false;
        }

        size_t size()
        {
            auto const r = readPos.load();
            auto const w = writePos.load();

            return numFullSlots(r, w);
        }

    private:
        size_t numFullSlots(size_t const r, size_t const w)
        {
            // If the writer is ahead of the reader, then the full slots are
            // the ones ahead of the reader and behind the writer.
            if (w > r)
                return w - r;

            // Else, the writer has already wrapped around, so the free space is
            // what's ahead of the writer and behind the reader, and the full space
            // is what's left.
            return (maxElements - (r - w)) & indexMask;
        }

        size_t numFreeSlots(size_t const r, size_t const w)
        {
            // If the reader is ahead of the writer, then the writer must have
            // wrapped around already, so the only space available is whats ahead
            // of the writer, behind the reader.
            if (r > w)
                return r - w;

            // Else, the only full slots are ahead of the reader and behind the
            // writer, so the free slots are what's left.
            return maxElements - (w - r);
        }

        std::atomic<size_t> maxElements = 0;
        std::atomic<size_t> readPos = 0;
        std::atomic<size_t> writePos = 0;
        std::vector<T> queue;

        size_t indexMask = 0;
    };

} // namespace elem

namespace elem
{

    //==============================================================================
    // This is a simple lock-free single producer, single consumer queue for
    // buffered multichannel sample data.
    template <typename T>
    class MultiChannelRingBuffer
    {
    public:
        //==============================================================================
        MultiChannelRingBuffer(size_t numChannels, size_t capacity = 8192)
            : maxElements(capacity), indexMask(capacity - 1)
        {
            // We need the queue length to be non-zero and a power of two so
            // that our bit masking trick will work. Enforce that here with
            // an assert.
            ELEM_ASSERT(capacity > 0 && ((capacity & indexMask) == 0));
            ELEM_ASSERT(numChannels > 0);

            for (size_t i = 0; i < numChannels; ++i) {
                buffers.push_back(std::vector<T>(capacity));
            }
        }

        ~MultiChannelRingBuffer() = default;

        //==============================================================================
        void write (T const** data, size_t numChannels, size_t numSamples)
        {
            auto const w = writePos.load();
            auto const r = readPos.load();

            // If we're asked to write more than we have room for, we clobber and
            // nudge the read pointer up
            bool const shouldMoveReadPointer = (numSamples >= numFreeSlots(r, w));
            auto const desiredWritePosition = (w + numSamples) & indexMask;
            auto const desiredReadPosition = shouldMoveReadPointer ? ((desiredWritePosition + 1) & indexMask) : r;

            for (size_t i = 0; i < std::min(buffers.size(), numChannels); ++i) {
                if (w + numSamples >= maxElements) {
                    auto const s1 = maxElements - w;
                    std::copy_n(data[i], s1, buffers[i].data() + w);
                    std::copy_n(data[i] + s1, numSamples - s1, buffers[i].data());
                } else {
                    std::copy_n(data[i], numSamples, buffers[i].data() + w);
                }
            }

            writePos.store(desiredWritePosition);
            readPos.store(desiredReadPosition);
        }

        bool read (T** destination, size_t numChannels, size_t numSamples)
        {
            auto const r = readPos.load();
            auto const w = writePos.load();

            if (numFullSlots(r, w) >= numSamples)
            {
                for (size_t i = 0; i < std::min(buffers.size(), numChannels); ++i) {
                    if (r + numSamples >= maxElements) {
                        auto const s1 = maxElements - r;
                        std::copy_n(buffers[i].data() + r, s1, destination[i]);
                        std::copy_n(buffers[i].data(), numSamples - s1, destination[i] + s1);
                    } else {
                        std::copy_n(buffers[i].data() + r, numSamples, destination[i]);
                    }
                }

                auto const desiredReadPosition = (r + numSamples) & indexMask;
                readPos.store(desiredReadPosition);

                return true;
            }

            return false;
        }

        size_t size()
        {
            auto const r = readPos.load();
            auto const w = writePos.load();

            return numFullSlots(r, w);
        }

    private:
        size_t numFullSlots(size_t const r, size_t const w)
        {
            // If the writer is ahead of the reader, then the full slots are
            // the ones ahead of the reader and behind the writer.
            if (w > r)
                return w - r;

            // Else, the writer has already wrapped around, so the free space is
            // what's ahead of the writer and behind the reader, and the full space
            // is what's left.
            return (maxElements - (r - w)) & indexMask;
        }

        size_t numFreeSlots(size_t const r, size_t const w)
        {
            // If the reader is ahead of the writer, then the writer must have
            // wrapped around already, so the only space available is whats ahead
            // of the writer, behind the reader.
            if (r > w)
                return r - w;

            // Else, the only full slots are ahead of the reader and behind the
            // writer, so the free slots are what's left.
            return maxElements - (w - r);
        }

        std::atomic<size_t> maxElements = 0;
        std::atomic<size_t> readPos = 0;
        std::atomic<size_t> writePos = 0;
        std::vector<std::vector<T>> buffers;

        size_t indexMask = 0;
    };

} // namespace elem

namespace elem
{

    // A simple value metering node.
    //
    // Will pass its input through unaffected, but report the maximum and minimum
    // value seen through its event relay.
    //
    // Helpful for metering audio streams for things like drawing gain meters.
    template <typename FloatType>
    struct MeterNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // Copy input to output
            std::copy_n(inputData[0], numSamples, outputData);

            // Report the maximum value this block
            auto const [min, max] = std::minmax_element(inputData[0], inputData[0] + numSamples);
            (void) readoutQueue.push({ *min, *max });
        }

        void processEvents(std::function<void(std::string const&, js::Value)>& eventHandler) override {
            if (readoutQueue.size() <= 0)
                return;

            // Clear the readoutQueue into a local struct here. This way the readout
            // we propagate is the latest one and empties the queue for the processing thread
            ValueReadout ro;

            while (readoutQueue.size() > 0) {
                if (!readoutQueue.pop(ro)) {
                    return;
                }
            }

            // Now we propagate the latest value
            eventHandler("meter", js::Object({
                {"min", ro.min},
                {"max", ro.max},
                {"source", GraphNode<FloatType>::getPropertyWithDefault("name", js::Value())},
            }));
        }

        struct ValueReadout {
            FloatType min = 0;
            FloatType max = 0;
        };

        SingleWriterSingleReaderQueue<ValueReadout> readoutQueue;
    };

    // A simple value metering node whose callback is triggered on the rising
    // edge of an incoming pulse train.
    //
    // Will pass its input through unaffected, but report the value captured at
    // exactly the time of the rising edge.
    template <typename FloatType>
    struct SnapshotNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // We need the first channel to be the control signal and the second channel to
            // represent the signal we want to sample when the control changes
            if (numChannels < 2)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                const FloatType l = inputData[0][i];
                const FloatType x = inputData[1][i];
                const FloatType eps = std::numeric_limits<FloatType>::epsilon();

                // Check if it's time to latch
                // We're looking for L[i-1] near zero and L[i] non-zero, where L
                // is the discrete clock signal used to identify the hold. We latch
                // on the positive-going transition from a zero value to a non-zero
                // value.
                if (std::abs(z) <= eps && l > eps) {
                    (void) readoutQueue.push({ x });
                }

                z = l;
                outputData[i] = x;
            }
        }

        void processEvents(std::function<void(std::string const&, js::Value)>& eventHandler) override {
            // Now we propagate the latest value if we have one.
            if (readoutQueue.size() <= 0)
                return;

            // Clear the readoutQueue into a local struct here. This way the readout
            // we propagate is the latest one and empties the queue for the processing thread
            ValueReadout ro;

            while (readoutQueue.size() > 0) {
                if (!readoutQueue.pop(ro)) {
                    return;
                }
            }

            eventHandler("snapshot", js::Object({
                {"source", GraphNode<FloatType>::getPropertyWithDefault("name", js::Value())},
                {"data", ro.val},
            }));
        }

        struct ValueReadout {
            FloatType val = 0;
        };

        SingleWriterSingleReaderQueue<ValueReadout> readoutQueue;
        FloatType z = 0;
    };

    // An oscilloscope node which reports Float32Array buffers through
    // the event processing interface.
    //
    // Expecting potentially n > 1 children, will assemble an array of arrays representing each
    // child's buffer, so that we have coordinated streams
    template <typename FloatType>
    struct ScopeNode : public GraphNode<FloatType> {
        ScopeNode(NodeId id, FloatType const sr, int const blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize)
            , ringBuffer(4)
        {
            setProperty("channels", js::Value((js::Number) 1));
            setProperty("size", js::Value((js::Number) 512));
        }

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "size") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                if ((js::Number) val < 256 || (js::Number) val > 8192)
                    return ReturnCode::InvalidPropertyValue();
            }

            if (key == "channels") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                if ((js::Number) val < 0 || (js::Number) val > 4)
                    return ReturnCode::InvalidPropertyValue();
            }

            if (key == "name") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // Copy input to output
            std::copy_n(inputData[0], numSamples, outputData);

            // Fill our ring buffer
            ringBuffer.write(inputData, numChannels, numSamples);
        }

        void processEvents(std::function<void(std::string const&, js::Value)>& eventHandler) override {
            auto const size = static_cast<size_t>((js::Number) GraphNode<FloatType>::getPropertyWithDefault("size", js::Number(512)));
            auto const channels = static_cast<size_t>((js::Number) GraphNode<FloatType>::getPropertyWithDefault("channels", js::Number(1)));

            if (ringBuffer.size() > size) {
                // Retreive the scope data. Could improve the efficiency here using something
                // like a pmr pool allocator so that on each call to processEvents here we're
                // not actually allocating and deallocating new memory
                js::Array scopeData (channels);

                // If our ScopeNode instance is templated on `float` then we can read from the
                // ring buffer directly into a js::Float32Array value type
                if constexpr (std::is_same_v<FloatType, float>) {
                    for (size_t i = 0; i < channels; ++i) {
                        scopeData[i] = js::Float32Array(size);
                        scratchPointers[i] = scopeData[i].getFloat32Array().data();
                    }

                    if (ringBuffer.read(scratchPointers.data(), channels, size)) {
                        eventHandler("scope", js::Object({
                            {"source", GraphNode<FloatType>::getPropertyWithDefault("name", js::Value())},
                            {"data", std::move(scopeData)}
                        }));
                    }
                }

                // If our ScopeNode instance is templated on `double` then we read into a temporary
                // vector of doubles before copying (and casting) to a js::Float32Array value
                if constexpr (std::is_same_v<FloatType, double>) {
                    std::vector<double> temp(channels * size);

                    for (size_t i = 0; i < channels; ++i) {
                        scopeData[i] = js::Float32Array(size);
                        scratchPointers[i] = temp.data() + (i * size);
                    }

                    if (ringBuffer.read(scratchPointers.data(), channels, size)) {
                        for (size_t i = 0; i < channels; ++i) {
                            auto* dest = scopeData[i].getFloat32Array().data();

                            for (size_t j = 0; j < size; ++j) {
                                dest[j] = static_cast<float>(temp[(i * size) + j]);
                            }
                        }

                        eventHandler("scope", js::Object({
                            {"source", GraphNode<FloatType>::getPropertyWithDefault("name", js::Value())},
                            {"data", std::move(scopeData)}
                        }));
                    }
                }
            }
        }

        std::array<FloatType*, 8> scratchPointers;
        MultiChannelRingBuffer<FloatType> ringBuffer;
    };

} // namespace elem

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

namespace elem
{

    template <typename FloatType>
    struct RootNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int getChannelNumber()
        {
            return channelIndex.load();
        }

        FloatType getTargetGain()
        {
            return targetGain.load();
        }

        bool stillRunning()
        {
            auto const t = targetGain.load();
            auto const c = currentGain.load();

            return (t >= 0.5 || (std::abs(c - t) >= std::numeric_limits<FloatType>::epsilon()));
        }

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "active") {
                if (!val.isBool())
                    return ReturnCode::InvalidPropertyType();

                targetGain.store(FloatType(val ? 1 : 0));
            }

            if (key == "channel") {
                channelIndex.store(static_cast<int>((js::Number) val));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            auto const t = targetGain.load();
            auto c = currentGain.load();

            auto const direction = (t < c) ? FloatType(-1) : FloatType(1);
            auto const step = direction * FloatType(20) / FloatType(GraphNode<FloatType>::getSampleRate());

            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = inputData[0][i] * c;
                c = std::clamp(c + step, FloatType(0), FloatType(1));
            }

            currentGain.store(c);
        }

        std::atomic<FloatType> targetGain = 1;
        std::atomic<FloatType> currentGain = 0;
        std::atomic<int> channelIndex = -1;
    };

    template <typename FloatType, bool WithReset = false>
    struct PhasorNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        FloatType tick (FloatType freq) {
            FloatType step = freq * (FloatType(1.0) / FloatType(GraphNode<FloatType>::getSampleRate()));
            FloatType y = phase;

            FloatType next = phase + step;
            phase = next - std::floor(next);

            return y;
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            if constexpr (WithReset) {
                // If we don't have the inputs we need, we bail here and zero the buffer
                // hoping to prevent unexpected signals.
                if (numChannels < 2)
                    return (void) std::fill_n(outputData, numSamples, FloatType(0));

                // The seocnd input in this mode is for hard syncing our phasor back
                // to 0 when the reset signal goes high
                for (size_t i = 0; i < numSamples; ++i) {
                    auto const xn = inputData[0][i];

                    if (change(inputData[1][i]) > FloatType(0.5)) {
                      phase = FloatType(0);
                    }

                    outputData[i] = tick(xn);
                }
            } else {
                // If we don't have the inputs we need, we bail here and zero the buffer
                // hoping to prevent unexpected signals.
                if (numChannels < 1)
                    return (void) std::fill_n(outputData, numSamples, FloatType(0));

                for (size_t i = 0; i < numSamples; ++i) {
                    outputData[i] = tick(inputData[0][i]);
                }
            }
        }

        Change<FloatType> change;
        FloatType phase = 0;
    };

    template <typename FloatType>
    struct ConstNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "value") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                value.store(FloatType((js::Number) val));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto* outputData = ctx.outputData;
            auto numSamples = ctx.numSamples;

            auto const v = value.load();

            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = v;
            }
        }

        static_assert(std::atomic<FloatType>::is_always_lock_free);
        std::atomic<FloatType> value = 1;
    };

    template <typename FloatType>
    struct SampleRateNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto* outputData = ctx.outputData;
            auto numSamples = ctx.numSamples;

            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = FloatType(GraphNode<FloatType>::getSampleRate());
            }
        }
    };

    template <typename FloatType>
    struct CounterNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                auto const in = inputData[0][i];

                // When the gate is high, we just keep counting on a sample by sample basis
                if ((FloatType(1.0) - in) <= std::numeric_limits<FloatType>::epsilon()) {
                    outputData[i] = count;
                    count = count + FloatType(1);
                    continue;
                }

                // When the gate is closed, we reset
                count = FloatType(0);
                outputData[i] = FloatType(0);
            }
        }

        FloatType count = 0;
    };

    // Simply accumulates its input until reset
    template <typename FloatType>
    struct AccumNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 2)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                auto const in = inputData[0][i];
                auto const reset = inputData[1][i];

                if (change(reset) > FloatType(0.5)) {
                  runningTotal = FloatType(0);
                }

                runningTotal += in;
                outputData[i] = runningTotal;
            }
        }

        Change<FloatType> change;
        FloatType runningTotal = 0;
    };

    template <typename FloatType>
    struct LatchNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // We need the first channel to be the latch signal and the second channel to
            // represent the signal we want to sample when the latch changes
            if (numChannels < 2)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                const FloatType l = inputData[0][i];
                const FloatType x = inputData[1][i];
                const FloatType eps = std::numeric_limits<FloatType>::epsilon();

                // Check if it's time to latch
                // We're looking for L[i-1] near zero and L[i] non-zero, where L
                // is the discrete clock signal used to identify the hold. We latch
                // on the positive-going transition from a zero value to a non-zero
                // value.
                if (std::abs(z) <= eps && l > eps) {
                    hold = x;
                }

                z = l;
                outputData[i] = hold;
            }
        }

        FloatType z = 0;
        FloatType hold = 0;
    };

    template <typename FloatType>
    struct MaxHold : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "hold") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto h = GraphNode<FloatType>::getSampleRate() * 0.001 * (js::Number) val;
                holdTimeSamples.store(static_cast<uint32_t>(h));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // We want two input signals: the input to monitor and the reset signal
            if (numChannels < 2)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            auto const hts = holdTimeSamples.load();

            for (size_t i = 0; i < numSamples; ++i) {
                const FloatType in = inputData[0][i];
                const FloatType reset = inputData[1][i];

                if (change(reset) > FloatType(0.5) || ++samplesAtCurrentMax >= hts) {
                    max = in;
                    samplesAtCurrentMax = 0;
                } else {
                    if (in > max) {
                      samplesAtCurrentMax = 0;
                      max = in;
                    }
                }

                outputData[i] = max;
            }
        }

        Change<FloatType> change;
        std::atomic<uint32_t> holdTimeSamples = std::numeric_limits<uint32_t>::max();
        uint32_t samplesAtCurrentMax = 0;
        FloatType max = 0;
    };

    // This is a simple utility designed to act like a gate for pulse signals, except
    // that the gate closes immediately after letting a single pulse through.
    //
    // The use case here is for quantized events: suppose you want to trigger a sample
    // on the next quarter-note, you can arm the OnceNode and feed it a quarter-note rate
    // pulse train. No matter when you arm it, the _next_ pulse will be the one through,
    // and to repeat the behavior you need to re-arm (i.e. set arm = false, then arm = true).
    template <typename FloatType>
    struct OnceNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "arm") {
                if (!val.isBool())
                    return ReturnCode::InvalidPropertyType();

                // We don't let the incoming prop actually disarm.
                if (!armed.load()) {
                    armed.store((bool) val);
                }
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            auto const isArmed = armed.load();

            for (size_t i = 0; i < numSamples; ++i) {
                auto const delta = change(inputData[0][i]);
                auto const risingEdge = delta > FloatType(0.5);
                auto const fallingEdge = delta < FloatType(-0.5);

                // On the first rising edge when armed, we gain to 1 and disarm. We
                // will then let through the complete pulse, setting gain back to 0
                // on the falling edge of that pulse.
                if (isArmed && risingEdge) {
                    gain = FloatType(1);
                    armed.store(false);
                }

                if (fallingEdge) {
                    gain = FloatType(0);
                }

                outputData[i] = inputData[0][i] * gain;
            }
        }

        static_assert(std::atomic<FloatType>::is_always_lock_free);
        std::atomic<FloatType> armed = false;
        FloatType gain = 0;
        Change<FloatType> change;
    };

    template <typename FloatType>
    struct SequenceNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "hold") {
                if (!val.isBool())
                    return ReturnCode::InvalidPropertyType();

                wantsHold.store((js::Boolean) val);
            }

            if (key == "loop") {
                if (!val.isBool())
                    return ReturnCode::InvalidPropertyType();

                wantsLoop.store((js::Boolean) val);
            }

            if (key == "offset") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                if ((js::Number) val < 0.0)
                    return ReturnCode::InvalidPropertyValue();

                seqOffset.store(static_cast<size_t>((js::Number) val));
            }

            if (key == "seq") {
                if (!val.isArray())
                    return ReturnCode::InvalidPropertyType();

                auto& seq = val.getArray();
                auto data = sequencePool.allocate();

                // The data array that we get from the pool may have been
                // previously used to represent a different sequence. Need to
                // resize here and then overwrite below.
                data->resize(seq.size());

                for (size_t i = 0; i < seq.size(); ++i)
                    data->at(i) = FloatType((js::Number) seq[i]);

                // Finally, we push our new sequence data into the event
                // queue for the realtime thread.
                sequenceQueue.push(std::move(data));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // First order of business: grab the most recent sequence buffer to use if
            // there's anything in the queue
            if (sequenceQueue.size() > 0) {
                while (sequenceQueue.size() > 0) {
                    sequenceQueue.pop(activeSequence);
                }

                // Here we want to catch the case where the new sequence we pulled in has a different
                // length. If the new length is smaller than the prior length, we modulo our current read
                // index around, attempting to provide a nice default behavior. Otherwise, the index
                // remains unchanged.
                //
                // If the user needs specific behavior, they can achieve it by setting the offset property
                // and employing the reset train.
                seqIndex = (seqIndex % activeSequence->size());

                // Next, we want to catch the case where the user is changing sequences in the middle
                // of emitting values with hold: true. In that case, we need to update our holdValue immediately
                // to sample from the new sequence data to reflect the change, otherwise they won't hear the
                // new sequence values until the next rising edge of the trigger train.
                //
                // Only do this if we've already initiated the trigger sequence, otherwise we'll move
                // our train ahead too early (i.e. the first time seq data comes in but before the first pulse comes in)
                if (hasReceivedFirstPulse) {
                  holdValue = activeSequence->at(seqIndex);
                }
            }

            // Next, if we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1 || activeSequence == nullptr)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // We let the user optionally supply a second input, a pulse train whose
            // rising edge will reset our sequence position back to the start
            bool hasResetSignal = numChannels > 1;
            bool const hold = wantsHold.load();
            bool const loop = wantsLoop.load();

            for (size_t i = 0; i < numSamples; ++i) {
                auto const in = inputData[0][i];
                auto const reset = hasResetSignal ? inputData[1][i] : FloatType(0);

                // Reset our sequence index if we're on the rising edge of the reset signal
                //
                // We want this to happen before checking the trigger train so that if we reeive
                // a reset and a trigger at exactly the same time, we trigger from the reset index.
                if (resetChange(reset) > FloatType(0.5)) {
                    // We snap our seqIndex back to the offset at the time of the reset edge but
                    // we don't touch our holdValue until the next rising edge of the trigger input.
                    //
                    // This can yield a situation like the following: imagine a couple of small (low resolution)
                    // sequences playing half-note chords in a synth. Suppose I'm halfway through bar 2 playing
                    // a Dm7 and I want to drop my timeline/playhead a quarter of the way through bar 1 where I have
                    // an Am7 programmed in. The reset fires right away, but we won't take the new values of our chord
                    // sequences until the _next_ rising edge of the pulse train, i.e. at the boundary of the next half
                    // note. So we won't hear our Am7 (and in the interrim might continue hearing our Dm7) until we get
                    // to the next chord in the chord progression.
                    //
                    // This is undesirable, of course, but can be solved conceptually by using a massively high resolution
                    // sequence. That way I might not pick up the Am7 at exactly the right point in sampleTime but I'll pick
                    // it up maybe 1/256th note later (so quickly that the difference would be imperceptible). This is conceptually
                    // a totally acceptable behavior for el.seq but comes at the cost of using super high resolution sequences.
                    // Perhaps we need a new node for defining high resolution sequences without massive arrays...
                    seqIndex = seqOffset.load();
                }

                // Increment our sequence index on the rising edge of the input signal
                if (change(in) > FloatType(0.5)) {
                    holdValue = activeSequence->at(std::min(seqIndex, activeSequence->size() - 1));
                    hasReceivedFirstPulse = true;

                    // When looping we wrap around back to 0
                    if ((++seqIndex >= activeSequence->size()) && loop) {
                        seqIndex = 0;
                    }
                }

                // Now, if our seqIndex has run past the end of the array, we either
                //  * Emit 0 if hold = false
                //  * Emit the last value in the seq array if hold = true
                //
                // Finally, when holding, we don't fall to 0 with the incoming pulse train.
                if (seqIndex < activeSequence->size()) {
                    outputData[i] = (hold ? holdValue : holdValue * in);
                } else {
                    outputData[i] = (hold ? holdValue : 0);
                }
            }
        }

        using SequenceData = std::vector<FloatType>;

        RefCountedPool<SequenceData> sequencePool;
        SingleWriterSingleReaderQueue<std::shared_ptr<SequenceData>> sequenceQueue;
        std::shared_ptr<SequenceData> activeSequence;

        Change<FloatType> change;
        Change<FloatType> resetChange;

        std::atomic<bool> wantsHold = false;
        std::atomic<bool> wantsLoop = true;
        std::atomic<size_t> seqOffset = 0;

        FloatType holdValue = 0;
        size_t seqIndex = 0;
        bool hasReceivedFirstPulse = false;
    };

} // namespace elem

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

namespace elem
{

    // A single sample delay node.
    template <typename FloatType>
    struct SingleSampleDelayNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                auto const x = inputData[0][i];

                outputData[i] = z;
                z = x;
            }
        }

        FloatType z = 0;
    };

    // A variable length delay line with a feedback path.
    //
    //   el.delay({size: 44100}, el.ms2samps(len), fb, x)
    //
    // A feedforward component can be added trivially:
    //
    //   el.add(el.mul(ff, x), el.delay({size: 44100}, el.ms2samps(len), fb, x))
    //
    // At small delay lengths and with a feedback or feedforward component, this becomes
    // a simple comb filter.
    template <typename FloatType>
    struct VariableDelayNode : public GraphNode<FloatType> {
        VariableDelayNode(NodeId id, FloatType const sr, int const blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize)
        {
            (void) setProperty("size", js::Value((js::Number) blockSize));
        }

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "size") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto const size = static_cast<int>((js::Number) val);
                auto data = bufferPool.allocate();

                // The buffer that we get from the pool may have been
                // previously used for a different delay buffer. Need to
                // resize here and then overwrite below.
                data->resize(size);

                for (size_t i = 0; i < data->size(); ++i)
                    data->at(i) = FloatType(0);

                // Finally, we push our new buffer into the event
                // queue for the realtime thread.
                bufferQueue.push(std::move(data));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // First order of business: grab the most recent delay buffer to use if
            // there's anything in the queue
            while (bufferQueue.size() > 0) {
                bufferQueue.pop(activeBuffer);
                writeIndex = 0;
            }

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 3)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            auto const size = static_cast<int>(activeBuffer->size());
            auto* delayData = activeBuffer->data();

            if (size == 0)
                return (void) std::copy_n(inputData[0], numSamples, outputData);

            for (size_t i = 0; i < numSamples; ++i) {
                auto const offset = std::clamp(inputData[0][i], FloatType(0), FloatType(size));

                // In order to get the feedback right for non-zero read offsets, we read from the
                // line and sum with the input before writing. If we have a read offset of zero, then
                // reading before writing doesn't work: we should be reading the same thing that we're
                // writing. In that case, feedback doesn't make much sense anyways, so here we have a
                // special case which ignores the feedback value and reads and writes correctly
                if (offset <= std::numeric_limits<FloatType>::epsilon()) {
                    auto const in = inputData[2][i];

                    delayData[writeIndex] = in;
                    outputData[i] = in;

                    // Nudge the write index
                    if (++writeIndex >= size)
                        writeIndex -= size;

                    continue;
                }

                auto const readFrac = FloatType(size + writeIndex) - offset;
                auto const readLeft = static_cast<int>(readFrac);
                auto const readRight = readLeft + 1;
                auto const frac = readFrac - std::floor(readFrac);

                // Because of summing `size` with `writeIndex` when forming
                // `readFrac` above, we know that we don't run our read pointers
                // below 0 ever, so here we only need to account for wrapping past
                // size which we do via modulo.
                auto const left = delayData[readLeft % size];
                auto const right = delayData[readRight % size];

                // Now we can read the next sample out of the delay line with linear
                // interpolation for sub-sample delays.
                auto const out = left + frac * (right - left);

                // Then we can form the input to the delay line with the feedback
                // component
                auto const fb = std::clamp(inputData[1][i], FloatType(-1), FloatType(1));
                auto const in = inputData[2][i] + fb * out;

                // Write to the delay line
                delayData[writeIndex] = in;

                // Write to the output buffer
                outputData[i] = out;

                // Nudge the write index
                if (++writeIndex >= size)
                    writeIndex -= size;
            }
        }

        using DelayBuffer = std::vector<FloatType>;

        RefCountedPool<DelayBuffer> bufferPool;
        SingleWriterSingleReaderQueue<std::shared_ptr<DelayBuffer>> bufferQueue;
        std::shared_ptr<DelayBuffer> activeBuffer;

        int writeIndex = 0;
    };

    // A simple fixed length, non-interpolated delay line.
    //
    //   el.sdelay({size: 44100}, x)
    //
    // Intended for use cases where `el.delay` is overkill, and `sdelay`
    // can provide helpful performance benefits.
    template <typename FloatType>
    struct SampleDelayNode : public GraphNode<FloatType> {
        SampleDelayNode(NodeId id, FloatType const sr, int const bs)
            : GraphNode<FloatType>::GraphNode(id, sr, bs)
            , blockSize(bs)
        {
            (void) setProperty("size", js::Value((js::Number) blockSize));
        }

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "size") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                // Here we make sure that we allocate at least one block size
                // more than the desired delay length so that we can run our write
                // pointer freely forward each block, never risking clobbering. Then
                // we round up to the next power of two for bit masking tricks around
                // the delay line length.
                auto const len = static_cast<int>((js::Number) val);
                auto const size = elem::bitceil(len + blockSize);
                auto data = bufferPool.allocate();

                // The buffer that we get from the pool may have been
                // previously used for a different delay buffer. Need to
                // resize here and then overwrite below.
                data->resize(size);

                for (size_t i = 0; i < data->size(); ++i)
                    data->at(i) = FloatType(0);

                // Finally, we push our new buffer into the event
                // queue for the realtime thread.
                bufferQueue.push(std::move(data));
                length.store(len);
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // First order of business: grab the most recent delay buffer to use if
            // there's anything in the queue
            while (bufferQueue.size() > 0) {
                bufferQueue.pop(activeBuffer);
                writeIndex = 0;
            }

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1 || activeBuffer == nullptr)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            auto const size = static_cast<int>(activeBuffer->size());
            auto const mask = size - 1;
            auto const len = length.load();

            if (size == 0)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            auto* delayData = activeBuffer->data();
            auto* inData = inputData[0];

            auto readStart = writeIndex - len;

            for (size_t i = 0; i < numSamples; ++i) {
                // Write to the delay line
                auto const in = inData[i];
                delayData[writeIndex] = in;

                // Nudge the write index
                writeIndex = (writeIndex + 1) & mask;
            }

            for (int i = 0; i < static_cast<int>(numSamples); ++i) {
                // Write to the output buffer
                outputData[i] = delayData[(size + readStart + i) & mask];
            }
        }

        using DelayBuffer = std::vector<FloatType>;

        RefCountedPool<DelayBuffer> bufferPool;
        SingleWriterSingleReaderQueue<std::shared_ptr<DelayBuffer>> bufferQueue;
        std::shared_ptr<DelayBuffer> activeBuffer;

        std::atomic<int> length = 0;
        int writeIndex = 0;
        int blockSize = 0;
    };

} // namespace elem

#include <functional>

namespace elem
{

    // A special graph node type for receiving feedback from within the graph.
    //
    // Will attempt to feed input from a shared mutable resource to its output. If
    // no resource is found by the given name, will emit silence.
    template <typename FloatType>
    struct TapInNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val, SharedResourceMap<FloatType>& resources) override
        {
            if (key == "name") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                auto ref = resources.getOrCreateMutable((js::String) val, GraphNode<FloatType>::getBlockSize());
                bufferQueue.push(std::move(ref));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto* outputData = ctx.outputData;
            auto numSamples = ctx.numSamples;

            while (bufferQueue.size() > 0) {
                bufferQueue.pop(activeBuffer);
            }

            if (!activeBuffer)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            std::copy_n(activeBuffer->data(), numSamples, outputData);
        }

        SingleWriterSingleReaderQueue<MutableSharedResourceBuffer<FloatType>> bufferQueue;
        MutableSharedResourceBuffer<FloatType> activeBuffer;
    };

    // A special graph node type for producing feedback from within the graph.
    //
    // Will pass its input through unaffected, while also buffering the same
    // signal into a circular buffer which should be drained by the GraphRenderer
    // via `promoteTapBuffers` and then served to the appropriate TapInNodes during
    // the render pass.
    template <typename FloatType>
    struct TapOutNode : public GraphNode<FloatType> {
        TapOutNode(NodeId id, FloatType const sr, int const blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize)
        {
            delayBuffer.resize(blockSize);
            std::fill_n(delayBuffer.data(), blockSize, FloatType(0));
        }

        int setProperty(std::string const& key, js::Value const& val, SharedResourceMap<FloatType>& resources) override
        {
            if (key == "name") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                auto ref = resources.getOrCreateMutable((js::String) val, GraphNode<FloatType>::getBlockSize());
                tapBufferQueue.push(std::move(ref));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void promoteTapBuffers(size_t numSamples) {
            // First order of business: grab the most recent feedback buffer to use if
            // there's anything in the queue. We do this here as opposed to in `process`
            // because we know that the GraphRenderer will call `promoteTapBuffers` before
            // running the graph traversal and calling `process`. Knowing that this step comes first,
            // we cycle in the appropriate active buffer here so that we have a chance to move the
            // read pointer before moving the write pointer below. This way the writer doesn't get
            // ahead of the reader, and the reader remains appropriately behind the writer.
            while (tapBufferQueue.size() > 0) {
                tapBufferQueue.pop(activeTapBuffer);
            }

            // If after the above we still don't have an active buffer, there's nothing to do
            if (activeTapBuffer == nullptr)
                return;

            // Here, we're good to go draining the buffered delay data into the tap line
            std::copy_n(delayBuffer.data(), numSamples, activeTapBuffer->data());
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // We can shortcut if there's nothing to fill the feedback buffer with
            if (numChannels < 1 || numSamples > delayBuffer.size())
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // Else, we write to our delay line and pass through our input
            auto* delayData = delayBuffer.data();

            std::copy_n(inputData[0], numSamples, delayData);
            std::copy_n(inputData[0], numSamples, outputData);
        }

        std::vector<FloatType> delayBuffer;
        SingleWriterSingleReaderQueue<MutableSharedResourceBuffer<FloatType>> tapBufferQueue;
        MutableSharedResourceBuffer<FloatType> activeTapBuffer;
    };

} // namespace elem

namespace elem
{

    // A very simple one pole filter.
    //
    // Receives the pole position, p, as a signal, and the input to filter as the second signal.
    // See: https://ccrma.stanford.edu/~jos/filters/One_Pole.html
    template <typename FloatType>
    struct OnePoleNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 2)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // First channel is the pole position, second channel is the input signal
            for (size_t i = 0; i < numSamples; ++i) {
                auto const p = inputData[0][i];
                auto const x = inputData[1][i];

                z = x + p * z;
                outputData[i] = z;
            }
        }

        FloatType z = 0;
    };

    // A unity-gain one pole filter which takes a different coefficient depending on the
    // amplitude of the incoming signal.
    //
    // This is an envelope follower with a parameterizable attack and release
    // coefficient, given by the first and second input channel respectively.
    template <typename FloatType>
    struct EnvelopeNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            if (numChannels < 3)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // First channel is the pole position when the input exceeds the current
            // filter output, second channel is the pole position otherwise. The third
            // channel is the input signal to be filtered
            for (size_t i = 0; i < numSamples; ++i) {
                auto const ap = inputData[0][i];
                auto const rp = inputData[1][i];
                auto const vn = std::abs(inputData[2][i]);

                if (std::abs(vn) > z) {
                    z = ap * (z - vn) + vn;
                } else {
                    z = rp * (z - vn) + vn;
                }

                outputData[i] = z;
            }
        }

        FloatType z = 0;
    };

    // The classic second order Transposed Direct Form II filter implementation.
    //
    // Receives each coefficient as a signal, with the signal to filter as the last child.
    // See:
    //  https://ccrma.stanford.edu/~jos/filters/Transposed_Direct_Forms.html
    //  https://os.mbed.com/users/simon/code/dsp/docs/tip/group__BiquadCascadeDF2T.html
    template <typename FloatType>
    struct BiquadFilterNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 6)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                auto const b0 = inputData[0][i];
                auto const b1 = inputData[1][i];
                auto const b2 = inputData[2][i];
                auto const a1 = inputData[3][i];
                auto const a2 = inputData[4][i];
                auto const x = inputData[5][i];

                auto const y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;

                outputData[i] = y;
            }
        }

        FloatType z1 = 0;
        FloatType z2 = 0;
    };

} // namespace elem

// For later....
// This is my gendsp patch for remnant's tpt svf. Rewrite it as a filter node here.
/*
// Prewarp the cutoff frequency according to the
// bilinear transform.
prewarp_bt(fc) {
	wd = 2.0 * pi * fc;
	T = 1.0 / samplerate;
	wa = (2.0 / T) * tan(wd * T / 2.0);

	return wa;
}

// Implements Vadim Zavalishin's TPT 2-pole SVF, following Section 4
// from his book v2.0.0-alpha (page 110-111), and adding a cheap
// nonlinearity in the integrator.
//
// TODO: A nice improvement would be to properly address the
// nonlinearity in the feedback path and implement a root solver for
// a more accurate approximation.
// 
// Params:
//  `xn`: Input sample
//	`wa`: Analog cutoff requency; result of the cutoff prewarping
//	`R`: Filter resonance
svf_nl(xn, wa, R) {
	// State registers
	History z1(0.);
	History z2(0.);
	
	// Integrator gain coefficient
	T = 1.0 / samplerate;
	g = wa * T / 2.0;
	
	// Filter taps
	hp = (xn - (2.0 * R + g) * z1 - z2) / (1.0 + 2.0 * R * g + g * g);
	v1 = g * hp;
	bp = v1 + z1;
	v2 = g * bp;
	lp = v2 + z2;
	
	// Update state
	z1 = asinh(bp + v1);
	z2 = asinh(lp + v2);
	
	// We form the normalized, "classic" bandpass here.
	bpNorm = bp * 2.0 * R;
	
	return lp, bpNorm, hp;
}

// Tick the filter
lp, bp, hp = svf_nl(in1, prewarp_bt(in2), in3);

out1 = lp;
out2 = bp;
out3 = hp;
*/

namespace elem
{

    template <typename FloatType>
    struct CutoffPrewarpNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, bail here and zero the buffer
            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            double const T = 1.0 / GraphNode<FloatType>::getSampleRate();

            for (size_t i = 0; i < numSamples; ++i) {
                auto fc = inputData[0][i];

                // Cutoff prewarping
                double const twoPi = 2.0 * 3.141592653589793238;
                double const wd = twoPi * static_cast<double>(fc);
                double const g = std::tan(wd * T / 2.0);

                outputData[i] = FloatType(g);
            }
        }
    };

    template <typename FloatType>
    struct MultiMode1p : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        enum class Mode {
            Low = 0,
            High = 2,
            All = 4,
        };

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "mode") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                auto const m = (js::String) val;

                if (m == "lowpass")     { _mode.store(Mode::Low); }
                if (m == "highpass")    { _mode.store(Mode::High); }
                if (m == "allpass")     { _mode.store(Mode::All); }
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, bail here and zero the buffer
            if (numChannels < 2)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // Set up our output derivation
            auto const m = _mode.load();

            // Run the filter
            for (size_t i = 0; i < numSamples; ++i) {
                auto const g = std::clamp(static_cast<double>(inputData[0][i]), 0.0, 0.9999);
                auto xn = inputData[1][i];

                // Resolve the instantaneous gain
                double const G = g / (1.0 + g);

                // Tick the filter
                double const v = (static_cast<double>(xn) - z) * G;
                double const lp = v + z;

                z = lp + v;

                switch (m) {
                    case Mode::Low:
                        outputData[i] = FloatType(lp);
                        break;
                    case Mode::High:
                        outputData[i] = xn - FloatType(lp);
                        break;
                    case Mode::All:
                        outputData[i] = FloatType(lp + lp - xn);
                        break;
                    default:
                        break;
                }
            }
        }

        // Props
        std::atomic<Mode> _mode { Mode::Low };
        static_assert(std::atomic<Mode>::is_always_lock_free);

        // Coefficients
        double z = 0;
    };

} // namespace elem

namespace elem
{

    // A linear State Variable Filter based on Andy Simper's work
    //
    // See: https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
    //
    // This filter supports one "mode" property and expects three children: one
    // which defines the cutoff frequency, one which defines the filter Q, and finally
    // the input signal itself. Accepting these inputs as audio signals allows for
    // fast modulation at the processing expense of computing the coefficients on
    // every tick of the filter.
    template <typename FloatType>
    struct StateVariableFilterNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        enum class Mode {
            Low = 0,
            Band = 1,
            High = 2,
            Notch = 3,
            All = 4,
        };

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "mode") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                auto const m = (js::String) val;

                if (m == "lowpass")     { _mode.store(Mode::Low); }
                if (m == "bandpass")    { _mode.store(Mode::Band); }
                if (m == "highpass")    { _mode.store(Mode::High); }
                if (m == "notch")       { _mode.store(Mode::Notch); }
                if (m == "allpass")     { _mode.store(Mode::All); }
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        inline FloatType tick (Mode m, FloatType v0) {
            double v3 = v0 - _ic2eq;
            double v1 = _ic1eq * _a1 + v3 * _a2;
            double v2 = _ic2eq + _ic1eq * _a2 + v3 * _a3;

            _ic1eq = v1 * 2.0 - _ic1eq;
            _ic2eq = v2 * 2.0 - _ic2eq;

            switch (m) {
                case Mode::Low:
                    return FloatType(v2);
                case Mode::Band:
                    return FloatType(v1);
                case Mode::High:
                    return FloatType(v0 - _k * v1 - v2);
                case Mode::Notch:
                    return FloatType(v0 - _k * v1);
                case Mode::All:
                    return FloatType(v0 - 2.0 * _k * v1);
                default:
                    return FloatType(0);
            }
        }

        inline void updateCoeffs (double fc, double q) {
            auto const sr = GraphNode<FloatType>::getSampleRate();

            _g = std::tan(3.14159265359 * std::clamp(fc, 20.0, sr / 2.0001)  / sr);
            _k = 1.0 / std::clamp(q, 0.25, 20.0);
            _a1 = 1.0 / (1.0 + _g * (_g + _k));
            _a2 = _g * _a1;
            _a3 = _g * _a2;
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            auto m = _mode.load();

            // If we don't have the inputs we need, bail here and zero the buffer
            if (numChannels < 3)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                auto fc = inputData[0][i];
                auto q = inputData[1][i];
                auto xn = inputData[2][i];

                // Update coeffs at audio rate
                updateCoeffs(fc, q);

                // Tick the filter
                outputData[i] = tick(m, xn);
            }
        }

        // Props
        std::atomic<Mode> _mode { Mode::Low };
        static_assert(std::atomic<Mode>::is_always_lock_free);

        // Coefficients
        double _g = 0;
        double _k = 0;
        double _a1 = 0;
        double _a2 = 0;
        double _a3 = 0;

        // State
        double _ic1eq = 0;
        double _ic2eq = 0;
    };

} // namespace elem

namespace elem
{

    // A linear State Variable Shelf Filter based on Andy Simper's work
    //
    // See: https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
    //
    // This filter supports one "mode" property and expects four children: one
    // which defines the cutoff frequency, one which defines the filter Q, one which
    // defines the band gain (in decibels), and finally the input signal itself.
    //
    // Accepting these inputs as audio signals allows for fast modulation at the
    // processing expense of computing the coefficients on every tick of the filter.
    template <typename FloatType>
    struct StateVariableShelfFilterNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        enum class Mode {
            Lowshelf = 0,
            Highshelf = 1,
            Bell = 2,
        };

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "mode") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                auto const m = (js::String) val;

                if (m == "lowshelf")     { _mode.store(Mode::Lowshelf); }
                if (m == "highshelf")    { _mode.store(Mode::Highshelf); }

                if (m == "bell" || m == "peak") { _mode.store(Mode::Bell); }
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        inline FloatType tick (Mode m, FloatType v0) {
            double v3 = v0 - _ic2eq;
            double v1 = _ic1eq * _a1 + v3 * _a2;
            double v2 = _ic2eq + _ic1eq * _a2 + v3 * _a3;

            _ic1eq = v1 * 2.0 - _ic1eq;
            _ic2eq = v2 * 2.0 - _ic2eq;

            switch (m) {
                case Mode::Bell:
                    return FloatType(v0 + _k * (_A * _A - 1.0) * v1);
                case Mode::Lowshelf:
                    return FloatType(v0 + _k * (_A - 1.0) * v1 + (_A * _A - 1.0) * v2);
                case Mode::Highshelf:
                    return FloatType(_A * _A * v0 + _k * (1.0 - _A) * _A * v1 + (1.0 - _A * _A) * v2);
                default:
                    return FloatType(0);
            }
        }

        inline void updateCoeffs (Mode m, double fc, double q, double gainDecibels) {
            auto const sr = GraphNode<FloatType>::getSampleRate();

            _A = std::pow(10, gainDecibels / 40.0);
            _g = std::tan(3.14159265359 * std::clamp(fc, 20.0, sr / 2.0001)  / sr);
            _k = 1.0 / std::clamp(q, 0.25, 20.0);

            if (m == Mode::Lowshelf)
                _g /= _A;
            if (m == Mode::Highshelf)
                _g *= _A;
            if (m == Mode::Bell)
                _k /= _A;

            _a1 = 1.0 / (1.0 + _g * (_g + _k));
            _a2 = _g * _a1;
            _a3 = _g * _a2;
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            auto m = _mode.load();

            // If we don't have the inputs we need, bail here and zero the buffer
            if (numChannels < 4)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                auto fc = inputData[0][i];
                auto q = inputData[1][i];
                auto gain = inputData[2][i];
                auto xn = inputData[3][i];

                // Update coeffs at audio rate
                updateCoeffs(m, fc, q, gain);

                // Tick the filter
                outputData[i] = tick(m, xn);
            }
        }

        // Props
        std::atomic<Mode> _mode { Mode::Lowshelf };
        static_assert(std::atomic<Mode>::is_always_lock_free);

        // Coefficients
        double _A = 0;
        double _g = 0;
        double _k = 0;
        double _a1 = 0;
        double _a2 = 0;
        double _a3 = 0;

        // State
        double _ic1eq = 0;
        double _ic2eq = 0;
    };

} // namespace elem

namespace elem
{

    template <typename FloatType>
    struct CaptureNode : public GraphNode<FloatType> {
        CaptureNode(NodeId id, FloatType const sr, int const blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize),
              ringBuffer(1, elem::bitceil(static_cast<size_t>(sr)))
        {
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 2)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // Propagate input
            std::copy_n(inputData[1], numSamples, outputData);

            // Capture
            for (size_t i = 0; i < numSamples; ++i) {
                auto const g = static_cast<bool>(inputData[0][i]);
                auto const dg = change(inputData[0][i]);
                auto const fallingEdge = (dg < FloatType(-0.5));

                // If we're at the falling edge of the gate signal or need space in our scratch
                // we propagate the data into the ring
                if (fallingEdge || scratchSize >= scratchBuffer.size()) {
                    auto const* writeData = scratchBuffer.data();
                    ringBuffer.write(&writeData, 1, scratchSize);
                    scratchSize = 0;

                    // And if it's the falling edge we alert the event processor
                    if (fallingEdge) {
                        relayReady.store(true);
                    }
                }

                // Otherwise, if the record signal is on, write to our scratch
                if (g) {
                    scratchBuffer[scratchSize++] = inputData[1][i];
                }
            }
        }

        void processEvents(std::function<void(std::string const&, js::Value)>& eventHandler) override {
            auto const samplesAvailable = ringBuffer.size();

            if (samplesAvailable > 0) {
                auto currentSize = relayBuffer.size();
                relayBuffer.resize(currentSize + samplesAvailable);

                auto relayData = relayBuffer.data() + currentSize;

                if (!ringBuffer.read(&relayData, 1, samplesAvailable))
                    return;
            }

            if (relayReady.exchange(false)) {
                // Now we can go ahead and relay the data
                js::Float32Array copy;

                auto relaySize = relayBuffer.size();
                auto relayData = relayBuffer.data();

                copy.resize(relaySize);

                // Can't use std::copy_n here if FloatType is double, so we do it manually
                for (size_t i = 0; i < relaySize; ++i) {
                    copy[i] = static_cast<float>(relayData[i]);
                }

                relayBuffer.clear();

                eventHandler("capture", js::Object({
                    {"source", GraphNode<FloatType>::getPropertyWithDefault("name", js::Value())},
                    {"data", copy}
                }));
            }
        }

        Change<FloatType> change;
        MultiChannelRingBuffer<FloatType> ringBuffer;
        std::array<FloatType, 128> scratchBuffer;
        size_t scratchSize = 0;

        std::vector<FloatType> relayBuffer;
        std::atomic<bool> relayReady = false;
    };

} // namespace elem

namespace elem
{

    template <typename FloatType, FloatType op(FloatType)>
    struct UnaryOperationNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = op(inputData[0][i]);
            }
        }
    };

    template <typename FloatType, typename BinaryOp>
    struct BinaryOperationNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 2)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // Copy the first input to the output buffer
            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = inputData[0][i];
            }

            // Then walk the second channel with the operator
            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = op(outputData[i], inputData[1][i]);
            }
        }

        BinaryOp op;
    };

    template <typename FloatType, typename BinaryOp>
    struct BinaryReducingNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // Copy the first input to the output buffer
            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = inputData[0][i];
            }

            // Then for each remaining channel, perform the arithmetic operation
            // into the output buffer.
            for (size_t i = 1; i < numChannels; ++i) {
                for (size_t j = 0; j < numSamples; ++j) {
                    outputData[j] = op(outputData[j], inputData[i][j]);
                }
            }
        }

        BinaryOp op;
    };

    template <typename FloatType>
    struct IdentityNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "channel") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                channel.store(static_cast<int>((js::Number) val));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            auto const ch = static_cast<size_t>(channel.load());

            // If we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (ch < 0 || ch >= numChannels)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = inputData[ch][i];
            }
        }

        std::atomic<int> channel = 0;
    };

    template <typename FloatType>
    struct Modulus {
        FloatType operator() (FloatType x, FloatType y) {
            return std::fmod(x, y);
        }
    };

    template <typename FloatType>
    struct SafeDivides {
        FloatType operator() (FloatType x, FloatType y) {
            return (y == FloatType(0)) ? 0 : x / y;
        }
    };

    template <typename FloatType>
    struct Eq {
        FloatType operator() (FloatType x, FloatType y) {
            return std::abs(x - y) <= std::numeric_limits<FloatType>::epsilon();
        }
    };

    template <typename FloatType>
    struct BinaryAnd {
        FloatType operator() (FloatType x, FloatType y) {
            return FloatType(std::abs(FloatType(1) - x) <= std::numeric_limits<FloatType>::epsilon()
                && std::abs(FloatType(1) - y) <= std::numeric_limits<FloatType>::epsilon());
        }
    };

    template <typename FloatType>
    struct BinaryOr {
        FloatType operator() (FloatType x, FloatType y) {
            return FloatType(std::abs(FloatType(1) - x) <= std::numeric_limits<FloatType>::epsilon()
                || std::abs(FloatType(1) - y) <= std::numeric_limits<FloatType>::epsilon());
        }
    };

    template <typename FloatType>
    struct Min {
        FloatType operator() (FloatType x, FloatType y) {
            return std::min(x, y);
        }
    };

    template <typename FloatType>
    struct Max {
        FloatType operator() (FloatType x, FloatType y) {
            return std::max(x, y);
        }
    };

    template <typename FloatType>
    struct SafePow {
        FloatType operator() (FloatType x, FloatType y) {
            // Catch the case of a negative base and a non-integer exponent
            if (x < FloatType(0) && y != std::floor(y))
                return FloatType(0);

            return std::pow(x, y);
        }
    };

} // namespace elem

namespace elem
{

    namespace detail
    {

        enum class BlepMode {
            Saw = 0,
            Square = 1,
            Triangle = 2,
        };
    }

    template <typename FloatType, detail::BlepMode Mode>
    struct PolyBlepOscillatorNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        inline FloatType blep (FloatType phase, FloatType increment)
        {
            if (phase < increment)
            {
                auto p = phase / increment;
                return (FloatType(2) - p) * p - FloatType(1);
            }

            if (phase > (FloatType(1) - increment))
            {
                auto p = (phase - FloatType(1)) / increment;
                return (p + FloatType(2)) * p + FloatType(1);
            }

            return FloatType(0);
        }

        inline FloatType tick (FloatType phase, FloatType increment)
        {
            if constexpr (Mode == detail::BlepMode::Saw) {
                return FloatType(2) * phase - FloatType(1) - blep(phase, increment);
            }

            if constexpr (Mode == detail::BlepMode::Square || Mode == detail::BlepMode::Triangle) {
                auto const naive = phase < FloatType(0.5)
                    ? FloatType(1)
                    : FloatType(-1);

                auto const halfPhase = std::fmod(phase + FloatType(0.5), FloatType(1));
                auto const square = naive + blep(phase, increment) - blep(halfPhase, increment);

                if constexpr (Mode == detail::BlepMode::Square) {
                    return square;
                }

                acc += FloatType(4) * increment * square;
                return acc;
            }

            return FloatType(0);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // If we don't have the inputs we need, bail here and zero the buffer
            if (numChannels < 1)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            FloatType const sr = elem::GraphNode<FloatType>::getSampleRate();

            for (size_t i = 0; i < numSamples; ++i) {
                auto freq = inputData[0][i];
                auto increment = freq / sr;

                auto out = tick(phase, increment);

                phase += increment;

                if (phase >= FloatType(1))
                    phase -= FloatType(1);

                outputData[i] = out;
            }
        }

        FloatType phase = 0;
        FloatType acc = 0;
    };

} // namespace elem

namespace elem
{

    template <typename FloatType>
    struct UniformRandomNoiseNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "seed") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                seed = static_cast<uint32_t>((js::Number) val);
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        // Neat random number generator found here: https://stackoverflow.com/a/3747462/2320243
        // Turns out it's almost exactly the same one used in Max/MSP's Gen for its [noise] block
        inline int fastRand()
        {
            seed = 214013 * seed + 2531011;
            return (seed >> 16) & 0x7FFF;
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto* outputData = ctx.outputData;
            auto numSamples = ctx.numSamples;

            for (size_t i = 0; i < numSamples; ++i) {
                outputData[i] = (fastRand() / static_cast<FloatType>(0x7FFF));
            }
        }

        uint32_t seed = static_cast<uint32_t>(std::rand());
    };

} // namespace elem

namespace elem
{

    template <typename FloatType>
    struct VariablePitchLerpReader;

    // SampleNode is a core builtin for sample playback.
    //
    // The sample file is loaded from disk or from virtual memory with a path set by the `path` property.
    // The sample is then triggered on the rising edge of an incoming pulse train, so
    // this node expects a single child node delivering that train.
    template <typename FloatType, typename ReaderType = VariablePitchLerpReader<FloatType>>
    struct SampleNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val, SharedResourceMap<FloatType>& resources) override
        {
            if (key == "path") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                if (!resources.has((js::String) val))
                    return ReturnCode::InvalidPropertyValue();

                auto ref = resources.get((js::String) val);
                bufferQueue.push(std::move(ref));
            }

            if (key == "mode") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                auto v = (js::String) val;

                if (v == "trigger") { mode.store(Mode::Trigger); }
                if (v == "gate") { mode.store(Mode::Gate); }
                if (v == "loop") { mode.store(Mode::Loop); }
            }

            if (key == "startOffset") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto const v = (js::Number) val;
                auto const vi = static_cast<int>(v);

                if (vi < 0)
                    return ReturnCode::InvalidPropertyValue();

                startOffset.store(static_cast<size_t>(vi));
            }

            if (key == "stopOffset") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto const v = (js::Number) val;
                auto const vi = static_cast<int>(v);

                if (vi < 0)
                    return ReturnCode::InvalidPropertyValue();

                stopOffset.store(static_cast<size_t>(vi));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void reset() override {
            readers[0].noteOff();
            readers[1].noteOff();
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            auto const sampleRate = GraphNode<FloatType>::getSampleRate();

            // First order of business: grab the most recent sample buffer to use if
            // there's anything in the queue. This behavior means that changing the buffer
            // while playing the sample will cause a discontinuity.
            while (bufferQueue.size() > 0) {
                bufferQueue.pop(activeBuffer);

                readers[0] = ReaderType(sampleRate, activeBuffer);
                readers[1] = ReaderType(sampleRate, activeBuffer);
            }

            // If we don't have an input trigger or an active buffer, we can just return here
            if (numChannels < 1 || activeBuffer == nullptr)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // Now we expect the first input channel to carry a pulse train, and we
            // look through that input for the next rising edge. When that edge is found,
            // we process the current chunk and then allocate the next reader.
            auto const playbackMode = mode.load();
            auto const wantsLoop = mode == Mode::Loop;
            auto const ostart = startOffset.load();
            auto const ostop = stopOffset.load();

            // Optionally accept a second input signal specifying the playback rate
            auto const hasPlaybackRateSignal = numChannels >= 2;

            for (size_t i = 0; i < numSamples; ++i) {
                auto cv = change(inputData[0][i]);
                auto const rate = hasPlaybackRateSignal ? inputData[1][i] : FloatType(1);

                // Rising edge
                if (cv > FloatType(0.5)) {
                    readers[currentReader & 1].noteOff();
                    readers[++currentReader & 1].noteOn(ostart);
                }

                // If we're in trigger mode then we can ignore falling edges
                if (cv < FloatType(-0.5) && playbackMode != Mode::Trigger) {
                    readers[currentReader & 1].noteOff();
                }

                // Process both readers for the current sample
                outputData[i] = readers[0].tick(ostart, ostop, rate, wantsLoop) + readers[1].tick(ostart, ostop, rate, wantsLoop);
            }
        }

        SingleWriterSingleReaderQueue<SharedResourceBuffer<FloatType>> bufferQueue;
        SharedResourceBuffer<FloatType> activeBuffer;

        Change<FloatType> change;
        std::array<ReaderType, 2> readers;
        size_t currentReader = 0;

        enum class Mode
        {
            Trigger = 0,
            Gate = 1,
            Loop = 2,
        };

        std::atomic<Mode> mode = Mode::Trigger;
        std::atomic<size_t> startOffset = 0;
        std::atomic<size_t> stopOffset = 0;
    };

    // A helper struct for reading from sample data with variable rate using
    // linear interpolation.
    template <typename FloatType>
    struct VariablePitchLerpReader
    {
        VariablePitchLerpReader() = default;

        VariablePitchLerpReader(FloatType _sampleRate, SharedResourceBuffer<FloatType> _sourceBuffer)
            : sourceBuffer(_sourceBuffer), sampleRate(_sampleRate), gainSmoothAlpha(1.0 - std::exp(-1.0 / (0.01 * _sampleRate))) {}

        VariablePitchLerpReader(VariablePitchLerpReader& other)
            : sourceBuffer(other.sourceBuffer), sampleRate(other.sampleRate), gainSmoothAlpha(1.0 - std::exp(-1.0 / (0.01 * other.sampleRate))) {}

        void noteOn(size_t const startOffset)
        {
            targetGain = FloatType(1);
            pos = FloatType(startOffset);
        }

        void noteOff()
        {
            targetGain = FloatType(0);
        }

        FloatType tick (size_t const startOffset, size_t const stopOffset, FloatType const stepSize, bool const wantsLoop)
        {
            if (sourceBuffer == nullptr || pos < 0.0 || (gain == FloatType(0) && targetGain == FloatType(0)))
                return FloatType(0);

            auto* sourceData = sourceBuffer->data();
            size_t const sourceLength = sourceBuffer->size();

            if (pos >= (double) (sourceLength - stopOffset)) {
                if (!wantsLoop) {
                    return FloatType(0);
                }

                pos = (double) startOffset;
            }

            // Linear interpolation on the buffer read
            auto readLeft = static_cast<size_t>(pos);
            auto readRight = readLeft + 1;
            auto const frac = FloatType(pos - (double) readLeft);

            if (readLeft >= sourceLength)
                readLeft -= sourceLength;

            if (readRight >= sourceLength)
                readRight -= sourceLength;

            auto const left = sourceData[readLeft];
            auto const right = sourceData[readRight];

            // Now we can read the next sample out of the buffer with linear
            // interpolation for sub-sample reads.
            auto const out = gain * (left + frac * (right - left));
            auto const gainSettled = std::abs(targetGain - gain) <= std::numeric_limits<FloatType>::epsilon();

            // Update our state
            pos = pos + (double) stepSize;
            gain = gainSettled ? targetGain : gain + gainSmoothAlpha * (targetGain - gain);
            gain = std::clamp(gain, FloatType(0), FloatType(1));

            // And return
            return out;
        }

        SharedResourceBuffer<FloatType> sourceBuffer;

        FloatType sampleRate = 0;
        FloatType gainSmoothAlpha = 0;
        FloatType targetGain = 0;
        FloatType gain = 0;
        double pos = 0;
    };

} // namespace elem

#ifndef SIGNALSMITH_STRETCH_H
#define SIGNALSMITH_STRETCH_H

#ifndef SIGNALSMITH_DSP_SPECTRAL_H
#define SIGNALSMITH_DSP_SPECTRAL_H

#ifndef SIGNALSMITH_DSP_COMMON_H
#define SIGNALSMITH_DSP_COMMON_H

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

namespace signalsmith {
	/**	@defgroup Common Common
		@brief Definitions and helper classes used by the rest of the library
		
		@{
		@file
	*/

#define SIGNALSMITH_DSP_VERSION_MAJOR 1
#define SIGNALSMITH_DSP_VERSION_MINOR 3
#define SIGNALSMITH_DSP_VERSION_PATCH 3
#define SIGNALSMITH_DSP_VERSION_STRING "1.3.3"

	/** Version compatability check.
	\code{.cpp}
		static_assert(signalsmith::version(1, 0, 0), "version check");
	\endcode
	... or use the equivalent `SIGNALSMITH_DSP_VERSION_CHECK`.
	Major versions are not compatible with each other.  Minor and patch versions are backwards-compatible.
	*/
	constexpr bool versionCheck(int major, int minor, int patch=0) {
		return major == SIGNALSMITH_DSP_VERSION_MAJOR
			&& (SIGNALSMITH_DSP_VERSION_MINOR > minor
				|| (SIGNALSMITH_DSP_VERSION_MINOR == minor && SIGNALSMITH_DSP_VERSION_PATCH >= patch));
	}

/// Check the library version is compatible (semver).
#define SIGNALSMITH_DSP_VERSION_CHECK(major, minor, patch) \
	static_assert(::signalsmith::versionCheck(major, minor, patch), "signalsmith library version is " SIGNALSMITH_DSP_VERSION_STRING);

/** @} */
} // signalsmith::
#endif // include guard

#ifndef SIGNALSMITH_DSP_PERF_H
#define SIGNALSMITH_DSP_PERF_H

#include <complex>

namespace signalsmith {
namespace perf {
	/**	@defgroup Performance Performance helpers
		@brief Nothing serious, just some `#defines` and helpers
		
		@{
		@file
	*/
		
	/// *Really* insist that a function/method is inlined (mostly for performance in DEBUG builds)
	#ifndef SIGNALSMITH_INLINE
	#ifdef __GNUC__
	#define SIGNALSMITH_INLINE __attribute__((always_inline)) inline
	#elif defined(__MSVC__)
	#define SIGNALSMITH_INLINE __forceinline inline
	#else
	#define SIGNALSMITH_INLINE inline
	#endif
	#endif

	/** @brief Complex-multiplication (with optional conjugate second-arg), without handling NaN/Infinity
		The `std::complex` multiplication has edge-cases around NaNs which slow things down and prevent auto-vectorisation.
	*/
	template <bool conjugateSecond=false, typename V>
	SIGNALSMITH_INLINE static std::complex<V> mul(const std::complex<V> &a, const std::complex<V> &b) {
		return conjugateSecond ? std::complex<V>{
			b.real()*a.real() + b.imag()*a.imag(),
			b.real()*a.imag() - b.imag()*a.real()
		} : std::complex<V>{
			a.real()*b.real() - a.imag()*b.imag(),
			a.real()*b.imag() + a.imag()*b.real()
		};
	}

/** @} */
}} // signalsmith::perf::

#endif // include guard

#ifndef SIGNALSMITH_FFT_V5
#define SIGNALSMITH_FFT_V5

#ifndef SIGNALSMITH_DSP_COMMON_H
#define SIGNALSMITH_DSP_COMMON_H

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

namespace signalsmith {
	/**	@defgroup Common Common
		@brief Definitions and helper classes used by the rest of the library
		
		@{
		@file
	*/

#define SIGNALSMITH_DSP_VERSION_MAJOR 1
#define SIGNALSMITH_DSP_VERSION_MINOR 3
#define SIGNALSMITH_DSP_VERSION_PATCH 3
#define SIGNALSMITH_DSP_VERSION_STRING "1.3.3"

	/** Version compatability check.
	\code{.cpp}
		static_assert(signalsmith::version(1, 0, 0), "version check");
	\endcode
	... or use the equivalent `SIGNALSMITH_DSP_VERSION_CHECK`.
	Major versions are not compatible with each other.  Minor and patch versions are backwards-compatible.
	*/
	constexpr bool versionCheck(int major, int minor, int patch=0) {
		return major == SIGNALSMITH_DSP_VERSION_MAJOR
			&& (SIGNALSMITH_DSP_VERSION_MINOR > minor
				|| (SIGNALSMITH_DSP_VERSION_MINOR == minor && SIGNALSMITH_DSP_VERSION_PATCH >= patch));
	}

/// Check the library version is compatible (semver).
#define SIGNALSMITH_DSP_VERSION_CHECK(major, minor, patch) \
	static_assert(::signalsmith::versionCheck(major, minor, patch), "signalsmith library version is " SIGNALSMITH_DSP_VERSION_STRING);

/** @} */
} // signalsmith::
#endif // include guard

#ifndef SIGNALSMITH_DSP_PERF_H
#define SIGNALSMITH_DSP_PERF_H

#include <complex>

namespace signalsmith {
namespace perf {
	/**	@defgroup Performance Performance helpers
		@brief Nothing serious, just some `#defines` and helpers
		
		@{
		@file
	*/
		
	/// *Really* insist that a function/method is inlined (mostly for performance in DEBUG builds)
	#ifndef SIGNALSMITH_INLINE
	#ifdef __GNUC__
	#define SIGNALSMITH_INLINE __attribute__((always_inline)) inline
	#elif defined(__MSVC__)
	#define SIGNALSMITH_INLINE __forceinline inline
	#else
	#define SIGNALSMITH_INLINE inline
	#endif
	#endif

	/** @brief Complex-multiplication (with optional conjugate second-arg), without handling NaN/Infinity
		The `std::complex` multiplication has edge-cases around NaNs which slow things down and prevent auto-vectorisation.
	*/
	template <bool conjugateSecond=false, typename V>
	SIGNALSMITH_INLINE static std::complex<V> mul(const std::complex<V> &a, const std::complex<V> &b) {
		return conjugateSecond ? std::complex<V>{
			b.real()*a.real() + b.imag()*a.imag(),
			b.real()*a.imag() - b.imag()*a.real()
		} : std::complex<V>{
			a.real()*b.real() - a.imag()*b.imag(),
			a.real()*b.imag() + a.imag()*b.real()
		};
	}

/** @} */
}} // signalsmith::perf::

#endif // include guard

#include <vector>
#include <complex>
#include <cmath>

namespace signalsmith { namespace fft {
	/**	@defgroup FFT FFT (complex and real)
		@brief Fourier transforms (complex and real)

		@{
		@file
	*/

	namespace _fft_impl {

		template <typename V>
		SIGNALSMITH_INLINE V complexReal(const std::complex<V> &c) {
			return ((V*)(&c))[0];
		}
		template <typename V>
		SIGNALSMITH_INLINE V complexImag(const std::complex<V> &c) {
			return ((V*)(&c))[1];
		}

		// Complex multiplication has edge-cases around Inf/NaN - handling those properly makes std::complex non-inlineable, so we use our own
		template <bool conjugateSecond, typename V>
		SIGNALSMITH_INLINE std::complex<V> complexMul(const std::complex<V> &a, const std::complex<V> &b) {
			V aReal = complexReal(a), aImag = complexImag(a);
			V bReal = complexReal(b), bImag = complexImag(b);
			return conjugateSecond ? std::complex<V>{
				bReal*aReal + bImag*aImag,
				bReal*aImag - bImag*aReal
			} : std::complex<V>{
				aReal*bReal - aImag*bImag,
				aReal*bImag + aImag*bReal
			};
		}

		template<bool flipped, typename V>
		SIGNALSMITH_INLINE std::complex<V> complexAddI(const std::complex<V> &a, const std::complex<V> &b) {
			V aReal = complexReal(a), aImag = complexImag(a);
			V bReal = complexReal(b), bImag = complexImag(b);
			return flipped ? std::complex<V>{
				aReal + bImag,
				aImag - bReal
			} : std::complex<V>{
				aReal - bImag,
				aImag + bReal
			};
		}

		// Use SFINAE to get an iterator from std::begin(), if supported - otherwise assume the value itself is an iterator
		template<typename T, typename=void>
		struct GetIterator {
			static T get(const T &t) {
				return t;
			}
		};
		template<typename T>
		struct GetIterator<T, decltype((void)std::begin(std::declval<T>()))> {
			static auto get(const T &t) -> decltype(std::begin(t)) {
				return std::begin(t);
			}
		};
	}

	/** Floating-point FFT implementation.
	It is fast for 2^a * 3^b.
	Here are the peak and RMS errors for `float`/`double` computation:
	\diagram{fft-errors.svg Simulated errors for pure-tone harmonic inputs\, compared to a theoretical upper bound from "Roundoff error analysis of the fast Fourier transform" (G. Ramos, 1971)}
	*/
	template<typename V=double>
	class FFT {
		using complex = std::complex<V>;
		size_t _size;
		std::vector<complex> workingVector;
		
		enum class StepType {
			generic, step2, step3, step4
		};
		struct Step {
			StepType type;
			size_t factor;
			size_t startIndex;
			size_t innerRepeats;
			size_t outerRepeats;
			size_t twiddleIndex;
		};
		std::vector<size_t> factors;
		std::vector<Step> plan;
		std::vector<complex> twiddleVector;
		
		struct PermutationPair {size_t from, to;};
		std::vector<PermutationPair> permutation;
		
		void addPlanSteps(size_t factorIndex, size_t start, size_t length, size_t repeats) {
			if (factorIndex >= factors.size()) return;
			
			size_t factor = factors[factorIndex];
			if (factorIndex + 1 < factors.size()) {
				if (factors[factorIndex] == 2 && factors[factorIndex + 1] == 2) {
					++factorIndex;
					factor = 4;
				}
			}

			size_t subLength = length/factor;
			Step mainStep{StepType::generic, factor, start, subLength, repeats, twiddleVector.size()};

			if (factor == 2) mainStep.type = StepType::step2;
			if (factor == 3) mainStep.type = StepType::step3;
			if (factor == 4) mainStep.type = StepType::step4;

			// Twiddles
			bool foundStep = false;
			for (const Step &existingStep : plan) {
				if (existingStep.factor == mainStep.factor && existingStep.innerRepeats == mainStep.innerRepeats) {
					foundStep = true;
					mainStep.twiddleIndex = existingStep.twiddleIndex;
					break;
				}
			}
			if (!foundStep) {
				for (size_t i = 0; i < subLength; ++i) {
					for (size_t f = 0; f < factor; ++f) {
						double phase = 2*M_PI*i*f/length;
						complex twiddle = {V(std::cos(phase)), V(-std::sin(phase))};
						twiddleVector.push_back(twiddle);
					}
				}
			}

			if (repeats == 1 && sizeof(complex)*subLength > 65536) {
				for (size_t i = 0; i < factor; ++i) {
					addPlanSteps(factorIndex + 1, start + i*subLength, subLength, 1);
				}
			} else {
				addPlanSteps(factorIndex + 1, start, subLength, repeats*factor);
			}
			plan.push_back(mainStep);
		}
		void setPlan() {
			factors.resize(0);
			size_t size = _size, factor = 2;
			while (size > 1) {
				if (size%factor == 0) {
					factors.push_back(factor);
					size /= factor;
				} else if (factor > sqrt(size)) {
					factor = size;
				} else {
					++factor;
				}
			}

			plan.resize(0);
			twiddleVector.resize(0);
			addPlanSteps(0, 0, _size, 1);
			
			permutation.resize(0);
			permutation.push_back(PermutationPair{0, 0});
			size_t indexLow = 0, indexHigh = factors.size();
			size_t inputStepLow = _size, outputStepLow = 1;
			size_t inputStepHigh = 1, outputStepHigh = _size;
			while (outputStepLow*inputStepHigh < _size) {
				size_t f, inputStep, outputStep;
				if (outputStepLow <= inputStepHigh) {
					f = factors[indexLow++];
					inputStep = (inputStepLow /= f);
					outputStep = outputStepLow;
					outputStepLow *= f;
				} else {
					f = factors[--indexHigh];
					inputStep = inputStepHigh;
					inputStepHigh *= f;
					outputStep = (outputStepHigh /= f);
				}
				size_t oldSize = permutation.size();
				for (size_t i = 1; i < f; ++i) {
					for (size_t j = 0; j < oldSize; ++j) {
						PermutationPair pair = permutation[j];
						pair.from += i*inputStep;
						pair.to += i*outputStep;
						permutation.push_back(pair);
					}
				}
			}
		}

		template<bool inverse, typename RandomAccessIterator>
		void fftStepGeneric(RandomAccessIterator &&origData, const Step &step) {
			complex *working = workingVector.data();
			const size_t stride = step.innerRepeats;

			for (size_t outerRepeat = 0; outerRepeat < step.outerRepeats; ++outerRepeat) {
				RandomAccessIterator data = origData;
				
				const complex *twiddles = twiddleVector.data() + step.twiddleIndex;
				const size_t factor = step.factor;
				for (size_t repeat = 0; repeat < step.innerRepeats; ++repeat) {
					for (size_t i = 0; i < step.factor; ++i) {
						working[i] = _fft_impl::complexMul<inverse>(data[i*stride], twiddles[i]);
					}
					for (size_t f = 0; f < factor; ++f) {
						complex sum = working[0];
						for (size_t i = 1; i < factor; ++i) {
							double phase = 2*M_PI*f*i/factor;
							complex twiddle = {V(std::cos(phase)), V(-std::sin(phase))};
							sum += _fft_impl::complexMul<inverse>(working[i], twiddle);
						}
						data[f*stride] = sum;
					}
					++data;
					twiddles += factor;
				}
				origData += step.factor*step.innerRepeats;
			}
		}

		template<bool inverse, typename RandomAccessIterator>
		SIGNALSMITH_INLINE void fftStep2(RandomAccessIterator &&origData, const Step &step) {
			const size_t stride = step.innerRepeats;
			const complex *origTwiddles = twiddleVector.data() + step.twiddleIndex;
			for (size_t outerRepeat = 0; outerRepeat < step.outerRepeats; ++outerRepeat) {
				const complex* twiddles = origTwiddles;
				for (RandomAccessIterator data = origData; data < origData + stride; ++data) {
					complex A = data[0];
					complex B = _fft_impl::complexMul<inverse>(data[stride], twiddles[1]);
					
					data[0] = A + B;
					data[stride] = A - B;
					twiddles += 2;
				}
				origData += 2*stride;
			}
		}

		template<bool inverse, typename RandomAccessIterator>
		SIGNALSMITH_INLINE void fftStep3(RandomAccessIterator &&origData, const Step &step) {
			constexpr complex factor3 = {-0.5, inverse ? 0.8660254037844386 : -0.8660254037844386};
			const size_t stride = step.innerRepeats;
			const complex *origTwiddles = twiddleVector.data() + step.twiddleIndex;
			
			for (size_t outerRepeat = 0; outerRepeat < step.outerRepeats; ++outerRepeat) {
				const complex* twiddles = origTwiddles;
				for (RandomAccessIterator data = origData; data < origData + stride; ++data) {
					complex A = data[0];
					complex B = _fft_impl::complexMul<inverse>(data[stride], twiddles[1]);
					complex C = _fft_impl::complexMul<inverse>(data[stride*2], twiddles[2]);
					
					complex realSum = A + (B + C)*factor3.real();
					complex imagSum = (B - C)*factor3.imag();

					data[0] = A + B + C;
					data[stride] = _fft_impl::complexAddI<false>(realSum, imagSum);
					data[stride*2] = _fft_impl::complexAddI<true>(realSum, imagSum);

					twiddles += 3;
				}
				origData += 3*stride;
			}
		}

		template<bool inverse, typename RandomAccessIterator>
		SIGNALSMITH_INLINE void fftStep4(RandomAccessIterator &&origData, const Step &step) {
			const size_t stride = step.innerRepeats;
			const complex *origTwiddles = twiddleVector.data() + step.twiddleIndex;
			
			for (size_t outerRepeat = 0; outerRepeat < step.outerRepeats; ++outerRepeat) {
				const complex* twiddles = origTwiddles;
				for (RandomAccessIterator data = origData; data < origData + stride; ++data) {
					complex A = data[0];
					complex C = _fft_impl::complexMul<inverse>(data[stride], twiddles[2]);
					complex B = _fft_impl::complexMul<inverse>(data[stride*2], twiddles[1]);
					complex D = _fft_impl::complexMul<inverse>(data[stride*3], twiddles[3]);

					complex sumAC = A + C, sumBD = B + D;
					complex diffAC = A - C, diffBD = B - D;

					data[0] = sumAC + sumBD;
					data[stride] = _fft_impl::complexAddI<!inverse>(diffAC, diffBD);
					data[stride*2] = sumAC - sumBD;
					data[stride*3] = _fft_impl::complexAddI<inverse>(diffAC, diffBD);

					twiddles += 4;
				}
				origData += 4*stride;
			}
		}
		
		template<typename InputIterator, typename OutputIterator>
		void permute(InputIterator input, OutputIterator data) {
			for (auto pair : permutation) {
				data[pair.from] = input[pair.to];
			}
		}

		template<bool inverse, typename InputIterator, typename OutputIterator>
		void run(InputIterator &&input, OutputIterator &&data) {
			permute(input, data);
			
			for (const Step &step : plan) {
				switch (step.type) {
					case StepType::generic:
						fftStepGeneric<inverse>(data + step.startIndex, step);
						break;
					case StepType::step2:
						fftStep2<inverse>(data + step.startIndex, step);
						break;
					case StepType::step3:
						fftStep3<inverse>(data + step.startIndex, step);
						break;
					case StepType::step4:
						fftStep4<inverse>(data + step.startIndex, step);
						break;
				}
			}
		}

		static bool validSize(size_t size) {
			constexpr static bool filter[32] = {
				1, 1, 1, 1, 1, 0, 1, 0, 1, 1, // 0-9
				0, 0, 1, 0, 0, 0, 1, 0, 1, 0, // 10-19
				0, 0, 0, 0, 1, 0, 0, 0, 0, 0, // 20-29
				0, 0
			};
			return filter[size];
		}
	public:
		static size_t fastSizeAbove(size_t size) {
			size_t power2 = 1;
			while (size >= 32) {
				size = (size - 1)/2 + 1;
				power2 *= 2;
			}
			while (size < 32 && !validSize(size)) {
				++size;
			}
			return power2*size;
		}
		static size_t fastSizeBelow(size_t size) {
			size_t power2 = 1;
			while (size >= 32) {
				size /= 2;
				power2 *= 2;
			}
			while (size > 1 && !validSize(size)) {
				--size;
			}
			return power2*size;
		}

		FFT(size_t size, int fastDirection=0) : _size(0) {
			if (fastDirection > 0) size = fastSizeAbove(size);
			if (fastDirection < 0) size = fastSizeBelow(size);
			this->setSize(size);
		}

		size_t setSize(size_t size) {
			if (size != _size) {
				_size = size;
				workingVector.resize(size);
				setPlan();
			}
			return _size;
		}
		size_t setFastSizeAbove(size_t size) {
			return setSize(fastSizeAbove(size));
		}
		size_t setFastSizeBelow(size_t size) {
			return setSize(fastSizeBelow(size));
		}
		const size_t & size() const {
			return _size;
		}

		template<typename InputIterator, typename OutputIterator>
		void fft(InputIterator &&input, OutputIterator &&output) {
			auto inputIter = _fft_impl::GetIterator<InputIterator>::get(input);
			auto outputIter = _fft_impl::GetIterator<OutputIterator>::get(output);
			return run<false>(inputIter, outputIter);
		}

		template<typename InputIterator, typename OutputIterator>
		void ifft(InputIterator &&input, OutputIterator &&output) {
			auto inputIter = _fft_impl::GetIterator<InputIterator>::get(input);
			auto outputIter = _fft_impl::GetIterator<OutputIterator>::get(output);
			return run<true>(inputIter, outputIter);
		}
	};

	struct FFTOptions {
		static constexpr int halfFreqShift = 1;
	};

	template<typename V, int optionFlags=0>
	class RealFFT {
		static constexpr bool modified = (optionFlags&FFTOptions::halfFreqShift);

		using complex = std::complex<V>;
		std::vector<complex> complexBuffer1, complexBuffer2;
		std::vector<complex> twiddlesMinusI;
		std::vector<complex> modifiedRotations;
		FFT<V> complexFft;
	public:
		static size_t fastSizeAbove(size_t size) {
			return FFT<V>::fastSizeAbove((size + 1)/2)*2;
		}
		static size_t fastSizeBelow(size_t size) {
			return FFT<V>::fastSizeBelow(size/2)*2;
		}

		RealFFT(size_t size=0, int fastDirection=0) : complexFft(0) {
			if (fastDirection > 0) size = fastSizeAbove(size);
			if (fastDirection < 0) size = fastSizeBelow(size);
			this->setSize(std::max<size_t>(size, 2));
		}

		size_t setSize(size_t size) {
			complexBuffer1.resize(size/2);
			complexBuffer2.resize(size/2);

			size_t hhSize = size/4 + 1;
			twiddlesMinusI.resize(hhSize);
			for (size_t i = 0; i < hhSize; ++i) {
				V rotPhase = -2*M_PI*(modified ? i + 0.5 : i)/size;
				twiddlesMinusI[i] = {std::sin(rotPhase), -std::cos(rotPhase)};
			}
			if (modified) {
				modifiedRotations.resize(size/2);
				for (size_t i = 0; i < size/2; ++i) {
					V rotPhase = -2*M_PI*i/size;
					modifiedRotations[i] = {std::cos(rotPhase), std::sin(rotPhase)};
				}
			}
			
			return complexFft.setSize(size/2);
		}
		size_t setFastSizeAbove(size_t size) {
			return setSize(fastSizeAbove(size));
		}
		size_t setFastSizeBelow(size_t size) {
			return setSize(fastSizeBelow(size));
		}
		size_t size() const {
			return complexFft.size()*2;
		}

		template<typename InputIterator, typename OutputIterator>
		void fft(InputIterator &&input, OutputIterator &&output) {
			size_t hSize = complexFft.size();
			for (size_t i = 0; i < hSize; ++i) {
				if (modified) {
					complexBuffer1[i] = _fft_impl::complexMul<false>({input[2*i], input[2*i + 1]}, modifiedRotations[i]);
				} else {
					complexBuffer1[i] = {input[2*i], input[2*i + 1]};
				}
			}
			
			complexFft.fft(complexBuffer1.data(), complexBuffer2.data());
			
			if (!modified) output[0] = {
				complexBuffer2[0].real() + complexBuffer2[0].imag(),
				complexBuffer2[0].real() - complexBuffer2[0].imag()
			};
			for (size_t i = modified ? 0 : 1; i <= hSize/2; ++i) {
				size_t conjI = modified ? (hSize  - 1 - i) : (hSize - i);
				
				complex odd = (complexBuffer2[i] + conj(complexBuffer2[conjI]))*(V)0.5;
				complex evenI = (complexBuffer2[i] - conj(complexBuffer2[conjI]))*(V)0.5;
				complex evenRotMinusI = _fft_impl::complexMul<false>(evenI, twiddlesMinusI[i]);

				output[i] = odd + evenRotMinusI;
				output[conjI] = conj(odd - evenRotMinusI);
			}
		}

		template<typename InputIterator, typename OutputIterator>
		void ifft(InputIterator &&input, OutputIterator &&output) {
			size_t hSize = complexFft.size();
			if (!modified) complexBuffer1[0] = {
				input[0].real() + input[0].imag(),
				input[0].real() - input[0].imag()
			};
			for (size_t i = modified ? 0 : 1; i <= hSize/2; ++i) {
				size_t conjI = modified ? (hSize  - 1 - i) : (hSize - i);
				complex v = input[i], v2 = input[conjI];

				complex odd = v + conj(v2);
				complex evenRotMinusI = v - conj(v2);
				complex evenI = _fft_impl::complexMul<true>(evenRotMinusI, twiddlesMinusI[i]);
				
				complexBuffer1[i] = odd + evenI;
				complexBuffer1[conjI] = conj(odd - evenI);
			}
			
			complexFft.ifft(complexBuffer1.data(), complexBuffer2.data());
			
			for (size_t i = 0; i < hSize; ++i) {
				complex v = complexBuffer2[i];
				if (modified) v = _fft_impl::complexMul<true>(v, modifiedRotations[i]);
				output[2*i] = v.real();
				output[2*i + 1] = v.imag();
			}
		}
	};

	template<typename V>
	struct ModifiedRealFFT : public RealFFT<V, FFTOptions::halfFreqShift> {
		using RealFFT<V, FFTOptions::halfFreqShift>::RealFFT;
	};

/// @}
}} // namespace
#endif // include guard

#ifndef SIGNALSMITH_DSP_WINDOWS_H
#define SIGNALSMITH_DSP_WINDOWS_H

#ifndef SIGNALSMITH_DSP_COMMON_H
#define SIGNALSMITH_DSP_COMMON_H

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

namespace signalsmith {
	/**	@defgroup Common Common
		@brief Definitions and helper classes used by the rest of the library
		
		@{
		@file
	*/

#define SIGNALSMITH_DSP_VERSION_MAJOR 1
#define SIGNALSMITH_DSP_VERSION_MINOR 3
#define SIGNALSMITH_DSP_VERSION_PATCH 3
#define SIGNALSMITH_DSP_VERSION_STRING "1.3.3"

	/** Version compatability check.
	\code{.cpp}
		static_assert(signalsmith::version(1, 0, 0), "version check");
	\endcode
	... or use the equivalent `SIGNALSMITH_DSP_VERSION_CHECK`.
	Major versions are not compatible with each other.  Minor and patch versions are backwards-compatible.
	*/
	constexpr bool versionCheck(int major, int minor, int patch=0) {
		return major == SIGNALSMITH_DSP_VERSION_MAJOR
			&& (SIGNALSMITH_DSP_VERSION_MINOR > minor
				|| (SIGNALSMITH_DSP_VERSION_MINOR == minor && SIGNALSMITH_DSP_VERSION_PATCH >= patch));
	}

/// Check the library version is compatible (semver).
#define SIGNALSMITH_DSP_VERSION_CHECK(major, minor, patch) \
	static_assert(::signalsmith::versionCheck(major, minor, patch), "signalsmith library version is " SIGNALSMITH_DSP_VERSION_STRING);

/** @} */
} // signalsmith::
#endif // include guard

#include <cmath>

namespace signalsmith {
namespace windows {
	/**	@defgroup Windows Window functions
		@brief Windows for spectral analysis
		
		These are generally double-precision, because they are mostly calculated during setup/reconfiguring, not real-time code.
		
		@{
		@file
	*/
	
	/** @brief The Kaiser window (almost) maximises the energy in the main-lobe compared to the side-lobes.
		
		Kaiser windows can be constructing using the shape-parameter (beta) or using the static `with???()` methods.*/
	class Kaiser {
		// I_0(x)=\sum_{k=0}^{N}\frac{x^{2k}}{(k!)^2\cdot4^k}
		inline static double bessel0(double x) {
			const double significanceLimit = 1e-4;
			double result = 0;
			double term = 1;
			double m = 0;
			while (term > significanceLimit) {
				result += term;
				++m;
				term *= (x*x)/(4*m*m);
			}

			return result;
		}
		double beta;
		double invB0;
		
		static double heuristicBandwidth(double bandwidth) {
			// Good peaks
			//return bandwidth + 8/((bandwidth + 3)*(bandwidth + 3));
			// Good average
			//return bandwidth + 14/((bandwidth + 2.5)*(bandwidth + 2.5));
			// Compromise
			return bandwidth + 8/((bandwidth + 3)*(bandwidth + 3)) + 0.25*std::max(3 - bandwidth, 0.0);
		}
	public:
		/// Set up a Kaiser window with a given shape.  `beta` is `pi*alpha` (since there is ambiguity about shape parameters)
		Kaiser(double beta) : beta(beta), invB0(1/bessel0(beta)) {}

		/// @name Bandwidth methods
		/// @{
		static Kaiser withBandwidth(double bandwidth, bool heuristicOptimal=false) {
			return Kaiser(bandwidthToBeta(bandwidth, heuristicOptimal));
		}

		/** Returns the Kaiser shape where the main lobe has the specified bandwidth (as a factor of 1/window-length).
		\diagram{kaiser-windows.svg,You can see that the main lobe matches the specified bandwidth.}
		If `heuristicOptimal` is enabled, the main lobe width is _slightly_ wider, improving both the peak and total energy - see `bandwidthToEnergyDb()` and `bandwidthToPeakDb()`.
		\diagram{kaiser-windows-heuristic.svg, The main lobe extends to ±bandwidth/2.} */
		static double bandwidthToBeta(double bandwidth, bool heuristicOptimal=false) {
			if (heuristicOptimal) { // Heuristic based on numerical search
				bandwidth = heuristicBandwidth(bandwidth);
			}
			bandwidth = std::max(bandwidth, 2.0);
			double alpha = std::sqrt(bandwidth*bandwidth*0.25 - 1);
			return alpha*M_PI;
		}
		
		static double betaToBandwidth(double beta) {
			double alpha = beta*(1.0/M_PI);
			return 2*std::sqrt(alpha*alpha + 1);
		}
		/// @}

		/// @name Performance methods
		/// @{
		/** @brief Total energy ratio (in dB) between side-lobes and the main lobe.
			\diagram{kaiser-bandwidth-sidelobes-energy.svg,Measured main/side lobe energy ratio.  You can see that the heuristic improves performance for all bandwidth values.}
			This function uses an approximation which is accurate to ±0.5dB for 2 ⩽ bandwidth ≤ 10, or 1 ⩽ bandwidth ≤ 10 when `heuristicOptimal`is enabled.
		*/
		static double bandwidthToEnergyDb(double bandwidth, bool heuristicOptimal=false) {
			// Horrible heuristic fits
			if (heuristicOptimal) {
				if (bandwidth < 3) bandwidth += (3 - bandwidth)*0.5;
				return 12.9 + -3/(bandwidth + 0.4) - 13.4*bandwidth + (bandwidth < 3)*-9.6*(bandwidth - 3);
			}
			return 10.5 + 15/(bandwidth + 0.4) - 13.25*bandwidth + (bandwidth < 2)*13*(bandwidth - 2);
		}
		static double energyDbToBandwidth(double energyDb, bool heuristicOptimal=false) {
			double bw = 1;
			while (bw < 20 && bandwidthToEnergyDb(bw, heuristicOptimal) > energyDb) {
				bw *= 2;
			}
			double step = bw/2;
			while (step > 0.0001) {
				if (bandwidthToEnergyDb(bw, heuristicOptimal) > energyDb) {
					bw += step;
				} else {
					bw -= step;
				}
				step *= 0.5;
			}
			return bw;
		}
		/** @brief Peak ratio (in dB) between side-lobes and the main lobe.
			\diagram{kaiser-bandwidth-sidelobes-peak.svg,Measured main/side lobe peak ratio.  You can see that the heuristic improves performance, except in the bandwidth range 1-2 where peak ratio was sacrificed to improve total energy ratio.}
			This function uses an approximation which is accurate to ±0.5dB for 2 ⩽ bandwidth ≤ 9, or 0.5 ⩽ bandwidth ≤ 9 when `heuristicOptimal`is enabled.
		*/
		static double bandwidthToPeakDb(double bandwidth, bool heuristicOptimal=false) {
			// Horrible heuristic fits
			if (heuristicOptimal) {
				return 14.2 - 20/(bandwidth + 1) - 13*bandwidth + (bandwidth < 3)*-6*(bandwidth - 3) + (bandwidth < 2.25)*5.8*(bandwidth - 2.25);
			}
			return 10 + 8/(bandwidth + 2) - 12.75*bandwidth + (bandwidth < 2)*4*(bandwidth - 2);
		}
		static double peakDbToBandwidth(double peakDb, bool heuristicOptimal=false) {
			double bw = 1;
			while (bw < 20 && bandwidthToPeakDb(bw, heuristicOptimal) > peakDb) {
				bw *= 2;
			}
			double step = bw/2;
			while (step > 0.0001) {
				if (bandwidthToPeakDb(bw, heuristicOptimal) > peakDb) {
					bw += step;
				} else {
					bw -= step;
				}
				step *= 0.5;
			}
			return bw;
		}
		/** @} */

		/** Equivalent noise bandwidth (ENBW), a measure of frequency resolution.
			\diagram{kaiser-bandwidth-enbw.svg,Measured ENBW, with and without the heuristic bandwidth adjustment.}
			This approximation is accurate to ±0.05 up to a bandwidth of 22.
		*/
		static double bandwidthToEnbw(double bandwidth, bool heuristicOptimal=false) {
			if (heuristicOptimal) bandwidth = heuristicBandwidth(bandwidth);
			double b2 = std::max<double>(bandwidth - 2, 0);
			return 1 + b2*(0.2 + b2*(-0.005 + b2*(-0.000005 + b2*0.0000022)));
		}

		/// Return the window's value for position in the range [0, 1]
		double operator ()(double unit) {
			double r = 2*unit - 1;
			double arg = std::sqrt(1 - r*r);
			return bessel0(beta*arg)*invB0;
		}
	
		/// Fills an arbitrary container with a Kaiser window
		template<typename Data>
		void fill(Data &data, int size) const {
			double invSize = 1.0/size;
			for (int i = 0; i < size; ++i) {
				double r = (2*i + 1)*invSize - 1;
				double arg = std::sqrt(1 - r*r);
				data[i] = bessel0(beta*arg)*invB0;
			}
		}
	};

	/** Forces STFT perfect-reconstruction (WOLA) on an existing window, for a given STFT interval.
	For example, here are perfect-reconstruction versions of the approximately-optimal @ref Kaiser windows:
	\diagram{kaiser-windows-heuristic-pr.svg,Note the lower overall energy\, and the pointy top for 2x bandwidth. Spectral performance is about the same\, though.}
	*/
	template<typename Data>
	void forcePerfectReconstruction(Data &data, int windowLength, int interval) {
		for (int i = 0; i < interval; ++i) {
			double sum2 = 0;
			for (int index = i; index < windowLength; index += interval) {
				sum2 += data[index]*data[index];
			}
			double factor = 1/std::sqrt(sum2);
			for (int index = i; index < windowLength; index += interval) {
				data[index] *= factor;
			}
		}
	}

/** @} */
}} // signalsmith::windows
#endif // include guard

#ifndef SIGNALSMITH_DSP_DELAY_H
#define SIGNALSMITH_DSP_DELAY_H

#ifndef SIGNALSMITH_DSP_COMMON_H
#define SIGNALSMITH_DSP_COMMON_H

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

namespace signalsmith {
	/**	@defgroup Common Common
		@brief Definitions and helper classes used by the rest of the library
		
		@{
		@file
	*/

#define SIGNALSMITH_DSP_VERSION_MAJOR 1
#define SIGNALSMITH_DSP_VERSION_MINOR 3
#define SIGNALSMITH_DSP_VERSION_PATCH 3
#define SIGNALSMITH_DSP_VERSION_STRING "1.3.3"

	/** Version compatability check.
	\code{.cpp}
		static_assert(signalsmith::version(1, 0, 0), "version check");
	\endcode
	... or use the equivalent `SIGNALSMITH_DSP_VERSION_CHECK`.
	Major versions are not compatible with each other.  Minor and patch versions are backwards-compatible.
	*/
	constexpr bool versionCheck(int major, int minor, int patch=0) {
		return major == SIGNALSMITH_DSP_VERSION_MAJOR
			&& (SIGNALSMITH_DSP_VERSION_MINOR > minor
				|| (SIGNALSMITH_DSP_VERSION_MINOR == minor && SIGNALSMITH_DSP_VERSION_PATCH >= patch));
	}

/// Check the library version is compatible (semver).
#define SIGNALSMITH_DSP_VERSION_CHECK(major, minor, patch) \
	static_assert(::signalsmith::versionCheck(major, minor, patch), "signalsmith library version is " SIGNALSMITH_DSP_VERSION_STRING);

/** @} */
} // signalsmith::
#endif // include guard

#include <vector>
#include <array>
#include <cmath> // for std::ceil()
#include <type_traits>

#include <complex>
#ifndef SIGNALSMITH_FFT_V5
#define SIGNALSMITH_FFT_V5

#ifndef SIGNALSMITH_DSP_COMMON_H
#define SIGNALSMITH_DSP_COMMON_H

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

namespace signalsmith {
	/**	@defgroup Common Common
		@brief Definitions and helper classes used by the rest of the library
		
		@{
		@file
	*/

#define SIGNALSMITH_DSP_VERSION_MAJOR 1
#define SIGNALSMITH_DSP_VERSION_MINOR 3
#define SIGNALSMITH_DSP_VERSION_PATCH 3
#define SIGNALSMITH_DSP_VERSION_STRING "1.3.3"

	/** Version compatability check.
	\code{.cpp}
		static_assert(signalsmith::version(1, 0, 0), "version check");
	\endcode
	... or use the equivalent `SIGNALSMITH_DSP_VERSION_CHECK`.
	Major versions are not compatible with each other.  Minor and patch versions are backwards-compatible.
	*/
	constexpr bool versionCheck(int major, int minor, int patch=0) {
		return major == SIGNALSMITH_DSP_VERSION_MAJOR
			&& (SIGNALSMITH_DSP_VERSION_MINOR > minor
				|| (SIGNALSMITH_DSP_VERSION_MINOR == minor && SIGNALSMITH_DSP_VERSION_PATCH >= patch));
	}

/// Check the library version is compatible (semver).
#define SIGNALSMITH_DSP_VERSION_CHECK(major, minor, patch) \
	static_assert(::signalsmith::versionCheck(major, minor, patch), "signalsmith library version is " SIGNALSMITH_DSP_VERSION_STRING);

/** @} */
} // signalsmith::
#endif // include guard

#ifndef SIGNALSMITH_DSP_PERF_H
#define SIGNALSMITH_DSP_PERF_H

#include <complex>

namespace signalsmith {
namespace perf {
	/**	@defgroup Performance Performance helpers
		@brief Nothing serious, just some `#defines` and helpers
		
		@{
		@file
	*/
		
	/// *Really* insist that a function/method is inlined (mostly for performance in DEBUG builds)
	#ifndef SIGNALSMITH_INLINE
	#ifdef __GNUC__
	#define SIGNALSMITH_INLINE __attribute__((always_inline)) inline
	#elif defined(__MSVC__)
	#define SIGNALSMITH_INLINE __forceinline inline
	#else
	#define SIGNALSMITH_INLINE inline
	#endif
	#endif

	/** @brief Complex-multiplication (with optional conjugate second-arg), without handling NaN/Infinity
		The `std::complex` multiplication has edge-cases around NaNs which slow things down and prevent auto-vectorisation.
	*/
	template <bool conjugateSecond=false, typename V>
	SIGNALSMITH_INLINE static std::complex<V> mul(const std::complex<V> &a, const std::complex<V> &b) {
		return conjugateSecond ? std::complex<V>{
			b.real()*a.real() + b.imag()*a.imag(),
			b.real()*a.imag() - b.imag()*a.real()
		} : std::complex<V>{
			a.real()*b.real() - a.imag()*b.imag(),
			a.real()*b.imag() + a.imag()*b.real()
		};
	}

/** @} */
}} // signalsmith::perf::

#endif // include guard

#include <vector>
#include <complex>
#include <cmath>

namespace signalsmith { namespace fft {
	/**	@defgroup FFT FFT (complex and real)
		@brief Fourier transforms (complex and real)

		@{
		@file
	*/

	namespace _fft_impl {

		template <typename V>
		SIGNALSMITH_INLINE V complexReal(const std::complex<V> &c) {
			return ((V*)(&c))[0];
		}
		template <typename V>
		SIGNALSMITH_INLINE V complexImag(const std::complex<V> &c) {
			return ((V*)(&c))[1];
		}

		// Complex multiplication has edge-cases around Inf/NaN - handling those properly makes std::complex non-inlineable, so we use our own
		template <bool conjugateSecond, typename V>
		SIGNALSMITH_INLINE std::complex<V> complexMul(const std::complex<V> &a, const std::complex<V> &b) {
			V aReal = complexReal(a), aImag = complexImag(a);
			V bReal = complexReal(b), bImag = complexImag(b);
			return conjugateSecond ? std::complex<V>{
				bReal*aReal + bImag*aImag,
				bReal*aImag - bImag*aReal
			} : std::complex<V>{
				aReal*bReal - aImag*bImag,
				aReal*bImag + aImag*bReal
			};
		}

		template<bool flipped, typename V>
		SIGNALSMITH_INLINE std::complex<V> complexAddI(const std::complex<V> &a, const std::complex<V> &b) {
			V aReal = complexReal(a), aImag = complexImag(a);
			V bReal = complexReal(b), bImag = complexImag(b);
			return flipped ? std::complex<V>{
				aReal + bImag,
				aImag - bReal
			} : std::complex<V>{
				aReal - bImag,
				aImag + bReal
			};
		}

		// Use SFINAE to get an iterator from std::begin(), if supported - otherwise assume the value itself is an iterator
		template<typename T, typename=void>
		struct GetIterator {
			static T get(const T &t) {
				return t;
			}
		};
		template<typename T>
		struct GetIterator<T, decltype((void)std::begin(std::declval<T>()))> {
			static auto get(const T &t) -> decltype(std::begin(t)) {
				return std::begin(t);
			}
		};
	}

	/** Floating-point FFT implementation.
	It is fast for 2^a * 3^b.
	Here are the peak and RMS errors for `float`/`double` computation:
	\diagram{fft-errors.svg Simulated errors for pure-tone harmonic inputs\, compared to a theoretical upper bound from "Roundoff error analysis of the fast Fourier transform" (G. Ramos, 1971)}
	*/
	template<typename V=double>
	class FFT {
		using complex = std::complex<V>;
		size_t _size;
		std::vector<complex> workingVector;
		
		enum class StepType {
			generic, step2, step3, step4
		};
		struct Step {
			StepType type;
			size_t factor;
			size_t startIndex;
			size_t innerRepeats;
			size_t outerRepeats;
			size_t twiddleIndex;
		};
		std::vector<size_t> factors;
		std::vector<Step> plan;
		std::vector<complex> twiddleVector;
		
		struct PermutationPair {size_t from, to;};
		std::vector<PermutationPair> permutation;
		
		void addPlanSteps(size_t factorIndex, size_t start, size_t length, size_t repeats) {
			if (factorIndex >= factors.size()) return;
			
			size_t factor = factors[factorIndex];
			if (factorIndex + 1 < factors.size()) {
				if (factors[factorIndex] == 2 && factors[factorIndex + 1] == 2) {
					++factorIndex;
					factor = 4;
				}
			}

			size_t subLength = length/factor;
			Step mainStep{StepType::generic, factor, start, subLength, repeats, twiddleVector.size()};

			if (factor == 2) mainStep.type = StepType::step2;
			if (factor == 3) mainStep.type = StepType::step3;
			if (factor == 4) mainStep.type = StepType::step4;

			// Twiddles
			bool foundStep = false;
			for (const Step &existingStep : plan) {
				if (existingStep.factor == mainStep.factor && existingStep.innerRepeats == mainStep.innerRepeats) {
					foundStep = true;
					mainStep.twiddleIndex = existingStep.twiddleIndex;
					break;
				}
			}
			if (!foundStep) {
				for (size_t i = 0; i < subLength; ++i) {
					for (size_t f = 0; f < factor; ++f) {
						double phase = 2*M_PI*i*f/length;
						complex twiddle = {V(std::cos(phase)), V(-std::sin(phase))};
						twiddleVector.push_back(twiddle);
					}
				}
			}

			if (repeats == 1 && sizeof(complex)*subLength > 65536) {
				for (size_t i = 0; i < factor; ++i) {
					addPlanSteps(factorIndex + 1, start + i*subLength, subLength, 1);
				}
			} else {
				addPlanSteps(factorIndex + 1, start, subLength, repeats*factor);
			}
			plan.push_back(mainStep);
		}
		void setPlan() {
			factors.resize(0);
			size_t size = _size, factor = 2;
			while (size > 1) {
				if (size%factor == 0) {
					factors.push_back(factor);
					size /= factor;
				} else if (factor > sqrt(size)) {
					factor = size;
				} else {
					++factor;
				}
			}

			plan.resize(0);
			twiddleVector.resize(0);
			addPlanSteps(0, 0, _size, 1);
			
			permutation.resize(0);
			permutation.push_back(PermutationPair{0, 0});
			size_t indexLow = 0, indexHigh = factors.size();
			size_t inputStepLow = _size, outputStepLow = 1;
			size_t inputStepHigh = 1, outputStepHigh = _size;
			while (outputStepLow*inputStepHigh < _size) {
				size_t f, inputStep, outputStep;
				if (outputStepLow <= inputStepHigh) {
					f = factors[indexLow++];
					inputStep = (inputStepLow /= f);
					outputStep = outputStepLow;
					outputStepLow *= f;
				} else {
					f = factors[--indexHigh];
					inputStep = inputStepHigh;
					inputStepHigh *= f;
					outputStep = (outputStepHigh /= f);
				}
				size_t oldSize = permutation.size();
				for (size_t i = 1; i < f; ++i) {
					for (size_t j = 0; j < oldSize; ++j) {
						PermutationPair pair = permutation[j];
						pair.from += i*inputStep;
						pair.to += i*outputStep;
						permutation.push_back(pair);
					}
				}
			}
		}

		template<bool inverse, typename RandomAccessIterator>
		void fftStepGeneric(RandomAccessIterator &&origData, const Step &step) {
			complex *working = workingVector.data();
			const size_t stride = step.innerRepeats;

			for (size_t outerRepeat = 0; outerRepeat < step.outerRepeats; ++outerRepeat) {
				RandomAccessIterator data = origData;
				
				const complex *twiddles = twiddleVector.data() + step.twiddleIndex;
				const size_t factor = step.factor;
				for (size_t repeat = 0; repeat < step.innerRepeats; ++repeat) {
					for (size_t i = 0; i < step.factor; ++i) {
						working[i] = _fft_impl::complexMul<inverse>(data[i*stride], twiddles[i]);
					}
					for (size_t f = 0; f < factor; ++f) {
						complex sum = working[0];
						for (size_t i = 1; i < factor; ++i) {
							double phase = 2*M_PI*f*i/factor;
							complex twiddle = {V(std::cos(phase)), V(-std::sin(phase))};
							sum += _fft_impl::complexMul<inverse>(working[i], twiddle);
						}
						data[f*stride] = sum;
					}
					++data;
					twiddles += factor;
				}
				origData += step.factor*step.innerRepeats;
			}
		}

		template<bool inverse, typename RandomAccessIterator>
		SIGNALSMITH_INLINE void fftStep2(RandomAccessIterator &&origData, const Step &step) {
			const size_t stride = step.innerRepeats;
			const complex *origTwiddles = twiddleVector.data() + step.twiddleIndex;
			for (size_t outerRepeat = 0; outerRepeat < step.outerRepeats; ++outerRepeat) {
				const complex* twiddles = origTwiddles;
				for (RandomAccessIterator data = origData; data < origData + stride; ++data) {
					complex A = data[0];
					complex B = _fft_impl::complexMul<inverse>(data[stride], twiddles[1]);
					
					data[0] = A + B;
					data[stride] = A - B;
					twiddles += 2;
				}
				origData += 2*stride;
			}
		}

		template<bool inverse, typename RandomAccessIterator>
		SIGNALSMITH_INLINE void fftStep3(RandomAccessIterator &&origData, const Step &step) {
			constexpr complex factor3 = {-0.5, inverse ? 0.8660254037844386 : -0.8660254037844386};
			const size_t stride = step.innerRepeats;
			const complex *origTwiddles = twiddleVector.data() + step.twiddleIndex;
			
			for (size_t outerRepeat = 0; outerRepeat < step.outerRepeats; ++outerRepeat) {
				const complex* twiddles = origTwiddles;
				for (RandomAccessIterator data = origData; data < origData + stride; ++data) {
					complex A = data[0];
					complex B = _fft_impl::complexMul<inverse>(data[stride], twiddles[1]);
					complex C = _fft_impl::complexMul<inverse>(data[stride*2], twiddles[2]);
					
					complex realSum = A + (B + C)*factor3.real();
					complex imagSum = (B - C)*factor3.imag();

					data[0] = A + B + C;
					data[stride] = _fft_impl::complexAddI<false>(realSum, imagSum);
					data[stride*2] = _fft_impl::complexAddI<true>(realSum, imagSum);

					twiddles += 3;
				}
				origData += 3*stride;
			}
		}

		template<bool inverse, typename RandomAccessIterator>
		SIGNALSMITH_INLINE void fftStep4(RandomAccessIterator &&origData, const Step &step) {
			const size_t stride = step.innerRepeats;
			const complex *origTwiddles = twiddleVector.data() + step.twiddleIndex;
			
			for (size_t outerRepeat = 0; outerRepeat < step.outerRepeats; ++outerRepeat) {
				const complex* twiddles = origTwiddles;
				for (RandomAccessIterator data = origData; data < origData + stride; ++data) {
					complex A = data[0];
					complex C = _fft_impl::complexMul<inverse>(data[stride], twiddles[2]);
					complex B = _fft_impl::complexMul<inverse>(data[stride*2], twiddles[1]);
					complex D = _fft_impl::complexMul<inverse>(data[stride*3], twiddles[3]);

					complex sumAC = A + C, sumBD = B + D;
					complex diffAC = A - C, diffBD = B - D;

					data[0] = sumAC + sumBD;
					data[stride] = _fft_impl::complexAddI<!inverse>(diffAC, diffBD);
					data[stride*2] = sumAC - sumBD;
					data[stride*3] = _fft_impl::complexAddI<inverse>(diffAC, diffBD);

					twiddles += 4;
				}
				origData += 4*stride;
			}
		}
		
		template<typename InputIterator, typename OutputIterator>
		void permute(InputIterator input, OutputIterator data) {
			for (auto pair : permutation) {
				data[pair.from] = input[pair.to];
			}
		}

		template<bool inverse, typename InputIterator, typename OutputIterator>
		void run(InputIterator &&input, OutputIterator &&data) {
			permute(input, data);
			
			for (const Step &step : plan) {
				switch (step.type) {
					case StepType::generic:
						fftStepGeneric<inverse>(data + step.startIndex, step);
						break;
					case StepType::step2:
						fftStep2<inverse>(data + step.startIndex, step);
						break;
					case StepType::step3:
						fftStep3<inverse>(data + step.startIndex, step);
						break;
					case StepType::step4:
						fftStep4<inverse>(data + step.startIndex, step);
						break;
				}
			}
		}

		static bool validSize(size_t size) {
			constexpr static bool filter[32] = {
				1, 1, 1, 1, 1, 0, 1, 0, 1, 1, // 0-9
				0, 0, 1, 0, 0, 0, 1, 0, 1, 0, // 10-19
				0, 0, 0, 0, 1, 0, 0, 0, 0, 0, // 20-29
				0, 0
			};
			return filter[size];
		}
	public:
		static size_t fastSizeAbove(size_t size) {
			size_t power2 = 1;
			while (size >= 32) {
				size = (size - 1)/2 + 1;
				power2 *= 2;
			}
			while (size < 32 && !validSize(size)) {
				++size;
			}
			return power2*size;
		}
		static size_t fastSizeBelow(size_t size) {
			size_t power2 = 1;
			while (size >= 32) {
				size /= 2;
				power2 *= 2;
			}
			while (size > 1 && !validSize(size)) {
				--size;
			}
			return power2*size;
		}

		FFT(size_t size, int fastDirection=0) : _size(0) {
			if (fastDirection > 0) size = fastSizeAbove(size);
			if (fastDirection < 0) size = fastSizeBelow(size);
			this->setSize(size);
		}

		size_t setSize(size_t size) {
			if (size != _size) {
				_size = size;
				workingVector.resize(size);
				setPlan();
			}
			return _size;
		}
		size_t setFastSizeAbove(size_t size) {
			return setSize(fastSizeAbove(size));
		}
		size_t setFastSizeBelow(size_t size) {
			return setSize(fastSizeBelow(size));
		}
		const size_t & size() const {
			return _size;
		}

		template<typename InputIterator, typename OutputIterator>
		void fft(InputIterator &&input, OutputIterator &&output) {
			auto inputIter = _fft_impl::GetIterator<InputIterator>::get(input);
			auto outputIter = _fft_impl::GetIterator<OutputIterator>::get(output);
			return run<false>(inputIter, outputIter);
		}

		template<typename InputIterator, typename OutputIterator>
		void ifft(InputIterator &&input, OutputIterator &&output) {
			auto inputIter = _fft_impl::GetIterator<InputIterator>::get(input);
			auto outputIter = _fft_impl::GetIterator<OutputIterator>::get(output);
			return run<true>(inputIter, outputIter);
		}
	};

	struct FFTOptions {
		static constexpr int halfFreqShift = 1;
	};

	template<typename V, int optionFlags=0>
	class RealFFT {
		static constexpr bool modified = (optionFlags&FFTOptions::halfFreqShift);

		using complex = std::complex<V>;
		std::vector<complex> complexBuffer1, complexBuffer2;
		std::vector<complex> twiddlesMinusI;
		std::vector<complex> modifiedRotations;
		FFT<V> complexFft;
	public:
		static size_t fastSizeAbove(size_t size) {
			return FFT<V>::fastSizeAbove((size + 1)/2)*2;
		}
		static size_t fastSizeBelow(size_t size) {
			return FFT<V>::fastSizeBelow(size/2)*2;
		}

		RealFFT(size_t size=0, int fastDirection=0) : complexFft(0) {
			if (fastDirection > 0) size = fastSizeAbove(size);
			if (fastDirection < 0) size = fastSizeBelow(size);
			this->setSize(std::max<size_t>(size, 2));
		}

		size_t setSize(size_t size) {
			complexBuffer1.resize(size/2);
			complexBuffer2.resize(size/2);

			size_t hhSize = size/4 + 1;
			twiddlesMinusI.resize(hhSize);
			for (size_t i = 0; i < hhSize; ++i) {
				V rotPhase = -2*M_PI*(modified ? i + 0.5 : i)/size;
				twiddlesMinusI[i] = {std::sin(rotPhase), -std::cos(rotPhase)};
			}
			if (modified) {
				modifiedRotations.resize(size/2);
				for (size_t i = 0; i < size/2; ++i) {
					V rotPhase = -2*M_PI*i/size;
					modifiedRotations[i] = {std::cos(rotPhase), std::sin(rotPhase)};
				}
			}
			
			return complexFft.setSize(size/2);
		}
		size_t setFastSizeAbove(size_t size) {
			return setSize(fastSizeAbove(size));
		}
		size_t setFastSizeBelow(size_t size) {
			return setSize(fastSizeBelow(size));
		}
		size_t size() const {
			return complexFft.size()*2;
		}

		template<typename InputIterator, typename OutputIterator>
		void fft(InputIterator &&input, OutputIterator &&output) {
			size_t hSize = complexFft.size();
			for (size_t i = 0; i < hSize; ++i) {
				if (modified) {
					complexBuffer1[i] = _fft_impl::complexMul<false>({input[2*i], input[2*i + 1]}, modifiedRotations[i]);
				} else {
					complexBuffer1[i] = {input[2*i], input[2*i + 1]};
				}
			}
			
			complexFft.fft(complexBuffer1.data(), complexBuffer2.data());
			
			if (!modified) output[0] = {
				complexBuffer2[0].real() + complexBuffer2[0].imag(),
				complexBuffer2[0].real() - complexBuffer2[0].imag()
			};
			for (size_t i = modified ? 0 : 1; i <= hSize/2; ++i) {
				size_t conjI = modified ? (hSize  - 1 - i) : (hSize - i);
				
				complex odd = (complexBuffer2[i] + conj(complexBuffer2[conjI]))*(V)0.5;
				complex evenI = (complexBuffer2[i] - conj(complexBuffer2[conjI]))*(V)0.5;
				complex evenRotMinusI = _fft_impl::complexMul<false>(evenI, twiddlesMinusI[i]);

				output[i] = odd + evenRotMinusI;
				output[conjI] = conj(odd - evenRotMinusI);
			}
		}

		template<typename InputIterator, typename OutputIterator>
		void ifft(InputIterator &&input, OutputIterator &&output) {
			size_t hSize = complexFft.size();
			if (!modified) complexBuffer1[0] = {
				input[0].real() + input[0].imag(),
				input[0].real() - input[0].imag()
			};
			for (size_t i = modified ? 0 : 1; i <= hSize/2; ++i) {
				size_t conjI = modified ? (hSize  - 1 - i) : (hSize - i);
				complex v = input[i], v2 = input[conjI];

				complex odd = v + conj(v2);
				complex evenRotMinusI = v - conj(v2);
				complex evenI = _fft_impl::complexMul<true>(evenRotMinusI, twiddlesMinusI[i]);
				
				complexBuffer1[i] = odd + evenI;
				complexBuffer1[conjI] = conj(odd - evenI);
			}
			
			complexFft.ifft(complexBuffer1.data(), complexBuffer2.data());
			
			for (size_t i = 0; i < hSize; ++i) {
				complex v = complexBuffer2[i];
				if (modified) v = _fft_impl::complexMul<true>(v, modifiedRotations[i]);
				output[2*i] = v.real();
				output[2*i + 1] = v.imag();
			}
		}
	};

	template<typename V>
	struct ModifiedRealFFT : public RealFFT<V, FFTOptions::halfFreqShift> {
		using RealFFT<V, FFTOptions::halfFreqShift>::RealFFT;
	};

/// @}
}} // namespace
#endif // include guard

#ifndef SIGNALSMITH_DSP_WINDOWS_H
#define SIGNALSMITH_DSP_WINDOWS_H

#ifndef SIGNALSMITH_DSP_COMMON_H
#define SIGNALSMITH_DSP_COMMON_H

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

namespace signalsmith {
	/**	@defgroup Common Common
		@brief Definitions and helper classes used by the rest of the library
		
		@{
		@file
	*/

#define SIGNALSMITH_DSP_VERSION_MAJOR 1
#define SIGNALSMITH_DSP_VERSION_MINOR 3
#define SIGNALSMITH_DSP_VERSION_PATCH 3
#define SIGNALSMITH_DSP_VERSION_STRING "1.3.3"

	/** Version compatability check.
	\code{.cpp}
		static_assert(signalsmith::version(1, 0, 0), "version check");
	\endcode
	... or use the equivalent `SIGNALSMITH_DSP_VERSION_CHECK`.
	Major versions are not compatible with each other.  Minor and patch versions are backwards-compatible.
	*/
	constexpr bool versionCheck(int major, int minor, int patch=0) {
		return major == SIGNALSMITH_DSP_VERSION_MAJOR
			&& (SIGNALSMITH_DSP_VERSION_MINOR > minor
				|| (SIGNALSMITH_DSP_VERSION_MINOR == minor && SIGNALSMITH_DSP_VERSION_PATCH >= patch));
	}

/// Check the library version is compatible (semver).
#define SIGNALSMITH_DSP_VERSION_CHECK(major, minor, patch) \
	static_assert(::signalsmith::versionCheck(major, minor, patch), "signalsmith library version is " SIGNALSMITH_DSP_VERSION_STRING);

/** @} */
} // signalsmith::
#endif // include guard

#include <cmath>

namespace signalsmith {
namespace windows {
	/**	@defgroup Windows Window functions
		@brief Windows for spectral analysis
		
		These are generally double-precision, because they are mostly calculated during setup/reconfiguring, not real-time code.
		
		@{
		@file
	*/
	
	/** @brief The Kaiser window (almost) maximises the energy in the main-lobe compared to the side-lobes.
		
		Kaiser windows can be constructing using the shape-parameter (beta) or using the static `with???()` methods.*/
	class Kaiser {
		// I_0(x)=\sum_{k=0}^{N}\frac{x^{2k}}{(k!)^2\cdot4^k}
		inline static double bessel0(double x) {
			const double significanceLimit = 1e-4;
			double result = 0;
			double term = 1;
			double m = 0;
			while (term > significanceLimit) {
				result += term;
				++m;
				term *= (x*x)/(4*m*m);
			}

			return result;
		}
		double beta;
		double invB0;
		
		static double heuristicBandwidth(double bandwidth) {
			// Good peaks
			//return bandwidth + 8/((bandwidth + 3)*(bandwidth + 3));
			// Good average
			//return bandwidth + 14/((bandwidth + 2.5)*(bandwidth + 2.5));
			// Compromise
			return bandwidth + 8/((bandwidth + 3)*(bandwidth + 3)) + 0.25*std::max(3 - bandwidth, 0.0);
		}
	public:
		/// Set up a Kaiser window with a given shape.  `beta` is `pi*alpha` (since there is ambiguity about shape parameters)
		Kaiser(double beta) : beta(beta), invB0(1/bessel0(beta)) {}

		/// @name Bandwidth methods
		/// @{
		static Kaiser withBandwidth(double bandwidth, bool heuristicOptimal=false) {
			return Kaiser(bandwidthToBeta(bandwidth, heuristicOptimal));
		}

		/** Returns the Kaiser shape where the main lobe has the specified bandwidth (as a factor of 1/window-length).
		\diagram{kaiser-windows.svg,You can see that the main lobe matches the specified bandwidth.}
		If `heuristicOptimal` is enabled, the main lobe width is _slightly_ wider, improving both the peak and total energy - see `bandwidthToEnergyDb()` and `bandwidthToPeakDb()`.
		\diagram{kaiser-windows-heuristic.svg, The main lobe extends to ±bandwidth/2.} */
		static double bandwidthToBeta(double bandwidth, bool heuristicOptimal=false) {
			if (heuristicOptimal) { // Heuristic based on numerical search
				bandwidth = heuristicBandwidth(bandwidth);
			}
			bandwidth = std::max(bandwidth, 2.0);
			double alpha = std::sqrt(bandwidth*bandwidth*0.25 - 1);
			return alpha*M_PI;
		}
		
		static double betaToBandwidth(double beta) {
			double alpha = beta*(1.0/M_PI);
			return 2*std::sqrt(alpha*alpha + 1);
		}
		/// @}

		/// @name Performance methods
		/// @{
		/** @brief Total energy ratio (in dB) between side-lobes and the main lobe.
			\diagram{kaiser-bandwidth-sidelobes-energy.svg,Measured main/side lobe energy ratio.  You can see that the heuristic improves performance for all bandwidth values.}
			This function uses an approximation which is accurate to ±0.5dB for 2 ⩽ bandwidth ≤ 10, or 1 ⩽ bandwidth ≤ 10 when `heuristicOptimal`is enabled.
		*/
		static double bandwidthToEnergyDb(double bandwidth, bool heuristicOptimal=false) {
			// Horrible heuristic fits
			if (heuristicOptimal) {
				if (bandwidth < 3) bandwidth += (3 - bandwidth)*0.5;
				return 12.9 + -3/(bandwidth + 0.4) - 13.4*bandwidth + (bandwidth < 3)*-9.6*(bandwidth - 3);
			}
			return 10.5 + 15/(bandwidth + 0.4) - 13.25*bandwidth + (bandwidth < 2)*13*(bandwidth - 2);
		}
		static double energyDbToBandwidth(double energyDb, bool heuristicOptimal=false) {
			double bw = 1;
			while (bw < 20 && bandwidthToEnergyDb(bw, heuristicOptimal) > energyDb) {
				bw *= 2;
			}
			double step = bw/2;
			while (step > 0.0001) {
				if (bandwidthToEnergyDb(bw, heuristicOptimal) > energyDb) {
					bw += step;
				} else {
					bw -= step;
				}
				step *= 0.5;
			}
			return bw;
		}
		/** @brief Peak ratio (in dB) between side-lobes and the main lobe.
			\diagram{kaiser-bandwidth-sidelobes-peak.svg,Measured main/side lobe peak ratio.  You can see that the heuristic improves performance, except in the bandwidth range 1-2 where peak ratio was sacrificed to improve total energy ratio.}
			This function uses an approximation which is accurate to ±0.5dB for 2 ⩽ bandwidth ≤ 9, or 0.5 ⩽ bandwidth ≤ 9 when `heuristicOptimal`is enabled.
		*/
		static double bandwidthToPeakDb(double bandwidth, bool heuristicOptimal=false) {
			// Horrible heuristic fits
			if (heuristicOptimal) {
				return 14.2 - 20/(bandwidth + 1) - 13*bandwidth + (bandwidth < 3)*-6*(bandwidth - 3) + (bandwidth < 2.25)*5.8*(bandwidth - 2.25);
			}
			return 10 + 8/(bandwidth + 2) - 12.75*bandwidth + (bandwidth < 2)*4*(bandwidth - 2);
		}
		static double peakDbToBandwidth(double peakDb, bool heuristicOptimal=false) {
			double bw = 1;
			while (bw < 20 && bandwidthToPeakDb(bw, heuristicOptimal) > peakDb) {
				bw *= 2;
			}
			double step = bw/2;
			while (step > 0.0001) {
				if (bandwidthToPeakDb(bw, heuristicOptimal) > peakDb) {
					bw += step;
				} else {
					bw -= step;
				}
				step *= 0.5;
			}
			return bw;
		}
		/** @} */

		/** Equivalent noise bandwidth (ENBW), a measure of frequency resolution.
			\diagram{kaiser-bandwidth-enbw.svg,Measured ENBW, with and without the heuristic bandwidth adjustment.}
			This approximation is accurate to ±0.05 up to a bandwidth of 22.
		*/
		static double bandwidthToEnbw(double bandwidth, bool heuristicOptimal=false) {
			if (heuristicOptimal) bandwidth = heuristicBandwidth(bandwidth);
			double b2 = std::max<double>(bandwidth - 2, 0);
			return 1 + b2*(0.2 + b2*(-0.005 + b2*(-0.000005 + b2*0.0000022)));
		}

		/// Return the window's value for position in the range [0, 1]
		double operator ()(double unit) {
			double r = 2*unit - 1;
			double arg = std::sqrt(1 - r*r);
			return bessel0(beta*arg)*invB0;
		}
	
		/// Fills an arbitrary container with a Kaiser window
		template<typename Data>
		void fill(Data &data, int size) const {
			double invSize = 1.0/size;
			for (int i = 0; i < size; ++i) {
				double r = (2*i + 1)*invSize - 1;
				double arg = std::sqrt(1 - r*r);
				data[i] = bessel0(beta*arg)*invB0;
			}
		}
	};

	/** Forces STFT perfect-reconstruction (WOLA) on an existing window, for a given STFT interval.
	For example, here are perfect-reconstruction versions of the approximately-optimal @ref Kaiser windows:
	\diagram{kaiser-windows-heuristic-pr.svg,Note the lower overall energy\, and the pointy top for 2x bandwidth. Spectral performance is about the same\, though.}
	*/
	template<typename Data>
	void forcePerfectReconstruction(Data &data, int windowLength, int interval) {
		for (int i = 0; i < interval; ++i) {
			double sum2 = 0;
			for (int index = i; index < windowLength; index += interval) {
				sum2 += data[index]*data[index];
			}
			double factor = 1/std::sqrt(sum2);
			for (int index = i; index < windowLength; index += interval) {
				data[index] *= factor;
			}
		}
	}

/** @} */
}} // signalsmith::windows
#endif // include guard

namespace signalsmith {
namespace delay {
	/**	@defgroup Delay Delay utilities
		@brief Standalone templated classes for delays
		
		You can set up a `Buffer` or `MultiBuffer`, and get interpolated samples using a `Reader` (separately on each channel in the multi-channel case) - or you can use `Delay`/`MultiDelay` which include their own buffers.

		Interpolation quality is chosen using a template class, from @ref Interpolators.

		@{
		@file
	*/

	/** @brief Single-channel delay buffer
 
		Access is used with `buffer[]`, relative to the internal read/write position ("head").  This head is moved using `++buffer` (or `buffer += n`), such that `buffer[1] == (buffer + 1)[0]` in a similar way iterators/pointers.
		
		Operations like `buffer - 10` or `buffer++` return a View, which holds a fixed position in the buffer (based on the read/write position at the time).
		
		The capacity includes both positive and negative indices.  For example, a capacity of 100 would support using any of the ranges:
		
		* `buffer[-99]` to buffer[0]`
		* `buffer[-50]` to buffer[49]`
		* `buffer[0]` to buffer[99]`

		Although buffers are usually used with historical samples accessed using negative indices e.g. `buffer[-10]`, you could equally use it flipped around (moving the head backwards through the buffer using `--buffer`).
	*/
	template<typename Sample>
	class Buffer {
		unsigned bufferIndex;
		unsigned bufferMask;
		std::vector<Sample> buffer;
	public:
		Buffer(int minCapacity=0) {
			resize(minCapacity);
		}
		// We shouldn't accidentally copy a delay buffer
		Buffer(const Buffer &other) = delete;
		Buffer & operator =(const Buffer &other) = delete;
		// But moving one is fine
		Buffer(Buffer &&other) = default;
		Buffer & operator =(Buffer &&other) = default;

		void resize(int minCapacity, Sample value=Sample()) {
			int bufferLength = 1;
			while (bufferLength < minCapacity) bufferLength *= 2;
			buffer.assign(bufferLength, value);
			bufferMask = unsigned(bufferLength - 1);
			bufferIndex = 0;
		}
		void reset(Sample value=Sample()) {
			buffer.assign(buffer.size(), value);
		}

		/// Holds a view for a particular position in the buffer
		template<bool isConst>
		class View {
			using CBuffer = typename std::conditional<isConst, const Buffer, Buffer>::type;
			using CSample = typename std::conditional<isConst, const Sample, Sample>::type;
			CBuffer *buffer = nullptr;
			unsigned bufferIndex = 0;
		public:
			View(CBuffer &buffer, int offset=0) : buffer(&buffer), bufferIndex(buffer.bufferIndex + (unsigned)offset) {}
			View(const View &other, int offset=0) : buffer(other.buffer), bufferIndex(other.bufferIndex + (unsigned)offset) {}
			View & operator =(const View &other) {
				buffer = other.buffer;
				bufferIndex = other.bufferIndex;
				return *this;
			}
			
			CSample & operator[](int offset) {
				return buffer->buffer[(bufferIndex + (unsigned)offset)&buffer->bufferMask];
			}
			const Sample & operator[](int offset) const {
				return buffer->buffer[(bufferIndex + (unsigned)offset)&buffer->bufferMask];
			}

			/// Write data into the buffer
			template<typename Data>
			void write(Data &&data, int length) {
				for (int i = 0; i < length; ++i) {
					(*this)[i] = data[i];
				}
			}
			/// Read data out from the buffer
			template<typename Data>
			void read(int length, Data &&data) const {
				for (int i = 0; i < length; ++i) {
					data[i] = (*this)[i];
				}
			}

			View operator +(int offset) const {
				return View(*this, offset);
			}
			View operator -(int offset) const {
				return View(*this, -offset);
			}
		};
		using MutableView = View<false>;
		using ConstView = View<true>;
		
		MutableView view(int offset=0) {
			return MutableView(*this, offset);
		}
		ConstView view(int offset=0) const {
			return ConstView(*this, offset);
		}
		ConstView constView(int offset=0) const {
			return ConstView(*this, offset);
		}

		Sample & operator[](int offset) {
			return buffer[(bufferIndex + (unsigned)offset)&bufferMask];
		}
		const Sample & operator[](int offset) const {
			return buffer[(bufferIndex + (unsigned)offset)&bufferMask];
		}

		/// Write data into the buffer
		template<typename Data>
		void write(Data &&data, int length) {
			for (int i = 0; i < length; ++i) {
				(*this)[i] = data[i];
			}
		}
		/// Read data out from the buffer
		template<typename Data>
		void read(int length, Data &&data) const {
			for (int i = 0; i < length; ++i) {
				data[i] = (*this)[i];
			}
		}
		
		Buffer & operator ++() {
			++bufferIndex;
			return *this;
		}
		Buffer & operator +=(int i) {
			bufferIndex += (unsigned)i;
			return *this;
		}
		Buffer & operator --() {
			--bufferIndex;
			return *this;
		}
		Buffer & operator -=(int i) {
			bufferIndex -= (unsigned)i;
			return *this;
		}

		MutableView operator ++(int) {
			MutableView view(*this);
			++bufferIndex;
			return view;
		}
		MutableView operator +(int i) {
			return MutableView(*this, i);
		}
		ConstView operator +(int i) const {
			return ConstView(*this, i);
		}
		MutableView operator --(int) {
			MutableView view(*this);
			--bufferIndex;
			return view;
		}
		MutableView operator -(int i) {
			return MutableView(*this, -i);
		}
		ConstView operator -(int i) const {
			return ConstView(*this, -i);
		}
	};

	/** @brief Multi-channel delay buffer

		This behaves similarly to the single-channel `Buffer`, with the following differences:
		
		* `buffer[c]` returns a view for a single channel, which behaves like the single-channel `Buffer::View`.
		* The constructor and `.resize()` take an additional first `channel` argument.
	*/
	template<typename Sample>
	class MultiBuffer {
		int channels, stride;
		Buffer<Sample> buffer;
	public:
		using ConstChannel = typename Buffer<Sample>::ConstView;
		using MutableChannel = typename Buffer<Sample>::MutableView;

		MultiBuffer(int channels=0, int capacity=0) : channels(channels), stride(capacity), buffer(channels*capacity) {}

		void resize(int nChannels, int capacity, Sample value=Sample()) {
			channels = nChannels;
			stride = capacity;
			buffer.resize(channels*capacity, value);
		}
		void reset(Sample value=Sample()) {
			buffer.reset(value);
		}

		/// A reference-like multi-channel result for a particular sample index
		template<bool isConst>
		class Stride {
			using CChannel = typename std::conditional<isConst, ConstChannel, MutableChannel>::type;
			using CSample = typename std::conditional<isConst, const Sample, Sample>::type;
			CChannel view;
			int channels, stride;
		public:
			Stride(CChannel view, int channels, int stride) : view(view), channels(channels), stride(stride) {}
			Stride(const Stride &other) : view(other.view), channels(other.channels), stride(other.stride) {}
			
			CSample & operator[](int channel) {
				return view[channel*stride];
			}
			const Sample & operator[](int channel) const {
				return view[channel*stride];
			}

			/// Reads from the buffer into a multi-channel result
			template<class Data>
			void get(Data &&result) const {
				for (int c = 0; c < channels; ++c) {
					result[c] = view[c*stride];
				}
			}
			/// Writes from multi-channel data into the buffer
			template<class Data>
			void set(Data &&data) {
				for (int c = 0; c < channels; ++c) {
					view[c*stride] = data[c];
				}
			}
			template<class Data>
			Stride & operator =(const Data &data) {
				set(data);
				return *this;
			}
			Stride & operator =(const Stride &data) {
				set(data);
				return *this;
			}
		};
		
		Stride<false> at(int offset) {
			return {buffer.view(offset), channels, stride};
		}
		Stride<true> at(int offset) const {
			return {buffer.view(offset), channels, stride};
		}

		/// Holds a particular position in the buffer
		template<bool isConst>
		class View {
			using CChannel = typename std::conditional<isConst, ConstChannel, MutableChannel>::type;
			CChannel view;
			int channels, stride;
		public:
			View(CChannel view, int channels, int stride) : view(view), channels(channels), stride(stride) {}
			
			CChannel operator[](int channel) {
				return view + channel*stride;
			}
			ConstChannel operator[](int channel) const {
				return view + channel*stride;
			}

			Stride<isConst> at(int offset) {
				return {view + offset, channels, stride};
			}
			Stride<true> at(int offset) const {
				return {view + offset, channels, stride};
			}
		};
		using MutableView = View<false>;
		using ConstView = View<true>;

		MutableView view(int offset=0) {
			return MutableView(buffer.view(offset), channels, stride);
		}
		ConstView view(int offset=0) const {
			return ConstView(buffer.view(offset), channels, stride);
		}
		ConstView constView(int offset=0) const {
			return ConstView(buffer.view(offset), channels, stride);
		}

		MutableChannel operator[](int channel) {
			return buffer + channel*stride;
		}
		ConstChannel operator[](int channel) const {
			return buffer + channel*stride;
		}
		
		MultiBuffer & operator ++() {
			++buffer;
			return *this;
		}
		MultiBuffer & operator +=(int i) {
			buffer += i;
			return *this;
		}
		MutableView operator ++(int) {
			return MutableView(buffer++, channels, stride);
		}
		MutableView operator +(int i) {
			return MutableView(buffer + i, channels, stride);
		}
		ConstView operator +(int i) const {
			return ConstView(buffer + i, channels, stride);
		}
		MultiBuffer & operator --() {
			--buffer;
			return *this;
		}
		MultiBuffer & operator -=(int i) {
			buffer -= i;
			return *this;
		}
		MutableView operator --(int) {
			return MutableView(buffer--, channels, stride);
		}
		MutableView operator -(int i) {
			return MutableView(buffer - i, channels, stride);
		}
		ConstView operator -(int i) const {
			return ConstView(buffer - i, channels, stride);
		}
	};
	
	/** \defgroup Interpolators Interpolators
		\ingroup Delay
		@{ */
	/// Nearest-neighbour interpolator
	/// \diagram{delay-random-access-nearest.svg,aliasing and maximum amplitude/delay errors for different input frequencies}
	template<typename Sample>
	struct InterpolatorNearest {
		static constexpr int inputLength = 1;
		static constexpr Sample latency = -0.5; // Because we're truncating, which rounds down too often
	
		template<class Data>
		static Sample fractional(const Data &data, Sample) {
			return data[0];
		}
	};
	/// Linear interpolator
	/// \diagram{delay-random-access-linear.svg,aliasing and maximum amplitude/delay errors for different input frequencies}
	template<typename Sample>
	struct InterpolatorLinear {
		static constexpr int inputLength = 2;
		static constexpr int latency = 0;
	
		template<class Data>
		static Sample fractional(const Data &data, Sample fractional) {
			Sample a = data[0], b = data[1];
			return a + fractional*(b - a);
		}
	};
	/// Spline cubic interpolator
	/// \diagram{delay-random-access-cubic.svg,aliasing and maximum amplitude/delay errors for different input frequencies}
	template<typename Sample>
	struct InterpolatorCubic {
		static constexpr int inputLength = 4;
		static constexpr int latency = 1;
	
		template<class Data>
		static Sample fractional(const Data &data, Sample fractional) {
			// Cubic interpolation
			Sample a = data[0], b = data[1], c = data[2], d = data[3];
			Sample cbDiff = c - b;
			Sample k1 = (c - a)*0.5;
			Sample k3 = k1 + (d - b)*0.5 - cbDiff*2;
			Sample k2 = cbDiff - k3 - k1;
			return b + fractional*(k1 + fractional*(k2 + fractional*k3)); // 16 ops total, not including the indexing
		}
	};

	// Efficient Algorithms and Structures for Fractional Delay Filtering Based on Lagrange Interpolation
	// Franck 2009 https://www.aes.org/e-lib/browse.cfm?elib=14647
	namespace _franck_impl {
		template<typename Sample, int n, int low, int high>
		struct ProductRange {
			using Array = std::array<Sample, (n + 1)>;
			static constexpr int mid = (low + high)/2;
			using Left = ProductRange<Sample, n, low, mid>;
			using Right = ProductRange<Sample, n, mid + 1, high>;

			Left left;
			Right right;

			const Sample total;
			ProductRange(Sample x) : left(x), right(x), total(left.total*right.total) {}

			template<class Data>
			Sample calculateResult(Sample extraFactor, const Data &data, const Array &invFactors) {
				return left.calculateResult(extraFactor*right.total, data, invFactors)
					+ right.calculateResult(extraFactor*left.total, data, invFactors);
			}
		};
		template<typename Sample, int n, int index>
		struct ProductRange<Sample, n, index, index> {
			using Array = std::array<Sample, (n + 1)>;

			const Sample total;
			ProductRange(Sample x) : total(x - index) {}

			template<class Data>
			Sample calculateResult(Sample extraFactor, const Data &data, const Array &invFactors) {
				return extraFactor*data[index]*invFactors[index];
			}
		};
	}
	/** Fixed-order Lagrange interpolation.
	\diagram{interpolator-LagrangeN.svg,aliasing and amplitude/delay errors for different sizes}
	*/
	template<typename Sample, int n>
	struct InterpolatorLagrangeN {
		static constexpr int inputLength = n + 1;
		static constexpr int latency = (n - 1)/2;

		using Array = std::array<Sample, (n + 1)>;
		Array invDivisors;

		InterpolatorLagrangeN() {
			for (int j = 0; j <= n; ++j) {
				double divisor = 1;
				for (int k = 0; k < j; ++k) divisor *= (j - k);
				for (int k = j + 1; k <= n; ++k) divisor *= (j - k);
				invDivisors[j] = 1/divisor;
			}
		}

		template<class Data>
		Sample fractional(const Data &data, Sample fractional) const {
			constexpr int mid = n/2;
			using Left = _franck_impl::ProductRange<Sample, n, 0, mid>;
			using Right = _franck_impl::ProductRange<Sample, n, mid + 1, n>;

			Sample x = fractional + latency;

			Left left(x);
			Right right(x);

			return left.calculateResult(right.total, data, invDivisors) + right.calculateResult(left.total, data, invDivisors);
		}
	};
	template<typename Sample>
	using InterpolatorLagrange3 = InterpolatorLagrangeN<Sample, 3>;
	template<typename Sample>
	using InterpolatorLagrange7 = InterpolatorLagrangeN<Sample, 7>;
	template<typename Sample>
	using InterpolatorLagrange19 = InterpolatorLagrangeN<Sample, 19>;

	/** Fixed-size Kaiser-windowed sinc interpolation.
	\diagram{interpolator-KaiserSincN.svg,aliasing and amplitude/delay errors for different sizes}
	If `minimumPhase` is enabled, a minimum-phase version of the kernel is used:
	\diagram{interpolator-KaiserSincN-min.svg,aliasing and amplitude/delay errors for minimum-phase mode}
	*/
	template<typename Sample, int n, bool minimumPhase=false>
	struct InterpolatorKaiserSincN {
		static constexpr int inputLength = n;
		static constexpr Sample latency = minimumPhase ? 0 : (n*Sample(0.5) - 1);

		int subSampleSteps;
		std::vector<Sample> coefficients;
		
		InterpolatorKaiserSincN() : InterpolatorKaiserSincN(0.5 - 0.45/std::sqrt(n)) {}
		InterpolatorKaiserSincN(double passFreq) : InterpolatorKaiserSincN(passFreq, 1 - passFreq) {}
		InterpolatorKaiserSincN(double passFreq, double stopFreq) {
			subSampleSteps = 2*n; // Heuristic again.  Really it depends on the bandwidth as well.
			double kaiserBandwidth = (stopFreq - passFreq)*(n + 1.0/subSampleSteps);
			kaiserBandwidth += 1.25/kaiserBandwidth; // We want to place the first zero, but (because using this to window a sinc essentially integrates it in the freq-domain), our ripples (and therefore zeroes) are out of phase.  This is a heuristic fix.
			
			double centreIndex = n*subSampleSteps*0.5, scaleFactor = 1.0/subSampleSteps;
			std::vector<Sample> windowedSinc(subSampleSteps*n + 1);
			
			::signalsmith::windows::Kaiser::withBandwidth(kaiserBandwidth, false).fill(windowedSinc, windowedSinc.size());

			for (size_t i = 0; i < windowedSinc.size(); ++i) {
				double x = (i - centreIndex)*scaleFactor;
				int intX = std::round(x);
				if (intX != 0 && std::abs(x - intX) < 1e-6) {
					// Exact 0s
					windowedSinc[i] = 0;
				} else if (std::abs(x) > 1e-6) {
					double p = x*M_PI;
					windowedSinc[i] *= std::sin(p)/p;
				}
			}
			
			if (minimumPhase) {
				signalsmith::fft::FFT<Sample> fft(windowedSinc.size()*2, 1);
				windowedSinc.resize(fft.size(), 0);
				std::vector<std::complex<Sample>> spectrum(fft.size());
				std::vector<std::complex<Sample>> cepstrum(fft.size());
				fft.fft(windowedSinc, spectrum);
				for (size_t i = 0; i < fft.size(); ++i) {
					spectrum[i] = std::log(std::abs(spectrum[i]) + 1e-30);
				}
				fft.fft(spectrum, cepstrum);
				for (size_t i = 1; i < fft.size()/2; ++i) {
					cepstrum[i] *= 0;
				}
				for (size_t i = fft.size()/2 + 1; i < fft.size(); ++i) {
					cepstrum[i] *= 2;
				}
				Sample scaling = Sample(1)/fft.size();
				fft.ifft(cepstrum, spectrum);

				for (size_t i = 0; i < fft.size(); ++i) {
					Sample phase = spectrum[i].imag()*scaling;
					Sample mag = std::exp(spectrum[i].real()*scaling);
					spectrum[i] = {mag*std::cos(phase), mag*std::sin(phase)};
				}
				fft.ifft(spectrum, cepstrum);
				windowedSinc.resize(subSampleSteps*n + 1);
				windowedSinc.shrink_to_fit();
				for (size_t i = 0; i < windowedSinc.size(); ++i) {
					windowedSinc[i] = cepstrum[i].real()*scaling;
				}
			}
			
			// Re-order into FIR fractional-delay blocks
			coefficients.resize(n*(subSampleSteps + 1));
			for (int k = 0; k <= subSampleSteps; ++k) {
				for (int i = 0; i < n; ++i) {
					coefficients[k*n + i] = windowedSinc[(subSampleSteps - k) + i*subSampleSteps];
				}
			}
		}
		
		template<class Data>
		Sample fractional(const Data &data, Sample fractional) const {
			Sample subSampleDelay = fractional*subSampleSteps;
			int lowIndex = subSampleDelay;
			if (lowIndex >= subSampleSteps) lowIndex = subSampleSteps - 1;
			Sample subSampleFractional = subSampleDelay - lowIndex;
			int highIndex = lowIndex + 1;
			
			Sample sumLow = 0, sumHigh = 0;
			const Sample *coeffLow = coefficients.data() + lowIndex*n;
			const Sample *coeffHigh = coefficients.data() + highIndex*n;
			for (int i = 0; i < n; ++i) {
				sumLow += data[i]*coeffLow[i];
				sumHigh += data[i]*coeffHigh[i];
			}
			return sumLow + (sumHigh - sumLow)*subSampleFractional;
		}
	};

	template<typename Sample>
	using InterpolatorKaiserSinc20 = InterpolatorKaiserSincN<Sample, 20>;
	template<typename Sample>
	using InterpolatorKaiserSinc8 = InterpolatorKaiserSincN<Sample, 8>;
	template<typename Sample>
	using InterpolatorKaiserSinc4 = InterpolatorKaiserSincN<Sample, 4>;

	template<typename Sample>
	using InterpolatorKaiserSinc20Min = InterpolatorKaiserSincN<Sample, 20, true>;
	template<typename Sample>
	using InterpolatorKaiserSinc8Min = InterpolatorKaiserSincN<Sample, 8, true>;
	template<typename Sample>
	using InterpolatorKaiserSinc4Min = InterpolatorKaiserSincN<Sample, 4, true>;
	///  @}
	
	/** @brief A delay-line reader which uses an external buffer
 
		This is useful if you have multiple delay-lines reading from the same buffer.
	*/
	template<class Sample, template<typename> class Interpolator=InterpolatorLinear>
	class Reader : public Interpolator<Sample> /* so we can get the empty-base-class optimisation */ {
		using Super = Interpolator<Sample>;
	public:
		Reader () {}
		/// Pass in a configured interpolator
		Reader (const Interpolator<Sample> &interpolator) : Super(interpolator) {}
	
		template<typename Buffer>
		Sample read(const Buffer &buffer, Sample delaySamples) const {
			int startIndex = delaySamples;
			Sample remainder = delaySamples - startIndex;
			
			// Delay buffers use negative indices, but interpolators use positive ones
			using View = decltype(buffer - startIndex);
			struct Flipped {
				 View view;
				 Sample operator [](int i) const {
					return view[-i];
				 }
			};
			return Super::fractional(Flipped{buffer - startIndex}, remainder);
		}
	};

	/**	@brief A single-channel delay-line containing its own buffer.*/
	template<class Sample, template<typename> class Interpolator=InterpolatorLinear>
	class Delay : private Reader<Sample, Interpolator> {
		using Super = Reader<Sample, Interpolator>;
		Buffer<Sample> buffer;
	public:
		static constexpr Sample latency = Super::latency;

		Delay(int capacity=0) : buffer(1 + capacity + Super::inputLength) {}
		/// Pass in a configured interpolator
		Delay(const Interpolator<Sample> &interp, int capacity=0) : Super(interp), buffer(1 + capacity + Super::inputLength) {}
		
		void reset(Sample value=Sample()) {
			buffer.reset(value);
		}
		void resize(int minCapacity, Sample value=Sample()) {
			buffer.resize(minCapacity + Super::inputLength, value);
		}
		
		/** Read a sample from `delaySamples` >= 0 in the past.
		The interpolator may add its own latency on top of this (see `Delay::latency`).  The default interpolation (linear) has 0 latency.
		*/
		Sample read(Sample delaySamples) const {
			return Super::read(buffer, delaySamples);
		}
		/// Writes a sample. Returns the same object, so that you can say `delay.write(v).read(delay)`.
		Delay & write(Sample value) {
			++buffer;
			buffer[0] = value;
			return *this;
		}
	};

	/**	@brief A multi-channel delay-line with its own buffer. */
	template<class Sample, template<typename> class Interpolator=InterpolatorLinear>
	class MultiDelay : private Reader<Sample, Interpolator> {
		using Super = Reader<Sample, Interpolator>;
		int channels;
		MultiBuffer<Sample> multiBuffer;
	public:
		static constexpr Sample latency = Super::latency;

		MultiDelay(int channels=0, int capacity=0) : channels(channels), multiBuffer(channels, 1 + capacity + Super::inputLength) {}

		void reset(Sample value=Sample()) {
			multiBuffer.reset(value);
		}
		void resize(int nChannels, int capacity, Sample value=Sample()) {
			channels = nChannels;
			multiBuffer.resize(channels, capacity + Super::inputLength, value);
		}
		
		/// A single-channel delay-line view, similar to a `const Delay`
		struct ChannelView {
			static constexpr Sample latency = Super::latency;

			const Super &reader;
			typename MultiBuffer<Sample>::ConstChannel channel;
			
			Sample read(Sample delaySamples) const {
				return reader.read(channel, delaySamples);
			}
		};
		ChannelView operator [](int channel) const {
			return ChannelView{*this, multiBuffer[channel]};
		}

		/// A multi-channel result, lazily calculating samples
		struct DelayView {
			Super &reader;
			typename MultiBuffer<Sample>::ConstView view;
			Sample delaySamples;
			
			// Calculate samples on-the-fly
			Sample operator [](int c) const {
				return reader.read(view[c], delaySamples);
			}
		};
		DelayView read(Sample delaySamples) {
			return DelayView{*this, multiBuffer.constView(), delaySamples};
		}
		/// Reads into the provided output structure
		template<class Output>
		void read(Sample delaySamples, Output &output) {
			for (int c = 0; c < channels; ++c) {
				output[c] = Super::read(multiBuffer[c], delaySamples);
			}
		}
		/// Reads separate delays for each channel
		template<class Delays, class Output>
		void readMulti(const Delays &delays, Output &output) {
			for (int c = 0; c < channels; ++c) {
				output[c] = Super::read(multiBuffer[c], delays[c]);
			}
		}
		template<class Data>
		MultiDelay & write(const Data &data) {
			++multiBuffer;
			for (int c = 0; c < channels; ++c) {
				multiBuffer[c][0] = data[c];
			}
			return *this;
		}
	};

/** @} */
}} // signalsmith::delay::
#endif // include guard

#include <cmath>

namespace signalsmith {
namespace spectral {
	/**	@defgroup Spectral Spectral Processing
		@brief Tools for frequency-domain manipulation of audio signals
		
		@{
		@file
	*/
	
	/** @brief An FFT with built-in windowing and round-trip scaling
	
		This uses a Modified Real FFT, which applies half-bin shift before the transform.  The result therefore has `N/2` bins, centred at the frequencies: `(i + 0.5)/N`.
		
		This avoids the awkward (real-valued) bands for DC-offset and Nyquist.
	 */
	template<typename Sample>
	class WindowedFFT {
		using MRFFT = signalsmith::fft::ModifiedRealFFT<Sample>;
		using Complex = std::complex<Sample>;
		MRFFT mrfft{2};

		std::vector<Sample> fftWindow;
		std::vector<Sample> timeBuffer;
		std::vector<Complex> freqBuffer;
	public:
		/// Returns a fast FFT size <= `size`
		static int fastSizeAbove(int size, int divisor=1) {
			return MRFFT::fastSizeAbove(size/divisor)*divisor;
		}
		/// Returns a fast FFT size >= `size`
		static int fastSizeBelow(int size, int divisor=1) {
			return MRFFT::fastSizeBelow(1 + (size - 1)/divisor)*divisor;
		}

		WindowedFFT() {}
		WindowedFFT(int size) {
			setSize(size);
		}
		template<class WindowFn>
		WindowedFFT(int size, WindowFn fn, Sample windowOffset=0.5) {
			setSize(size, fn, windowOffset);
		}

		/// Sets the size, returning the window for modification (initially all 1s)
		std::vector<Sample> & setSizeWindow(int size) {
			mrfft.setSize(size);
			fftWindow.resize(size, 1);
			timeBuffer.resize(size);
			freqBuffer.resize(size);
			return fftWindow;
		}
		/// Sets the FFT size, with a user-defined functor for the window
		template<class WindowFn>
		void setSize(int size, WindowFn fn, Sample windowOffset=0.5) {
			setSizeWindow(size);
		
			Sample invSize = 1/(Sample)size;
			for (int i = 0; i < size; ++i) {
				Sample r = (i + windowOffset)*invSize;
				fftWindow[i] = fn(r);
			}
		}
		/// Sets the size (using the default Blackman-Harris window)
		void setSize(int size) {
			setSize(size, [](double x) {
				double phase = 2*M_PI*x;
				// Blackman-Harris
				return 0.35875 + 0.48829*std::cos(phase) + 0.14128*std::cos(phase*2) + 0.1168*std::cos(phase*3);
			});
		}

		const std::vector<Sample> & window() const {
			return this->fftWindow;
		}
		int size() const {
			return mrfft.size();
		}
		
		/// Performs an FFT (with windowing)
		template<class Input, class Output>
		void fft(Input &&input, Output &&output) {
			struct WindowedInput {
				const Input &input;
				std::vector<Sample> &window;
				SIGNALSMITH_INLINE Sample operator [](int i) {
					return input[i]*window[i];
				}
			};
		
			mrfft.fft(WindowedInput{input, fftWindow}, output);
		}
		/// Performs an FFT (no windowing)
		template<class Input, class Output>
		void fftRaw(Input &&input, Output &&output) {
			mrfft.fft(input, output);
		}

		/// Inverse FFT, with windowing and 1/N scaling
		template<class Input, class Output>
		void ifft(Input &&input, Output &&output) {
			mrfft.ifft(input, output);
			int size = mrfft.size();
			Sample norm = 1/(Sample)size;
			for (int i = 0; i < size; ++i) {
				output[i] *= norm*fftWindow[i];
			}
		}
		/// Performs an IFFT (no windowing)
		template<class Input, class Output>
		void ifftRaw(Input &&input, Output &&output) {
			mrfft.ifft(input, output);
		}
	};
	
	/** STFT synthesis, built on a `MultiBuffer`.
 
		Any window length and block interval is supported, but the FFT size may be rounded up to a faster size (by zero-padding).  It uses a heuristically-optimal Kaiser window modified for perfect-reconstruction.
		
		\diagram{stft-aliasing-simulated.svg,Simulated bad-case aliasing (random phase-shift for each band) for overlapping ratios}

		There is a "latest valid index", and you can read the output up to one `historyLength` behind this (see `.resize()`).  You can read up to one window-length _ahead_ to get partially-summed future output.
		
		\diagram{stft-buffer-validity.svg}
		
		You move the valid index along using `.ensureValid()`, passing in a functor which provides spectra (using `.analyse()` and/or direct modification through `.spectrum[c]`):

		\code
			void processSample(...) {
				stft.ensureValid([&](int) {
					// Here, we introduce (1 - windowSize) of latency
					stft.analyse(inputBuffer.view(1 - windowSize))
				});
				// read as a MultiBuffer
				auto result = stft.at(0);
				++stft; // also moves the latest valid index
			}

			void processBlock(...) {
				// assuming `historyLength` == blockSize
				stft.ensureValid(blockSize, [&](int blockStartIndex) {
					int inputStart = blockStartIndex + (1 - windowSize);
					stft.analyse(inputBuffer.view(inputStart));
				});
				auto earliestValid = stft.at(0);
				auto latestValid = stft.at(blockSize);
				stft += blockSize;
			}
		\endcode
		
		The index passed to this functor will be greater than the previous valid index, and `<=` the index you pass in.  Therefore, if you call `.ensureValid()` every sample, it can only ever be `0`.
	*/
	template<typename Sample>
	class STFT : public signalsmith::delay::MultiBuffer<Sample> {
		using Super = signalsmith::delay::MultiBuffer<Sample>;
		using Complex = std::complex<Sample>;

		int channels = 0, _windowSize = 0, _fftSize = 0, _interval = 1;
		int validUntilIndex = 0;

		class MultiSpectrum {
			int channels, stride;
			std::vector<Complex> buffer;
		public:
			MultiSpectrum() : MultiSpectrum(0, 0) {}
			MultiSpectrum(int channels, int bands) : channels(channels), stride(bands), buffer(channels*bands, 0) {}
			
			void resize(int nChannels, int nBands) {
				channels = nChannels;
				stride = nBands;
				buffer.assign(channels*stride, 0);
			}
			
			void reset() {
				buffer.assign(buffer.size(), 0);
			}
			
			void swap(MultiSpectrum &other) {
				using std::swap;
				swap(buffer, other.buffer);
			}

			Complex * operator [](int channel) {
				return buffer.data() + channel*stride;
			}
			const Complex * operator [](int channel) const {
				return buffer.data() + channel*stride;
			}
		};
		std::vector<Sample> timeBuffer;

		void resizeInternal(int newChannels, int windowSize, int newInterval, int historyLength, int zeroPadding) {
			Super::resize(newChannels,
				windowSize /* for output summing */
				+ newInterval /* so we can read `windowSize` ahead (we'll be at most `interval-1` from the most recent block */
				+ historyLength);

			int fftSize = fft.fastSizeAbove(windowSize + zeroPadding);
			
			this->channels = newChannels;
			_windowSize = windowSize;
			this->_fftSize = fftSize;
			this->_interval = newInterval;
			validUntilIndex = -1;
			
			using Kaiser = ::signalsmith::windows::Kaiser;

			/// Roughly optimal Kaiser for STFT analysis (forced to perfect reconstruction)
			auto &window = fft.setSizeWindow(fftSize);
			auto kaiser = Kaiser::withBandwidth(windowSize/(double)_interval, true);
			kaiser.fill(window, windowSize);
			::signalsmith::windows::forcePerfectReconstruction(window, windowSize, _interval);
			
			// TODO: fill extra bits of an input buffer with NaN/Infinity, to break this, and then fix by adding zero-padding to WindowedFFT (as opposed to zero-valued window sections)
			for (int i = windowSize; i < fftSize; ++i) {
				window[i] = 0;
			}

			spectrum.resize(channels, fftSize/2);
			timeBuffer.resize(fftSize);
		}
	public:
		using Spectrum = MultiSpectrum;
		Spectrum spectrum;
		WindowedFFT<Sample> fft;

		STFT() {}
		/// Parameters passed straight to `.resize()`
		STFT(int channels, int windowSize, int interval, int historyLength=0, int zeroPadding=0) {
			resize(channels, windowSize, interval, historyLength, zeroPadding);
		}

		/// Sets the channel-count, FFT size and interval.
		void resize(int nChannels, int windowSize, int interval, int historyLength=0, int zeroPadding=0) {
			resizeInternal(nChannels, windowSize, interval, historyLength, zeroPadding);
		}
		
		int windowSize() const {
			return _windowSize;
		}
		int fftSize() const {
			return _fftSize;
		}
		int interval() const {
			return _interval;
		}
		/// Returns the (analysis and synthesis) window
		decltype(fft.window()) window() const {
			return fft.window();
		}
		/// Calculates the effective window for the partially-summed future output (relative to the most recent block)
		std::vector<Sample> partialSumWindow() const {
			const auto &w = window();
			std::vector<Sample> result(_windowSize, 0);
			for (int offset = 0; offset < _windowSize; offset += _interval) {
				for (int i = 0; i < _windowSize - offset; ++i) {
					Sample value = w[i + offset];
					result[i] += value*value;
				}
			}
			return result;
		}
		
		/// Resets everything - since we clear the output sum, it will take `windowSize` samples to get proper output.
		void reset() {
			Super::reset();
			spectrum.reset();
			validUntilIndex = -1;
		}
		
		/** Generates valid output up to the specified index (or 0), using the callback as many times as needed.
			
			The callback should be a functor accepting a single integer argument, which is the index for which a spectrum is required.
			
			The block created from these spectra will start at this index in the output, plus `.latency()`.
		*/
		template<class AnalysisFn>
		void ensureValid(int i, AnalysisFn fn) {
			while (validUntilIndex < i) {
				int blockIndex = validUntilIndex + 1;
				fn(blockIndex);

				auto output = this->view(blockIndex);
				for (int c = 0; c < channels; ++c) {
					auto channel = output[c];

					// Clear out the future sum, a window-length and an interval ahead
					for (int wi = _windowSize; wi < _windowSize + _interval; ++wi) {
						channel[wi] = 0;
					}

					// Add in the IFFT'd result
					fft.ifft(spectrum[c], timeBuffer);
					for (int wi = 0; wi < _windowSize; ++wi) {
						channel[wi] += timeBuffer[wi];
					}
				}
				validUntilIndex += _interval;
			}
		}
		/// The same as above, assuming index 0
		template<class AnalysisFn>
		void ensureValid(AnalysisFn fn) {
			return ensureValid(0, fn);
		}
		
		/** Analyse a multi-channel input, for any type where `data[channel][index]` returns samples
 
		Results can be read/edited using `.spectrum`. */
		template<class Data>
		void analyse(Data &&data) {
			for (int c = 0; c < channels; ++c) {
				fft.fft(data[c], spectrum[c]);
			}
		}
		template<class Data>
		void analyse(int c, Data &&data) {
			fft.fft(data, spectrum[c]);
		}
		/// Analyse without windowing
		template<class Data>
		void analyseRaw(Data &&data) {
			for (int c = 0; c < channels; ++c) {
				fft.fftRaw(data[c], spectrum[c]);
			}
		}
		template<class Data>
		void analyseRaw(int c, Data &&data) {
			fft.fftRaw(data, spectrum[c]);
		}

		int bands() const {
			return _fftSize/2;
		}

		/** Internal latency (between the block-index requested in `.ensureValid()` and its position in the output)
 
		Currently unused, but it's in here to allow for a future implementation which spreads the FFT calculations out across each interval.*/
		int latency() {
			return 0;
		}
		
		// @name Shift the underlying buffer (moving the "valid" index accordingly)
		// @{
		STFT & operator ++() {
			Super::operator ++();
			validUntilIndex--;
			return *this;
		}
		STFT & operator +=(int i) {
			Super::operator +=(i);
			validUntilIndex -= i;
			return *this;
		}
		STFT & operator --() {
			Super::operator --();
			validUntilIndex++;
			return *this;
		}
		STFT & operator -=(int i) {
			Super::operator -=(i);
			validUntilIndex += i;
			return *this;
		}
		// @}

		typename Super::MutableView operator ++(int postIncrement) {
			auto result = Super::operator ++(postIncrement);
			validUntilIndex--;
			return result;
		}
		typename Super::MutableView operator --(int postIncrement) {
			auto result = Super::operator --(postIncrement);
			validUntilIndex++;
			return result;
		}
	};

	/** STFT processing, with input/output.
		Before calling `.ensureValid(index)`, you should make sure the input is filled up to `index`.
	*/
	template<typename Sample>
	class ProcessSTFT : public STFT<Sample> {
		using Super = STFT<Sample>;
	public:
		signalsmith::delay::MultiBuffer<Sample> input;
	
		ProcessSTFT(int inChannels, int outChannels, int windowSize, int interval, int historyLength=0) {
			resize(inChannels, outChannels, windowSize, interval, historyLength);
		}

		/** Alter the spectrum, using input up to this point, for the output block starting from this point.
			Sub-classes should replace this with whatever processing is desired. */
		virtual void processSpectrum(int /*blockIndex*/) {}
		
		/// Sets the input/output channels, FFT size and interval.
		void resize(int inChannels, int outChannels, int windowSize, int interval, int historyLength=0) {
			Super::resize(outChannels, windowSize, interval, historyLength);
			input.resize(inChannels, windowSize + interval + historyLength);
		}
		void reset(Sample value=Sample()) {
			Super::reset(value);
			input.reset(value);
		}

		/// Internal latency, including buffering samples for analysis.
		int latency() {
			return Super::latency() + (this->windowSize() - 1);
		}
		
		void ensureValid(int i=0) {
			Super::ensureValid(i, [&](int blockIndex) {
				this->analyse(input.view(blockIndex - this->windowSize() + 1));
				this->processSpectrum(blockIndex);
			});
		}

		// @name Shift the output, input, and valid index.
		// @{
		ProcessSTFT & operator ++() {
			Super::operator ++();
			++input;
			return *this;
		}
		ProcessSTFT & operator +=(int i) {
			Super::operator +=(i);
			input += i;
			return *this;
		}
		ProcessSTFT & operator --() {
			Super::operator --();
			--input;
			return *this;
		}
		ProcessSTFT & operator -=(int i) {
			Super::operator -=(i);
			input -= i;
			return *this;
		}
		// @}
	};

/** @} */
}} // signalsmith::spectral::
#endif // include guard

#ifndef SIGNALSMITH_DSP_DELAY_H
#define SIGNALSMITH_DSP_DELAY_H

#ifndef SIGNALSMITH_DSP_COMMON_H
#define SIGNALSMITH_DSP_COMMON_H

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

namespace signalsmith {
	/**	@defgroup Common Common
		@brief Definitions and helper classes used by the rest of the library
		
		@{
		@file
	*/

#define SIGNALSMITH_DSP_VERSION_MAJOR 1
#define SIGNALSMITH_DSP_VERSION_MINOR 3
#define SIGNALSMITH_DSP_VERSION_PATCH 3
#define SIGNALSMITH_DSP_VERSION_STRING "1.3.3"

	/** Version compatability check.
	\code{.cpp}
		static_assert(signalsmith::version(1, 0, 0), "version check");
	\endcode
	... or use the equivalent `SIGNALSMITH_DSP_VERSION_CHECK`.
	Major versions are not compatible with each other.  Minor and patch versions are backwards-compatible.
	*/
	constexpr bool versionCheck(int major, int minor, int patch=0) {
		return major == SIGNALSMITH_DSP_VERSION_MAJOR
			&& (SIGNALSMITH_DSP_VERSION_MINOR > minor
				|| (SIGNALSMITH_DSP_VERSION_MINOR == minor && SIGNALSMITH_DSP_VERSION_PATCH >= patch));
	}

/// Check the library version is compatible (semver).
#define SIGNALSMITH_DSP_VERSION_CHECK(major, minor, patch) \
	static_assert(::signalsmith::versionCheck(major, minor, patch), "signalsmith library version is " SIGNALSMITH_DSP_VERSION_STRING);

/** @} */
} // signalsmith::
#endif // include guard

#include <vector>
#include <array>
#include <cmath> // for std::ceil()
#include <type_traits>

#include <complex>
#ifndef SIGNALSMITH_FFT_V5
#define SIGNALSMITH_FFT_V5

#ifndef SIGNALSMITH_DSP_COMMON_H
#define SIGNALSMITH_DSP_COMMON_H

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

namespace signalsmith {
	/**	@defgroup Common Common
		@brief Definitions and helper classes used by the rest of the library
		
		@{
		@file
	*/

#define SIGNALSMITH_DSP_VERSION_MAJOR 1
#define SIGNALSMITH_DSP_VERSION_MINOR 3
#define SIGNALSMITH_DSP_VERSION_PATCH 3
#define SIGNALSMITH_DSP_VERSION_STRING "1.3.3"

	/** Version compatability check.
	\code{.cpp}
		static_assert(signalsmith::version(1, 0, 0), "version check");
	\endcode
	... or use the equivalent `SIGNALSMITH_DSP_VERSION_CHECK`.
	Major versions are not compatible with each other.  Minor and patch versions are backwards-compatible.
	*/
	constexpr bool versionCheck(int major, int minor, int patch=0) {
		return major == SIGNALSMITH_DSP_VERSION_MAJOR
			&& (SIGNALSMITH_DSP_VERSION_MINOR > minor
				|| (SIGNALSMITH_DSP_VERSION_MINOR == minor && SIGNALSMITH_DSP_VERSION_PATCH >= patch));
	}

/// Check the library version is compatible (semver).
#define SIGNALSMITH_DSP_VERSION_CHECK(major, minor, patch) \
	static_assert(::signalsmith::versionCheck(major, minor, patch), "signalsmith library version is " SIGNALSMITH_DSP_VERSION_STRING);

/** @} */
} // signalsmith::
#endif // include guard

#ifndef SIGNALSMITH_DSP_PERF_H
#define SIGNALSMITH_DSP_PERF_H

#include <complex>

namespace signalsmith {
namespace perf {
	/**	@defgroup Performance Performance helpers
		@brief Nothing serious, just some `#defines` and helpers
		
		@{
		@file
	*/
		
	/// *Really* insist that a function/method is inlined (mostly for performance in DEBUG builds)
	#ifndef SIGNALSMITH_INLINE
	#ifdef __GNUC__
	#define SIGNALSMITH_INLINE __attribute__((always_inline)) inline
	#elif defined(__MSVC__)
	#define SIGNALSMITH_INLINE __forceinline inline
	#else
	#define SIGNALSMITH_INLINE inline
	#endif
	#endif

	/** @brief Complex-multiplication (with optional conjugate second-arg), without handling NaN/Infinity
		The `std::complex` multiplication has edge-cases around NaNs which slow things down and prevent auto-vectorisation.
	*/
	template <bool conjugateSecond=false, typename V>
	SIGNALSMITH_INLINE static std::complex<V> mul(const std::complex<V> &a, const std::complex<V> &b) {
		return conjugateSecond ? std::complex<V>{
			b.real()*a.real() + b.imag()*a.imag(),
			b.real()*a.imag() - b.imag()*a.real()
		} : std::complex<V>{
			a.real()*b.real() - a.imag()*b.imag(),
			a.real()*b.imag() + a.imag()*b.real()
		};
	}

/** @} */
}} // signalsmith::perf::

#endif // include guard

#include <vector>
#include <complex>
#include <cmath>

namespace signalsmith { namespace fft {
	/**	@defgroup FFT FFT (complex and real)
		@brief Fourier transforms (complex and real)

		@{
		@file
	*/

	namespace _fft_impl {

		template <typename V>
		SIGNALSMITH_INLINE V complexReal(const std::complex<V> &c) {
			return ((V*)(&c))[0];
		}
		template <typename V>
		SIGNALSMITH_INLINE V complexImag(const std::complex<V> &c) {
			return ((V*)(&c))[1];
		}

		// Complex multiplication has edge-cases around Inf/NaN - handling those properly makes std::complex non-inlineable, so we use our own
		template <bool conjugateSecond, typename V>
		SIGNALSMITH_INLINE std::complex<V> complexMul(const std::complex<V> &a, const std::complex<V> &b) {
			V aReal = complexReal(a), aImag = complexImag(a);
			V bReal = complexReal(b), bImag = complexImag(b);
			return conjugateSecond ? std::complex<V>{
				bReal*aReal + bImag*aImag,
				bReal*aImag - bImag*aReal
			} : std::complex<V>{
				aReal*bReal - aImag*bImag,
				aReal*bImag + aImag*bReal
			};
		}

		template<bool flipped, typename V>
		SIGNALSMITH_INLINE std::complex<V> complexAddI(const std::complex<V> &a, const std::complex<V> &b) {
			V aReal = complexReal(a), aImag = complexImag(a);
			V bReal = complexReal(b), bImag = complexImag(b);
			return flipped ? std::complex<V>{
				aReal + bImag,
				aImag - bReal
			} : std::complex<V>{
				aReal - bImag,
				aImag + bReal
			};
		}

		// Use SFINAE to get an iterator from std::begin(), if supported - otherwise assume the value itself is an iterator
		template<typename T, typename=void>
		struct GetIterator {
			static T get(const T &t) {
				return t;
			}
		};
		template<typename T>
		struct GetIterator<T, decltype((void)std::begin(std::declval<T>()))> {
			static auto get(const T &t) -> decltype(std::begin(t)) {
				return std::begin(t);
			}
		};
	}

	/** Floating-point FFT implementation.
	It is fast for 2^a * 3^b.
	Here are the peak and RMS errors for `float`/`double` computation:
	\diagram{fft-errors.svg Simulated errors for pure-tone harmonic inputs\, compared to a theoretical upper bound from "Roundoff error analysis of the fast Fourier transform" (G. Ramos, 1971)}
	*/
	template<typename V=double>
	class FFT {
		using complex = std::complex<V>;
		size_t _size;
		std::vector<complex> workingVector;
		
		enum class StepType {
			generic, step2, step3, step4
		};
		struct Step {
			StepType type;
			size_t factor;
			size_t startIndex;
			size_t innerRepeats;
			size_t outerRepeats;
			size_t twiddleIndex;
		};
		std::vector<size_t> factors;
		std::vector<Step> plan;
		std::vector<complex> twiddleVector;
		
		struct PermutationPair {size_t from, to;};
		std::vector<PermutationPair> permutation;
		
		void addPlanSteps(size_t factorIndex, size_t start, size_t length, size_t repeats) {
			if (factorIndex >= factors.size()) return;
			
			size_t factor = factors[factorIndex];
			if (factorIndex + 1 < factors.size()) {
				if (factors[factorIndex] == 2 && factors[factorIndex + 1] == 2) {
					++factorIndex;
					factor = 4;
				}
			}

			size_t subLength = length/factor;
			Step mainStep{StepType::generic, factor, start, subLength, repeats, twiddleVector.size()};

			if (factor == 2) mainStep.type = StepType::step2;
			if (factor == 3) mainStep.type = StepType::step3;
			if (factor == 4) mainStep.type = StepType::step4;

			// Twiddles
			bool foundStep = false;
			for (const Step &existingStep : plan) {
				if (existingStep.factor == mainStep.factor && existingStep.innerRepeats == mainStep.innerRepeats) {
					foundStep = true;
					mainStep.twiddleIndex = existingStep.twiddleIndex;
					break;
				}
			}
			if (!foundStep) {
				for (size_t i = 0; i < subLength; ++i) {
					for (size_t f = 0; f < factor; ++f) {
						double phase = 2*M_PI*i*f/length;
						complex twiddle = {V(std::cos(phase)), V(-std::sin(phase))};
						twiddleVector.push_back(twiddle);
					}
				}
			}

			if (repeats == 1 && sizeof(complex)*subLength > 65536) {
				for (size_t i = 0; i < factor; ++i) {
					addPlanSteps(factorIndex + 1, start + i*subLength, subLength, 1);
				}
			} else {
				addPlanSteps(factorIndex + 1, start, subLength, repeats*factor);
			}
			plan.push_back(mainStep);
		}
		void setPlan() {
			factors.resize(0);
			size_t size = _size, factor = 2;
			while (size > 1) {
				if (size%factor == 0) {
					factors.push_back(factor);
					size /= factor;
				} else if (factor > sqrt(size)) {
					factor = size;
				} else {
					++factor;
				}
			}

			plan.resize(0);
			twiddleVector.resize(0);
			addPlanSteps(0, 0, _size, 1);
			
			permutation.resize(0);
			permutation.push_back(PermutationPair{0, 0});
			size_t indexLow = 0, indexHigh = factors.size();
			size_t inputStepLow = _size, outputStepLow = 1;
			size_t inputStepHigh = 1, outputStepHigh = _size;
			while (outputStepLow*inputStepHigh < _size) {
				size_t f, inputStep, outputStep;
				if (outputStepLow <= inputStepHigh) {
					f = factors[indexLow++];
					inputStep = (inputStepLow /= f);
					outputStep = outputStepLow;
					outputStepLow *= f;
				} else {
					f = factors[--indexHigh];
					inputStep = inputStepHigh;
					inputStepHigh *= f;
					outputStep = (outputStepHigh /= f);
				}
				size_t oldSize = permutation.size();
				for (size_t i = 1; i < f; ++i) {
					for (size_t j = 0; j < oldSize; ++j) {
						PermutationPair pair = permutation[j];
						pair.from += i*inputStep;
						pair.to += i*outputStep;
						permutation.push_back(pair);
					}
				}
			}
		}

		template<bool inverse, typename RandomAccessIterator>
		void fftStepGeneric(RandomAccessIterator &&origData, const Step &step) {
			complex *working = workingVector.data();
			const size_t stride = step.innerRepeats;

			for (size_t outerRepeat = 0; outerRepeat < step.outerRepeats; ++outerRepeat) {
				RandomAccessIterator data = origData;
				
				const complex *twiddles = twiddleVector.data() + step.twiddleIndex;
				const size_t factor = step.factor;
				for (size_t repeat = 0; repeat < step.innerRepeats; ++repeat) {
					for (size_t i = 0; i < step.factor; ++i) {
						working[i] = _fft_impl::complexMul<inverse>(data[i*stride], twiddles[i]);
					}
					for (size_t f = 0; f < factor; ++f) {
						complex sum = working[0];
						for (size_t i = 1; i < factor; ++i) {
							double phase = 2*M_PI*f*i/factor;
							complex twiddle = {V(std::cos(phase)), V(-std::sin(phase))};
							sum += _fft_impl::complexMul<inverse>(working[i], twiddle);
						}
						data[f*stride] = sum;
					}
					++data;
					twiddles += factor;
				}
				origData += step.factor*step.innerRepeats;
			}
		}

		template<bool inverse, typename RandomAccessIterator>
		SIGNALSMITH_INLINE void fftStep2(RandomAccessIterator &&origData, const Step &step) {
			const size_t stride = step.innerRepeats;
			const complex *origTwiddles = twiddleVector.data() + step.twiddleIndex;
			for (size_t outerRepeat = 0; outerRepeat < step.outerRepeats; ++outerRepeat) {
				const complex* twiddles = origTwiddles;
				for (RandomAccessIterator data = origData; data < origData + stride; ++data) {
					complex A = data[0];
					complex B = _fft_impl::complexMul<inverse>(data[stride], twiddles[1]);
					
					data[0] = A + B;
					data[stride] = A - B;
					twiddles += 2;
				}
				origData += 2*stride;
			}
		}

		template<bool inverse, typename RandomAccessIterator>
		SIGNALSMITH_INLINE void fftStep3(RandomAccessIterator &&origData, const Step &step) {
			constexpr complex factor3 = {-0.5, inverse ? 0.8660254037844386 : -0.8660254037844386};
			const size_t stride = step.innerRepeats;
			const complex *origTwiddles = twiddleVector.data() + step.twiddleIndex;
			
			for (size_t outerRepeat = 0; outerRepeat < step.outerRepeats; ++outerRepeat) {
				const complex* twiddles = origTwiddles;
				for (RandomAccessIterator data = origData; data < origData + stride; ++data) {
					complex A = data[0];
					complex B = _fft_impl::complexMul<inverse>(data[stride], twiddles[1]);
					complex C = _fft_impl::complexMul<inverse>(data[stride*2], twiddles[2]);
					
					complex realSum = A + (B + C)*factor3.real();
					complex imagSum = (B - C)*factor3.imag();

					data[0] = A + B + C;
					data[stride] = _fft_impl::complexAddI<false>(realSum, imagSum);
					data[stride*2] = _fft_impl::complexAddI<true>(realSum, imagSum);

					twiddles += 3;
				}
				origData += 3*stride;
			}
		}

		template<bool inverse, typename RandomAccessIterator>
		SIGNALSMITH_INLINE void fftStep4(RandomAccessIterator &&origData, const Step &step) {
			const size_t stride = step.innerRepeats;
			const complex *origTwiddles = twiddleVector.data() + step.twiddleIndex;
			
			for (size_t outerRepeat = 0; outerRepeat < step.outerRepeats; ++outerRepeat) {
				const complex* twiddles = origTwiddles;
				for (RandomAccessIterator data = origData; data < origData + stride; ++data) {
					complex A = data[0];
					complex C = _fft_impl::complexMul<inverse>(data[stride], twiddles[2]);
					complex B = _fft_impl::complexMul<inverse>(data[stride*2], twiddles[1]);
					complex D = _fft_impl::complexMul<inverse>(data[stride*3], twiddles[3]);

					complex sumAC = A + C, sumBD = B + D;
					complex diffAC = A - C, diffBD = B - D;

					data[0] = sumAC + sumBD;
					data[stride] = _fft_impl::complexAddI<!inverse>(diffAC, diffBD);
					data[stride*2] = sumAC - sumBD;
					data[stride*3] = _fft_impl::complexAddI<inverse>(diffAC, diffBD);

					twiddles += 4;
				}
				origData += 4*stride;
			}
		}
		
		template<typename InputIterator, typename OutputIterator>
		void permute(InputIterator input, OutputIterator data) {
			for (auto pair : permutation) {
				data[pair.from] = input[pair.to];
			}
		}

		template<bool inverse, typename InputIterator, typename OutputIterator>
		void run(InputIterator &&input, OutputIterator &&data) {
			permute(input, data);
			
			for (const Step &step : plan) {
				switch (step.type) {
					case StepType::generic:
						fftStepGeneric<inverse>(data + step.startIndex, step);
						break;
					case StepType::step2:
						fftStep2<inverse>(data + step.startIndex, step);
						break;
					case StepType::step3:
						fftStep3<inverse>(data + step.startIndex, step);
						break;
					case StepType::step4:
						fftStep4<inverse>(data + step.startIndex, step);
						break;
				}
			}
		}

		static bool validSize(size_t size) {
			constexpr static bool filter[32] = {
				1, 1, 1, 1, 1, 0, 1, 0, 1, 1, // 0-9
				0, 0, 1, 0, 0, 0, 1, 0, 1, 0, // 10-19
				0, 0, 0, 0, 1, 0, 0, 0, 0, 0, // 20-29
				0, 0
			};
			return filter[size];
		}
	public:
		static size_t fastSizeAbove(size_t size) {
			size_t power2 = 1;
			while (size >= 32) {
				size = (size - 1)/2 + 1;
				power2 *= 2;
			}
			while (size < 32 && !validSize(size)) {
				++size;
			}
			return power2*size;
		}
		static size_t fastSizeBelow(size_t size) {
			size_t power2 = 1;
			while (size >= 32) {
				size /= 2;
				power2 *= 2;
			}
			while (size > 1 && !validSize(size)) {
				--size;
			}
			return power2*size;
		}

		FFT(size_t size, int fastDirection=0) : _size(0) {
			if (fastDirection > 0) size = fastSizeAbove(size);
			if (fastDirection < 0) size = fastSizeBelow(size);
			this->setSize(size);
		}

		size_t setSize(size_t size) {
			if (size != _size) {
				_size = size;
				workingVector.resize(size);
				setPlan();
			}
			return _size;
		}
		size_t setFastSizeAbove(size_t size) {
			return setSize(fastSizeAbove(size));
		}
		size_t setFastSizeBelow(size_t size) {
			return setSize(fastSizeBelow(size));
		}
		const size_t & size() const {
			return _size;
		}

		template<typename InputIterator, typename OutputIterator>
		void fft(InputIterator &&input, OutputIterator &&output) {
			auto inputIter = _fft_impl::GetIterator<InputIterator>::get(input);
			auto outputIter = _fft_impl::GetIterator<OutputIterator>::get(output);
			return run<false>(inputIter, outputIter);
		}

		template<typename InputIterator, typename OutputIterator>
		void ifft(InputIterator &&input, OutputIterator &&output) {
			auto inputIter = _fft_impl::GetIterator<InputIterator>::get(input);
			auto outputIter = _fft_impl::GetIterator<OutputIterator>::get(output);
			return run<true>(inputIter, outputIter);
		}
	};

	struct FFTOptions {
		static constexpr int halfFreqShift = 1;
	};

	template<typename V, int optionFlags=0>
	class RealFFT {
		static constexpr bool modified = (optionFlags&FFTOptions::halfFreqShift);

		using complex = std::complex<V>;
		std::vector<complex> complexBuffer1, complexBuffer2;
		std::vector<complex> twiddlesMinusI;
		std::vector<complex> modifiedRotations;
		FFT<V> complexFft;
	public:
		static size_t fastSizeAbove(size_t size) {
			return FFT<V>::fastSizeAbove((size + 1)/2)*2;
		}
		static size_t fastSizeBelow(size_t size) {
			return FFT<V>::fastSizeBelow(size/2)*2;
		}

		RealFFT(size_t size=0, int fastDirection=0) : complexFft(0) {
			if (fastDirection > 0) size = fastSizeAbove(size);
			if (fastDirection < 0) size = fastSizeBelow(size);
			this->setSize(std::max<size_t>(size, 2));
		}

		size_t setSize(size_t size) {
			complexBuffer1.resize(size/2);
			complexBuffer2.resize(size/2);

			size_t hhSize = size/4 + 1;
			twiddlesMinusI.resize(hhSize);
			for (size_t i = 0; i < hhSize; ++i) {
				V rotPhase = -2*M_PI*(modified ? i + 0.5 : i)/size;
				twiddlesMinusI[i] = {std::sin(rotPhase), -std::cos(rotPhase)};
			}
			if (modified) {
				modifiedRotations.resize(size/2);
				for (size_t i = 0; i < size/2; ++i) {
					V rotPhase = -2*M_PI*i/size;
					modifiedRotations[i] = {std::cos(rotPhase), std::sin(rotPhase)};
				}
			}
			
			return complexFft.setSize(size/2);
		}
		size_t setFastSizeAbove(size_t size) {
			return setSize(fastSizeAbove(size));
		}
		size_t setFastSizeBelow(size_t size) {
			return setSize(fastSizeBelow(size));
		}
		size_t size() const {
			return complexFft.size()*2;
		}

		template<typename InputIterator, typename OutputIterator>
		void fft(InputIterator &&input, OutputIterator &&output) {
			size_t hSize = complexFft.size();
			for (size_t i = 0; i < hSize; ++i) {
				if (modified) {
					complexBuffer1[i] = _fft_impl::complexMul<false>({input[2*i], input[2*i + 1]}, modifiedRotations[i]);
				} else {
					complexBuffer1[i] = {input[2*i], input[2*i + 1]};
				}
			}
			
			complexFft.fft(complexBuffer1.data(), complexBuffer2.data());
			
			if (!modified) output[0] = {
				complexBuffer2[0].real() + complexBuffer2[0].imag(),
				complexBuffer2[0].real() - complexBuffer2[0].imag()
			};
			for (size_t i = modified ? 0 : 1; i <= hSize/2; ++i) {
				size_t conjI = modified ? (hSize  - 1 - i) : (hSize - i);
				
				complex odd = (complexBuffer2[i] + conj(complexBuffer2[conjI]))*(V)0.5;
				complex evenI = (complexBuffer2[i] - conj(complexBuffer2[conjI]))*(V)0.5;
				complex evenRotMinusI = _fft_impl::complexMul<false>(evenI, twiddlesMinusI[i]);

				output[i] = odd + evenRotMinusI;
				output[conjI] = conj(odd - evenRotMinusI);
			}
		}

		template<typename InputIterator, typename OutputIterator>
		void ifft(InputIterator &&input, OutputIterator &&output) {
			size_t hSize = complexFft.size();
			if (!modified) complexBuffer1[0] = {
				input[0].real() + input[0].imag(),
				input[0].real() - input[0].imag()
			};
			for (size_t i = modified ? 0 : 1; i <= hSize/2; ++i) {
				size_t conjI = modified ? (hSize  - 1 - i) : (hSize - i);
				complex v = input[i], v2 = input[conjI];

				complex odd = v + conj(v2);
				complex evenRotMinusI = v - conj(v2);
				complex evenI = _fft_impl::complexMul<true>(evenRotMinusI, twiddlesMinusI[i]);
				
				complexBuffer1[i] = odd + evenI;
				complexBuffer1[conjI] = conj(odd - evenI);
			}
			
			complexFft.ifft(complexBuffer1.data(), complexBuffer2.data());
			
			for (size_t i = 0; i < hSize; ++i) {
				complex v = complexBuffer2[i];
				if (modified) v = _fft_impl::complexMul<true>(v, modifiedRotations[i]);
				output[2*i] = v.real();
				output[2*i + 1] = v.imag();
			}
		}
	};

	template<typename V>
	struct ModifiedRealFFT : public RealFFT<V, FFTOptions::halfFreqShift> {
		using RealFFT<V, FFTOptions::halfFreqShift>::RealFFT;
	};

/// @}
}} // namespace
#endif // include guard

#ifndef SIGNALSMITH_DSP_WINDOWS_H
#define SIGNALSMITH_DSP_WINDOWS_H

#ifndef SIGNALSMITH_DSP_COMMON_H
#define SIGNALSMITH_DSP_COMMON_H

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

namespace signalsmith {
	/**	@defgroup Common Common
		@brief Definitions and helper classes used by the rest of the library
		
		@{
		@file
	*/

#define SIGNALSMITH_DSP_VERSION_MAJOR 1
#define SIGNALSMITH_DSP_VERSION_MINOR 3
#define SIGNALSMITH_DSP_VERSION_PATCH 3
#define SIGNALSMITH_DSP_VERSION_STRING "1.3.3"

	/** Version compatability check.
	\code{.cpp}
		static_assert(signalsmith::version(1, 0, 0), "version check");
	\endcode
	... or use the equivalent `SIGNALSMITH_DSP_VERSION_CHECK`.
	Major versions are not compatible with each other.  Minor and patch versions are backwards-compatible.
	*/
	constexpr bool versionCheck(int major, int minor, int patch=0) {
		return major == SIGNALSMITH_DSP_VERSION_MAJOR
			&& (SIGNALSMITH_DSP_VERSION_MINOR > minor
				|| (SIGNALSMITH_DSP_VERSION_MINOR == minor && SIGNALSMITH_DSP_VERSION_PATCH >= patch));
	}

/// Check the library version is compatible (semver).
#define SIGNALSMITH_DSP_VERSION_CHECK(major, minor, patch) \
	static_assert(::signalsmith::versionCheck(major, minor, patch), "signalsmith library version is " SIGNALSMITH_DSP_VERSION_STRING);

/** @} */
} // signalsmith::
#endif // include guard

#include <cmath>

namespace signalsmith {
namespace windows {
	/**	@defgroup Windows Window functions
		@brief Windows for spectral analysis
		
		These are generally double-precision, because they are mostly calculated during setup/reconfiguring, not real-time code.
		
		@{
		@file
	*/
	
	/** @brief The Kaiser window (almost) maximises the energy in the main-lobe compared to the side-lobes.
		
		Kaiser windows can be constructing using the shape-parameter (beta) or using the static `with???()` methods.*/
	class Kaiser {
		// I_0(x)=\sum_{k=0}^{N}\frac{x^{2k}}{(k!)^2\cdot4^k}
		inline static double bessel0(double x) {
			const double significanceLimit = 1e-4;
			double result = 0;
			double term = 1;
			double m = 0;
			while (term > significanceLimit) {
				result += term;
				++m;
				term *= (x*x)/(4*m*m);
			}

			return result;
		}
		double beta;
		double invB0;
		
		static double heuristicBandwidth(double bandwidth) {
			// Good peaks
			//return bandwidth + 8/((bandwidth + 3)*(bandwidth + 3));
			// Good average
			//return bandwidth + 14/((bandwidth + 2.5)*(bandwidth + 2.5));
			// Compromise
			return bandwidth + 8/((bandwidth + 3)*(bandwidth + 3)) + 0.25*std::max(3 - bandwidth, 0.0);
		}
	public:
		/// Set up a Kaiser window with a given shape.  `beta` is `pi*alpha` (since there is ambiguity about shape parameters)
		Kaiser(double beta) : beta(beta), invB0(1/bessel0(beta)) {}

		/// @name Bandwidth methods
		/// @{
		static Kaiser withBandwidth(double bandwidth, bool heuristicOptimal=false) {
			return Kaiser(bandwidthToBeta(bandwidth, heuristicOptimal));
		}

		/** Returns the Kaiser shape where the main lobe has the specified bandwidth (as a factor of 1/window-length).
		\diagram{kaiser-windows.svg,You can see that the main lobe matches the specified bandwidth.}
		If `heuristicOptimal` is enabled, the main lobe width is _slightly_ wider, improving both the peak and total energy - see `bandwidthToEnergyDb()` and `bandwidthToPeakDb()`.
		\diagram{kaiser-windows-heuristic.svg, The main lobe extends to ±bandwidth/2.} */
		static double bandwidthToBeta(double bandwidth, bool heuristicOptimal=false) {
			if (heuristicOptimal) { // Heuristic based on numerical search
				bandwidth = heuristicBandwidth(bandwidth);
			}
			bandwidth = std::max(bandwidth, 2.0);
			double alpha = std::sqrt(bandwidth*bandwidth*0.25 - 1);
			return alpha*M_PI;
		}
		
		static double betaToBandwidth(double beta) {
			double alpha = beta*(1.0/M_PI);
			return 2*std::sqrt(alpha*alpha + 1);
		}
		/// @}

		/// @name Performance methods
		/// @{
		/** @brief Total energy ratio (in dB) between side-lobes and the main lobe.
			\diagram{kaiser-bandwidth-sidelobes-energy.svg,Measured main/side lobe energy ratio.  You can see that the heuristic improves performance for all bandwidth values.}
			This function uses an approximation which is accurate to ±0.5dB for 2 ⩽ bandwidth ≤ 10, or 1 ⩽ bandwidth ≤ 10 when `heuristicOptimal`is enabled.
		*/
		static double bandwidthToEnergyDb(double bandwidth, bool heuristicOptimal=false) {
			// Horrible heuristic fits
			if (heuristicOptimal) {
				if (bandwidth < 3) bandwidth += (3 - bandwidth)*0.5;
				return 12.9 + -3/(bandwidth + 0.4) - 13.4*bandwidth + (bandwidth < 3)*-9.6*(bandwidth - 3);
			}
			return 10.5 + 15/(bandwidth + 0.4) - 13.25*bandwidth + (bandwidth < 2)*13*(bandwidth - 2);
		}
		static double energyDbToBandwidth(double energyDb, bool heuristicOptimal=false) {
			double bw = 1;
			while (bw < 20 && bandwidthToEnergyDb(bw, heuristicOptimal) > energyDb) {
				bw *= 2;
			}
			double step = bw/2;
			while (step > 0.0001) {
				if (bandwidthToEnergyDb(bw, heuristicOptimal) > energyDb) {
					bw += step;
				} else {
					bw -= step;
				}
				step *= 0.5;
			}
			return bw;
		}
		/** @brief Peak ratio (in dB) between side-lobes and the main lobe.
			\diagram{kaiser-bandwidth-sidelobes-peak.svg,Measured main/side lobe peak ratio.  You can see that the heuristic improves performance, except in the bandwidth range 1-2 where peak ratio was sacrificed to improve total energy ratio.}
			This function uses an approximation which is accurate to ±0.5dB for 2 ⩽ bandwidth ≤ 9, or 0.5 ⩽ bandwidth ≤ 9 when `heuristicOptimal`is enabled.
		*/
		static double bandwidthToPeakDb(double bandwidth, bool heuristicOptimal=false) {
			// Horrible heuristic fits
			if (heuristicOptimal) {
				return 14.2 - 20/(bandwidth + 1) - 13*bandwidth + (bandwidth < 3)*-6*(bandwidth - 3) + (bandwidth < 2.25)*5.8*(bandwidth - 2.25);
			}
			return 10 + 8/(bandwidth + 2) - 12.75*bandwidth + (bandwidth < 2)*4*(bandwidth - 2);
		}
		static double peakDbToBandwidth(double peakDb, bool heuristicOptimal=false) {
			double bw = 1;
			while (bw < 20 && bandwidthToPeakDb(bw, heuristicOptimal) > peakDb) {
				bw *= 2;
			}
			double step = bw/2;
			while (step > 0.0001) {
				if (bandwidthToPeakDb(bw, heuristicOptimal) > peakDb) {
					bw += step;
				} else {
					bw -= step;
				}
				step *= 0.5;
			}
			return bw;
		}
		/** @} */

		/** Equivalent noise bandwidth (ENBW), a measure of frequency resolution.
			\diagram{kaiser-bandwidth-enbw.svg,Measured ENBW, with and without the heuristic bandwidth adjustment.}
			This approximation is accurate to ±0.05 up to a bandwidth of 22.
		*/
		static double bandwidthToEnbw(double bandwidth, bool heuristicOptimal=false) {
			if (heuristicOptimal) bandwidth = heuristicBandwidth(bandwidth);
			double b2 = std::max<double>(bandwidth - 2, 0);
			return 1 + b2*(0.2 + b2*(-0.005 + b2*(-0.000005 + b2*0.0000022)));
		}

		/// Return the window's value for position in the range [0, 1]
		double operator ()(double unit) {
			double r = 2*unit - 1;
			double arg = std::sqrt(1 - r*r);
			return bessel0(beta*arg)*invB0;
		}
	
		/// Fills an arbitrary container with a Kaiser window
		template<typename Data>
		void fill(Data &data, int size) const {
			double invSize = 1.0/size;
			for (int i = 0; i < size; ++i) {
				double r = (2*i + 1)*invSize - 1;
				double arg = std::sqrt(1 - r*r);
				data[i] = bessel0(beta*arg)*invB0;
			}
		}
	};

	/** Forces STFT perfect-reconstruction (WOLA) on an existing window, for a given STFT interval.
	For example, here are perfect-reconstruction versions of the approximately-optimal @ref Kaiser windows:
	\diagram{kaiser-windows-heuristic-pr.svg,Note the lower overall energy\, and the pointy top for 2x bandwidth. Spectral performance is about the same\, though.}
	*/
	template<typename Data>
	void forcePerfectReconstruction(Data &data, int windowLength, int interval) {
		for (int i = 0; i < interval; ++i) {
			double sum2 = 0;
			for (int index = i; index < windowLength; index += interval) {
				sum2 += data[index]*data[index];
			}
			double factor = 1/std::sqrt(sum2);
			for (int index = i; index < windowLength; index += interval) {
				data[index] *= factor;
			}
		}
	}

/** @} */
}} // signalsmith::windows
#endif // include guard

namespace signalsmith {
namespace delay {
	/**	@defgroup Delay Delay utilities
		@brief Standalone templated classes for delays
		
		You can set up a `Buffer` or `MultiBuffer`, and get interpolated samples using a `Reader` (separately on each channel in the multi-channel case) - or you can use `Delay`/`MultiDelay` which include their own buffers.

		Interpolation quality is chosen using a template class, from @ref Interpolators.

		@{
		@file
	*/

	/** @brief Single-channel delay buffer
 
		Access is used with `buffer[]`, relative to the internal read/write position ("head").  This head is moved using `++buffer` (or `buffer += n`), such that `buffer[1] == (buffer + 1)[0]` in a similar way iterators/pointers.
		
		Operations like `buffer - 10` or `buffer++` return a View, which holds a fixed position in the buffer (based on the read/write position at the time).
		
		The capacity includes both positive and negative indices.  For example, a capacity of 100 would support using any of the ranges:
		
		* `buffer[-99]` to buffer[0]`
		* `buffer[-50]` to buffer[49]`
		* `buffer[0]` to buffer[99]`

		Although buffers are usually used with historical samples accessed using negative indices e.g. `buffer[-10]`, you could equally use it flipped around (moving the head backwards through the buffer using `--buffer`).
	*/
	template<typename Sample>
	class Buffer {
		unsigned bufferIndex;
		unsigned bufferMask;
		std::vector<Sample> buffer;
	public:
		Buffer(int minCapacity=0) {
			resize(minCapacity);
		}
		// We shouldn't accidentally copy a delay buffer
		Buffer(const Buffer &other) = delete;
		Buffer & operator =(const Buffer &other) = delete;
		// But moving one is fine
		Buffer(Buffer &&other) = default;
		Buffer & operator =(Buffer &&other) = default;

		void resize(int minCapacity, Sample value=Sample()) {
			int bufferLength = 1;
			while (bufferLength < minCapacity) bufferLength *= 2;
			buffer.assign(bufferLength, value);
			bufferMask = unsigned(bufferLength - 1);
			bufferIndex = 0;
		}
		void reset(Sample value=Sample()) {
			buffer.assign(buffer.size(), value);
		}

		/// Holds a view for a particular position in the buffer
		template<bool isConst>
		class View {
			using CBuffer = typename std::conditional<isConst, const Buffer, Buffer>::type;
			using CSample = typename std::conditional<isConst, const Sample, Sample>::type;
			CBuffer *buffer = nullptr;
			unsigned bufferIndex = 0;
		public:
			View(CBuffer &buffer, int offset=0) : buffer(&buffer), bufferIndex(buffer.bufferIndex + (unsigned)offset) {}
			View(const View &other, int offset=0) : buffer(other.buffer), bufferIndex(other.bufferIndex + (unsigned)offset) {}
			View & operator =(const View &other) {
				buffer = other.buffer;
				bufferIndex = other.bufferIndex;
				return *this;
			}
			
			CSample & operator[](int offset) {
				return buffer->buffer[(bufferIndex + (unsigned)offset)&buffer->bufferMask];
			}
			const Sample & operator[](int offset) const {
				return buffer->buffer[(bufferIndex + (unsigned)offset)&buffer->bufferMask];
			}

			/// Write data into the buffer
			template<typename Data>
			void write(Data &&data, int length) {
				for (int i = 0; i < length; ++i) {
					(*this)[i] = data[i];
				}
			}
			/// Read data out from the buffer
			template<typename Data>
			void read(int length, Data &&data) const {
				for (int i = 0; i < length; ++i) {
					data[i] = (*this)[i];
				}
			}

			View operator +(int offset) const {
				return View(*this, offset);
			}
			View operator -(int offset) const {
				return View(*this, -offset);
			}
		};
		using MutableView = View<false>;
		using ConstView = View<true>;
		
		MutableView view(int offset=0) {
			return MutableView(*this, offset);
		}
		ConstView view(int offset=0) const {
			return ConstView(*this, offset);
		}
		ConstView constView(int offset=0) const {
			return ConstView(*this, offset);
		}

		Sample & operator[](int offset) {
			return buffer[(bufferIndex + (unsigned)offset)&bufferMask];
		}
		const Sample & operator[](int offset) const {
			return buffer[(bufferIndex + (unsigned)offset)&bufferMask];
		}

		/// Write data into the buffer
		template<typename Data>
		void write(Data &&data, int length) {
			for (int i = 0; i < length; ++i) {
				(*this)[i] = data[i];
			}
		}
		/// Read data out from the buffer
		template<typename Data>
		void read(int length, Data &&data) const {
			for (int i = 0; i < length; ++i) {
				data[i] = (*this)[i];
			}
		}
		
		Buffer & operator ++() {
			++bufferIndex;
			return *this;
		}
		Buffer & operator +=(int i) {
			bufferIndex += (unsigned)i;
			return *this;
		}
		Buffer & operator --() {
			--bufferIndex;
			return *this;
		}
		Buffer & operator -=(int i) {
			bufferIndex -= (unsigned)i;
			return *this;
		}

		MutableView operator ++(int) {
			MutableView view(*this);
			++bufferIndex;
			return view;
		}
		MutableView operator +(int i) {
			return MutableView(*this, i);
		}
		ConstView operator +(int i) const {
			return ConstView(*this, i);
		}
		MutableView operator --(int) {
			MutableView view(*this);
			--bufferIndex;
			return view;
		}
		MutableView operator -(int i) {
			return MutableView(*this, -i);
		}
		ConstView operator -(int i) const {
			return ConstView(*this, -i);
		}
	};

	/** @brief Multi-channel delay buffer

		This behaves similarly to the single-channel `Buffer`, with the following differences:
		
		* `buffer[c]` returns a view for a single channel, which behaves like the single-channel `Buffer::View`.
		* The constructor and `.resize()` take an additional first `channel` argument.
	*/
	template<typename Sample>
	class MultiBuffer {
		int channels, stride;
		Buffer<Sample> buffer;
	public:
		using ConstChannel = typename Buffer<Sample>::ConstView;
		using MutableChannel = typename Buffer<Sample>::MutableView;

		MultiBuffer(int channels=0, int capacity=0) : channels(channels), stride(capacity), buffer(channels*capacity) {}

		void resize(int nChannels, int capacity, Sample value=Sample()) {
			channels = nChannels;
			stride = capacity;
			buffer.resize(channels*capacity, value);
		}
		void reset(Sample value=Sample()) {
			buffer.reset(value);
		}

		/// A reference-like multi-channel result for a particular sample index
		template<bool isConst>
		class Stride {
			using CChannel = typename std::conditional<isConst, ConstChannel, MutableChannel>::type;
			using CSample = typename std::conditional<isConst, const Sample, Sample>::type;
			CChannel view;
			int channels, stride;
		public:
			Stride(CChannel view, int channels, int stride) : view(view), channels(channels), stride(stride) {}
			Stride(const Stride &other) : view(other.view), channels(other.channels), stride(other.stride) {}
			
			CSample & operator[](int channel) {
				return view[channel*stride];
			}
			const Sample & operator[](int channel) const {
				return view[channel*stride];
			}

			/// Reads from the buffer into a multi-channel result
			template<class Data>
			void get(Data &&result) const {
				for (int c = 0; c < channels; ++c) {
					result[c] = view[c*stride];
				}
			}
			/// Writes from multi-channel data into the buffer
			template<class Data>
			void set(Data &&data) {
				for (int c = 0; c < channels; ++c) {
					view[c*stride] = data[c];
				}
			}
			template<class Data>
			Stride & operator =(const Data &data) {
				set(data);
				return *this;
			}
			Stride & operator =(const Stride &data) {
				set(data);
				return *this;
			}
		};
		
		Stride<false> at(int offset) {
			return {buffer.view(offset), channels, stride};
		}
		Stride<true> at(int offset) const {
			return {buffer.view(offset), channels, stride};
		}

		/// Holds a particular position in the buffer
		template<bool isConst>
		class View {
			using CChannel = typename std::conditional<isConst, ConstChannel, MutableChannel>::type;
			CChannel view;
			int channels, stride;
		public:
			View(CChannel view, int channels, int stride) : view(view), channels(channels), stride(stride) {}
			
			CChannel operator[](int channel) {
				return view + channel*stride;
			}
			ConstChannel operator[](int channel) const {
				return view + channel*stride;
			}

			Stride<isConst> at(int offset) {
				return {view + offset, channels, stride};
			}
			Stride<true> at(int offset) const {
				return {view + offset, channels, stride};
			}
		};
		using MutableView = View<false>;
		using ConstView = View<true>;

		MutableView view(int offset=0) {
			return MutableView(buffer.view(offset), channels, stride);
		}
		ConstView view(int offset=0) const {
			return ConstView(buffer.view(offset), channels, stride);
		}
		ConstView constView(int offset=0) const {
			return ConstView(buffer.view(offset), channels, stride);
		}

		MutableChannel operator[](int channel) {
			return buffer + channel*stride;
		}
		ConstChannel operator[](int channel) const {
			return buffer + channel*stride;
		}
		
		MultiBuffer & operator ++() {
			++buffer;
			return *this;
		}
		MultiBuffer & operator +=(int i) {
			buffer += i;
			return *this;
		}
		MutableView operator ++(int) {
			return MutableView(buffer++, channels, stride);
		}
		MutableView operator +(int i) {
			return MutableView(buffer + i, channels, stride);
		}
		ConstView operator +(int i) const {
			return ConstView(buffer + i, channels, stride);
		}
		MultiBuffer & operator --() {
			--buffer;
			return *this;
		}
		MultiBuffer & operator -=(int i) {
			buffer -= i;
			return *this;
		}
		MutableView operator --(int) {
			return MutableView(buffer--, channels, stride);
		}
		MutableView operator -(int i) {
			return MutableView(buffer - i, channels, stride);
		}
		ConstView operator -(int i) const {
			return ConstView(buffer - i, channels, stride);
		}
	};
	
	/** \defgroup Interpolators Interpolators
		\ingroup Delay
		@{ */
	/// Nearest-neighbour interpolator
	/// \diagram{delay-random-access-nearest.svg,aliasing and maximum amplitude/delay errors for different input frequencies}
	template<typename Sample>
	struct InterpolatorNearest {
		static constexpr int inputLength = 1;
		static constexpr Sample latency = -0.5; // Because we're truncating, which rounds down too often
	
		template<class Data>
		static Sample fractional(const Data &data, Sample) {
			return data[0];
		}
	};
	/// Linear interpolator
	/// \diagram{delay-random-access-linear.svg,aliasing and maximum amplitude/delay errors for different input frequencies}
	template<typename Sample>
	struct InterpolatorLinear {
		static constexpr int inputLength = 2;
		static constexpr int latency = 0;
	
		template<class Data>
		static Sample fractional(const Data &data, Sample fractional) {
			Sample a = data[0], b = data[1];
			return a + fractional*(b - a);
		}
	};
	/// Spline cubic interpolator
	/// \diagram{delay-random-access-cubic.svg,aliasing and maximum amplitude/delay errors for different input frequencies}
	template<typename Sample>
	struct InterpolatorCubic {
		static constexpr int inputLength = 4;
		static constexpr int latency = 1;
	
		template<class Data>
		static Sample fractional(const Data &data, Sample fractional) {
			// Cubic interpolation
			Sample a = data[0], b = data[1], c = data[2], d = data[3];
			Sample cbDiff = c - b;
			Sample k1 = (c - a)*0.5;
			Sample k3 = k1 + (d - b)*0.5 - cbDiff*2;
			Sample k2 = cbDiff - k3 - k1;
			return b + fractional*(k1 + fractional*(k2 + fractional*k3)); // 16 ops total, not including the indexing
		}
	};

	// Efficient Algorithms and Structures for Fractional Delay Filtering Based on Lagrange Interpolation
	// Franck 2009 https://www.aes.org/e-lib/browse.cfm?elib=14647
	namespace _franck_impl {
		template<typename Sample, int n, int low, int high>
		struct ProductRange {
			using Array = std::array<Sample, (n + 1)>;
			static constexpr int mid = (low + high)/2;
			using Left = ProductRange<Sample, n, low, mid>;
			using Right = ProductRange<Sample, n, mid + 1, high>;

			Left left;
			Right right;

			const Sample total;
			ProductRange(Sample x) : left(x), right(x), total(left.total*right.total) {}

			template<class Data>
			Sample calculateResult(Sample extraFactor, const Data &data, const Array &invFactors) {
				return left.calculateResult(extraFactor*right.total, data, invFactors)
					+ right.calculateResult(extraFactor*left.total, data, invFactors);
			}
		};
		template<typename Sample, int n, int index>
		struct ProductRange<Sample, n, index, index> {
			using Array = std::array<Sample, (n + 1)>;

			const Sample total;
			ProductRange(Sample x) : total(x - index) {}

			template<class Data>
			Sample calculateResult(Sample extraFactor, const Data &data, const Array &invFactors) {
				return extraFactor*data[index]*invFactors[index];
			}
		};
	}
	/** Fixed-order Lagrange interpolation.
	\diagram{interpolator-LagrangeN.svg,aliasing and amplitude/delay errors for different sizes}
	*/
	template<typename Sample, int n>
	struct InterpolatorLagrangeN {
		static constexpr int inputLength = n + 1;
		static constexpr int latency = (n - 1)/2;

		using Array = std::array<Sample, (n + 1)>;
		Array invDivisors;

		InterpolatorLagrangeN() {
			for (int j = 0; j <= n; ++j) {
				double divisor = 1;
				for (int k = 0; k < j; ++k) divisor *= (j - k);
				for (int k = j + 1; k <= n; ++k) divisor *= (j - k);
				invDivisors[j] = 1/divisor;
			}
		}

		template<class Data>
		Sample fractional(const Data &data, Sample fractional) const {
			constexpr int mid = n/2;
			using Left = _franck_impl::ProductRange<Sample, n, 0, mid>;
			using Right = _franck_impl::ProductRange<Sample, n, mid + 1, n>;

			Sample x = fractional + latency;

			Left left(x);
			Right right(x);

			return left.calculateResult(right.total, data, invDivisors) + right.calculateResult(left.total, data, invDivisors);
		}
	};
	template<typename Sample>
	using InterpolatorLagrange3 = InterpolatorLagrangeN<Sample, 3>;
	template<typename Sample>
	using InterpolatorLagrange7 = InterpolatorLagrangeN<Sample, 7>;
	template<typename Sample>
	using InterpolatorLagrange19 = InterpolatorLagrangeN<Sample, 19>;

	/** Fixed-size Kaiser-windowed sinc interpolation.
	\diagram{interpolator-KaiserSincN.svg,aliasing and amplitude/delay errors for different sizes}
	If `minimumPhase` is enabled, a minimum-phase version of the kernel is used:
	\diagram{interpolator-KaiserSincN-min.svg,aliasing and amplitude/delay errors for minimum-phase mode}
	*/
	template<typename Sample, int n, bool minimumPhase=false>
	struct InterpolatorKaiserSincN {
		static constexpr int inputLength = n;
		static constexpr Sample latency = minimumPhase ? 0 : (n*Sample(0.5) - 1);

		int subSampleSteps;
		std::vector<Sample> coefficients;
		
		InterpolatorKaiserSincN() : InterpolatorKaiserSincN(0.5 - 0.45/std::sqrt(n)) {}
		InterpolatorKaiserSincN(double passFreq) : InterpolatorKaiserSincN(passFreq, 1 - passFreq) {}
		InterpolatorKaiserSincN(double passFreq, double stopFreq) {
			subSampleSteps = 2*n; // Heuristic again.  Really it depends on the bandwidth as well.
			double kaiserBandwidth = (stopFreq - passFreq)*(n + 1.0/subSampleSteps);
			kaiserBandwidth += 1.25/kaiserBandwidth; // We want to place the first zero, but (because using this to window a sinc essentially integrates it in the freq-domain), our ripples (and therefore zeroes) are out of phase.  This is a heuristic fix.
			
			double centreIndex = n*subSampleSteps*0.5, scaleFactor = 1.0/subSampleSteps;
			std::vector<Sample> windowedSinc(subSampleSteps*n + 1);
			
			::signalsmith::windows::Kaiser::withBandwidth(kaiserBandwidth, false).fill(windowedSinc, windowedSinc.size());

			for (size_t i = 0; i < windowedSinc.size(); ++i) {
				double x = (i - centreIndex)*scaleFactor;
				int intX = std::round(x);
				if (intX != 0 && std::abs(x - intX) < 1e-6) {
					// Exact 0s
					windowedSinc[i] = 0;
				} else if (std::abs(x) > 1e-6) {
					double p = x*M_PI;
					windowedSinc[i] *= std::sin(p)/p;
				}
			}
			
			if (minimumPhase) {
				signalsmith::fft::FFT<Sample> fft(windowedSinc.size()*2, 1);
				windowedSinc.resize(fft.size(), 0);
				std::vector<std::complex<Sample>> spectrum(fft.size());
				std::vector<std::complex<Sample>> cepstrum(fft.size());
				fft.fft(windowedSinc, spectrum);
				for (size_t i = 0; i < fft.size(); ++i) {
					spectrum[i] = std::log(std::abs(spectrum[i]) + 1e-30);
				}
				fft.fft(spectrum, cepstrum);
				for (size_t i = 1; i < fft.size()/2; ++i) {
					cepstrum[i] *= 0;
				}
				for (size_t i = fft.size()/2 + 1; i < fft.size(); ++i) {
					cepstrum[i] *= 2;
				}
				Sample scaling = Sample(1)/fft.size();
				fft.ifft(cepstrum, spectrum);

				for (size_t i = 0; i < fft.size(); ++i) {
					Sample phase = spectrum[i].imag()*scaling;
					Sample mag = std::exp(spectrum[i].real()*scaling);
					spectrum[i] = {mag*std::cos(phase), mag*std::sin(phase)};
				}
				fft.ifft(spectrum, cepstrum);
				windowedSinc.resize(subSampleSteps*n + 1);
				windowedSinc.shrink_to_fit();
				for (size_t i = 0; i < windowedSinc.size(); ++i) {
					windowedSinc[i] = cepstrum[i].real()*scaling;
				}
			}
			
			// Re-order into FIR fractional-delay blocks
			coefficients.resize(n*(subSampleSteps + 1));
			for (int k = 0; k <= subSampleSteps; ++k) {
				for (int i = 0; i < n; ++i) {
					coefficients[k*n + i] = windowedSinc[(subSampleSteps - k) + i*subSampleSteps];
				}
			}
		}
		
		template<class Data>
		Sample fractional(const Data &data, Sample fractional) const {
			Sample subSampleDelay = fractional*subSampleSteps;
			int lowIndex = subSampleDelay;
			if (lowIndex >= subSampleSteps) lowIndex = subSampleSteps - 1;
			Sample subSampleFractional = subSampleDelay - lowIndex;
			int highIndex = lowIndex + 1;
			
			Sample sumLow = 0, sumHigh = 0;
			const Sample *coeffLow = coefficients.data() + lowIndex*n;
			const Sample *coeffHigh = coefficients.data() + highIndex*n;
			for (int i = 0; i < n; ++i) {
				sumLow += data[i]*coeffLow[i];
				sumHigh += data[i]*coeffHigh[i];
			}
			return sumLow + (sumHigh - sumLow)*subSampleFractional;
		}
	};

	template<typename Sample>
	using InterpolatorKaiserSinc20 = InterpolatorKaiserSincN<Sample, 20>;
	template<typename Sample>
	using InterpolatorKaiserSinc8 = InterpolatorKaiserSincN<Sample, 8>;
	template<typename Sample>
	using InterpolatorKaiserSinc4 = InterpolatorKaiserSincN<Sample, 4>;

	template<typename Sample>
	using InterpolatorKaiserSinc20Min = InterpolatorKaiserSincN<Sample, 20, true>;
	template<typename Sample>
	using InterpolatorKaiserSinc8Min = InterpolatorKaiserSincN<Sample, 8, true>;
	template<typename Sample>
	using InterpolatorKaiserSinc4Min = InterpolatorKaiserSincN<Sample, 4, true>;
	///  @}
	
	/** @brief A delay-line reader which uses an external buffer
 
		This is useful if you have multiple delay-lines reading from the same buffer.
	*/
	template<class Sample, template<typename> class Interpolator=InterpolatorLinear>
	class Reader : public Interpolator<Sample> /* so we can get the empty-base-class optimisation */ {
		using Super = Interpolator<Sample>;
	public:
		Reader () {}
		/// Pass in a configured interpolator
		Reader (const Interpolator<Sample> &interpolator) : Super(interpolator) {}
	
		template<typename Buffer>
		Sample read(const Buffer &buffer, Sample delaySamples) const {
			int startIndex = delaySamples;
			Sample remainder = delaySamples - startIndex;
			
			// Delay buffers use negative indices, but interpolators use positive ones
			using View = decltype(buffer - startIndex);
			struct Flipped {
				 View view;
				 Sample operator [](int i) const {
					return view[-i];
				 }
			};
			return Super::fractional(Flipped{buffer - startIndex}, remainder);
		}
	};

	/**	@brief A single-channel delay-line containing its own buffer.*/
	template<class Sample, template<typename> class Interpolator=InterpolatorLinear>
	class Delay : private Reader<Sample, Interpolator> {
		using Super = Reader<Sample, Interpolator>;
		Buffer<Sample> buffer;
	public:
		static constexpr Sample latency = Super::latency;

		Delay(int capacity=0) : buffer(1 + capacity + Super::inputLength) {}
		/// Pass in a configured interpolator
		Delay(const Interpolator<Sample> &interp, int capacity=0) : Super(interp), buffer(1 + capacity + Super::inputLength) {}
		
		void reset(Sample value=Sample()) {
			buffer.reset(value);
		}
		void resize(int minCapacity, Sample value=Sample()) {
			buffer.resize(minCapacity + Super::inputLength, value);
		}
		
		/** Read a sample from `delaySamples` >= 0 in the past.
		The interpolator may add its own latency on top of this (see `Delay::latency`).  The default interpolation (linear) has 0 latency.
		*/
		Sample read(Sample delaySamples) const {
			return Super::read(buffer, delaySamples);
		}
		/// Writes a sample. Returns the same object, so that you can say `delay.write(v).read(delay)`.
		Delay & write(Sample value) {
			++buffer;
			buffer[0] = value;
			return *this;
		}
	};

	/**	@brief A multi-channel delay-line with its own buffer. */
	template<class Sample, template<typename> class Interpolator=InterpolatorLinear>
	class MultiDelay : private Reader<Sample, Interpolator> {
		using Super = Reader<Sample, Interpolator>;
		int channels;
		MultiBuffer<Sample> multiBuffer;
	public:
		static constexpr Sample latency = Super::latency;

		MultiDelay(int channels=0, int capacity=0) : channels(channels), multiBuffer(channels, 1 + capacity + Super::inputLength) {}

		void reset(Sample value=Sample()) {
			multiBuffer.reset(value);
		}
		void resize(int nChannels, int capacity, Sample value=Sample()) {
			channels = nChannels;
			multiBuffer.resize(channels, capacity + Super::inputLength, value);
		}
		
		/// A single-channel delay-line view, similar to a `const Delay`
		struct ChannelView {
			static constexpr Sample latency = Super::latency;

			const Super &reader;
			typename MultiBuffer<Sample>::ConstChannel channel;
			
			Sample read(Sample delaySamples) const {
				return reader.read(channel, delaySamples);
			}
		};
		ChannelView operator [](int channel) const {
			return ChannelView{*this, multiBuffer[channel]};
		}

		/// A multi-channel result, lazily calculating samples
		struct DelayView {
			Super &reader;
			typename MultiBuffer<Sample>::ConstView view;
			Sample delaySamples;
			
			// Calculate samples on-the-fly
			Sample operator [](int c) const {
				return reader.read(view[c], delaySamples);
			}
		};
		DelayView read(Sample delaySamples) {
			return DelayView{*this, multiBuffer.constView(), delaySamples};
		}
		/// Reads into the provided output structure
		template<class Output>
		void read(Sample delaySamples, Output &output) {
			for (int c = 0; c < channels; ++c) {
				output[c] = Super::read(multiBuffer[c], delaySamples);
			}
		}
		/// Reads separate delays for each channel
		template<class Delays, class Output>
		void readMulti(const Delays &delays, Output &output) {
			for (int c = 0; c < channels; ++c) {
				output[c] = Super::read(multiBuffer[c], delays[c]);
			}
		}
		template<class Data>
		MultiDelay & write(const Data &data) {
			++multiBuffer;
			for (int c = 0; c < channels; ++c) {
				multiBuffer[c][0] = data[c];
			}
			return *this;
		}
	};

/** @} */
}} // signalsmith::delay::
#endif // include guard

#ifndef SIGNALSMITH_DSP_PERF_H
#define SIGNALSMITH_DSP_PERF_H

#include <complex>

namespace signalsmith {
namespace perf {
	/**	@defgroup Performance Performance helpers
		@brief Nothing serious, just some `#defines` and helpers
		
		@{
		@file
	*/
		
	/// *Really* insist that a function/method is inlined (mostly for performance in DEBUG builds)
	#ifndef SIGNALSMITH_INLINE
	#ifdef __GNUC__
	#define SIGNALSMITH_INLINE __attribute__((always_inline)) inline
	#elif defined(__MSVC__)
	#define SIGNALSMITH_INLINE __forceinline inline
	#else
	#define SIGNALSMITH_INLINE inline
	#endif
	#endif

	/** @brief Complex-multiplication (with optional conjugate second-arg), without handling NaN/Infinity
		The `std::complex` multiplication has edge-cases around NaNs which slow things down and prevent auto-vectorisation.
	*/
	template <bool conjugateSecond=false, typename V>
	SIGNALSMITH_INLINE static std::complex<V> mul(const std::complex<V> &a, const std::complex<V> &b) {
		return conjugateSecond ? std::complex<V>{
			b.real()*a.real() + b.imag()*a.imag(),
			b.real()*a.imag() - b.imag()*a.real()
		} : std::complex<V>{
			a.real()*b.real() - a.imag()*b.imag(),
			a.real()*b.imag() + a.imag()*b.real()
		};
	}

/** @} */
}} // signalsmith::perf::

#endif // include guard

SIGNALSMITH_DSP_VERSION_CHECK(1, 3, 3); // Check version is compatible
#include <vector>
#include <algorithm>
#include <functional>
#include <random>

namespace signalsmith { namespace stretch {

template<typename Sample=float>
struct SignalsmithStretch {

	SignalsmithStretch() : randomEngine(std::random_device{}()) {}
	SignalsmithStretch(long seed) : randomEngine(seed) {}

	int blockSamples() const {
		return stft.windowSize();
	}
	int intervalSamples() const {
		return stft.interval();
	}
	int inputLatency() const {
		return stft.windowSize()/2;
	}
	int outputLatency() const {
		return stft.windowSize() - inputLatency();
	}
	
	void reset() {
		stft.reset();
		inputBuffer.reset();
		prevInputOffset = -1;
		channelBands.assign(channelBands.size(), Band());
		silenceCounter = 2*stft.windowSize();
	}

	// Configures using a default preset
	void presetDefault(int nChannels, Sample sampleRate) {
		configure(nChannels, sampleRate*0.12, sampleRate*0.03);
	}
	void presetCheaper(int nChannels, Sample sampleRate) {
		configure(nChannels, sampleRate*0.1, sampleRate*0.04);
	}

	// Manual setup
	void configure(int nChannels, int blockSamples, int intervalSamples) {
		channels = nChannels;
		stft.resize(channels, blockSamples, intervalSamples);
		bands = stft.bands();
		inputBuffer.resize(channels, blockSamples + intervalSamples + 1);
		timeBuffer.assign(stft.fftSize(), 0);
		channelBands.assign(bands*channels, Band());
		
		// Various phase rotations
		rotCentreSpectrum.resize(bands);
		rotPrevInterval.assign(bands, 0);
		timeShiftPhases(blockSamples*Sample(-0.5), rotCentreSpectrum);
		timeShiftPhases(-intervalSamples, rotPrevInterval);
		peaks.reserve(bands);
		energy.resize(bands);
		smoothedEnergy.resize(bands);
		outputMap.resize(bands);
		channelPredictions.resize(channels*bands);
	}

	/// Frequency multiplier, and optional tonality limit (as multiple of sample-rate)
	void setTransposeFactor(Sample multiplier, Sample tonalityLimit=0) {
		freqMultiplier = multiplier;
		if (tonalityLimit > 0) {
			freqTonalityLimit = tonalityLimit/std::sqrt(multiplier); // compromise between input and output limits
		} else {
			freqTonalityLimit = 1;
		}
		customFreqMap = nullptr;
	}
	void setTransposeSemitones(Sample semitones, Sample tonalityLimit=0) {
		setTransposeFactor(std::pow(2, semitones/12), tonalityLimit);
		customFreqMap = nullptr;
	}
	// Sets a custom frequency map - should be monotonically increasing
	void setFreqMap(std::function<Sample(Sample)> inputToOutput) {
		customFreqMap = inputToOutput;
	}
	
	template<class Inputs, class Outputs>
	void process(Inputs &&inputs, int inputSamples, Outputs &&outputs, int outputSamples) {
		Sample totalEnergy = 0;
		for (int c = 0; c < channels; ++c) {
			auto &&inputChannel = inputs[c];
			for (int i = 0; i < inputSamples; ++i) {
				Sample s = inputChannel[i];
				totalEnergy += s*s;
			}
		}
		if (totalEnergy < noiseFloor) {
			if (silenceCounter >= 2*stft.windowSize()) {
				if (silenceFirst) {
					silenceFirst = false;
					for (auto &b : channelBands) {
						b.input = b.prevInput = b.output = b.prevOutput = 0;
						b.inputEnergy = 0;
					}
				}
			
				if (inputSamples > 0) {
					// copy from the input, wrapping around if needed
					for (int outputIndex = 0; outputIndex < outputSamples; ++outputIndex) {
						int inputIndex = outputIndex%inputSamples;
						for (int c = 0; c < channels; ++c) {
							outputs[c][outputIndex] = inputs[c][inputIndex];
						}
					}
				} else {
					for (int c = 0; c < channels; ++c) {
						auto &&outputChannel = outputs[c];
						for (int outputIndex = 0; outputIndex < outputSamples; ++outputIndex) {
							outputChannel[outputIndex] = 0;
						}
					}
				}

				// Store input in history buffer
				for (int c = 0; c < channels; ++c) {
					auto &&inputChannel = inputs[c];
					auto &&bufferChannel = inputBuffer[c];
					int startIndex = std::max<int>(0, inputSamples - stft.windowSize());
					for (int i = startIndex; i < inputSamples; ++i) {
						bufferChannel[i] = inputChannel[i];
					}
				}
				inputBuffer += inputSamples;
				return;
			} else {
				silenceCounter += inputSamples;
			}
		} else {
			silenceCounter = 0;
			silenceFirst = true;
		}

		for (int outputIndex = 0; outputIndex < outputSamples; ++outputIndex) {
			stft.ensureValid(outputIndex, [&](int outputOffset) {
				// Time to process a spectrum!  Where should it come from in the input?
				int inputOffset = std::round(outputOffset*Sample(inputSamples)/outputSamples) - stft.windowSize();
				int inputInterval = inputOffset - prevInputOffset;
				prevInputOffset = inputOffset;

				bool newSpectrum = (inputInterval > 0);
				if (newSpectrum) {
					for (int c = 0; c < channels; ++c) {
						// Copy from the history buffer, if needed
						auto &&bufferChannel = inputBuffer[c];
						for (int i = 0; i < -inputOffset; ++i) {
							timeBuffer[i] = bufferChannel[i + inputOffset];
						}
						// Copy the rest from the input
						auto &&inputChannel = inputs[c];
						for (int i = std::max<int>(0, -inputOffset); i < stft.windowSize(); ++i) {
							timeBuffer[i] = inputChannel[i + inputOffset];
						}
						stft.analyse(c, timeBuffer);
					}

					for (int c = 0; c < channels; ++c) {
						auto channelBands = bandsForChannel(c);
						auto &&spectrumBands = stft.spectrum[c];
						for (int b = 0; b < bands; ++b) {
							channelBands[b].input = signalsmith::perf::mul(spectrumBands[b], rotCentreSpectrum[b]);
						}
					}

					if (inputInterval != stft.interval()) { // make sure the previous input is the correct distance in the past
						int prevIntervalOffset = inputOffset - stft.interval();
						for (int c = 0; c < channels; ++c) {
							// Copy from the history buffer, if needed
							auto &&bufferChannel = inputBuffer[c];
							for (int i = 0; i < std::min(-prevIntervalOffset, stft.windowSize()); ++i) {
								timeBuffer[i] = bufferChannel[i + prevIntervalOffset];
							}
							// Copy the rest from the input
							auto &&inputChannel = inputs[c];
							for (int i = std::max<int>(0, -prevIntervalOffset); i < stft.windowSize(); ++i) {
								timeBuffer[i] = inputChannel[i + prevIntervalOffset];
							}
							stft.analyse(c, timeBuffer);
						}
						for (int c = 0; c < channels; ++c) {
							auto channelBands = bandsForChannel(c);
							auto &&spectrumBands = stft.spectrum[c];
							for (int b = 0; b < bands; ++b) {
								channelBands[b].prevInput = signalsmith::perf::mul(spectrumBands[b], rotCentreSpectrum[b]);
							}
						}
					}
				}
				
				Sample timeFactor = stft.interval()/std::max<Sample>(1, inputInterval);
				processSpectrum(newSpectrum, timeFactor);

				for (int c = 0; c < channels; ++c) {
					auto channelBands = bandsForChannel(c);
					auto &&spectrumBands = stft.spectrum[c];
					for (int b = 0; b < bands; ++b) {
						spectrumBands[b] = signalsmith::perf::mul<true>(channelBands[b].output, rotCentreSpectrum[b]);
					}
				}
			});

			for (int c = 0; c < channels; ++c) {
				auto &&outputChannel = outputs[c];
				auto &&stftChannel = stft[c];
				outputChannel[outputIndex] = stftChannel[outputIndex];
			}
		}

		// Store input in history buffer
		for (int c = 0; c < channels; ++c) {
			auto &&inputChannel = inputs[c];
			auto &&bufferChannel = inputBuffer[c];
			int startIndex = std::max<int>(0, inputSamples - stft.windowSize());
			for (int i = startIndex; i < inputSamples; ++i) {
				bufferChannel[i] = inputChannel[i];
			}
		}
		inputBuffer += inputSamples;
		stft += outputSamples;
		prevInputOffset -= inputSamples;
	}
	
private:
	using Complex = std::complex<Sample>;
	static constexpr Sample noiseFloor{1e-15};
	static constexpr Sample maxCleanStretch{2}; // time-stretch ratio before we start randomising phases
	int silenceCounter = 0;
	bool silenceFirst = true;

	Sample freqMultiplier = 1, freqTonalityLimit = 0.5;
	std::function<Sample(Sample)> customFreqMap = nullptr;

	signalsmith::spectral::STFT<Sample> stft{0, 1, 1};
	signalsmith::delay::MultiBuffer<Sample> inputBuffer;
	int channels = 0, bands = 0;
	int prevInputOffset = -1;
	std::vector<Sample> timeBuffer;

	std::vector<Complex> rotCentreSpectrum, rotPrevInterval;
	Sample bandToFreq(Sample b) const {
		return (b + Sample(0.5))/stft.fftSize();
	}
	Sample freqToBand(Sample f) const {
		return f*stft.fftSize() - Sample(0.5);
	}
	void timeShiftPhases(Sample shiftSamples, std::vector<Complex> &output) const {
		for (int b = 0; b < bands; ++b) {
			Sample phase = bandToFreq(b)*shiftSamples*Sample(-2*M_PI);
			output[b] = {std::cos(phase), std::sin(phase)};
		}
	}
	
	struct Band {
		Complex input, prevInput{0};
		Complex output, prevOutput{0};
		Sample inputEnergy;
	};
	std::vector<Band> channelBands;
	Band * bandsForChannel(int channel) {
		return channelBands.data() + channel*bands;
	}
	template<Complex Band::*member>
	Complex getBand(int channel, int index) {
		if (index < 0 || index >= bands) return 0;
		return channelBands[index + channel*bands].*member;
	}
	template<Complex Band::*member>
	Complex getFractional(int channel, int lowIndex, Sample fractional) {
		Complex low = getBand<member>(channel, lowIndex);
		Complex high = getBand<member>(channel, lowIndex + 1);
		return low + (high - low)*fractional;
	}
	template<Complex Band::*member>
	Complex getFractional(int channel, Sample inputIndex) {
		int lowIndex = std::floor(inputIndex);
		Sample fracIndex = inputIndex - lowIndex;
		return getFractional<member>(channel, lowIndex, fracIndex);
	}
	template<Sample Band::*member>
	Sample getBand(int channel, int index) {
		if (index < 0 || index >= bands) return 0;
		return channelBands[index + channel*bands].*member;
	}
	template<Sample Band::*member>
	Sample getFractional(int channel, int lowIndex, Sample fractional) {
		Sample low = getBand<member>(channel, lowIndex);
		Sample high = getBand<member>(channel, lowIndex + 1);
		return low + (high - low)*fractional;
	}
	template<Sample Band::*member>
	Sample getFractional(int channel, Sample inputIndex) {
		int lowIndex = std::floor(inputIndex);
		Sample fracIndex = inputIndex - lowIndex;
		return getFractional<member>(channel, lowIndex, fracIndex);
	}

	struct Peak {
		Sample input, output;
	};
	std::vector<Peak> peaks;
	std::vector<Sample> energy, smoothedEnergy;
	struct PitchMapPoint {
		Sample inputBin, freqGrad;
	};
	std::vector<PitchMapPoint> outputMap;
	
	struct Prediction {
		Sample energy = 0;
		Complex input;
		Complex shortVerticalTwist, longVerticalTwist;

		Complex makeOutput(Complex phase) {
			Sample phaseNorm = std::norm(phase);
			if (phaseNorm <= noiseFloor) {
				phase = input; // prediction is too weak, fall back to the input
				phaseNorm = std::norm(input) + noiseFloor;
			}
			return phase*std::sqrt(energy/phaseNorm);
		}
	};
	std::vector<Prediction> channelPredictions;
	Prediction * predictionsForChannel(int c) {
		return channelPredictions.data() + c*bands;
	}
	
	std::default_random_engine randomEngine;

	void processSpectrum(bool newSpectrum, Sample timeFactor) {
		timeFactor = std::max<Sample>(timeFactor, 1/maxCleanStretch);
		bool randomTimeFactor = (timeFactor > maxCleanStretch);
		std::uniform_real_distribution<Sample> timeFactorDist(maxCleanStretch*2*randomTimeFactor - timeFactor, timeFactor);
		
		if (newSpectrum) {
			for (int c = 0; c < channels; ++c) {
				auto bins = bandsForChannel(c);
				for (int b = 0; b < bands; ++b) {
					auto &bin = bins[b];
					bin.prevOutput = signalsmith::perf::mul(bin.prevOutput, rotPrevInterval[b]);
					bin.prevInput = signalsmith::perf::mul(bin.prevInput, rotPrevInterval[b]);
				}
			}
		}

		Sample smoothingBins = Sample(stft.fftSize())/stft.interval();
		int longVerticalStep = std::round(smoothingBins);
		if (customFreqMap || freqMultiplier != 1) {
			findPeaks(smoothingBins);
			updateOutputMap();
		} else { // we're not pitch-shifting, so no need to find peaks etc.
			for (int c = 0; c < channels; ++c) {
				Band *bins = bandsForChannel(c);
				for (int b = 0; b < bands; ++b) {
					bins[b].inputEnergy = std::norm(bins[b].input);
				}
			}
			for (int b = 0; b < bands; ++b) {
				outputMap[b] = {Sample(b), 1};
			}
		}

		// Preliminary output prediction from phase-vocoder
		for (int c = 0; c < channels; ++c) {
			Band *bins = bandsForChannel(c);
			auto *predictions = predictionsForChannel(c);
			for (int b = 0; b < bands; ++b) {
				auto mapPoint = outputMap[b];
				int lowIndex = std::floor(mapPoint.inputBin);
				Sample fracIndex = mapPoint.inputBin - lowIndex;

				Prediction &prediction = predictions[b];
				Sample prevEnergy = prediction.energy;
				prediction.energy = getFractional<&Band::inputEnergy>(c, lowIndex, fracIndex);
				prediction.energy *= std::max<Sample>(0, mapPoint.freqGrad); // scale the energy according to local stretch factor
				prediction.input = getFractional<&Band::input>(c, lowIndex, fracIndex);

				auto &outputBin = bins[b];
				Complex prevInput = getFractional<&Band::prevInput>(c, lowIndex, fracIndex);
				Complex freqTwist = signalsmith::perf::mul<true>(prediction.input, prevInput);
				Complex phase = signalsmith::perf::mul(outputBin.prevOutput, freqTwist);
				outputBin.output = phase/(std::max(prevEnergy, prediction.energy) + noiseFloor);

				if (b > 0) {
					Sample binTimeFactor = randomTimeFactor ? timeFactorDist(randomEngine) : timeFactor;
					Complex downInput = getFractional<&Band::input>(c, mapPoint.inputBin - binTimeFactor);
					prediction.shortVerticalTwist = signalsmith::perf::mul<true>(prediction.input, downInput);
					if (b >= longVerticalStep) {
						Complex longDownInput = getFractional<&Band::input>(c, mapPoint.inputBin - longVerticalStep*binTimeFactor);
						prediction.longVerticalTwist = signalsmith::perf::mul<true>(prediction.input, longDownInput);
					} else {
						prediction.longVerticalTwist = 0;
					}
				} else {
					prediction.shortVerticalTwist = prediction.longVerticalTwist = 0;
				}
			}
		}

		// Re-predict using phase differences between frequencies
		for (int b = 0; b < bands; ++b) {
			// Find maximum-energy channel and calculate that
			int maxChannel = 0;
			Sample maxEnergy = predictionsForChannel(0)[b].energy;
			for (int c = 1; c < channels; ++c) {
				Sample e = predictionsForChannel(c)[b].energy;
				if (e > maxEnergy) {
					maxChannel = c;
					maxEnergy = e;
				}
			}

			auto *predictions = predictionsForChannel(maxChannel);
			auto &prediction = predictions[b];
			auto *bins = bandsForChannel(maxChannel);
			auto &outputBin = bins[b];

			Complex phase = 0;

			// Upwards vertical steps
			if (b > 0) {
				auto &downBin = bins[b - 1];
				phase += signalsmith::perf::mul(downBin.output, prediction.shortVerticalTwist);
				
				if (b >= longVerticalStep) {
					auto &longDownBin = bins[b - longVerticalStep];
					phase += signalsmith::perf::mul(longDownBin.output, prediction.longVerticalTwist);
				}
			}
			// Downwards vertical steps
			if (b < bands - 1) {
				auto &upPrediction = predictions[b + 1];
				auto &upBin = bins[b + 1];
				phase += signalsmith::perf::mul<true>(upBin.output, upPrediction.shortVerticalTwist);
				
				if (b < bands - longVerticalStep) {
					auto &longUpPrediction = predictions[b + longVerticalStep];
					auto &longUpBin = bins[b + longVerticalStep];
					phase += signalsmith::perf::mul<true>(longUpBin.output, longUpPrediction.longVerticalTwist);
				}
			}

			outputBin.output = prediction.makeOutput(phase);
			
			// All other bins are locked in phase
			for (int c = 0; c < channels; ++c) {
				if (c != maxChannel) {
					auto &channelBin = bandsForChannel(c)[b];
					auto &channelPrediction = predictionsForChannel(c)[b];
					
					Complex channelTwist = signalsmith::perf::mul<true>(channelPrediction.input, prediction.input);
					Complex channelPhase = signalsmith::perf::mul(outputBin.output, channelTwist);
					channelBin.output = channelPrediction.makeOutput(channelPhase);
				}
			}
		}

		if (newSpectrum) {
			for (auto &bin : channelBands) {
				bin.prevOutput = bin.output;
				bin.prevInput = bin.input;
			}
		} else {
			for (auto &bin : channelBands) bin.prevOutput = bin.output;
		}
	}
	
	// Produces smoothed energy across all channels
	void smoothEnergy(Sample smoothingBins) {
		Sample smoothingSlew = 1/(1 + smoothingBins*Sample(0.5));
		for (auto &e : energy) e = 0;
		for (int c = 0; c < channels; ++c) {
			Band *bins = bandsForChannel(c);
			for (int b = 0; b < bands; ++b) {
				Sample e = std::norm(bins[b].input);
				bins[b].inputEnergy = e; // Used for interpolating prediction energy
				energy[b] += e;
			}
		}
		for (int b = 0; b < bands; ++b) {
			smoothedEnergy[b] = energy[b];
		}
		Sample e = 0;
		for (int repeat = 0; repeat < 2; ++repeat) {
			for (int b = bands - 1; b >= 0; --b) {
				e += (smoothedEnergy[b] - e)*smoothingSlew;
				smoothedEnergy[b] = e;
			}
			for (int b = 0; b < bands; ++b) {
				e += (smoothedEnergy[b] - e)*smoothingSlew;
				smoothedEnergy[b] = e;
			}
		}
	}
	
	Sample mapFreq(Sample freq) const {
		if (customFreqMap) return customFreqMap(freq);
		if (freq > freqTonalityLimit) {
			Sample diff = freq - freqTonalityLimit;
			return freqTonalityLimit*freqMultiplier + diff;
		}
		return freq*freqMultiplier;
	}
	
	// Identifies spectral peaks using energy across all channels
	void findPeaks(Sample smoothingBins) {
		smoothEnergy(smoothingBins);

		peaks.resize(0);
		
		int start = 0;
		while (start < bands) {
			if (energy[start] > smoothedEnergy[start]) {
				int end = start;
				Sample bandSum = 0, energySum = 0;
				while (end < bands && energy[end] > smoothedEnergy[end]) {
					bandSum += end*energy[end];
					energySum += energy[end];
					++end;
				}
				Sample avgBand = bandSum/energySum;
				Sample avgFreq = bandToFreq(avgBand);
				peaks.emplace_back(Peak{avgBand, freqToBand(mapFreq(avgFreq))});

				start = end;
			}
			++start;
		}
	}
	
	void updateOutputMap() {
		if (peaks.empty()) {
			for (int b = 0; b < bands; ++b) {
				outputMap[b] = {Sample(b), 1};
			}
			return;
		}
		Sample bottomOffset = peaks[0].input - peaks[0].output;
		for (int b = 0; b < std::min<int>(bands, std::ceil(peaks[0].output)); ++b) {
			outputMap[b] = {b + bottomOffset, 1};
		}
		// Interpolate between points
		for (size_t p = 1; p < peaks.size(); ++p) {
			const Peak &prev = peaks[p - 1], &next = peaks[p];
			Sample rangeScale = 1/(next.output - prev.output);
			Sample outOffset = prev.input - prev.output;
			Sample outScale = next.input - next.output - prev.input + prev.output;
			Sample gradScale = outScale*rangeScale;
			int startBin = std::max<int>(0, std::ceil(prev.output));
			int endBin = std::min<int>(bands, std::ceil(next.output));
			for (int b = startBin; b < endBin; ++b) {
				Sample r = (b - prev.output)*rangeScale;
				Sample h = r*r*(3 - 2*r);
				Sample outB = b + outOffset + h*outScale;
				
				Sample gradH = 6*r*(1 - r);
				Sample gradB = 1 + gradH*gradScale;
				
				outputMap[b] = {outB, gradB};
			}
		}
		Sample topOffset = peaks.back().input - peaks.back().output;
		for (int b = std::max<int>(0, peaks.back().output); b < bands; ++b) {
			outputMap[b] = {b + topOffset, 1};
		}
	}
};

}} // namespace
#endif // include guard

#include <map>
#include <iostream>

namespace elem
{

    namespace detail
    {
        template <typename FloatType>
        FloatType lerp (FloatType alpha, FloatType x, FloatType y) {
            return x + alpha * (y - x);
        }

        template <typename FloatType>
        FloatType fpEqual (FloatType x, FloatType y) {
            return std::abs(x - y) <= FloatType(1e-6);
        }

        template <typename FloatType>
        struct GainFade {
            GainFade() = default;

            void setTargetGain (FloatType g) {
                targetGain = g;

                if (targetGain < currentGain) {
                    step = FloatType(-1) * std::abs(step);
                } else {
                    step = std::abs(step);
                }
            }

            FloatType operator() (FloatType x) {
                if (currentGain == targetGain)
                    return (currentGain * x);

                auto y = x * currentGain;
                currentGain = std::clamp(currentGain + step, FloatType(0), FloatType(1));

                return y;
            }

            bool on() {
                return fpEqual(targetGain, FloatType(1));
            }

            bool silent() {
                return fpEqual(targetGain, FloatType(0)) && fpEqual(currentGain, FloatType(0));
            }

            void reset() {
                currentGain = FloatType(0);
                targetGain = FloatType(0);
            }

            FloatType currentGain = 0;
            FloatType targetGain = 0;
            FloatType step = 0.02; // TODO
        };

        template <typename FloatType>
        struct BufferReader {
            BufferReader() = default;

            void engage (double start, double currentTime, FloatType* _buffer, size_t _size) {
                startTime = start;
                buffer = _buffer;
                bufferSize = _size;
                fade.setTargetGain(FloatType(1));

                position = static_cast<size_t>(((currentTime - startTime) / sampleDuration) * (double) (bufferSize - 1u));
                position = std::clamp<size_t>(position, 0, bufferSize);
            }

            void disengage() {
                fade.setTargetGain(FloatType(0));
            }

            void readAdding(FloatType* outputData, size_t numSamples) {
                for (size_t i = 0; (i < numSamples) && (position < bufferSize); ++i) {
                    outputData[i] += fade(buffer[position++]);
                }
            }

            FloatType read (FloatType const* buffer, size_t size, double t)
            {
                if (fade.silent() || sampleDuration <= FloatType(0))
                    return FloatType(0);

                // An allocated but inactive reader is currently fading out at the point in time
                // from which we jumped to allocate a new reader
                double const pos = fade.on()
                    ? (t - startTime) / sampleDuration
                    : (stepStopTime() - startTime) / sampleDuration;

                // While we're still active, track last position so that we can stop effectively
                if (fade.on()) {
                    dt = t - lastTimeStep;
                    lastTimeStep = t;
                }

                // Deallocate if we've run out of bounds
                if (pos < 0.0 || pos >= 1.0) {
                    disengage();
                    return FloatType(0);
                }

                // Instead of clamping here, we could accept loop points in the sample and
                // mod the playback position within those loop points. Property loop: [start, stop]
                auto l = static_cast<size_t>(pos * (double) (size - 1u));
                auto r = std::min(size, l + 1u);
                auto const alpha = FloatType((pos * (double) (size - 1u)) - static_cast<double>(l));

                return fade(lerp(alpha, buffer[l], buffer[r]));
            }

            FloatType stepStopTime() {
                lastTimeStep += dt;
                return lastTimeStep;
            }

            void reset (double sampleDur) {
                fade.reset();

                sampleDuration = sampleDur;
                startTime = 0.0;
                dt = 0.0;
            }

            GainFade<FloatType> fade;
            FloatType* buffer = nullptr;
            size_t bufferSize = 0;
            size_t position = 0;

            double sampleDuration = 0;
            double startTime = 0;
            double lastTimeStep = 0;
            double dt = 0;
        };
    }

    template <typename FloatType, bool WithStretch = false>
    struct SampleSeqNode : public GraphNode<FloatType> {
        SampleSeqNode(NodeId id, FloatType const sr, int const blockSize)
            : GraphNode<FloatType>::GraphNode(id, sr, blockSize)
        {
            if constexpr (WithStretch) {
                stretch.presetDefault(1, sr);
                scratchBuffer.resize(blockSize * 4);
            }
        }

        int setProperty(std::string const& key, js::Value const& val, SharedResourceMap<FloatType>& resources) override
        {
            if constexpr (WithStretch) {
                if (key == "shift") {
                    if (!val.isNumber())
                        return ReturnCode::InvalidPropertyType();

                    auto shift = (js::Number) val;
                    stretch.setTransposeSemitones(shift);
                }

                if (key == "stretch") {
                    if (!val.isNumber())
                        return ReturnCode::InvalidPropertyType();

                    auto _stretchFactor = (js::Number) val;

                    if (_stretchFactor < 0.25 || _stretchFactor > 4.0)
                        return ReturnCode::InvalidPropertyValue();

                    stretchFactor.store(_stretchFactor);
                }
            }

            if (key == "duration") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto dur = (js::Number) val;

                if (dur <= 0.0)
                    return ReturnCode::InvalidPropertyValue();

                sampleDuration.store(dur);
            }

            if (key == "path") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                if (!resources.has((js::String) val))
                    return ReturnCode::InvalidPropertyValue();

                auto ref = resources.get((js::String) val);
                bufferQueue.push(std::move(ref));
            }

            if (key == "seq") {
                if (!val.isArray())
                    return ReturnCode::InvalidPropertyType();

                auto& seq = val.getArray();

                if (seq.size() == 0)
                    return ReturnCode::InvalidPropertyValue();

                auto data = seqPool.allocate();

                // The data array that we get from the pool may have been
                // previously used to represent a different sequence
                data->clear();

                // We expect from the JavaScript side an array of event objects, where each
                // event includes a value to take and a time at which to take that value
                for (size_t i = 0; i < seq.size(); ++i) {
                    auto& event = seq[i].getObject();

                    FloatType value = static_cast<FloatType>((js::Number) event.at("value"));
                    double time = static_cast<double>((js::Number) event.at("time"));

                    data->insert({ time, value });
                }

                seqQueue.push(std::move(data));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void updateEventBoundaries(double t) {
            nextEvent = activeSeq->upper_bound(t);

            // The next event is the first one in the sequence
            if (nextEvent == activeSeq->begin()) {
                prevEvent = activeSeq->end();

                // Here we know that nothing should be playing, so, easy:
                readers[0].disengage();
                readers[1].disengage();
            } else {
                prevEvent = std::prev(nextEvent);

                // And here we decide based on the previous event
                readers[activeReader].disengage();
                activeReader = (activeReader + 1) & (readers.size() - 1);

                // Here a value of 1.0 is considered an onset, and anything else
                // considered an offset.
                if (detail::fpEqual(prevEvent->second, FloatType(1.0))) {
                    readers[activeReader].engage(prevEvent->first, t, const_cast<FloatType*>(activeBuffer->data()), activeBuffer->size());
                }
            }
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // Load sample duration
            auto const sampleDur = sampleDuration.load();

            if (sampleDur != rtSampleDuration) {
                readers[0].reset(sampleDur);
                readers[1].reset(sampleDur);
                rtSampleDuration = sampleDur;
            }

            // Pull newest buffer from queue
            while (bufferQueue.size() > 0) {
                bufferQueue.pop(activeBuffer);

                readers[0].reset(sampleDur);
                readers[1].reset(sampleDur);
            }

            // Pull newest seq from queue
            if (seqQueue.size() > 0) {
                while (seqQueue.size() > 0) {
                    seqQueue.pop(activeSeq);
                }

                // New sequence means we'll have to find our new event boundaries given
                // the current input time
                prevEvent = activeSeq->end();
                nextEvent = activeSeq->end();
            }

            // Next, if we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1 || activeSeq == nullptr || activeBuffer == nullptr || sampleDur <= 0.0)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // We reference this a lot
            auto const seqEnd = activeSeq->end();
            auto* scratchData = scratchBuffer.data();

            // Helpers to add some tolerance to the time checks
            auto const before = [](double t1, double t2) { return t1 <= (t2 + 1e-6); };
            auto const after = [](double t1, double t2) { return t1 >= (t2 - 1e-6); };

            // Downsampling from a-rate to k-rate
            auto const t = static_cast<double>(inputData[0][0]);

            // Time check
            double const timeUnitsPerSample = sampleDur / (double) activeBuffer->size();
            int64_t const sampleTime = t / timeUnitsPerSample;
            bool const significantTimeChange = std::abs(sampleTime - nextExpectedBlockStart) > 16;
            nextExpectedBlockStart = sampleTime + numSamples;

            // We update our event boundaries if we just took a new sequence, if we've stepped
            // forwards or backwards over the next event time, or if the incoming time step differs
            // excessively from what we expected
            auto const shouldUpdateBounds = (prevEvent == seqEnd && nextEvent == seqEnd)
                || (prevEvent != seqEnd && before(t, prevEvent->first))
                || (nextEvent != seqEnd && after(t, nextEvent->first));

            // TODO: if the input time has changed significantly, need to address the input latency of
            // the phase vocoder by resetting it and then pushing stretch.inputLatency * stretchFactor
            // samples ahead of `timeInSamples(t)`
            if (shouldUpdateBounds || significantTimeChange) {
                updateEventBoundaries(t);
            }

            if constexpr (WithStretch) {
                // TODO: Account for fractional samples here by running an accumulator
                auto const numSourceSamples = std::clamp(static_cast<size_t>((double) numSamples / stretchFactor.load()), (size_t) 0, scratchBuffer.size());

                // Clear and read
                std::fill_n(scratchData, numSourceSamples, FloatType(0));

                readers[0].readAdding(scratchData, numSourceSamples);
                readers[1].readAdding(scratchData, numSourceSamples);

                stretch.process(&scratchData, numSourceSamples, &outputData, numSamples);
            } else {
                // Clear and read
                std::fill_n(outputData, numSamples, FloatType(0));

                readers[0].readAdding(outputData, numSamples);
                readers[1].readAdding(outputData, numSamples);
            }
        }

        using Sequence = std::map<double, FloatType, std::less<double>>;

        RefCountedPool<Sequence> seqPool;
        SingleWriterSingleReaderQueue<std::shared_ptr<Sequence>> seqQueue;
        std::shared_ptr<Sequence> activeSeq;

        typename Sequence::iterator prevEvent;
        typename Sequence::iterator nextEvent;

        SingleWriterSingleReaderQueue<SharedResourceBuffer<FloatType>> bufferQueue;
        SharedResourceBuffer<FloatType> activeBuffer;

        std::array<detail::BufferReader<FloatType>, 2> readers;
        size_t activeReader = 0;
        int64_t nextExpectedBlockStart = 0;

        std::atomic<double> sampleDuration = 0;
        double rtSampleDuration = 0;

        signalsmith::stretch::SignalsmithStretch<FloatType> stretch;
        std::atomic<double> stretchFactor = 1.0;
        std::vector<FloatType> scratchBuffer;
    };

    template <typename FloatType>
    using SampleSeqWithStretchNode = SampleSeqNode<FloatType, true>;

} // namespace elem

namespace elem
{

    // The Seq2 node is a simple variant on the original SequenceNode with a
    // different implementation that is vastly simpler and more robust, but
    // which differs in behavior enough to warrant a new node rather than a change.
    //
    // The primary difference is that Seq2 accepts new offset values at any time
    // which take immediate effect, whereas the original Seq will only reset to
    // its prescribed offset value at the time of a rising edge in the reset signal.
    //
    // The problem with this older behavior is that the reconciler makes no guarantees
    // about ordering of events when performing a given render pass. We might set one Seq
    // node's offset value, then set some const node value to 1 indicating the rising edge
    // of a reset train, then set another Seq node's offset. The GraphHost just executes these
    // calls in order on the non-realtime thread while the realtime thread freely runs, which
    // means it's plenty possible for the first Seq here to receive its offset and then the
    // edge of the reset train while the second Seq receives the reset train and then its new
    // offset– but it already reset to some stale value and won't reset again until the next
    // rising edge.
    //
    // So here we simply count elapsed rising edges and continuously index into the active
    // sequence array summing in the current offset. Resets set our elapsed count to 0, and
    // new offset values are taken immediately (block level).
    template <typename FloatType>
    struct Seq2Node : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "hold") {
                if (!val.isBool())
                    return ReturnCode::InvalidPropertyType();

                wantsHold.store((js::Boolean) val);
            }

            if (key == "loop") {
                if (!val.isBool())
                    return ReturnCode::InvalidPropertyType();

                wantsLoop.store((js::Boolean) val);
            }

            if (key == "offset") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                if ((js::Number) val < 0.0)
                    return ReturnCode::InvalidPropertyValue();

                seqOffset.store(static_cast<size_t>((js::Number) val));
            }

            if (key == "seq") {
                if (!val.isArray())
                    return ReturnCode::InvalidPropertyType();

                auto& seq = val.getArray();
                auto data = sequencePool.allocate();

                // The data array that we get from the pool may have been
                // previously used to represent a different sequence. Need to
                // resize here and then overwrite below.
                data->resize(seq.size());

                for (size_t i = 0; i < seq.size(); ++i)
                    data->at(i) = FloatType((js::Number) seq[i]);

                // Finally, we push our new sequence data into the event
                // queue for the realtime thread.
                sequenceQueue.push(std::move(data));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // First order of business: grab the most recent sequence buffer to use if
            // there's anything in the queue
            if (sequenceQueue.size() > 0) {
                while (sequenceQueue.size() > 0) {
                    sequenceQueue.pop(activeSequence);
                }
            }

            // Next, if we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1 || activeSequence == nullptr)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // We let the user optionally supply a second input, a pulse train whose
            // rising edge will reset our sequence position back to the start
            bool hasResetSignal = numChannels > 1;
            bool const hold = wantsHold.load();
            bool const loop = wantsLoop.load();
            auto const offset = seqOffset.load();

            for (size_t i = 0; i < numSamples; ++i) {
                auto const in = inputData[0][i];
                auto const reset = hasResetSignal ? inputData[1][i] : FloatType(0);

                // Increment our sequence index on the rising edge of the input signal
                if (change(in) > FloatType(0.5)) {
                    edgeCount++;
                }

                // Reset our edge count if we're on the rising edge of the reset signal
                //
                // We want this to happen after checking the trigger train so that if we receive
                // a reset and a trigger at exactly the same time, we trigger from the reset index.
                if (resetChange(reset) > FloatType(0.5)) {
                    edgeCount = 0;
                }

                // Now, if our seqIndex has run past the end of the array, we either
                //  * Emit 0 if hold = false
                //  * Emit the last value in the seq array if hold = true
                //  * Loop around using the modulo operator if loop = true
                auto const seqSize = activeSequence->size();
                auto const seqIndex = offset + edgeCount;

                auto const nextOut = (seqIndex < seqSize)
                    ? activeSequence->at(seqIndex)
                    : (loop
                        ? activeSequence->at(seqIndex % seqSize)
                        : (hold
                            ? activeSequence->at(seqSize - 1)
                            : FloatType(0)));

                // Finally, when holding, we don't fall to 0 with the incoming pulse train.
                outputData[i] = (hold ? nextOut : nextOut * in);
            }
        }

        using SequenceData = std::vector<FloatType>;

        RefCountedPool<SequenceData> sequencePool;
        SingleWriterSingleReaderQueue<std::shared_ptr<SequenceData>> sequenceQueue;
        std::shared_ptr<SequenceData> activeSequence;

        Change<FloatType> change;
        Change<FloatType> resetChange;

        std::atomic<bool> wantsHold = false;
        std::atomic<bool> wantsLoop = true;
        std::atomic<size_t> seqOffset = 0;

        // The number of rising edges counted since the last reset event, or
        // since the beginning of this node's life.
        size_t edgeCount = 0;
    };

} // namespace elem

#include <optional>
#include <variant>

namespace elem
{

    template <typename FloatType>
    struct SparSeqNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;
        using SequenceData = std::map<int32_t, FloatType>;

        // Here we follow a pattern just like the larger GraphRenderer event queue pattern for
        // moving change events from the non-realtime thread into the realtime thread safely
        // without locking. We only have two event types: one for new sequence data, and one for
        // a pair of new loop points. The EmptyEvent primarily just enables default construction
        // into an event type that is neither of the two we care about.
        struct EmptyEvent {};

        struct NewSequenceEvent {
            std::shared_ptr<SequenceData> sequence;
        };

        struct NewLoopPointsEvent {
            int32_t loopStart;
            int32_t loopEnd;
        };

        // Our ChangeEvent union type.
        using ChangeEvent = std::variant<EmptyEvent, NewSequenceEvent, NewLoopPointsEvent>;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "offset") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                if ((js::Number) val < 0.0)
                    return ReturnCode::InvalidPropertyValue();

                seqOffset.store(static_cast<size_t>((js::Number) val));
            }

            if (key == "loop") {
                if (val.isNull() || (val.isBool() && !val)) {
                    changeEventQueue.push(ChangeEvent { NewLoopPointsEvent { -1, -1 } });
                } else {
                    if (!val.isArray())
                        return ReturnCode::InvalidPropertyType();

                    auto& points = val.getArray();

                    changeEventQueue.push(ChangeEvent { NewLoopPointsEvent {
                        static_cast<int32_t>((js::Number) points[0]),
                        static_cast<int32_t>((js::Number) points[1]),
                    }});
                }
            }

            if (key == "follow") {
                if (!val.isBool())
                    return ReturnCode::InvalidPropertyType();

                followAction.store((bool) val);
            }

            if (key == "interpolate") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                holdOrder.store(static_cast<int32_t>((js::Number) val));
            }

            // The estimated period of the incoming clock cycle in seconds. Used to improve the resolution
            // of the interpolation coefficient. Ignored if using zero order hold (no interpolation).
            if (key == "tickInterval") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                auto const ti = (js::Number) val;

                if (ti < 0.0)
                    return ReturnCode::InvalidPropertyValue();

                auto const tickIntervalSamples = GraphNode<FloatType>::getSampleRate() *  ti;
                tickInterval.store(tickIntervalSamples);
            }

            if (key == "seq") {
                if (!val.isArray())
                    return ReturnCode::InvalidPropertyType();

                auto& seq = val.getArray();
                auto data = sequencePool.allocate();

                // The data array that we get from the pool may have been
                // previously used to represent a different sequence.
                data->clear();

                // We expect from the JavaScript side an array of event objects, where each
                // event includes a value to take and a 32-bit int tick time at which to take that value.
                for (size_t i = 0; i < seq.size(); ++i) {
                    auto& event = seq[i].getObject();

                    FloatType value = static_cast<FloatType>((js::Number) event.at("value"));
                    int32_t time = static_cast<int32_t>((js::Number) event.at("tickTime"));

                    data->insert({time, value});
                }

                // Finally, we push our new sequence data into the event
                // queue for the realtime thread.
                changeEventQueue.push(ChangeEvent { NewSequenceEvent { std::move(data) } });
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        typename SequenceData::iterator findTickValue(int32_t tickTime) {
            // Look up the value we should take by considering where the counter
            // is in relation to our sparsely defined sequence. We find the first event _after_
            // the current time using upper_bound, then step back once to find the value we
            // should take currently.
            auto it = activeSequence->upper_bound(tickTime);

            // If we get back the beginning of the map, that means that either
            //   (1) the first entry specifies a value for a time that we haven't reached yet,
            //       in which case we just stay silent. Or,
            //   (2) the first entry defines a value for tickTime 0, in which case we take it
            if (it == activeSequence->begin()) {
                if (it->first == 0) {
                    return it;
                }

                return activeSequence->end();
            }

            return --it;
        }

        int32_t getTickTime(int32_t offset) {
            auto tickTime = static_cast<int32_t>(offset) + edgeCount;

            // If we're looping and we've walked past the end of the loop, we have to wrap back
            // around the tickTime before the lookup
            auto const ls = loopPoints.start;
            auto const le = loopPoints.end;

            if ((ls > -1) && (le > -1) && (tickTime >= le)) {
                auto const loopDuration = le - ls;

                if (loopDuration > 0) {
                    if (pendingLoopPoints) {
                        // Here, if we have pending loop points that means we didn't immediately promote
                        // them (i.e. follow = true), thus we're waiting for the end of the current loop to promote.
                        // In that case, we jump into the new loop points and promote right here.
                        loopPoints = *pendingLoopPoints;
                        pendingLoopPoints = std::nullopt;

                        // New loop points, post promotion
                        auto const nls = loopPoints.start;
                        auto const nle = loopPoints.end;

                        // If the promoted tick points indicate no looping, then we stop here.
                        if ((nls == -1) && (nle == -1)) {
                            return tickTime;
                        }

                        tickTime = nls + ((tickTime - le) % (nle - nls));
                    } else {
                        // Otherwise, we step into the existing loop points
                        tickTime = ls + ((tickTime - le) % loopDuration);
                    }

                    // This was previously the `resetAtLoopBoundaries` which meant that we clobber the edgeCount
                    // when resetting if and only if the user has requested it. I think it's pretty odd behavior if this _isn't_
                    // set, where the edgeCount runs off to some large number in the future while you're looping through
                    // your sequences. Without this, then any time you go from looping to not looping you would jump off into the
                    // future wherever the edgeCount is. Technically this is a breaking change to make this behavior the default,
                    // but now is the time.
                    edgeCount = tickTime - static_cast<int32_t>(offset);
                }
            }

            return tickTime;
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // We let the user optionally supply a second input, a pulse train whose
            // rising edge will reset our sequence position back to the start
            bool hasResetSignal = numChannels > 1;

            auto const offset = seqOffset.load();
            auto const follow = followAction.load();
            auto const ho = holdOrder.load();
            auto const samplesPerClockCycle = tickInterval.load();

            // Current sequence "time" in ticks
            auto tickTime = getTickTime(offset);

            // First order of business: grab the most recent sequence buffer to use if
            // there's anything in the queue
            if (changeEventQueue.size() > 0) {
                while (changeEventQueue.size() > 0) {
                    ChangeEvent nextEvent;
                    changeEventQueue.pop(nextEvent);

                    std::visit([this](auto && evt) {
                        using EventType = std::decay_t<decltype(evt)>;

                        if constexpr (std::is_same_v<EventType, NewSequenceEvent>) {
                            activeSequence = std::move(evt.sequence);
                        }

                        if constexpr (std::is_same_v<EventType, NewLoopPointsEvent>) {
                            pendingLoopPoints = LoopPoints { evt.loopStart, evt.loopEnd };
                        }
                    }, nextEvent);
                }

                // New sequence, but our internal count state is maintained so we immediately
                // perform a lookup.
                holdValue = findTickValue(tickTime);
            }

            // If after draining the changeEventQueue we have pending loop points, then here we
            // consider whether or not to immediately apply them. Generally, if we are already looping
            // and receiving new points, we only want to apply them immediately if !follow. However, regardless
            // of follow behavior, if we aren't already looping then we start immediately.
            if (pendingLoopPoints) {
                bool const takeImmediately = ((loopPoints.start == -1) && (loopPoints.end == -1)) || !follow;

                if (takeImmediately) {
                    loopPoints = *pendingLoopPoints;
                    pendingLoopPoints = std::nullopt;
                    tickTime = getTickTime(offset);
                }
            }

            // Next, if we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1 || activeSequence == nullptr)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            for (size_t i = 0; i < numSamples; ++i) {
                samplesSinceClockEdge++;

                auto const in = inputData[0][i];
                auto const reset = hasResetSignal ? inputData[1][i] : FloatType(0);

                auto const isTriggerEdge = change(in) > FloatType(0.5);
                auto const isResetEdge = resetChange(reset) > FloatType(0.5);

                // Reset our edge count if we're on the rising edge of the reset signal
                if (isResetEdge) {
                    edgeCount = 0;
                }

                // Increment our sequence index on the rising edge of the input signal
                if (isTriggerEdge) {
                    // We have a special case here; if we have a reset and a trigger edge at the exact
                    // same sample, then we trigger from an edgeCount of 0. Else, we increment our count.
                    edgeCount = isResetEdge ? 0 : edgeCount + 1;

                    // Reset our sample count
                    samplesSinceClockEdge = 0;

                    // Update the tick time given the new edgeCount
                    tickTime = getTickTime(offset);

                    // And update our current hold
                    holdValue = findTickValue(tickTime);
                }

                // Invalid hold value; output zeros
                if (holdValue == activeSequence->end()) {
                    outputData[i] = FloatType(0);
                    continue;
                }

                switch (ho) {
                    case 1:
                    {
                        auto holdRight = std::next(holdValue);

                        // Linear interpolation between two values. If our RHS is off the end of the container
                        // then we just return the last value in the sequence.
                        if (holdRight == activeSequence->end()) {
                            outputData[i] = holdValue->second;
                            break;
                        }

                        auto const tl = holdValue->first;
                        auto const tr = holdRight->first;
                        auto const leftValue = holdValue->second;
                        auto const rightValue = holdRight->second;

                        // This gets us linear interp but still stair-stepped according to the clock edge.
                        double alpha = (double) std::max(0, tickTime - tl) / (double) (tr - tl);

                        // We can improve our linear interpolation coefficient if we have an estimate for how many
                        // samples to expect per clock cycle.
                        //
                        // However, we're careful here not to interpolate _past_ one clock cycle based on elapsed
                        // samples. Otherwise the clock may have stopped running but we'll keep moving the output
                        // signal based on elapsed samples.
                        if (samplesPerClockCycle > 0.0) {
                            alpha += (std::min((double) samplesSinceClockEdge, samplesPerClockCycle) / samplesPerClockCycle) / (double) (tr - tl);
                        }

                        outputData[i] = leftValue + alpha * (rightValue - leftValue);
                        break;
                    }
                    case 0:
                    default:
                    {
                        outputData[i] = holdValue->second;
                        break;
                    }
                }
            }
        }

        RefCountedPool<SequenceData> sequencePool;
        SingleWriterSingleReaderQueue<ChangeEvent> changeEventQueue;
        std::shared_ptr<SequenceData> activeSequence;

        Change<FloatType> change;
        Change<FloatType> resetChange;

        struct LoopPoints {
            int32_t start = -1;
            int32_t end = -1;
        };

        LoopPoints loopPoints;
        std::optional<LoopPoints> pendingLoopPoints;

        std::atomic<size_t> seqOffset = 0;
        std::atomic<bool> followAction { false };

        // The number of rising edges counted since the last reset event, or
        // since the beginning of this node's life.
        //
        // We start here at -1 because the _first_ rising edge of the incoming
        // clock signal should trigger the sequence at tickTime 0.
        int32_t edgeCount = -1;

        // The number of elapsed samples counted since the last clock edge.
        size_t samplesSinceClockEdge = 0;

        // Because our sparse lookup uses a binary search, we only want to do it
        // when we absolutely have to. The rest of the time, we just hold that value here.
        typename SequenceData::iterator holdValue;
        std::atomic<int32_t> holdOrder { 0 };
        std::atomic<double> tickInterval { 0 };

        static_assert(std::atomic<double>::is_always_lock_free);
    };

} // namespace elem

#include <optional>
#include <variant>

namespace elem
{

    template <typename FloatType>
    struct SparSeq2Node : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val) override
        {
            if (key == "seq") {
                if (!val.isArray())
                    return ReturnCode::InvalidPropertyType();

                auto& seq = val.getArray();

                if (seq.size() <= 0)
                    return ReturnCode::InvalidPropertyValue();

                auto data = seqPool.allocate();

                // The data array that we get from the pool may have been
                // previously used to represent a different sequence
                data->clear();

                // We expect from the JavaScript side an array of event objects, where each
                // event includes a value to take and a time at which to take that value
                for (size_t i = 0; i < seq.size(); ++i) {
                    auto& event = seq[i].getObject();

                    FloatType value = static_cast<FloatType>((js::Number) event.at("value"));
                    double time = static_cast<double>((js::Number) event.at("time"));

                    data->insert({ time, value });
                }

                seqQueue.push(std::move(data));
            }

            if (key == "interpolate") {
                if (!val.isNumber())
                    return ReturnCode::InvalidPropertyType();

                interpOrder.store(static_cast<int32_t>((js::Number) val));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void updateEventBoundaries(double t) {
            nextEvent = activeSeq->upper_bound(t);

            // The next event is the first one in the sequence
            if (nextEvent == activeSeq->begin()) {
                prevEvent = activeSeq->end();
                return;
            }

            prevEvent = std::prev(nextEvent);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;
            auto const interp = interpOrder.load() == 1;

            // Pull newest seq from queue
            if (seqQueue.size() > 0) {
                while (seqQueue.size() > 0) {
                    seqQueue.pop(activeSeq);
                }

                // New sequence means we'll have to find our new event boundaries given
                // the current input time
                prevEvent = activeSeq->end();
                nextEvent = activeSeq->end();
            }

            // Next, if we don't have the inputs we need, we bail here and zero the buffer
            // hoping to prevent unexpected signals.
            if (numChannels < 1 || activeSeq == nullptr)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // We reference this a lot
            auto const seqEnd = activeSeq->end();

            // Helpers to add some tolerance to the time checks
            auto const before = [](double t1, double t2) { return t1 <= (t2 + 1e-9); };
            auto const after = [](double t1, double t2) { return t1 >= (t2 - 1e-9); };

            for (size_t i = 0; i < numSamples; ++i) {
                auto const t = static_cast<double>(inputData[0][i]);
                auto const shouldUpdateBounds = (prevEvent == seqEnd && nextEvent == seqEnd)
                    || (prevEvent != seqEnd && before(t, prevEvent->first))
                    || (nextEvent != seqEnd && after(t, nextEvent->first));

                if (shouldUpdateBounds) {
                    updateEventBoundaries(t);
                }

                // Now, if we still don't have a prevEvent, also output 0s
                if (prevEvent == seqEnd) {
                    outputData[i] = FloatType(0);
                    continue;
                }

                // If we don't have a nextEvent but do have a prevEvent, we output the prevEvent value indefinitely
                if (nextEvent == seqEnd) {
                    outputData[i] = prevEvent->second;
                    continue;
                }

                // Finally, here we have both bounds and can output accordingly
                double const alpha = interp ? ((t - prevEvent->first) / (nextEvent->first - prevEvent->first)) : 0.0;
                auto const out = prevEvent->second + FloatType(alpha) * (nextEvent->second - prevEvent->second);

                outputData[i] = out;
            }
        }

        using Sequence = std::map<double, FloatType, std::less<double>>;

        RefCountedPool<Sequence> seqPool;
        SingleWriterSingleReaderQueue<std::shared_ptr<Sequence>> seqQueue;
        std::shared_ptr<Sequence> activeSeq;

        typename Sequence::iterator prevEvent;
        typename Sequence::iterator nextEvent;

        std::atomic<double> interpOrder { 0 };
    };

} // namespace elem

namespace elem
{

    // TableNode is a core builtin for lookup table behaviors
    //
    // Can be used for loading sample buffers and reading from them at variable
    // playback rates, or as windowed grain readers, or for loading various functions
    // as lookup tables, etc.
    template <typename FloatType>
    struct TableNode : public GraphNode<FloatType> {
        using GraphNode<FloatType>::GraphNode;

        int setProperty(std::string const& key, js::Value const& val, SharedResourceMap<FloatType>& resources) override
        {
            if (key == "path") {
                if (!val.isString())
                    return ReturnCode::InvalidPropertyType();

                if (!resources.has((js::String) val))
                    return ReturnCode::InvalidPropertyValue();

                auto ref = resources.get((js::String) val);
                bufferQueue.push(std::move(ref));
            }

            return GraphNode<FloatType>::setProperty(key, val);
        }

        void process (BlockContext<FloatType> const& ctx) override {
            auto** inputData = ctx.inputData;
            auto* outputData = ctx.outputData;
            auto numChannels = ctx.numInputChannels;
            auto numSamples = ctx.numSamples;

            // First order of business: grab the most recent sample buffer to use if
            // there's anything in the queue. This behavior means that changing the buffer
            // while playing the sample will cause a discontinuity.
            while (bufferQueue.size() > 0)
                bufferQueue.pop(activeBuffer);

            if (numChannels == 0 || activeBuffer == nullptr)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            auto const bufferSize = static_cast<int>(activeBuffer->size());
            auto const bufferData = activeBuffer->data();

            if (bufferSize == 0)
                return (void) std::fill_n(outputData, numSamples, FloatType(0));

            // Finally, render sample output
            for (size_t i = 0; i < numSamples; ++i) {
                auto const readPos = std::clamp(inputData[0][i], FloatType(0), FloatType(1)) * FloatType(bufferSize - 1);
                auto const readLeft = static_cast<int>(readPos);
                auto const readRight = readLeft + 1;
                auto const frac = readPos - std::floor(readPos);

                auto const left = bufferData[readLeft % bufferSize];
                auto const right = bufferData[readRight % bufferSize];

                // Now we can read the next sample out with linear
                // interpolation for sub-sample reads.
                outputData[i] = left + frac * (right - left);
            }
        }

        SingleWriterSingleReaderQueue<SharedResourceBuffer<FloatType>> bufferQueue;
        SharedResourceBuffer<FloatType> activeBuffer;
    };

} // namespace elem

namespace elem
{

    namespace detail
    {
        /** A quick helper factory for default node types. */
        template <typename NodeType>
        struct GenericNodeFactory
        {
            template <typename NodeIdType>
            auto operator() (NodeIdType const id, double fs, int const blockSize) {
                return std::make_shared<NodeType>(id, fs, blockSize);
            }
        };
    }

    template <typename FloatType>
    struct DefaultNodeTypes {

        template <typename Fn>
        static void forEach(Fn&& callback) {
            using namespace detail;

            // Unary math nodes
            callback("in",              GenericNodeFactory<IdentityNode<FloatType>>());
            callback("sin",             GenericNodeFactory<UnaryOperationNode<FloatType, std::sin>>());
            callback("cos",             GenericNodeFactory<UnaryOperationNode<FloatType, std::cos>>());
            callback("tan",             GenericNodeFactory<UnaryOperationNode<FloatType, std::tan>>());
            callback("tanh",            GenericNodeFactory<UnaryOperationNode<FloatType, std::tanh>>());
            callback("asinh",           GenericNodeFactory<UnaryOperationNode<FloatType, std::asinh>>());
            callback("ln",              GenericNodeFactory<UnaryOperationNode<FloatType, std::log>>());
            callback("log",             GenericNodeFactory<UnaryOperationNode<FloatType, std::log10>>());
            callback("log2",            GenericNodeFactory<UnaryOperationNode<FloatType, std::log2>>());
            callback("ceil",            GenericNodeFactory<UnaryOperationNode<FloatType, std::ceil>>());
            callback("floor",           GenericNodeFactory<UnaryOperationNode<FloatType, std::floor>>());
            callback("round",           GenericNodeFactory<UnaryOperationNode<FloatType, std::round>>());
            callback("sqrt",            GenericNodeFactory<UnaryOperationNode<FloatType, std::sqrt>>());
            callback("exp",             GenericNodeFactory<UnaryOperationNode<FloatType, std::exp>>());
            callback("abs",             GenericNodeFactory<UnaryOperationNode<FloatType, std::abs>>());

            // Binary math nodes
            callback("le",              GenericNodeFactory<BinaryOperationNode<FloatType, std::less<FloatType>>>());
            callback("leq",             GenericNodeFactory<BinaryOperationNode<FloatType, std::less_equal<FloatType>>>());
            callback("ge",              GenericNodeFactory<BinaryOperationNode<FloatType, std::greater<FloatType>>>());
            callback("geq",             GenericNodeFactory<BinaryOperationNode<FloatType, std::greater_equal<FloatType>>>());
            callback("pow",             GenericNodeFactory<BinaryOperationNode<FloatType, SafePow<FloatType>>>());
            callback("eq",              GenericNodeFactory<BinaryOperationNode<FloatType, Eq<FloatType>>>());
            callback("and",             GenericNodeFactory<BinaryOperationNode<FloatType, BinaryAnd<FloatType>>>());
            callback("or",              GenericNodeFactory<BinaryOperationNode<FloatType, BinaryOr<FloatType>>>());

            // Reducing nodes
            callback("add",             GenericNodeFactory<BinaryReducingNode<FloatType, std::plus<FloatType>>>());
            callback("sub",             GenericNodeFactory<BinaryReducingNode<FloatType, std::minus<FloatType>>>());
            callback("mul",             GenericNodeFactory<BinaryReducingNode<FloatType, std::multiplies<FloatType>>>());
            callback("div",             GenericNodeFactory<BinaryReducingNode<FloatType, SafeDivides<FloatType>>>());
            callback("mod",             GenericNodeFactory<BinaryReducingNode<FloatType, Modulus<FloatType>>>());
            callback("min",             GenericNodeFactory<BinaryReducingNode<FloatType, Min<FloatType>>>());
            callback("max",             GenericNodeFactory<BinaryReducingNode<FloatType, Max<FloatType>>>());

            // Core nodes
            callback("root",            GenericNodeFactory<RootNode<FloatType>>());
            callback("const",           GenericNodeFactory<ConstNode<FloatType>>());
            callback("phasor",          GenericNodeFactory<PhasorNode<FloatType, false>>());
            callback("sphasor",         GenericNodeFactory<PhasorNode<FloatType, true>>());
            callback("sr",              GenericNodeFactory<SampleRateNode<FloatType>>());
            callback("seq",             GenericNodeFactory<SequenceNode<FloatType>>());
            callback("seq2",            GenericNodeFactory<Seq2Node<FloatType>>());
            callback("sparseq",         GenericNodeFactory<SparSeqNode<FloatType>>());
            callback("sparseq2",        GenericNodeFactory<SparSeq2Node<FloatType>>());
            callback("counter",         GenericNodeFactory<CounterNode<FloatType>>());
            callback("accum",           GenericNodeFactory<AccumNode<FloatType>>());
            callback("latch",           GenericNodeFactory<LatchNode<FloatType>>());
            callback("maxhold",         GenericNodeFactory<MaxHold<FloatType>>());
            callback("once",            GenericNodeFactory<OnceNode<FloatType>>());
            callback("rand",            GenericNodeFactory<UniformRandomNoiseNode<FloatType>>());

            // Delay nodes
            callback("delay",           GenericNodeFactory<VariableDelayNode<FloatType>>());
            callback("sdelay",          GenericNodeFactory<SampleDelayNode<FloatType>>());
            callback("z",               GenericNodeFactory<SingleSampleDelayNode<FloatType>>());

            // Filter nodes
            callback("pole",            GenericNodeFactory<OnePoleNode<FloatType>>());
            callback("env",             GenericNodeFactory<EnvelopeNode<FloatType>>());
            callback("biquad",          GenericNodeFactory<BiquadFilterNode<FloatType>>());
            callback("prewarp",         GenericNodeFactory<CutoffPrewarpNode<FloatType>>());
            callback("mm1p",            GenericNodeFactory<MultiMode1p<FloatType>>());
            callback("svf",             GenericNodeFactory<StateVariableFilterNode<FloatType>>());
            callback("svfshelf",        GenericNodeFactory<StateVariableShelfFilterNode<FloatType>>());

            // Feedback nodes
            callback("tapIn",           GenericNodeFactory<TapInNode<FloatType>>());
            callback("tapOut",          GenericNodeFactory<TapOutNode<FloatType>>());

            // Sample/Buffer nodes
            callback("sample",          GenericNodeFactory<SampleNode<FloatType>>());
            callback("sampleseq",       GenericNodeFactory<SampleSeqNode<FloatType>>());
            callback("sampleseq2",      GenericNodeFactory<SampleSeqWithStretchNode<FloatType>>());
            callback("table",           GenericNodeFactory<TableNode<FloatType>>());

            // Oscillator nodes
            callback("blepsaw",         GenericNodeFactory<PolyBlepOscillatorNode<FloatType, detail::BlepMode::Saw>>());
            callback("blepsquare",      GenericNodeFactory<PolyBlepOscillatorNode<FloatType, detail::BlepMode::Square>>());
            callback("bleptriangle",    GenericNodeFactory<PolyBlepOscillatorNode<FloatType, detail::BlepMode::Triangle>>());

            // Analyzer nodes
            callback("meter",           GenericNodeFactory<MeterNode<FloatType>>());
            callback("scope",           GenericNodeFactory<ScopeNode<FloatType>>());
            callback("snapshot",        GenericNodeFactory<SnapshotNode<FloatType>>());
            callback("capture",         GenericNodeFactory<CaptureNode<FloatType>>());
        }
    };

} // namespace elem

#include <list>
#include <unordered_map>

namespace elem
{

    //==============================================================================
    // A simple struct representing the audio processing inputs given to the runtime
    // by the host application.
    template <typename FloatType>
    struct HostContext
    {
        FloatType const** inputData;
        size_t numInputChannels;
        FloatType** outputData;
        size_t numOutputChannels;
        size_t numSamples;
        void* userData;
    };

    template <typename FloatType>
    class BufferAllocator
    {
    public:
        BufferAllocator(size_t blockSize)
            : blockSize(blockSize)
        {
            // We allocate buffer storage in chunks of 32 blocks
            storage.push_back(std::vector<FloatType>(32u * blockSize));
        }

        void reset()
        {
            nextChunk = 0;
            chunkOffset = 0;
        }

        FloatType* next()
        {
            if (nextChunk >= storage.size()) {
                storage.push_back(std::vector<FloatType>(32u * blockSize));
            }

            auto it = storage.begin();
            std::advance(it, nextChunk);

            auto& chunk = *it;
            auto* result = chunk.data() + chunkOffset;

            chunkOffset += blockSize;

            if (chunkOffset >= chunk.size()) {
                nextChunk++;
                chunkOffset = 0;
            }

            return result;
        }

    private:
        std::list<std::vector<FloatType>> storage;

        size_t blockSize = 0;
        size_t nextChunk = 0;
        size_t chunkOffset = 0;
    };

    template <typename FloatType>
    class RootRenderSequence
    {
    public:
        RootRenderSequence(std::unordered_map<NodeId, FloatType*>& bm, std::shared_ptr<RootNode<FloatType>>& root)
            : rootPtr(root)
            , bufferMap(bm)
        {}

        void push(BufferAllocator<FloatType>& ba, std::shared_ptr<GraphNode<FloatType>>& node)
        {
            // First we update our node and tap registry to make sure we can easily visit them
            // for tap promotion and event propagation
            nodeList.push_back(node);

            if (auto tap = std::dynamic_pointer_cast<TapOutNode<FloatType>>(node)) {
                tapList.push_back(tap);
            }

            // Next we prepare the render operation
            bufferMap.emplace(node->getId(), ba.next());

            renderOps.push_back([=, bufferMap = this->bufferMap](HostContext<FloatType>& ctx) {
                auto* outputData = bufferMap.at(node->getId());

                node->process(BlockContext<FloatType> {
                    ctx.inputData,
                    ctx.numInputChannels,
                    outputData,
                    ctx.numSamples,
                    ctx.userData,
                });
            });
        }

        void push(BufferAllocator<FloatType>& ba, std::shared_ptr<GraphNode<FloatType>>& node, std::vector<NodeId> const& children)
        {
            // First we update our node and tap registry to make sure we can easily visit them
            // for tap promotion and event propagation
            nodeList.push_back(node);

            if (auto tap = std::dynamic_pointer_cast<TapOutNode<FloatType>>(node)) {
                tapList.push_back(tap);
            }

            // Next we prepare the render operation
            bufferMap.emplace(node->getId(), ba.next());

            // Allocate room for the child pointers here, gets moved into the lambda capture group below
            std::vector<FloatType*> ptrs(children.size());

            renderOps.push_back([=, bufferMap = this->bufferMap, ptrs = std::move(ptrs)](HostContext<FloatType>& ctx) mutable {
                auto* outputData = bufferMap.at(node->getId());
                auto const numChildren = children.size();

                for (size_t j = 0; j < numChildren; ++j) {
                    ptrs[j] = bufferMap.at(children[j]);
                }

                node->process(BlockContext<FloatType> {
                    const_cast<const FloatType**>(ptrs.data()),
                    numChildren,
                    outputData,
                    ctx.numSamples,
                    ctx.userData,
                });
            });
        }

        void processQueuedEvents(std::function<void(std::string const&, js::Value)>& evtCallback)
        {
            // We don't process events if our root is inactive
            if (rootPtr->getPropertyWithDefault("active", false))
            {
                for (auto& n : nodeList) {
                    n->processEvents(evtCallback);
                }
            }
        }

        void promoteTapBuffers(size_t numSamples)
        {
            // Don't promote if our RootRenderSequence represents a RootNode that has become
            // inactive, even if it's still fading out
            if (rootPtr->getTargetGain() < FloatType(0.5))
                return;

            for (auto& n : tapList) {
                n->promoteTapBuffers(numSamples);
            }
        }

        void process(HostContext<FloatType>& ctx)
        {
            size_t const outChan = rootPtr->getChannelNumber();

            // Nothing to do if this root has stopped running or if it's aimed at
            // an invalid output channel
            if (!rootPtr->stillRunning() || outChan < 0u || outChan >= ctx.numOutputChannels)
                return;

            // Run the subsequence
            for (size_t i = 0; i < renderOps.size(); ++i) {
                renderOps[i](ctx);
            }

            // Sum into the output buffer
            auto* data = bufferMap.at(rootPtr->getId());

            for (size_t j = 0; j < ctx.numSamples; ++j) {
                ctx.outputData[outChan][j] += data[j];
            }
        }

    private:
        std::shared_ptr<RootNode<FloatType>> rootPtr;
        std::vector<std::shared_ptr<GraphNode<FloatType>>> nodeList;
        std::vector<std::shared_ptr<TapOutNode<FloatType>>> tapList;
        std::unordered_map<NodeId, FloatType*>& bufferMap;

        using RenderOperation = std::function<void(HostContext<FloatType>& context)>;
        std::vector<RenderOperation> renderOps;
    };

    template <typename FloatType>
    class GraphRenderSequence
    {
    public:
        GraphRenderSequence() = default;

        void reset()
        {
            subseqs.clear();
            bufferMap.clear();
        }

        void push(RootRenderSequence<FloatType>&& sq)
        {
            subseqs.push_back(std::move(sq));
        }

        void processQueuedEvents(std::function<void(std::string const&, js::Value)>&& evtCallback)
        {
            std::for_each(subseqs.begin(), subseqs.end(), [&](RootRenderSequence<FloatType>& sq) {
                sq.processQueuedEvents(evtCallback);
            });
        }

        void process(
            const FloatType** inputChannelData,
            size_t numInputChannels,
            FloatType** outputChannelData,
            size_t numOutputChannels,
            size_t numSamples,
            void* userData)
        {
            HostContext<FloatType> ctx {
                inputChannelData,
                numInputChannels,
                outputChannelData,
                numOutputChannels,
                numSamples,
                userData,
            };

            // Clear the output channels
            for (size_t i = 0; i < numOutputChannels; ++i) {
                for (size_t j = 0; j < numSamples; ++j) {
                    outputChannelData[i][j] = FloatType(0);
                }
            }

            // Process subsequences
            for (auto& sq : subseqs) {
                sq.process(ctx);
            }

            // Promote tap buffers.
            //
            // This step follows the processing step because we want read-then-write behavior
            // through the tap table for any feedback cycles. That means that, if we have a running
            // graph, and transition to a new graph, the tapIn node gets to read from the tap table,
            // which likely propagates to a corresponding tapOut node which can fill its internal delay
            // line. This then gets promoted in the subsequent step. If we went write-then-read, then
            // the new tapOut node would clobber whatever's in the tap table because it promotes before
            // it gets a chance to see what its corresponding tapIn is providing.
            for (auto& sq : subseqs) {
                sq.promoteTapBuffers(numSamples);
            }
        }

        std::unordered_map<NodeId, FloatType*> bufferMap;

    private:
        std::vector<RootRenderSequence<FloatType>> subseqs;
    };

} // namespace elem

#ifndef ELEM_DBG
  #ifdef NDEBUG
    #define ELEM_DBG(x)
  #else
    #include <iostream>
    #define ELEM_DBG(x) std::cout << "[Debug]" << x << std::endl;
  #endif
#endif

namespace elem
{

    // The Runtime is the primary interface for embedding the Elementary engine within
    // your project, independent of the JavaScript engine.
    //
    // A Runtime instance manages the processing graph, coordinates mutations against
    // that graph, and runs the realtime processing loop. Once the Runtime instance is
    // in place, it can receive instructions from the JavaScript frontend, wherever that
    // frontend is running.
    //
    // Users may also implement custom graph nodes by extending the GraphNode
    // interface and then registering with the Runtime via `registerNodeType`.
    template <typename FloatType>
    class Runtime
    {
    public:
        //==============================================================================
        Runtime(double sampleRate, int blockSize);

        //==============================================================================
        // Apply graph rendering instructions
        int applyInstructions(js::Array const& batch);

        // Run the internal audio processing callback
        void process(
            const FloatType** inputChannelData,
            size_t numInputChannels,
            FloatType** outputChannelData,
            size_t numOutputChannels,
            size_t numSamples,
            void* userData = nullptr);

        //==============================================================================
        // Process queued events
        //
        // This raises events from the processing graph such as new data from analysis nodes
        // or errors encountered while processing.
        void processQueuedEvents(std::function<void(std::string const&, js::Value)>&& evtCallback);

        // Reset the internal graph nodes
        //
        // This allows each node to optionally reset any internal state, such as
        // delay buffers or sample readers.
        void reset();

        //==============================================================================
        // Loads a shared buffer into memory.
        //
        // This method populates an internal map from which any GraphNode can request a
        // shared pointer to the data.
        bool updateSharedResourceMap(std::string const& name, FloatType const* data, size_t size);

        // Removes unused resources from the map
        //
        // This method will retain any resource with references held by an active
        // audio graph.
        void pruneSharedResourceMap();

        // Returns an iterator through the names of the entries in the shared resoure map.
        //
        // Intentionally, this does not provide access to the values in the map.
        typename SharedResourceMap<FloatType>::KeyViewType getSharedResourceMapKeys();

        //==============================================================================
        // For registering custom GraphNode factory functions.
        //
        // New node types must inherit GraphNode, and the factory function must produce a shared
        // pointer to a new node instance.
        //
        // After registering your node type, the runtime instance will be ready to receive
        // instructions for your new type, such as those produced by the frontend
        // from, e.g., `core.render(createNode("myNewNodeType", props, [children]))`
        using NodeFactoryFn = std::function<std::shared_ptr<GraphNode<FloatType>>(NodeId const id, double sampleRate, int const blockSize)>;
        int registerNodeType (std::string const& type, NodeFactoryFn && fn);

        //==============================================================================
        // Returns a copy of the internal graph state representing all known nodes and properties
        js::Object snapshot();

    private:
        //==============================================================================
        // The rendering interface
        enum class InstructionType {
          CREATE_NODE = 0,
          DELETE_NODE = 1,
          APPEND_CHILD = 2,
          SET_PROPERTY = 3,
          ACTIVATE_ROOTS = 4,
          COMMIT_UPDATES = 5,
        };

        int createNode(js::Value const& nodeId, js::Value const& type);
        int deleteNode(js::Value const& nodeId);
        int setProperty(js::Value const& nodeId, js::Value const& prop, js::Value const& v);
        int appendChild(js::Value const& parentId, js::Value const& childId);
        int activateRoots(js::Array const& v);

        BufferAllocator<FloatType> bufferAllocator;
        std::shared_ptr<GraphRenderSequence<FloatType>> rtRenderSeq;
        std::shared_ptr<GraphRenderSequence<FloatType>> buildRenderSequence();
        void traverse(std::set<NodeId>& visited, std::vector<NodeId>& visitOrder, NodeId const& n);

        SingleWriterSingleReaderQueue<std::shared_ptr<GraphRenderSequence<FloatType>>> rseqQueue;

        //==============================================================================
        std::unordered_map<std::string, NodeFactoryFn> nodeFactory;
        std::unordered_map<NodeId, std::shared_ptr<GraphNode<FloatType>>> nodeTable;
        std::unordered_map<NodeId, std::vector<NodeId>> edgeTable;
        std::unordered_map<NodeId, std::shared_ptr<GraphNode<FloatType>>> garbageTable;

        std::set<NodeId> currentRoots;
        RefCountedPool<GraphRenderSequence<FloatType>> renderSeqPool;

        SharedResourceMap<FloatType> sharedResourceMap;

        double sampleRate;
        int blockSize;
    };

    //==============================================================================
    // Details...
    template <typename FloatType>
    Runtime<FloatType>::Runtime(double sampleRate, int blockSize)
        : bufferAllocator(blockSize)
        , sampleRate(sampleRate)
        , blockSize(blockSize)
    {
        DefaultNodeTypes<FloatType>::forEach([this](std::string const& type, NodeFactoryFn&& fn) {
            registerNodeType(type, std::move(fn));
        });
    }

    //==============================================================================
    template <typename FloatType>
    int Runtime<FloatType>::applyInstructions(elem::js::Array const& batch)
    {
        bool shouldRebuild = false;

        // TODO: For correct transaction semantics here, we should createNode into a separate
        // map that only gets merged into the actual nodeMap on commitUpdaes
        for (auto& next : batch) {
            if (!next.isArray())
                return ReturnCode::InvalidInstructionFormat();

            auto const& ar = next.getArray();

            if(!ar[0].isNumber())
                return ReturnCode::InvalidInstructionFormat();

            auto const cmd = static_cast<InstructionType>(static_cast<int>((elem::js::Number) ar[0]));
            auto res = ReturnCode::Ok();

            switch (cmd) {
                case InstructionType::CREATE_NODE:
                    res = createNode(ar[1], ar[2]);
                    break;
                case InstructionType::DELETE_NODE:
                    res = deleteNode(ar[1]);
                    break;
                case InstructionType::SET_PROPERTY:
                    res = setProperty(ar[1], ar[2], ar[3]);
                    break;
                case InstructionType::APPEND_CHILD:
                    res = appendChild(ar[1], ar[2]);
                    break;
                case InstructionType::ACTIVATE_ROOTS:
                    res = activateRoots(ar[1]);
                    shouldRebuild = true;
                    break;
                case InstructionType::COMMIT_UPDATES:
                    if (shouldRebuild) {
                        rseqQueue.push(buildRenderSequence());
                    }
                    break;
                default:
                    break;
            }

            // TODO: And here we should abort the transaction and revert any applied properties
            if (res != ReturnCode::Ok()) {
                return res;
            }
        }

        // While we're here, we scan the garbageTable to see if we can deallocate
        // any nodes who are only held by the garbageTable. That means that the realtime
        // thread has already seen the corresponding DeleteNode events and dropped its references
        for (auto it = garbageTable.begin(); it != garbageTable.end();) {
            if (it->second.use_count() == 1) {
                ELEM_DBG("[Native] pruneNode " << nodeIdToHex(it->second->getId()));
                it = garbageTable.erase(it);
            } else {
                it++;
            }
        }

        return ReturnCode::Ok();
    }

    template <typename FloatType>
    void Runtime<FloatType>::process(const FloatType** inputChannelData, size_t numInputChannels, FloatType** outputChannelData, size_t numOutputChannels, size_t numSamples, void* userData)
    {
        if (rseqQueue.size() > 0) {
            std::shared_ptr<GraphRenderSequence<FloatType>> rseq;

            while (rseqQueue.size() > 0) {
                rseqQueue.pop(rseq);
            }

            rtRenderSeq = rseq;
        }

        if (rtRenderSeq) {
            rtRenderSeq->process(inputChannelData, numInputChannels, outputChannelData, numOutputChannels, numSamples, userData);
        }
    }

    //==============================================================================
    template <typename FloatType>
    int Runtime<FloatType>::createNode(js::Value const& a1, js::Value const& a2)
    {
        if (!a1.isNumber() || !a2.isString())
            return ReturnCode::InvalidInstructionFormat();

        auto const nodeId = static_cast<int32_t>((js::Number) a1);
        auto const type = (js::String) a2;

        ELEM_DBG("[Native] createNode " << type << "#" << nodeIdToHex(nodeId));

        if (nodeFactory.find(type) == nodeFactory.end())
            return ReturnCode::UnknownNodeType();
        if (nodeTable.find(nodeId) != nodeTable.end() || edgeTable.find(nodeId) != edgeTable.end())
            return ReturnCode::NodeAlreadyExists();

        auto node = nodeFactory[type](nodeId, sampleRate, blockSize);
        nodeTable.insert({nodeId, node});
        edgeTable.insert({nodeId, {}});

        return ReturnCode::Ok();
    }

    template <typename FloatType>
    int Runtime<FloatType>::deleteNode(js::Value const& a1)
    {
        if (!a1.isNumber())
            return ReturnCode::InvalidInstructionFormat();

        auto const nodeId = static_cast<int32_t>((js::Number) a1);
        ELEM_DBG("[Native] deleteNode " << nodeIdToHex(nodeId));

        if (nodeTable.find(nodeId) == nodeTable.end() || edgeTable.find(nodeId) == edgeTable.end())
            return ReturnCode::NodeNotFound();

        // Move the node out of the nodeTable. It will be pruned from the garbageTable
        // asynchronously after the renderer has dropped its references.
        garbageTable.insert(nodeTable.extract(nodeId));
        edgeTable.erase(nodeId);

        return ReturnCode::Ok();
    }

    template <typename FloatType>
    int Runtime<FloatType>::setProperty(js::Value const& a1, js::Value const& a2, js::Value const& v)
    {
        if (!a1.isNumber() || !a2.isString())
            return ReturnCode::InvalidInstructionFormat();

        auto const nodeId = static_cast<int32_t>((js::Number) a1);
        auto const prop = (js::String) a2;

        ELEM_DBG("[Native] setProperty " << nodeIdToHex(nodeId) << " " << prop << " " << v.toString());

        if (nodeTable.find(nodeId) == nodeTable.end())
            return ReturnCode::NodeNotFound();

        // This is intentionally called on the non-realtime thread. It is the job
        // of the GraphNode to ensure thread safety between calls to setProperty
        // and calls to its own proces function
        return nodeTable.at(nodeId)->setProperty(prop, v, sharedResourceMap);
    }

    template <typename FloatType>
    int Runtime<FloatType>::appendChild(js::Value const& a1, js::Value const& a2)
    {
        if (!a1.isNumber() || !a2.isNumber())
            return ReturnCode::InvalidInstructionFormat();

        auto const parentId = static_cast<int32_t>((js::Number) a1);
        auto const childId = static_cast<int32_t>((js::Number) a2);

        ELEM_DBG("[Native] appendChild " << nodeIdToHex(childId) << " to parent " << nodeIdToHex(parentId));

        if (nodeTable.find(parentId) == nodeTable.end() || edgeTable.find(parentId) == edgeTable.end())
            return ReturnCode::NodeNotFound();
        if (nodeTable.find(childId) == nodeTable.end())
            return ReturnCode::NodeNotFound();

        edgeTable.at(parentId).push_back(childId);

        return ReturnCode::Ok();
    }

    template <typename FloatType>
    int Runtime<FloatType>::activateRoots(js::Array const& roots)
    {
        // Populate and activate from the incoming event
        std::set<NodeId> active;

        for (auto const& v : roots) {
            if (!v.isNumber())
                return ReturnCode::InvalidInstructionFormat();

            int32_t nodeId = static_cast<int32_t>((js::Number) v);
            ELEM_DBG("[Native] activateRoot " << nodeIdToHex(nodeId));

            if (nodeTable.find(nodeId) == nodeTable.end())
                return ReturnCode::NodeNotFound();

            if (auto ptr = std::dynamic_pointer_cast<RootNode<FloatType>>(nodeTable.at(nodeId))) {
                ptr->setProperty("active", true);
                active.insert(nodeId);
            }
        }

        // Deactivate any prior roots
        for (auto const& n : currentRoots) {
            if (auto ptr = std::dynamic_pointer_cast<RootNode<FloatType>>(nodeTable.at(n))) {
                // If any current root was not marked active in this event, we deactivate it
                if (active.count(n) == 0) {
                    ptr->setProperty("active", false);
                }

                // And if it's still running, we hang onto it
                if (ptr->stillRunning()) {
                    active.insert(n);
                }
            }
        }

        // Merge
        currentRoots = active;
        return ReturnCode::Ok();
    }

    //==============================================================================
    template <typename FloatType>
    void Runtime<FloatType>::processQueuedEvents(std::function<void(std::string const&, js::Value)>&& evtCallback)
    {
        // This looks a little shady, but because of the atomic ref count in std::shared_ptr this assignment
        // is indeed thread-safe
        if (auto ptr = rtRenderSeq)
        {
            ptr->processQueuedEvents(std::move(evtCallback));
        }
    }

    template <typename FloatType>
    void Runtime<FloatType>::reset()
    {
        // TODO: Right now only the SampleNode actually does anything with reset(). Need
        // to add this behavior to nodes like delay and convolve and then should also do
        // a pass here through the tapTable bufers. Edit: wait, don't need that, just need
        // the TapOut nodes to zero themselves out, w00t
        for (auto it = nodeTable.begin(); it != nodeTable.end(); ++it) {
            it->second->reset();
        }
    }

    //==============================================================================
    template <typename FloatType>
    bool Runtime<FloatType>::updateSharedResourceMap(std::string const& name, FloatType const* data, size_t size)
    {
        return sharedResourceMap.insert(
            name,
            std::make_shared<typename SharedResourceBuffer<FloatType>::element_type>(data, data + size)
        );
    }

    template <typename FloatType>
    void Runtime<FloatType>::pruneSharedResourceMap()
    {
        sharedResourceMap.prune();
    }

    template <typename FloatType>
    typename SharedResourceMap<FloatType>::KeyViewType Runtime<FloatType>::getSharedResourceMapKeys()
    {
        return sharedResourceMap.keys();
    }

    template <typename FloatType>
    int Runtime<FloatType>::registerNodeType(std::string const& type, Runtime::NodeFactoryFn && fn)
    {
        if (nodeFactory.find(type) != nodeFactory.end())
            return ReturnCode::NodeTypeAlreadyExists();

        nodeFactory.emplace(type, std::move(fn));
        return ReturnCode::Ok();
    }

    template <typename FloatType>
    js::Object Runtime<FloatType>::snapshot()
    {
        js::Object ret;

        for (auto& [nodeId, node] : nodeTable) {
            ret.insert({nodeIdToHex(nodeId), node->getProperties()});
        }

        return ret;
    }

    //==============================================================================
    template <typename FloatType>
    void Runtime<FloatType>::traverse(std::set<NodeId>& visited, std::vector<NodeId>& visitOrder, NodeId const& n) {
        // If we've already visited this node, skip
        if (visited.count(n) > 0)
            return;

        auto& children = edgeTable.at(n);
        auto const numChildren = children.size();

        for (size_t i = 0; i < numChildren; ++i) {
            traverse(visited, visitOrder, children.at(i));
        }

        visitOrder.push_back(n);
        visited.insert(n);
    }

    template <typename FloatType>
    std::shared_ptr<GraphRenderSequence<FloatType>> Runtime<FloatType>::buildRenderSequence()
    {
        // Grab a fresh render sequence
        auto rseq = renderSeqPool.allocate();

        // Clear in case it was already used
        rseq->reset();

        // Reset our buffer allocator
        bufferAllocator.reset();

        // Here we iterate all current roots and visit the graph from each
        // root, pushing onto the render sequence.
        //
        // We're careful to traverse the graph starting from all active root
        // nodes before we traverse from any roots marked inactive. This way, after an
        // inactive root node has finished its fade, we can safely stop running its
        // associated RootRenderSequence knowing that any nodes in the dependency graph
        // of the active roots will still get visited.
        //
        // To put that another way, we make sure that all "live" nodes are associated with
        // the RootRenderSequences of the active roots, and any nodes which will be "dead" after
        // the associated root has finished its fade can be safely skipped.
        std::list<std::shared_ptr<RootNode<FloatType>>> sortedRoots;

        // Keep track of visits
        std::set<NodeId> visited;

        for (auto const& n : currentRoots) {
            if (auto ptr = std::dynamic_pointer_cast<RootNode<FloatType>>(nodeTable.at(n))) {
                auto isActive = ptr->getPropertyWithDefault("active", (js::Boolean) false);

                if (isActive) {
                    sortedRoots.push_front(ptr);
                } else {
                    sortedRoots.push_back(ptr);
                }
            }
        }

        for (auto& ptr : sortedRoots) {
            RootRenderSequence<FloatType> rrs(rseq->bufferMap, ptr);

            std::vector<NodeId> visitOrder;
            traverse(visited, visitOrder, ptr->getId());

            std::for_each(visitOrder.begin(), visitOrder.end(), [&](NodeId const& nid) {
                auto& node = nodeTable.at(nid);
                auto& children = edgeTable.at(nid);

                if (children.size() > 0) {
                    rrs.push(bufferAllocator, node, children);
                } else {
                    rrs.push(bufferAllocator, node);
                }
            });

            rseq->push(std::move(rrs));
        }

        return rseq;
    }

} // namespace elem
