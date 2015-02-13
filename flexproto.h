
#ifndef FLEXPROTO_H
#define FLEXPROTO_H

#include <cstdint>
#include <string>
#include <vector>
#include <type_traits>
#include <stdexcept>

// A very simple framework for constructing protocols and files. The intent is a very lightweight
// encoding that is platform independent and achieves reasonable efficiency with little design
// effort.
// 
// All integers are flex-encoded using base-128 varint encoding. The low 7 bits of each byte are
// data, the high bit indicates that higher significance bits exist in the subsequent byte(s).
// 
// Signed integers are encoded in zig-zag form, so their size grows with their magnitude regardless
// of their sign.
// 
// Strings/blobs are an unsigned integer followed by raw bytes.
// 
// There are no explicit floating point values. They can be achieved in effect by sending separate
// exponents and mantissas.
// 
// There are no tags, field IDs, etc.

namespace flexproto {

template<typename T> struct flexproto_traits {};

template<>
struct flexproto_traits<uint64_t> {
    static constexpr size_t max_encoded_size = 10;// 70 bits
};

template<>
struct flexproto_traits<int64_t> {
    static constexpr size_t max_encoded_size = 10;// 70 bits
};

template<>
struct flexproto_traits<uint32_t> {
    static constexpr size_t max_encoded_size = 5;// 35 bits
};

template<>
struct flexproto_traits<int32_t> {
    static constexpr size_t max_encoded_size = 5;// 35 bits
};

template<>
struct flexproto_traits<uint16_t> {
    static constexpr size_t max_encoded_size = 3;// 21 bits
};

template<>
struct flexproto_traits<int16_t> {
    static constexpr size_t max_encoded_size = 3;// 21 bits
};

template<>
struct flexproto_traits<uint8_t> {
    static constexpr size_t max_encoded_size = 2;// 14 bits
};

template<>
struct flexproto_traits<int8_t> {
    static constexpr size_t max_encoded_size = 2;// 14 bits
};



template<typename T>
auto zigzag(T value) -> typename std::make_unsigned<T>::type
{
    using unsignedT = typename std::make_unsigned<T>::type;
    auto unsigned_value = static_cast<unsignedT>(value);
    if(unsigned_value & (unsignedT{1} << (sizeof(unsignedT)*8 - 1)))
        return ~(unsigned_value << 1);
    else
        return (unsigned_value << 1);
}

template<typename T>
auto unzigzag(T value) -> typename std::make_signed<T>::type
{
    using signedT = typename std::make_signed<T>::type;
    if(value & 1)
        return ~signedT(value >> 1);
    else
        return signedT(value >> 1);
}


template<typename T,
         typename std::enable_if<std::is_unsigned<T>::value>::type * = nullptr>
auto encode(uint8_t *& data, uint8_t * end_data, T value) -> void
{
    while(value && data != end_data)
    {
        *data++ = ((value > 0x7F) << 7) | (value & 0x7F);
        value >>= 7;
    }
    if(value && data == end_data)
        throw std::length_error("flexproto::encode(): Insufficient room to encode flex-int.");
}

template<typename T,
         typename std::enable_if<std::is_signed<T>::value>::type * = nullptr>
auto encode(uint8_t *& data, uint8_t * end_data, T value) -> void
{
    encode(data, end_data, zigzag(value));
}



template<typename T,
         typename std::enable_if<std::is_unsigned<T>::value>::type * = nullptr>
auto decode(const uint8_t *& data, const uint8_t * end_data) -> T
{
    T value = *data & 0x7F;
    auto shift = 7;
    while(data != end_data && *data & 0x80)
    {
        value |= T(*(++data) & 0x7F) << shift;
        shift += 7;
    }
    if(data == end_data)
        throw std::length_error("flexproto::decode(): Input ended early.");
    
    ++data;
    return value;
}

template<typename T,
         typename std::enable_if<std::is_signed<T>::value>::type * = nullptr>
auto decode(const uint8_t *& data, const uint8_t * end_data) -> T
{
    using unsignedT = typename std::make_unsigned<T>::type;
    return unzigzag(decode<unsignedT>(data, end_data));
}


inline auto encode_string(uint8_t *& data, uint8_t * end_data, const std::string & value) -> void
{
    encode(data, end_data, value.length());
    
    if(data + value.length() > end_data)
        throw std::length_error("flexproto::encode_string(): Insufficient room to encode string.");
    
    memcpy(data, value.data(), value.length());
    data += value.length();
}

inline auto decode_string(const uint8_t *& data, const uint8_t * end_data) -> std::string
{
    auto length = decode<uint64_t>(data, end_data);
    
    if(data + length > end_data)
        throw std::length_error("flexproto::decode_string(): Input ended early.");
    
    auto strdata = reinterpret_cast<const char *>(data);
    data += length;
    return std::string(strdata, length);
}


inline auto encode_other(uint8_t *& data, uint8_t * end_data, const std::string & value) -> void
{
    encode_string(data, end_data, value);
}

inline auto decode_other(const uint8_t *& data, const uint8_t * end_data,
                         std::string & value) -> void
{
    value = decode_string(data, end_data);
}

inline auto encode_other(uint8_t *& data, uint8_t * end_data,
                         const std::vector<uint8_t> & value) -> void
{
    encode(data, end_data, value.size());
    
    if(data + value.size() > end_data)
        throw std::length_error("flexproto::encode_other<std::vector>(): Insufficient room to encode byte vector.");
    
    memcpy(data, value.data(), value.size());
    data += value.size();
}

inline auto decode_other(const uint8_t *& data, const uint8_t * end_data,
                         std::vector<uint8_t> & value) -> void
{
    auto size = decode<uint64_t>(data, end_data);
    
    if(data + size > end_data)
        throw std::length_error("flexproto::decode_other<std::vector>(): Input ended early.");
    
    value.insert(value.end(), data, data + size);
    data += size;
}

} // namespace flexproto
#endif // FLEXPROTO_H
