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

#ifndef CAF_DEFAULT_ATTACHABLE_HPP
#define CAF_DEFAULT_ATTACHABLE_HPP

#include "caf/actor_addr.hpp"
#include "caf/attachable.hpp"

namespace caf {

class default_attachable : public attachable {
 public:
  enum observe_type {
    monitor,
    link
  };

  struct observe_token {
    actor_addr observer;
    observe_type type;
  };

  void actor_exited(abstract_actor* self, uint32_t reason) override;

  bool matches(const token& what) override;

  inline static attachable_ptr make_monitor(actor_addr observer) {
    return attachable_ptr{new default_attachable(std::move(observer), monitor)};
  }

  inline static attachable_ptr make_link(actor_addr observer) {
    return attachable_ptr{new default_attachable(std::move(observer), link)};
  }

  class predicate {
   public:
    inline predicate(actor_addr observer, observe_type type)
        : m_observer(std::move(observer)),
          m_type(type) {
      // nop
    }

    inline bool operator()(const attachable_ptr& ptr) const {
      return ptr->matches(observe_token{m_observer, m_type});
    }

   private:
    actor_addr m_observer;
    observe_type m_type;
  };

 private:
  default_attachable(actor_addr observer, observe_type type);
  actor_addr m_observer;
  observe_type m_type;
};

} // namespace caf


#endif // CAF_DEFAULT_ATTACHABLE_HPP
