#!/usr/bin/env ruby

# Base types:
# :int8_t, :int16_t, :int32_t, :int64_t
# :uint8_t, :uint16_t, :uint32_t, :uint64_t
# :string: Flex coded unsigned int followed by N bytes of data.
# :blob: Synonymous with :string, but decoded to std::vector<uint8_t>.

BASIC_TYPES = [
    :int8_t, :int16_t, :int32_t, :int64_t,
    :uint8_t, :uint16_t, :uint32_t, :uint64_t
]

def emit_struct(fout, struct_name, struct_def)
    fout.puts "struct #{struct_name.to_s} {"
    struct_def.each{|field_name, field_type|
        if(field_type == :string)
            fout.puts "    std::string #{field_name.to_s};"
        elsif(field_type == :blob)
            fout.puts "    std::vector<uint8_t> #{field_name.to_s};"
        else
            fout.puts "    #{field_type.to_s} #{field_name.to_s};"
        end
    }
    fout.puts "};"
end

def emit_encoder(fout, struct_name, struct_def)
    fout.puts "inline auto encode_other(uint8_t *& data, const #{struct_name.to_s} & value) -> void"
    fout.puts "{"
    struct_def.each{|field_name, field_type|
        if(BASIC_TYPES.include?(field_type))
            fout.puts "    encode(data, value.#{field_name.to_s});"
        else
            fout.puts "    encode_other(data, value.#{field_name.to_s});"
        end
    }
    fout.puts "}"
end

def emit_decoder(fout, struct_name, struct_def)
    fout.puts "inline auto decode_other(const uint8_t *& data, #{struct_name.to_s} & value) -> void"
    fout.puts "{"
    struct_def.each{|field_name, field_type|
        if(BASIC_TYPES.include?(field_type))
            fout.puts "    value.#{field_name.to_s} = decode<#{field_type.to_s}>(data);"
        else
            fout.puts "    decode_other(data, value.#{field_name.to_s});"
        end
    }
    fout.puts "}"
end

def emit_structs(fout, struct_defs)
    struct_defs.each{|n, d|
        emit_struct(fout, n, d)
        fout.puts
        emit_encoder(fout, n, d)
        fout.puts
        emit_decoder(fout, n, d)
        fout.puts
        fout.puts
    }
end


def emit_cinc_header(fout, fname)
    fout.puts "#ifndef #{fname.gsub('.', '_').upcase}"
    fout.puts "#define #{fname.gsub('.', '_').upcase}"
    fout.puts ""
    fout.puts '#include "flexproto.h"'
    fout.puts ""
    fout.puts "using namespace flexproto;"
    fout.puts ""
end

def emit_cinc_footer(fout, fname)
    fout.puts "#endif // #{fname.gsub('.', '_').upcase}"
end


if(ARGV.length < 1)
    puts "Usage:"
    puts "flexproto DEFINITION_FILE"
    exit
end

filename = ARGV[0]
cpp_header_name = "#{filename.gsub('.', '_')}.h"

load(filename)

fout = File.new(cpp_header_name, 'w')
emit_cinc_header(fout, cpp_header_name)
emit_structs(fout, $struct_defs)
emit_cinc_footer(fout, cpp_header_name)
