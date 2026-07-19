/**
 *  @file ReplUtils.hpp
 *  @brief Utility helpers for the Vix Reply REPL.
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

#ifndef VIX_REPLY_CORE_REPL_UTILS_HPP
#define VIX_REPLY_CORE_REPL_UTILS_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <string_view>

namespace vix::reply
{
  /**
   * @brief Builds the interactive REPL prompt from the current directory.
   *
   * Example format: `vix(<dir>)>`.
   *
   * @param cwd Current working directory.
   * @return Formatted prompt string.
   */
  std::string make_prompt(const std::filesystem::path &cwd);

  /**
   * @brief Splits a command line into shell-like arguments.
   *
   * Supports quoted arguments using double quotes or single quotes.
   *
   * Examples:
   * - `run -- --port 8080`
   * - `new "my app"`
   *
   * @param line Command line to split.
   * @return Parsed command arguments.
   */
  std::vector<std::string> split_command_line(const std::string &line);

  /**
   * @brief Clears the terminal screen.
   *
   * Uses a cross-platform implementation.
   */
  void clear_screen();

  /**
   * @brief Resolves the current user's home directory.
   *
   * This is mainly used to locate the REPL history file.
   *
   * @return User home directory path.
   */
  std::filesystem::path user_home_dir();

  /**
   * @brief Returns a trimmed copy of a string.
   *
   * Removes leading and trailing whitespace.
   *
   * @param s Input string.
   * @return Trimmed string.
   */
  std::string trim_copy(std::string s);

  /**
   * @brief Checks whether a string starts with a prefix.
   *
   * @param s      Input string.
   * @param prefix Prefix to check.
   * @return True if `s` starts with `prefix`, false otherwise.
   */
  bool starts_with(const std::string &s, const std::string &prefix);

  /**
   * @brief Reads an environment variable in a cross-platform way.
   *
   * On Windows, this uses a safe duplicated environment buffer internally.
   * On Unix-like systems, this uses std::getenv().
   *
   * @param name Environment variable name.
   * @return Environment variable value, or std::nullopt if missing or empty.
   */
  std::optional<std::string> get_env(const std::string &name);

  /**
   * @brief Checks whether styled terminal output can be used.
   */
  bool terminal_colors_enabled();

  /**
   * @brief Applies an ANSI style when terminal colors are enabled.
   *
   * @param text Text to decorate.
   * @param code ANSI style code without the escape prefix.
   * @return Styled or plain text.
   */
  std::string terminal_style(
      std::string_view text,
      std::string_view code);
}

#endif
