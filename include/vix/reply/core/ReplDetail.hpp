/**
 *  @file ReplDetail.hpp
 *  @brief Configuration details for the Vix Reply REPL engine.
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

#ifndef VIX_REPLY_CORE_REPL_DETAIL_HPP
#define VIX_REPLY_CORE_REPL_DETAIL_HPP

#include <cstddef>
#include <string>

namespace vix::reply
{
  /**
   * @brief Runtime configuration for the interactive Reply REPL.
   *
   * ReplConfig controls optional REPL features such as persistent history,
   * startup banners, calculator shortcuts, and future completion support.
   */
  struct ReplConfig
  {
    /**
     * @brief Enables persistent history stored in a file.
     */
    bool enableFileHistory = true;

    /**
     * @brief Path to the history file resolved at runtime.
     *
     * Example: `~/.vix_history`.
     */
    std::string historyFile;

    /**
     * @brief Maximum number of history entries kept in memory and persisted.
     */
    std::size_t maxHistory = 2000;

    /**
     * @brief Shows the banner when the REPL starts.
     */
    bool showBannerOnStart = true;

    /**
     * @brief Shows the banner again when the REPL screen is cleared.
     */
    bool showBannerOnClear = true;

    /**
     * @brief Enables calculator shortcuts.
     *
     * Supported shortcuts:
     * - `.calc <expr>`
     * - `= <expr>`
     */
    bool enableCalculator = true;

    /**
     * @brief Enables completion support.
     *
     * This is currently reserved for future completion features and does not
     * require external dependencies yet.
     */
    bool enableCompletion = false;
  };
}

#endif
