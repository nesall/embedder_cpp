//
//  base64 encoding and decoding with C++.
//  Version: 2.rc.09 (header-only)
//
/*
   THIS FILE IS MODIFIED FROM ITS ORIGINAL .H and .CPP FILES BY COMBINING THEM INTO ONE HEADER FILE.

   base64 encoding and decoding with C++.
   More information at
     https://renenyffenegger.ch/notes/development/Base64/Encoding-and-decoding-base-64-with-cpp

   Version: 2.rc.09 (release candidate)

   Copyright (C) 2004-2017, 2020-2022 René Nyffenegger

   This source code is provided 'as-is', without any express or implied
   warranty. In no event will the author be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this source code must not be misrepresented; you must not
      claim that you wrote the original source code. If you use this source code
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original source code.

   3. This notice may not be removed or altered from any source distribution.

   René Nyffenegger rene.nyffenegger@adp-gmbh.ch

*/


#ifndef BASE64_H_C0CE2A47_D10E_42C9_A27C_C883944E704A
#define BASE64_H_C0CE2A47_D10E_42C9_A27C_C883944E704A

#include <string>
#include <algorithm>
#include <stdexcept>

#if __cplusplus >= 201703L
#include <string_view>
#endif  // __cplusplus >= 201703L

// ---------------------------------------------------------------------------
// Public API declarations
// ---------------------------------------------------------------------------

std::string base64_encode(std::string const &s, bool url = false);
std::string base64_encode_pem(std::string const &s);
std::string base64_encode_mime(std::string const &s);
std::string base64_decode(std::string const &s, bool remove_linebreaks = false);
std::string base64_encode(unsigned char const *, size_t len, bool url = false);

#if __cplusplus >= 201703L
std::string base64_encode(std::string_view s, bool url = false);
std::string base64_encode_pem(std::string_view s);
std::string base64_encode_mime(std::string_view s);
std::string base64_decode(std::string_view s, bool remove_linebreaks = false);
#endif

// ---------------------------------------------------------------------------
// Implementation (header-only)
// ---------------------------------------------------------------------------

namespace {

  static const char *base64_chars[2] = {
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789"
      "+/",
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789"
      "-_"
  };

  static unsigned int pos_of_char(const unsigned char chr) {
    if (chr >= 'A' && chr <= 'Z') return chr - 'A';
    else if (chr >= 'a' && chr <= 'z') return chr - 'a' + 26;
    else if (chr >= '0' && chr <= '9') return chr - '0' + 52;
    else if (chr == '+' || chr == '-') return 62;
    else if (chr == '/' || chr == '_') return 63;
    throw std::runtime_error("Input is not valid base64-encoded data.");
  }

  static std::string insert_linebreaks(std::string str, size_t distance) {
    if (str.empty()) return "";
    size_t pos = distance;
    while (pos < str.size()) {
      str.insert(pos, "\n");
      pos += distance + 1;
    }
    return str;
  }

  template <typename String, unsigned int line_length>
  static std::string encode_with_line_breaks(String s) {
    return insert_linebreaks(base64_encode(s, false), line_length);
  }

  template <typename String>
  static std::string encode_pem(String s) {
    return encode_with_line_breaks<String, 64>(s);
  }

  template <typename String>
  static std::string encode_mime(String s) {
    return encode_with_line_breaks<String, 76>(s);
  }

  template <typename String>
  static std::string encode(String s, bool url) {
    return base64_encode(reinterpret_cast<const unsigned char *>(s.data()), s.length(), url);
  }

  template <typename String>
  static std::string decode(String const &encoded_string, bool remove_linebreaks) {
    if (encoded_string.empty()) return {};

    if (remove_linebreaks) {
      std::string copy(encoded_string);
      copy.erase(std::remove(copy.begin(), copy.end(), '\n'), copy.end());
      return base64_decode(copy, false);
    }

    size_t length_of_string = encoded_string.length();
    size_t pos = 0;
    size_t approx_length = length_of_string / 4 * 3;
    std::string ret;
    ret.reserve(approx_length);

    while (pos < length_of_string) {
      size_t pos1 = pos_of_char(encoded_string.at(pos + 1));
      ret.push_back(static_cast<char>(((pos_of_char(encoded_string.at(pos + 0)) << 2)
        + ((pos1 & 0x30) >> 4))));

      if ((pos + 2 < length_of_string) &&
        encoded_string.at(pos + 2) != '=' && encoded_string.at(pos + 2) != '.') {
        unsigned int pos2 = pos_of_char(encoded_string.at(pos + 2));
        ret.push_back(static_cast<char>(((pos1 & 0x0f) << 4) +
          ((pos2 & 0x3c) >> 2)));

        if ((pos + 3 < length_of_string) &&
          encoded_string.at(pos + 3) != '=' && encoded_string.at(pos + 3) != '.') {
          ret.push_back(static_cast<char>(((pos2 & 0x03) << 6) +
            pos_of_char(encoded_string.at(pos + 3))));
        }
      }

      pos += 4;
    }

    return ret;
  }

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API implementation
// ---------------------------------------------------------------------------

inline std::string base64_encode(unsigned char const *bytes_to_encode,
  size_t in_len, bool url) {
  size_t len_encoded = (in_len + 2) / 3 * 4;
  unsigned char trailing_char = url ? '.' : '=';
  const char *base64_chars_ = base64_chars[url];

  std::string ret;
  ret.reserve(len_encoded);
  size_t pos = 0;

  while (pos < in_len) {
    ret.push_back(base64_chars_[(bytes_to_encode[pos + 0] & 0xfc) >> 2]);

    if (pos + 1 < in_len) {
      ret.push_back(base64_chars_[((bytes_to_encode[pos + 0] & 0x03) << 4)
        + ((bytes_to_encode[pos + 1] & 0xf0) >> 4)]);

      if (pos + 2 < in_len) {
        ret.push_back(base64_chars_[((bytes_to_encode[pos + 1] & 0x0f) << 2)
          + ((bytes_to_encode[pos + 2] & 0xc0) >> 6)]);
        ret.push_back(base64_chars_[bytes_to_encode[pos + 2] & 0x3f]);
      } else {
        ret.push_back(base64_chars_[(bytes_to_encode[pos + 1] & 0x0f) << 2]);
        ret.push_back(trailing_char);
      }
    } else {
      ret.push_back(base64_chars_[(bytes_to_encode[pos + 0] & 0x03) << 4]);
      ret.push_back(trailing_char);
      ret.push_back(trailing_char);
    }

    pos += 3;
  }

  return ret;
}

inline std::string base64_encode(std::string const &s, bool url) {
  return encode(s, url);
}

inline std::string base64_encode_pem(std::string const &s) {
  return encode_pem(s);
}

inline std::string base64_encode_mime(std::string const &s) {
  return encode_mime(s);
}

inline std::string base64_decode(std::string const &s, bool remove_linebreaks) {
  return decode(s, remove_linebreaks);
}

#if __cplusplus >= 201703L

inline std::string base64_encode(std::string_view s, bool url) {
  return encode(s, url);
}

inline std::string base64_encode_pem(std::string_view s) {
  return encode_pem(s);
}

inline std::string base64_encode_mime(std::string_view s) {
  return encode_mime(s);
}

inline std::string base64_decode(std::string_view s, bool remove_linebreaks) {
  return decode(s, remove_linebreaks);
}

#endif  // __cplusplus >= 201703L

#endif /* BASE64_H_C0CE2A47_D10E_42C9_A27C_C883944E704A */
