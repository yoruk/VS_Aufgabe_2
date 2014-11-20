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

#include "caf/all.hpp"

#include "caf/io/publish.hpp"
#include "caf/io/publish_local_groups.hpp"

namespace caf {
namespace io {

namespace {

struct group_nameserver : event_based_actor {
  behavior make_behavior() override {
    return {
      on(atom("GetGroup"), arg_match) >> [](const std::string& name) {
        return make_message(atom("Group"), group::get("local", name));
      }
    };
  }
};

} // namespace <anonymous>

void publish_local_groups(uint16_t port, const char* addr) {
  auto gn = spawn<group_nameserver, hidden>();
  try {
    publish(gn, port, addr);
  }
  catch (std::exception&) {
    anon_send_exit(gn, exit_reason::user_shutdown);
    throw;
  }
}

} // namespace io
} // namespace caf
