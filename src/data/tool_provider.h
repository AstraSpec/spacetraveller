#ifndef SPACETRAVELLER_TOOL_PROVIDER_H
#define SPACETRAVELLER_TOOL_PROVIDER_H

#include <cstdint>
#include <vector>

namespace godot {

struct ToolProviderInfo {
    std::vector<uint16_t> qualities;
};

}

#endif
