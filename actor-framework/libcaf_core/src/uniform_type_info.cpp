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

#include "caf/config.hpp"

#include <map>
#include <set>
#include <locale>
#include <string>
#include <atomic>
#include <limits>
#include <cstring>
#include <cstdint>
#include <type_traits>

#include "caf/intrusive_ptr.hpp"

#include "caf/atom.hpp"
#include "caf/actor.hpp"
#include "caf/message.hpp"
#include "caf/message.hpp"
#include "caf/announce.hpp"
#include "caf/duration.hpp"
#include "caf/uniform_type_info.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/demangle.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/actor_registry.hpp"
#include "caf/detail/to_uniform_name.hpp"
#include "caf/detail/uniform_type_info_map.hpp"

namespace caf {

namespace {

inline detail::uniform_type_info_map& uti_map() {
  return *detail::singletons::get_uniform_type_info_map();
}

} // namespace <anonymous>

uniform_value_t::~uniform_value_t() {
  // nop
}

const uniform_type_info* announce(const std::type_info&,
                  uniform_type_info_ptr utype) {
  return uti_map().insert(std::move(utype));
}

uniform_type_info::~uniform_type_info() {
  // nop
}

const uniform_type_info* uniform_type_info::from(const std::type_info& tinf) {
  auto result = uti_map().by_rtti(tinf);
  if (result == nullptr) {
    std::string error = "uniform_type_info::by_type_info(): ";
    error += detail::to_uniform_name(tinf);
    error += " is an unknown typeid name";
    CAF_LOGM_ERROR("caf::uniform_type_info", error);
    throw std::runtime_error(error);
  }
  return result;
}

const uniform_type_info* uniform_type_info::from(const std::string& name) {
  auto result = uti_map().by_uniform_name(name);
  if (result == nullptr) {
    throw std::runtime_error(name + " is an unknown typeid name");
  }
  return result;
}

uniform_value uniform_type_info::deserialize(deserializer* from) const {
  auto uval = create();
  deserialize(uval->val, from);
  return std::move(uval);
}

std::vector<const uniform_type_info*> uniform_type_info::instances() {
  return uti_map().get_all();
}

const uniform_type_info* uniform_typeid(const std::type_info& tinfo) {
  return uniform_type_info::from(tinfo);
}

} // namespace caf
