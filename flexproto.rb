#!/usr/bin/env ruby



def zigzag(value, nbits)
    (value & (1 << (nbits - 1))? ~(value << 1): (value << 1)
end

def unzigzag(value)
    (value & 1)? ~(value >> 1) : (value >> 1)
end


def encode_unsigned(value, nbits)

end

