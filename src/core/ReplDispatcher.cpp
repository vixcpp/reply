/**
 *  @file ReplDispatcher.cpp
 *  @brief Command dispatcher implementation for the Vix Reply REPL.
 *
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira. All rights reserved.
 *  https://github.com/vixcpp/reply
 *
 *  Use of this source code is governed by an MIT license
 *  that can be found in the LICENSE file.
 *
 *  Vix Reply
 */

#include <vix/reply/core/ReplDispatcher.hpp>

namespace vix::reply
{
  Dispatcher::Dispatcher() = default;

  void Dispatcher::register_command(const std::string &cmd, DispatchFn fn)
  {
    if (cmd.empty() || !fn)
    {
      return;
    }

    map_[cmd] = std::move(fn);
  }

  bool Dispatcher::has(const std::string &cmd) const
  {
    return map_.find(cmd) != map_.end();
  }

  int Dispatcher::dispatch(const std::string &cmd, const std::vector<std::string> &args) const
  {
    const auto it = map_.find(cmd);

    if (it == map_.end())
    {
      return 127;
    }

    return it->second(args);
  }
}
