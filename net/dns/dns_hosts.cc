// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_hosts.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"

using base::StringPiece;

namespace net {

namespace {

// Parses the contents of a hosts file.  Returns one token (IP or hostname) at
// a time.  Doesn't copy anything; accepts the file as a StringPiece and
// returns tokens as StringPieces.
class HostsParser {
 public:
  explicit HostsParser(const StringPiece& text, ParseHostsCommaMode comma_mode)
      : text_(text),
        data_(text.data()),
        end_(text.size()),
        pos_(0),
        token_is_ip_(false),
        comma_mode_(comma_mode) {}

  // Advances to the next token (IP or hostname).  Returns whether another
  // token was available.  |token_is_ip| and |token| can be used to find out
  // the type and text of the token.
  bool Advance() {
    bool next_is_ip = (pos_ == 0);
    while (pos_ < end_ && pos_ != std::string::npos) {
      switch (text_[pos_]) {
        case ' ':
        case '\t':
          SkipWhitespace();
          break;

        case '\r':
        case '\n':
          next_is_ip = true;
          pos_++;
          break;

        case '#':
          SkipRestOfLine();
          break;

        case ',':
          if (comma_mode_ == PARSE_HOSTS_COMMA_IS_WHITESPACE) {
            SkipWhitespace();
            break;
          }

          // If comma_mode_ is COMMA_IS_TOKEN, fall through:

        default: {
          size_t token_start = pos_;
          SkipToken();
          size_t token_end = (pos_ == std::string::npos) ? end_ : pos_;

          token_ = StringPiece(data_ + token_start, token_end - token_start);
          token_is_ip_ = next_is_ip;

          return true;
        }
      }
    }

    return false;
  }

  // Fast-forwards the parser to the next line.  Should be called if an IP
  // address doesn't parse, to avoid wasting time tokenizing hostnames that
  // will be ignored.
  void SkipRestOfLine() {
    pos_ = text_.find("\n", pos_);
  }

  // Returns whether the last-parsed token is an IP address (true) or a
  // hostname (false).
  bool token_is_ip() { return token_is_ip_; }

  // Returns the text of the last-parsed token as a StringPiece referencing
  // the same underlying memory as the StringPiece passed to the constructor.
  // Returns an empty StringPiece if no token has been parsed or the end of
  // the input string has been reached.
  const StringPiece& token() { return token_; }

 private:
  void SkipToken() {
    switch (comma_mode_) {
      case PARSE_HOSTS_COMMA_IS_TOKEN:
        pos_ = text_.find_first_of(" \t\n\r#", pos_);
        break;
      case PARSE_HOSTS_COMMA_IS_WHITESPACE:
        pos_ = text_.find_first_of(" ,\t\n\r#", pos_);
        break;
    }
  }

  void SkipWhitespace() {
    switch (comma_mode_) {
      case PARSE_HOSTS_COMMA_IS_TOKEN:
        pos_ = text_.find_first_not_of(" \t", pos_);
        break;
      case PARSE_HOSTS_COMMA_IS_WHITESPACE:
        pos_ = text_.find_first_not_of(" ,\t", pos_);
        break;
    }
  }

  const StringPiece text_;
  const char* data_;
  const size_t end_;

  size_t pos_;
  StringPiece token_;
  bool token_is_ip_;

  const ParseHostsCommaMode comma_mode_;

  DISALLOW_COPY_AND_ASSIGN(HostsParser);
};

void ParseHostsWithCommaMode(const std::string& contents,
                             DnsHosts* dns_hosts,
                             ParseHostsCommaMode comma_mode) {
  CHECK(dns_hosts);

  StringPiece ip_text;
  IPAddress ip;
  AddressFamily family = ADDRESS_FAMILY_IPV4;
  HostsParser parser(contents, comma_mode);
  while (parser.Advance()) {
    if (parser.token_is_ip()) {
      StringPiece new_ip_text = parser.token();
      // Some ad-blocking hosts files contain thousands of entries pointing to
      // the same IP address (usually 127.0.0.1).  Don't bother parsing the IP
      // again if it's the same as the one above it.
      if (new_ip_text != ip_text) {
        IPAddress new_ip;
        if (new_ip.AssignFromIPLiteral(parser.token())) {
          ip_text = new_ip_text;
          ip = new_ip;
          family = (ip.IsIPv4()) ? ADDRESS_FAMILY_IPV4 : ADDRESS_FAMILY_IPV6;
        } else {
          parser.SkipRestOfLine();
        }
      }
    } else {
      DnsHostsKey key(parser.token().as_string(), family);
      key.first = base::ToLowerASCII(key.first);
      IPAddress* mapped_ip = &(*dns_hosts)[key];
      if (mapped_ip->empty())
        *mapped_ip = ip;
      // else ignore this entry (first hit counts)
    }
  }
}

}  // namespace

void ParseHostsWithCommaModeForTesting(const std::string& contents,
                                       DnsHosts* dns_hosts,
                                       ParseHostsCommaMode comma_mode) {
  ParseHostsWithCommaMode(contents, dns_hosts, comma_mode);
}

void ParseHosts(const std::string& contents, DnsHosts* dns_hosts) {
  ParseHostsCommaMode comma_mode;
#if defined(OS_MACOSX)
  // Mac OS X allows commas to separate hostnames.
  comma_mode = PARSE_HOSTS_COMMA_IS_WHITESPACE;
#else
  // Linux allows commas in hostnames.
  comma_mode = PARSE_HOSTS_COMMA_IS_TOKEN;
#endif

  ParseHostsWithCommaMode(contents, dns_hosts, comma_mode);
}

bool ParseHostsFile(const base::FilePath& path, DnsHosts* dns_hosts) {
  dns_hosts->clear();
  // Missing file indicates empty HOSTS.
  if (!base::PathExists(path))
    return true;

  int64_t size;
  if (!base::GetFileSize(path, &size))
    return false;

  UMA_HISTOGRAM_COUNTS("AsyncDNS.HostsSize",
                       static_cast<base::HistogramBase::Sample>(size));

  // Reject HOSTS files larger than |kMaxHostsSize| bytes.
  const int64_t kMaxHostsSize = 1 << 25;  // 32MB
  if (size > kMaxHostsSize)
    return false;

  std::string contents;
  if (!base::ReadFileToString(path, &contents))
    return false;

  ParseHosts(contents, dns_hosts);
  return true;
}

}  // namespace net

