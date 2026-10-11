// Trok Chat (.asi) -- Victor_Trok
// As opcoes do Trok Chat (ver options.h).

#include "options.h"

namespace opt {
namespace {

Options g_options;
const Options g_defaults;

} // namespace

Options& Get() {
    return g_options;
}

const Options& Defaults() {
    return g_defaults;
}

} // namespace opt
