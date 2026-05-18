/**
 *  @file ReplFlow.hpp
 *  @brief Main execution flow for the Vix Reply REPL.
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

#ifndef VIX_REPLY_CORE_REPL_FLOW_HPP
#define VIX_REPLY_CORE_REPL_FLOW_HPP

#include <string>
#include <vector>

namespace vix::reply
{
  /**
   * @brief Runs the interactive Reply REPL flow.
   *
   * This function is the main entry point for launching the REPL engine.
   * It receives the arguments passed to the REPL command and returns a process
   * status code.
   *
   * @param replArgs Arguments passed to the REPL flow.
   * @return Process status code. A value of `0` usually means success.
   */
  int repl_flow_run(const std::vector<std::string> &replArgs);
}

#endif
