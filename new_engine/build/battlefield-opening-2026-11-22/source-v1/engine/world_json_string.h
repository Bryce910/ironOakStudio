#ifndef WORLD_JSON_STRING_H
#define WORLD_JSON_STRING_H

#include <string>
#include <string_view>

namespace world_json {
// Return one complete JSON string token. Valid UTF-8 bytes are preserved;
// ASCII controls, quotes and backslashes cannot escape the string value.
inline std::string quote(std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (unsigned char c : value) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                out += "\\u00";
                out.push_back(hex[c >> 4]);
                out.push_back(hex[c & 15]);
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    out.push_back('"');
    return out;
}
}

#endif
