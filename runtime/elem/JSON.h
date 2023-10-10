#pragma once

#include "./deps/json.hpp"
#include "Value.h"


namespace elem
{
namespace js
{

    // Deserialize a JSON string into a Value
    //
    // This uses the nlohmann/json library for parsing the json string, with the
    // SAX event consumer for building up our Value in response to the given
    // parse events.
    static Value parseJSON (std::string const& str)
    {
        using json = nlohmann::json;

        struct sax_event_consumer : public json::json_sax_t
        {
            Value top;
            std::vector<Value*> stack;
            std::string pendingKey;

            bool push(Value && v)
            {
                bool const descend = v.isArray() || v.isObject();

                // First value that we push will become our top-level result
                if (top.isUndefined())
                {
                    // If the top level value is neither an object nor an array,
                    // we have a parsing problem
                    if (!descend)
                        return false;

                    top = std::move(v);
                    stack.push_back(&top);
                    return true;
                }

                auto* current = stack.back();

                if (current->isArray())
                {
                    current->getArray().push_back(std::move(v));

                    if (descend)
                    {
                        auto& a = current->getArray();
                        stack.push_back(&(a.back()));
                    }

                    return true;
                }

                if (current->isObject())
                {
                    // If we're pushing a value onto an object and don't have a corresponding
                    // key to write it with, we have a problem
                    if (pendingKey.empty())
                        return false;

                    current->getObject().insert({pendingKey, std::move(v)});

                    if (descend)
                    {
                        auto& o = current->getObject();
                        stack.push_back(&(o.at(pendingKey)));
                    }

                    pendingKey.clear();
                    return true;
                }

                return false;
            }

            bool null() override
            {
                return push(Null());
            }

            bool boolean(bool val) override
            {
                return push(Value(val));
            }

            bool number_integer(number_integer_t val) override
            {
                return push(Value((double) val));
            }

            bool number_unsigned(number_unsigned_t val) override
            {
                return push(Value((double) val));
            }

            bool number_float(number_float_t val, const string_t& /* s */) override
            {
                return push(Value((double) val));
            }

            bool string(string_t& val) override
            {
                return push(Value(val));
            }

            bool start_object(std::size_t /* elements */) override
            {
                return push(Object());
            }

            bool end_object() override
            {
                stack.pop_back();
                return true;
            }

            bool start_array(std::size_t /* elements */) override
            {
                return push(Array());
            }

            bool end_array() override
            {
                stack.pop_back();
                return true;
            }

            bool key(string_t& val) override
            {
                pendingKey = val;
                return true;
            }

            bool binary(json::binary_t& /* val */) override
            {
                throw std::runtime_error("Deserializing binary is not supported.");
            }

            bool parse_error(std::size_t /* position */, const std::string& /* last_token */, const json::exception& ex) override
            {
                throw std::runtime_error("Parse error:" + std::string(ex.what()));
            }
        };

        sax_event_consumer sec;

        if (!json::sax_parse(str, &sec))
            throw std::runtime_error("Failed to parse json string.");

        return sec.top;
    }

    namespace detail
    {

        template <typename Stream>
        static void serialize (Stream& output, Value const& v);

        template <typename Stream>
        static void serializeNull (Stream& output) {
            output << "null";
        }

        template <typename Stream>
        static void serialize (Stream& output, Boolean const& v) {
            output << (v ? "true" : "false");
        }

        template <typename Stream>
        static void serialize (Stream& output, Number const& v) {
            // Using nlohmann::json's serializer to ensure that our float representation
            // matches the json spec
            output << nlohmann::json(v).dump();
        }

        template <typename Stream>
        static void serialize (Stream& output, String const& v) {
            // Using nlohmann::json's serializer to ensure that our strings are properly
            // escaped and handle unicode characters
            output << nlohmann::json(v).dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        }

        template <typename Stream>
        static void serialize (Stream& output, Array const& v) {
            output << '[';

            for (size_t i = 0; i < v.size(); ++i) {
                if (i != 0) output << ", ";
                serialize(output, v[i]);
            }

            output << ']';
        }

        template <typename Stream>
        static void serialize (Stream& output, Float32Array const& v) {
            output << '[';

            for (size_t i = 0; i < v.size(); ++i) {
                if (i != 0) output << ", ";
                serialize(output, v[i]);
            }

            output << ']';
        }

        template <typename Stream>
        static void serialize (Stream& output, Object const& v) {
            size_t i = 0;
            output << '{';

            for (const auto& [key, val] : v) {
                if (i++ != 0) output << ", ";

                // Using nlohmann::json's serializer to ensure that our key strings
                // are properly escaped and handle unicode characters
                output << nlohmann::json(key).dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
                output << ": ";
                serialize(output, val);
            }

            output << '}';
        }

        template <typename Stream>
        static void serialize (Stream& output, Value const& v) {
            if (v.isUndefined())    return (void) detail::serializeNull(output);
            if (v.isNull())         return (void) detail::serializeNull(output);
            if (v.isBool())         return (void) detail::serialize(output, (Boolean) v);
            if (v.isNumber())       return (void) detail::serialize(output, (Number) v);
            if (v.isString())       return (void) detail::serialize(output, (String) v);
            if (v.isArray())        return (void) detail::serialize(output, v.getArray());
            if (v.isFloat32Array()) return (void) detail::serialize(output, v.getFloat32Array());
            if (v.isObject())       return (void) detail::serialize(output, v.getObject());

            throw std::runtime_error("Failed to serialize Value: unsupported type.");
        }

    }

    // Serialize a Value object to a JSON string
    static std::string serialize (Value const& v)
    {
        std::ostringstream o;
        detail::serialize(o, v);
        return o.str();
    }

} // namespace js
} // namespace elem
