#include <onyx_image/types.hpp>

namespace onyx_image {

const char* to_string(decode_error err) noexcept {
    switch (err) {
        case decode_error::none:                return "none";
        case decode_error::invalid_format:      return "invalid_format";
        case decode_error::unsupported_version: return "unsupported_version";
        case decode_error::unsupported_encoding: return "unsupported_encoding";
        case decode_error::unsupported_bit_depth: return "unsupported_bit_depth";
        case decode_error::dimensions_exceeded: return "dimensions_exceeded";
        case decode_error::truncated_data:      return "truncated_data";
        case decode_error::io_error:            return "io_error";
        case decode_error::internal_error:      return "internal_error";
    }
    return "unknown";
}

} // namespace onyx_image
