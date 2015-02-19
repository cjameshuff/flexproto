
#ifndef FLEXPROTO_H
#define FLEXPROTO_H

#include <cstdint>
#include <string>
#include <vector>
#include <array>
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
// Variable size arrays are an unsigned integer followed by repetitions of the contained type.
// Fixed size arrays are simply repetitions of the contained type.
// 
// There are no tags, field IDs, etc.
// 
// Possible extension: optional fields, with a bit vector indicating present data.

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

template<typename T>
struct iobuffer_base {
    T * data;
    T * end_data;
    T * crsr;
    iobuffer_base(T * b, size_t s): data{b}, end_data{b + s}, crsr{b} {}
    
    auto size() const -> size_t {return crsr - data;}
    auto space() const -> size_t {return end_data - crsr;}
    auto clear() -> void {crsr = data;}
};


struct obuffer: public iobuffer_base<uint8_t> {
    using iobuffer_base::iobuffer_base;
    
    auto put(uint8_t x) -> void {
        if(crsr == end_data)
            throw std::length_error("DataO::put(): Reached end of output buffer.");
        *crsr++ = x;
    }
    
    auto put(const uint8_t * d, size_t s) -> void {
        if(space() < s)
            throw std::length_error("DataO::put(): Reached end of output buffer.");
        memcpy(crsr, d, s);
        crsr += s;
    }
};

struct ibuffer: public iobuffer_base<const uint8_t> {
    using iobuffer_base::iobuffer_base;
    
    auto get() -> uint8_t {
        if(crsr == end_data)
            throw std::length_error("DataI::get(): Reached end of input buffer.");
        return *crsr++;
    }
    
    auto get(uint8_t * d, size_t s) -> void {
        if(space() < s)
            throw std::length_error("DataI::put(): Reached end of input buffer.");
        memcpy(d, crsr, s);
        crsr += s;
    }
    
    auto get(size_t s) -> const uint8_t * {
        if(space() < s)
            throw std::length_error("DataI::put(): Reached end of input buffer.");
        auto tmp = crsr;
        crsr += s;
        return tmp;
    }
};


template<typename outT, typename T,
         typename std::enable_if<std::is_unsigned<T>::value>::type * = nullptr>
auto encode(outT & data, T value) -> void
{
    for(; value; value >>= 7)
        data.put(((value > 0x7F) << 7) | (value & 0x7F));
}

template<typename outT, typename T,
         typename std::enable_if<std::is_signed<T>::value>::type * = nullptr>
auto encode(outT & data, T value) -> void
{
    encode(data, zigzag(value));
}

template<typename outT, typename T,
         typename std::enable_if<std::is_enum<T>::value>::type * = nullptr>
auto encode(outT & data, T value) -> void
{
    using intT = typename std::underlying_type<T>::type;
    encode(data, intT(value));
}



template<typename inT, typename T,
         typename std::enable_if<std::is_unsigned<T>::value>::type * = nullptr>
auto decode(inT & data, T & value) -> void
{
    auto byte = data.get();
    value = T(byte & 0x7F);
    auto shift = 7;
    while(byte & 0x80)
    {
        byte = data.get();
        value |= T(byte & 0x7F) << shift;
        shift += 7;
    }
}

template<typename inT, typename T,
         typename std::enable_if<std::is_signed<T>::value>::type * = nullptr>
auto decode(inT & data, T & value) -> void
{
    using unsignedT = typename std::make_unsigned<T>::type;
    auto unsignedValue = unsignedT{};
    decode<inT, unsignedT>(data, unsignedValue);
    value = unzigzag(unsignedValue);
}

template<typename inT, typename T,
         typename std::enable_if<std::is_enum<T>::value>::type * = nullptr>
auto decode(inT & data, T & value) -> void
{
    using intT = typename std::underlying_type<T>::type;
    auto intValue = intT{};
    decode<inT, intT>(data, intValue);
    value = T(intValue);
}



// Encode/decode string
template<typename outT>
auto encode(outT & data, const std::string & value) -> void
{
    encode(data, value.length());
    data.put(reinterpret_cast<const uint8_t *>(value.data()), value.length());
}

template<typename inT>
auto decode(inT & data, std::string & value) -> void
{
    auto size = size_t{};
    decode(data, size);
    auto strdata = reinterpret_cast<const char *>(data.get(size));
    value = std::string(strdata, size);
}

// Encode/decode binary blob
template<typename outT>
auto encode_blob(outT & data, const std::vector<uint8_t> & value) -> void
{
    encode(data, value.size());
    data.put(value.data(), value.size());
}

template<typename inT>
auto decode_blob(inT & data, std::vector<uint8_t> & value) -> void
{
    auto size = size_t{};
    decode(data, size);
    value.resize(size);
    data.get(value.data(), size);
}


// Encode/decode arrays of structs
template<typename outT, typename T>
auto encode_variable_array(outT & data, const T & values) -> void
{
    encode(data, values.size());
    for(auto & entry: values)
        encode(data, entry);
}

template<typename inT, typename T>
auto decode_variable_array(inT & data, T & values) -> void
{
    auto size = size_t{};
    decode(data, size);
    values.resize(size);
    for(auto & entry: values)
        decode(data, entry);
}

template<typename outT, typename T>
auto encode_fixed_array(outT & data, const T & values) -> void
{
    for(auto & entry: values)
        encode(data, entry);
}

template<typename inT, typename T>
auto decode_fixed_array(inT & data, T & values) -> void
{
    for(auto & entry: values)
        decode(data, entry);
}

} // namespace flexproto
#endif // FLEXPROTO_H
