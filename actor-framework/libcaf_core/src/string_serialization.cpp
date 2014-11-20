/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <stack>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include "caf/string_algorithms.hpp"

#include "caf/atom.hpp"
#include "caf/actor.hpp"
#include "caf/channel.hpp"
#include "caf/message.hpp"
#include "caf/node_id.hpp"
#include "caf/to_string.hpp"
#include "caf/actor_addr.hpp"
#include "caf/serializer.hpp"
#include "caf/from_string.hpp"
#include "caf/deserializer.hpp"
#include "caf/skip_message.hpp"
#include "caf/actor_namespace.hpp"
#include "caf/primitive_variant.hpp"
#include "caf/uniform_type_info.hpp"

#include "caf/detail/singletons.hpp"
#include "caf/detail/uniform_type_info_map.hpp"

using std::string;
using std::ostream;
using std::u16string;
using std::u32string;
using std::istringstream;

namespace caf {

namespace {

bool isbuiltin(const string& type_name) {
  return type_name == "@str" || type_name == "@atom" || type_name == "@tuple";
}

class dummy_backend : public actor_namespace::backend {
 public:
  actor_proxy_ptr make_proxy(const node_id&, actor_id) override {
    return nullptr;
  }
};

// serializes types as type_name(...) except:
// - strings are serialized "..."
// - atoms are serialized '...'
class string_serializer : public serializer, public dummy_backend {
 public:
  using super = serializer;

  ostream& out;
  actor_namespace m_namespace;

  class pt_writer : public static_visitor<> {
   public:
    ostream& out;
    bool suppress_quotes;
    pt_writer(ostream& mout, bool suppress = false)
        : out(mout), suppress_quotes(suppress) {
      // nop
    }
    template <class T>
    void operator()(const T& value) {
      out << value;
    }
    // make sure char's are treated as int8_t number, not as character
    inline void operator()(const char& value) {
      out << static_cast<int>(value);
    }
    // make sure char's are treated as int8_t number, not as character
    inline void operator()(const unsigned char& value) {
      out << static_cast<unsigned int>(value);
    }
    void operator()(const string& str) {
      if (!suppress_quotes)
        out << "\"";
      for (char c : str) {
        if (c == '"')
          out << "\\\"";
        else
          out << c;
      }
      if (!suppress_quotes)
        out << "\"";
    }
    void operator()(const u16string&) {
      // nop
    }
    void operator()(const u32string&) {
      // nop
    }
    void operator()(const atom_value& value) {
      out << "'" << to_string(value) << "'";
    }
  };

  bool m_after_value;
  bool m_obj_just_opened;
  std::stack<size_t> m_object_pos;
  std::stack<string> m_open_objects;

  inline void clear() {
    if (m_after_value) {
      out << ", ";
      m_after_value = false;
    } else if (m_obj_just_opened) {
      if (!m_open_objects.empty() && !isbuiltin(m_open_objects.top())) {
        out << " ( ";
      }
      m_obj_just_opened = false;
    }
  }

  string_serializer(ostream& mout)
      : super(&m_namespace),
        out(mout),
        m_namespace(*this),
        m_after_value(false),
        m_obj_just_opened(false) {
    // nop
  }

  void begin_object(const uniform_type_info* uti) {
    clear();
    string tname = uti->name();
    m_open_objects.push(tname);
    // do not print type names for strings and atoms
    if (!isbuiltin(tname) && tname != "@message") {
      // suppress output of "@message ( ... )" because it's redundant
      // since each message immediately calls begin_object(...)
      // for the typed tuple it's containing
      out << tname;
      m_obj_just_opened = true;
    } else if (tname.compare(0, 3, "@<>") == 0) {
      std::vector<string> subtypes;
      split(subtypes, tname, is_any_of("+"), token_compress_on);
      m_obj_just_opened = true;
    }
  }

  void end_object() {
    if (m_obj_just_opened) {
      // no open brackets to close
      m_obj_just_opened = false;
    }
    m_after_value = true;
    if (!m_open_objects.empty()) {
      auto& open_obj = m_open_objects.top();
      if (!isbuiltin(open_obj) && open_obj != "@message") {
        out << (m_after_value ? " )" : ")");
      }
      m_open_objects.pop();
    }
  }

  void begin_sequence(size_t) {
    clear();
    out << "{ ";
  }

  void end_sequence() {
    out << (m_after_value ? " }" : "}");
    m_after_value = true;
  }

  void write_value(const primitive_variant& value) {
    clear();
    if (m_open_objects.empty()) {
      throw std::runtime_error("write_value(): m_open_objects.empty()");
    }
    pt_writer ptw(out);
    apply_visitor(ptw, value);
    m_after_value = true;
  }

  void write_tuple(size_t size, const primitive_variant* values) {
    clear();
    out << "{";
    const primitive_variant* end = values + size;
    for (; values != end; ++values) {
      write_value(*values);
    }
    out << (m_after_value ? " }" : "}");
  }

  void write_raw(size_t num_bytes, const void* buf) {
    clear();
    auto first = reinterpret_cast<const unsigned char*>(buf);
    auto last = first + num_bytes;
    out << std::hex;
    out << std::setfill('0');
    for (; first != last; ++first) {
      out << std::setw(2) << static_cast<size_t>(*first);
    }
    out << std::dec;
    m_after_value = true;
  }
};

class string_deserializer : public deserializer, public dummy_backend {
 public:
  using super = deserializer;

  using difference_type = string::iterator::difference_type;

  string_deserializer(string str)
      : super(&m_namespace), m_str(std::move(str)), m_namespace(*this) {
    m_pos = m_str.begin();
  }

  const uniform_type_info* begin_object() override {
    skip_space_and_comma();
    string type_name;
    // shortcuts for built-in types
    if (*m_pos == '"') {
      type_name = "@str";
    } else if (*m_pos == '\'') {
      type_name = "@atom";
    } else if (isdigit(*m_pos)) {
      type_name = "@i32";
    } else {
      auto substr_end = next_delimiter();
      if (m_pos == substr_end) {
        throw_malformed("could not seek object type name");
      }
      type_name = string(m_pos, substr_end);
      m_pos = substr_end;
    }
    m_open_objects.push(type_name);
    skip_space_and_comma();
    // suppress leading parenthesis for built-in types
    m_obj_had_left_parenthesis.push(try_consume('('));
    auto uti_map = detail::singletons::get_uniform_type_info_map();
    auto res = uti_map->by_uniform_name(type_name);
    if (!res) {
      std::string err = "read type name \"";
      err += type_name;
      err += "\" but no such type is known";
      throw std::runtime_error(err);
    }
    return res;
  }

  void end_object() override {
    if (m_open_objects.empty()) {
      throw std::runtime_error("no object to end");
    }
    if (m_obj_had_left_parenthesis.top() == true) {
      consume(')');
    }
    m_open_objects.pop();
    m_obj_had_left_parenthesis.pop();
    if (m_open_objects.empty()) {
      skip_space_and_comma();
      if (m_pos != m_str.end()) {
        throw_malformed(string("expected end of of string, found: ") + *m_pos);
      }
    }
  }

  size_t begin_sequence() override {
    integrity_check();
    consume('{');
    auto num_vals = count(m_pos, find(m_pos, m_str.end(), '}'), ',') + 1;
    return static_cast<size_t>(num_vals);
  }

  void end_sequence() override {
    integrity_check();
    consume('}');
  }

  struct from_string_reader : static_visitor<> {
    const string& str;
    from_string_reader(const string& s) : str(s) {}
    template <class T>
    void operator()(T& what) {
      istringstream s(str);
      s >> what;
    }
    void operator()(char& what) {
      istringstream s(str);
      int tmp;
      s >> tmp;
      what = static_cast<char>(tmp);
    }
    void operator()(unsigned char& what) {
      istringstream s(str);
      unsigned int tmp;
      s >> tmp;
      what = static_cast<unsigned char>(tmp);
    }
    void operator()(string& what) { what = str; }
    void operator()(atom_value& what) {
      what = static_cast<atom_value>(detail::atom_val(str.c_str(), 0xF));
    }
    void operator()(u16string&) {
      throw std::runtime_error(
        "u16string currently not supported "
        "by string_deserializer");
    }
    void operator()(u32string&) {
      throw std::runtime_error(
        "u32string currently not supported "
        "by string_deserializer");
    }
  };

  void read_value(primitive_variant& storage) override {
    integrity_check();
    skip_space_and_comma();
    if (m_open_objects.top() == "@node") {
      // example node_id: c5c978f5bc5c7e4975e407bb03c751c9374480d9:63768
      // deserialization will call read_raw() followed by read_value()
      // and ':' must be ignored
      consume(':');
    }
    string::iterator substr_end;
    auto find_if_cond = [](char c)->bool {
      switch (c) {
        case ')':
        case '}':
        case ' ':
        case ',':
        case '@':
          return true;
        default:
          return false;
      }
    };
    if (get<string>(&storage) || get<atom_value>(&storage)) {
      char needle = (get<string>(&storage)) ? '"' : '\'';
      if (*m_pos == needle) {
        // skip leading "
        ++m_pos;
        char last_char = needle;
        auto find_if_str_cond = [&last_char, needle ](char c)->bool {
          if (c == needle && last_char != '\\') {
            return true;
          }
          last_char = c;
          return false;
        };
        substr_end = find_if(m_pos, m_str.end(), find_if_str_cond);
      } else {
        substr_end = find_if(m_pos, m_str.end(), find_if_cond);
      }
    } else {
      substr_end = find_if(m_pos, m_str.end(), find_if_cond);
    }
    if (substr_end == m_str.end()) {
      throw std::runtime_error("malformed string (unterminated value)");
    }
    string substr(m_pos, substr_end);
    m_pos += static_cast<difference_type>(substr.size());
    if (get<string>(&storage) || get<atom_value>(&storage)) {
      char needle = (get<string>(&storage)) ? '"' : '\'';
      // skip trailing "
      if (*m_pos != needle) {
        string error_msg;
        error_msg = "malformed string, expected '";
        error_msg += needle;
        error_msg += "' found '";
        error_msg += *m_pos;
        error_msg += "'";
        throw std::runtime_error(error_msg);
      }
      ++m_pos;
      // replace '\"' by '"'
      char last_char = ' ';
      auto cond = [&last_char, needle ](char c)->bool {
        if (c == needle && last_char == '\\') {
          return true;
        }
        last_char = c;
        return false;
      };
      string tmp;
      auto sbegin = substr.begin();
      auto send = substr.end();
      for (auto i = find_if(sbegin, send, cond); i != send;
           i = find_if(i, send, cond)) {
        --i;
        tmp.append(sbegin, i);
        tmp += needle;
        i += 2;
        sbegin = i;
      }
      if (sbegin != substr.begin()) {
        tmp.append(sbegin, send);
      }
      if (!tmp.empty()) {
        substr = std::move(tmp);
      }
    }
    from_string_reader fsr(substr);
    apply_visitor(fsr, storage);
  }

  void read_raw(size_t buf_size, void* vbuf) override {
    if (buf_size == node_id::host_id_size && !m_open_objects.empty()
        && (m_open_objects.top() == "@actor"
            || m_open_objects.top() == "@actor_addr")) {
      // actor addresses are formatted as actor_id@host_id:process_id
      // this read_raw reads the host_id, i.e., we need
      // to skip the '@' character
      consume('@');
    }
    auto buf = reinterpret_cast<unsigned char*>(vbuf);
    integrity_check();
    skip_space_and_comma();
    auto next_nibble = [&]()->size_t {
      if (*m_pos == '\0') {
        throw_malformed("unexpected end-of-string");
      }
      char c = *m_pos++;
      if (!isxdigit(c)) {
        string errmsg = "unexpected character: '";
        errmsg += c;
        errmsg += "', expected [0-9a-f]";
        throw_malformed(errmsg);
      }
      return static_cast<size_t>(isdigit(c) ? c - '0' : (c - 'a' + 10));
    };
    for (size_t i = 0; i < buf_size; ++i) {
      // one byte consists of two nibbles
      auto nibble = next_nibble();
      *buf++ = static_cast<unsigned char>((nibble << 4) | next_nibble());
    }
  }

 private:
  string m_str;
  string::iterator m_pos;
  // size_t m_obj_count;
  std::stack<bool> m_obj_had_left_parenthesis;
  std::stack<string> m_open_objects;
  actor_namespace m_namespace;

  void skip_space_and_comma() {
    while (*m_pos == ' ' || *m_pos == ',')
      ++m_pos;
  }

  void throw_malformed(const string& error_msg) {
    throw std::runtime_error("malformed string: " + error_msg);
  }

  void consume(char c) {
    skip_space_and_comma();
    if (*m_pos != c) {
      string error;
      error += "expected '";
      error += c;
      error += "' found '";
      error += *m_pos;
      error += "'";
      if (m_open_objects.empty() == false) {
        error += "during deserialization an instance of ";
        error += m_open_objects.top();
      }
      throw_malformed(error);
    }
    ++m_pos;
  }

  bool try_consume(char c) {
    skip_space_and_comma();
    if (*m_pos == c) {
      ++m_pos;
      return true;
    }
    return false;
  }

  inline string::iterator next_delimiter() {
    return find_if(m_pos, m_str.end(), [](char c)->bool {
      switch (c) {
        case '(':
        case ')':
        case '{':
        case '}':
        case ' ':
        case ',': { return true; }
        default: { return false; }
      }
    });
  }

  void integrity_check() {
    if (m_open_objects.empty() || m_obj_had_left_parenthesis.empty()) {
      throw_malformed("missing begin_object()");
    }
    if (m_obj_had_left_parenthesis.top() == false
        && !isbuiltin(m_open_objects.top())) {
      throw_malformed(
        "expected left parenthesis after "
        "begin_object call or void value");
    }
  }
};

} // namespace <anonymous>

namespace detail {

string to_string_impl(const void* what, const uniform_type_info* utype) {
  std::ostringstream osstr;
  string_serializer strs(osstr);
  strs.begin_object(utype);
  utype->serialize(what, &strs);
  strs.end_object();
  return osstr.str();
}

template <class T>
inline string to_string_impl(const T& what) {
  return to_string_impl(&what, uniform_typeid<T>());
}

} // namespace detail

string to_string(const message& what) {
  return detail::to_string_impl(what);
}

string to_string(const group& what) {
  return detail::to_string_impl(what);
}

string to_string(const channel& what) {
  return detail::to_string_impl(what);
}

string to_string(const message_id& what) {
  return detail::to_string_impl(what);
}

string to_string(const actor_addr& what) {
  if (what == invalid_actor_addr) {
    return "0@00000000000000000000:0";
  }
  std::ostringstream oss;
  oss << what.id() << "@" << to_string(what.node());
  return oss.str();
}

string to_string(const actor& what) {
  return to_string(what.address());
}

string to_string(const atom_value& what) {
  auto x = static_cast<uint64_t>(what);
  string result;
  result.reserve(11);
  // don't read characters before we found the leading 0xF
  // first four bits set?
  bool read_chars = ((x & 0xF000000000000000) >> 60) == 0xF;
  uint64_t mask = 0x0FC0000000000000;
  for (int bitshift = 54; bitshift >= 0; bitshift -= 6, mask >>= 6) {
    if (read_chars) {
      result += detail::decoding_table[(x & mask) >> bitshift];
    } else if (((x & mask) >> bitshift) == 0xF) {
      read_chars = true;
    }
  }
  return result;
}

string to_string(const node_id::host_id_type& node_id) {
  std::ostringstream oss;
  oss << std::hex;
  oss.fill('0');
  for (size_t i = 0; i < node_id::host_id_size; ++i) {
    oss.width(2);
    oss << static_cast<uint32_t>(node_id[i]);
  }
  return oss.str();
}

string to_string(const node_id& what) {
  std::ostringstream oss;
  oss << to_string(what.host_id()) << ":" << what.process_id();
  return oss.str();
}

string to_verbose_string(const std::exception& e) {
  std::ostringstream oss;
  oss << detail::demangle(typeid(e)) << ": " << e.what();
  return oss.str();
}

ostream& operator<<(ostream& out, skip_message_t) {
  return out << "skip_message";
}

uniform_value from_string_impl(const string& what) {
  string_deserializer strd(what);
  try {
    auto utype = strd.begin_object();
    if (utype) {
      return utype->deserialize(&strd);
    }
    strd.end_object();
  }
  catch (std::runtime_error&) {
    // ignored, i.e., return an invalid value
  }
  return {};
}

} // namespace caf
