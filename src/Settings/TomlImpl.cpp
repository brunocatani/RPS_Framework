// toml++'s implementation, compiled once into RPSSettings. Store.cpp includes
// its declarations only (TOML_HEADER_ONLY=0, see CMakeLists.txt).
//
// F4VR-CommonFramework compiles the same toml++ the same way. A mod that links
// both libraries gets two objects with identical standalone definitions, and
// the linker pulls only the first one it reaches. With header-only toml++ here,
// Store.obj's inline copies clashed with the framework's (LNK2005).
#define TOML_IMPLEMENTATION
#include <toml++/toml.hpp>
