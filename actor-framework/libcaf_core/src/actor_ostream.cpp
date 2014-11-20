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

#include "caf/actor_ostream.hpp"

#include "caf/all.hpp"
#include "caf/local_actor.hpp"
#include "caf/scoped_actor.hpp"

#include "caf/scheduler/abstract_coordinator.hpp"

#include "caf/detail/singletons.hpp"

namespace caf {

actor_ostream::actor_ostream(actor self) : m_self(std::move(self)) {
  m_printer = detail::singletons::get_scheduling_coordinator()->printer();
}

actor_ostream& actor_ostream::write(std::string arg) {
  send_as(m_self, m_printer, atom("add"), std::move(arg));
  return *this;
}

actor_ostream& actor_ostream::flush() {
  send_as(m_self, m_printer, atom("flush"));
  return *this;
}

} // namespace caf

namespace std {

caf::actor_ostream& endl(caf::actor_ostream& o) {
  return o.write("\n");
}

caf::actor_ostream& flush(caf::actor_ostream& o) {
  return o.flush();
}

} // namespace std
