
// clang++ --std=c++11 test.cpp -oflexproto_test

#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <array>

#include "flexproto.h"
#include "test_defs.h"

using namespace std;
using namespace flexproto;

bool fail = false;


template<typename T,
         typename std::enable_if<std::is_unsigned<T>::value>::type * = nullptr>
auto print_signedness(T value) -> void
{
    std::cerr << "is unsigned" << std::endl;
}

template<typename T,
         typename std::enable_if<std::is_signed<T>::value>::type * = nullptr>
auto print_signedness(T value) -> void
{
    std::cerr << "is signed" << std::endl;
}


template<typename T,
         typename std::enable_if<std::is_unsigned<T>::value>::type * = nullptr>
auto print_signedness() -> void
{
    std::cerr << "is unsigned" << std::endl;
}

template<typename T,
         typename std::enable_if<std::is_signed<T>::value>::type * = nullptr>
auto print_signedness() -> void
{
    std::cerr << "is signed" << std::endl;
}


template<typename T>
auto TestFullRoundTrip() -> void
{
    // Test values: one of each bit set.
    for(auto j = int{0}; j < sizeof(T)*8; ++j)
    {
        auto buffer = std::array<uint8_t, flexproto_traits<T>::max_encoded_size + 1>();
        buffer.fill(0);
        
        auto val = T(T{1} << j);
        // print_signedness<T>();
        // print_signedness(val);
        
        auto crsr = buffer.data();
        auto readCrsr = (const uint8_t *)buffer.data();
        encode(crsr, val);
        auto roundtrip = decode<T>(readCrsr);
        if(roundtrip != val)
        {
            cout << "Expected to decode " << val << ", got " << roundtrip << endl;
            for(auto b: buffer)
            {
                cout << " "
                     << std::hex << std::setw(2) << std::setfill('0')
                     << uint32_t(b);
            }
            cout << endl;
            fail = true;
        }
    }
}


auto TestStringRoundTrip() -> void
{
    auto buffer = std::array<uint8_t, 64>();
    buffer.fill(0);
    
    auto val = std::string{"Hello World!"};
    auto crsr = buffer.data();
    auto readCrsr = (const uint8_t *)buffer.data();
    encode_string(crsr, val);
    auto roundtrip = decode_string(readCrsr);
    if(roundtrip != val)
    {
        cout << "Expected to decode " << val << ", got " << roundtrip << endl;
        for(auto b: buffer)
        {
            cout << " "
                 << std::hex << std::setw(2) << std::setfill('0')
                 << uint32_t(b);
        }
        cout << endl;
        fail = true;
    }
}


int main(int argc, const char * argv[])
{
    // TODO: automate these...
    for(auto j = int64_t{-15}; j < 16; ++j)
    {
        cout << "zigzag(" << j << "): " << zigzag(j) << endl;
    }
    cout << endl;
    
    for(auto j = uint64_t{0}; j < 30; ++j)
    {
        cout << "unzigzag(" << j << "): " << unzigzag(j) << endl;
    }
    cout << endl;
    
    for(auto j = int{0}; j < 64; ++j)
    {
        auto val = (int64_t{1} << j);
        auto roundtrip = unzigzag(zigzag(val));
        if(roundtrip != val)
        {
            cout << "unzigzag(zigzag(" << val << ")): " << unzigzag(zigzag(val)) << endl;
            fail = true;
        }
    }
    
    TestFullRoundTrip<uint64_t>();
    TestFullRoundTrip<int64_t>();
    TestFullRoundTrip<uint32_t>();
    TestFullRoundTrip<int32_t>();
    TestFullRoundTrip<uint16_t>();
    TestFullRoundTrip<int16_t>();
    TestFullRoundTrip<uint8_t>();
    TestFullRoundTrip<int8_t>();
    
    TestStringRoundTrip();
    
    
    if(!fail)
    {
        cout << "All tests passed." << endl;
    }
    else
    {
        cout << "Some tests failed." << endl;
    }
    
    return (fail)? EXIT_FAILURE : EXIT_SUCCESS;
}

