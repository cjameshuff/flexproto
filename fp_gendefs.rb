#!/usr/bin/env ruby

BASIC_TYPES = [
    :int8_t, :int16_t, :int32_t, :int64_t,
    :uint8_t, :uint16_t, :uint32_t, :uint64_t
]

def array_type(typename)
    matches = /\Aarray_(\w+)/.match(typename)
    matches[1]
end
def fixarray_parts(typename)
    matches = /\Afixarray_(\d+)_(\w+)/.match(typename)
    [matches[1], matches[2]]
end

def emit_struct(fout, struct_name, struct_def)
    fout.puts "struct #{struct_name.to_s} {"
    struct_def.each{|field_name, field_type|
        if(field_type == :string)
            fout.puts "    std::string #{field_name.to_s};"
        elsif(field_type == :blob)
            fout.puts "    std::vector<uint8_t> #{field_name.to_s};"
        elsif(field_type.to_s.start_with?('array_'))
            type = array_type(field_type.to_s)
            fout.puts "    std::vector<#{type}> #{field_name.to_s};"
        elsif(field_type.to_s.start_with?('fixarray_'))
            size, type = fixarray_parts(field_type.to_s)
            fout.puts "    std::array<#{type}, #{size}> #{field_name.to_s};"
        else
            fout.puts "    #{field_type.to_s} #{field_name.to_s};"
        end
    }
    fout.puts "};"
end

def emit_struct_encoder(fout, struct_name, struct_def)
    fout.puts "template<typename outT>"
    fout.puts "auto encode_other(outT & data, const #{struct_name.to_s} & value) -> void"
    fout.puts "{"
    struct_def.each{|field_name, field_type|
        if(BASIC_TYPES.include?(field_type))
            fout.puts "    encode(data, value.#{field_name});"
        elsif(field_type.to_s.start_with?('array_'))
            type = array_type(field_type.to_s)
            fout.puts "    encode_variable_array(data, value.#{field_name});"
        elsif(field_type.to_s.start_with?('fixarray_'))
            size, type = fixarray_parts(field_type.to_s)
            fout.puts "    encode_fixed_array(data, value.#{field_name});"
        else
            fout.puts "    encode_other(data, value.#{field_name});"
        end
    }
    fout.puts "}"
end

def emit_struct_decoder(fout, struct_name, struct_def)
    fout.puts "template<typename inT>"
    fout.puts "auto decode_other(inT & data, #{struct_name.to_s} & value) -> void"
    fout.puts "{"
    struct_def.each{|field_name, field_type|
        if(BASIC_TYPES.include?(field_type))
            fout.puts "    value.#{field_name} = decode<inT, #{field_type}>(data);"
        elsif(field_type.to_s.start_with?('array_'))
            type = array_type(field_type.to_s)
            fout.puts "    decode_variable_array(data, value.#{field_name});"
        elsif(field_type.to_s.start_with?('fixarray_'))
            size, type = fixarray_parts(field_type.to_s)
            fout.puts "    decode_fixed_array(data, value.#{field_name});"
        else
            fout.puts "    decode_other(data, value.#{field_name});"
        end
    }
    fout.puts "}"
end

def emit_forward_defs(fout, struct_defs)
    struct_defs.each{|n, d|
        if(!$external_structs.include?(n))
            fout.puts "struct #{n};"
        end
    }
    fout.puts
    fout.puts
end

def emit_structs(fout, struct_defs)
    struct_defs.each{|n, d|
        if(!$external_structs.include?(n))
            emit_struct(fout, n, d)
        end
        fout.puts
        emit_struct_encoder(fout, n, d)
        fout.puts
        emit_struct_decoder(fout, n, d)
        fout.puts
        fout.puts
    }
end

def emit_enums(fout, enum_defs)
    enum_defs.each{|n, d|
        fout.puts "enum #{n} {"
        fout.puts d.map{|const_name, const_val|
            if(const_val.nil?)
                "    #{const_name}"
            else
                "    #{const_name} = #{const_val}"
            end
        }.join(",\n")
        fout.puts "};"
        fout.puts
        fout.puts
    }
end

def emit_type_enums(fout, enum_defs)
    enum_defs.each{|n, d|
        fout.puts "template<typename T> struct #{n}_traits {};"
        d.each_with_index{|type_name, index|
            fout.puts "template<> struct #{n}_traits<#{type_name}> {static constexpr uint32_t id = #{index};};"
        }
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

$external_includes = []
$struct_defs = {}
$external_structs = []
$enum_defs = {}
$type_enum_defs = {}

load(filename)

fout = File.new(cpp_header_name, 'w')
emit_cinc_header(fout, cpp_header_name)
$external_includes.each {|incfile| fout.puts "#include \"#{incfile}\""}
fout.puts
emit_forward_defs(fout, $struct_defs)
emit_enums(fout, $enum_defs)
emit_type_enums(fout, $type_enum_defs)
emit_structs(fout, $struct_defs)
emit_cinc_footer(fout, cpp_header_name)
