#ifndef types_hh
#define types_hh

#include "config.h"

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#else
// #error No <inttypes.h> available, please verify the fallback typedefs!
namespace life {
	typedef unsigned int uint32_t;
	typedef unsigned char uint8_t;
}
#endif

namespace life {
	typedef uint32_t key_t;
	typedef uint32_t mask_t;
}

#endif
