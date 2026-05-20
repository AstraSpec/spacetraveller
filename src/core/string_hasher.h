#ifndef SPACETRAVELLER_STRING_HASHER_H
#define SPACETRAVELLER_STRING_HASHER_H

#include <godot_cpp/variant/string.hpp>

namespace godot {

struct StringHasher {
    size_t operator()(const String &p_string) const {
        return p_string.hash();
    }
};

}

#endif // SPACETRAVELLER_STRING_HASHER_H
