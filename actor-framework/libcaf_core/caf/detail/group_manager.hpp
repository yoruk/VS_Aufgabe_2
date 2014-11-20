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

#ifndef CAF_DETAIL_GROUP_MANAGER_HPP
#define CAF_DETAIL_GROUP_MANAGER_HPP

#include <map>
#include <mutex>
#include <thread>

#include "caf/abstract_group.hpp"
#include "caf/detail/shared_spinlock.hpp"

#include "caf/detail/singleton_mixin.hpp"

namespace caf {
namespace detail {

class group_manager : public singleton_mixin<group_manager> {

  friend class singleton_mixin<group_manager>;

 public:

  ~group_manager();

  group get(const std::string& module_name,
        const std::string& group_identifier);

  group anonymous();

  void add_module(abstract_group::unique_module_ptr);

  abstract_group::module_ptr get_module(const std::string& module_name);

 private:

  using modules_map = std::map<std::string, abstract_group::unique_module_ptr>;

  modules_map m_mmap;
  std::mutex m_mmap_mtx;

  group_manager();

};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_GROUP_MANAGER_HPP
