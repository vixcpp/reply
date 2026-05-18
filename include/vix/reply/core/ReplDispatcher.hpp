/**
 *  @file ReplDispatcher.hpp
 *  @brief Command dispatcher for the Vix Reply REPL engine.
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

#ifndef VIX_REPLY_CORE_REPL_DISPATCHER_HPP
#define VIX_REPLY_CORE_REPL_DISPATCHER_HPP

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vix::reply
{
  /**
   * @brief Function type used to execute a REPL command.
   *
   * The function receives the command arguments and returns an integer status
   * code. A value of `0` usually means success.
   */
  using DispatchFn = std::function<int(const std::vector<std::string> &)>;

  /**
   * @brief Maps REPL command names to executable command handlers.
   *
   * Dispatcher is intentionally generic. The standalone Reply module does not
   * depend on the Vix CLI command module. Commands can be registered by the
   * application embedding Reply.
   */
  class Dispatcher
  {
  public:
    /**
     * @brief Creates an empty dispatcher.
     */
    Dispatcher();

    /**
     * @brief Registers or replaces a command handler.
     *
     * @param cmd Command name.
     * @param fn  Command handler.
     */
    void register_command(const std::string &cmd, DispatchFn fn);

    /**
     * @brief Checks whether a command exists.
     *
     * @param cmd Command name.
     * @return True if the command exists, false otherwise.
     */
    bool has(const std::string &cmd) const;

    /**
     * @brief Executes a command by name.
     *
     * @param cmd  Command name.
     * @param args Command arguments.
     * @return Command status code.
     */
    int dispatch(const std::string &cmd, const std::vector<std::string> &args) const;

  private:
    /**
     * @brief Registered command handlers indexed by command name.
     */
    std::unordered_map<std::string, DispatchFn> map_;
  };
}

#endif
