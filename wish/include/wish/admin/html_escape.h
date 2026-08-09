#pragma once

#include <string>
#include <string_view>

namespace wish::admin {

// Escape text so it can be safely embedded in HTML. The five characters that
// can break out of HTML text or attribute contexts are replaced with their
// character entities:
//
//   &  ->  &amp;
//   <  ->  &lt;
//   >  ->  &gt;
//   "  ->  &quot;
//   '  ->  &#39;
//
// All other bytes (including UTF-8 sequences, whitespace, and punctuation)
// are preserved verbatim. Because the output is produced by a single pass over
// the input, an already-escaped input (e.g. "&lt;") is double-escaped
// ("&amp;lt;") as expected.
[[nodiscard]] std::string escape_html(std::string_view text);

} // namespace wish::admin
