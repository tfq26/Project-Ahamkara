#include "wish/admin/html_escape.h"

namespace wish::admin {

std::string escape_html(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size());

    for (const char c : text) {
        switch (c) {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '"':
            escaped += "&quot;";
            break;
        case '\'':
            escaped += "&#39;";
            break;
        default:
            escaped += c;
            break;
        }
    }

    return escaped;
}

} // namespace wish::admin
