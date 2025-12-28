#ifndef ONYX_IMAGE_ONYX_IMAGE_HPP_
#define ONYX_IMAGE_ONYX_IMAGE_HPP_

#include <onyx_image/onyx_image_export.h>
#include <onyx_image/types.hpp>
#include <onyx_image/surface.hpp>
#include <onyx_image/codec.hpp>
#include <onyx_image/palettes.hpp>
#include <onyx_image/codecs/pcx.hpp>
#include <onyx_image/codecs/png.hpp>
#include <onyx_image/codecs/lbm.hpp>
#include <onyx_image/codecs/jpeg.hpp>
#include <onyx_image/codecs/tga.hpp>
#include <onyx_image/codecs/gif.hpp>
#include <onyx_image/codecs/bmp.hpp>
#include <onyx_image/codecs/sunrast.hpp>
#include <onyx_image/codecs/pictor.hpp>
#include <onyx_image/codecs/sgi.hpp>
#include <onyx_image/codecs/pnm.hpp>
#include <onyx_image/codecs/dcx.hpp>
#include <onyx_image/codecs/msp.hpp>
#include <onyx_image/codecs/atarist.hpp>
#include <onyx_image/codecs/qoi.hpp>
#include <onyx_image/codecs/ico.hpp>
#include <onyx_image/codecs/koala.hpp>
#include <onyx_image/codecs/c64_doodle.hpp>
#include <onyx_image/codecs/drazlace.hpp>
#include <onyx_image/codecs/interpaint.hpp>
#include <onyx_image/codecs/ami.hpp>
#include <onyx_image/codecs/funpaint.hpp>
#include <onyx_image/codecs/c64_hires.hpp>
#include <onyx_image/codecs/runpaint.hpp>
#include <onyx_image/codecs/ega_raw.hpp>
#include <onyx_image/codecs/modex_raw.hpp>

namespace onyx_image {

// All public API is included via the headers above.
// See:
//   - types.hpp:    pixel_format, decode_error, decode_result, decode_options
//   - surface.hpp:  Surface concept, memory_surface
//   - codec.hpp:    decoder, codec_registry, decode()
//   - palettes.hpp: Standard retro computer palettes (CGA, EGA, VGA, C64, Amiga, etc.)
//   - codecs/*.hpp: Individual codec implementations

} // namespace onyx_image

#endif // ONYX_IMAGE_ONYX_IMAGE_HPP_
