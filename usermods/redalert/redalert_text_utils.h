#pragma once

#include <Arduino.h>

namespace RedAlertText {

inline int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

inline String decodeUnicodeEscapes(const String& in) {
  String out;
  out.reserve(in.length());

  for (uint16_t i = 0; i < in.length(); i++) {
    char ch = in[i];
    if (ch == '\\' && (i + 5) < in.length() && in[i + 1] == 'u') {
      int h1 = hexNibble(in[i + 2]);
      int h2 = hexNibble(in[i + 3]);
      int h3 = hexNibble(in[i + 4]);
      int h4 = hexNibble(in[i + 5]);
      if (h1 >= 0 && h2 >= 0 && h3 >= 0 && h4 >= 0) {
        uint16_t cp = (h1 << 12) | (h2 << 8) | (h3 << 4) | h4;
        if (cp <= 0x7F) {
          out += (char)cp;
        } else if (cp <= 0x7FF) {
          out += (char)(0xC0 | ((cp >> 6) & 0x1F));
          out += (char)(0x80 | (cp & 0x3F));
        } else {
          out += (char)(0xE0 | ((cp >> 12) & 0x0F));
          out += (char)(0x80 | ((cp >> 6) & 0x3F));
          out += (char)(0x80 | (cp & 0x3F));
        }
        i += 5;
        continue;
      }
    }
    out += ch;
  }
  return out;
}

inline String normalizeAreaText(const String& raw) {
  String s = raw;
  s.trim();
  if (s.indexOf("\\u") >= 0) s = decodeUnicodeEscapes(s);
  while (s.indexOf("  ") >= 0) s.replace("  ", " ");
  return s;
}

} // namespace RedAlertText
