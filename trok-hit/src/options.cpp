// Trok Hitmarker (.asi) -- Victor_Trok

#include "options.h"

namespace opt {

Options& Get() {
    static Options o;
    return o;
}

const Options& Defaults() {
    static const Options d;
    return d;
}

} // namespace opt
