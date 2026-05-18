/**
 *  @file ReplApi.hpp
 *  @brief Basic public API helpers exposed inside the Vix Reply REPL.
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

#ifndef VIX_REPLY_API_REPL_API_HPP
#define VIX_REPLY_API_REPL_API_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace vix::reply::api
{
  /**
   * @brief Prints text to stdout without adding a newline.
   *
   * @param s Text to print.
   */
  void print(std::string_view s);

  /**
   * @brief Prints text to stdout and appends a newline.
   *
   * @param s Text to print before the newline.
   */
  void println(std::string_view s = {});

  /**
   * @brief Prints text to stderr without adding a newline.
   *
   * @param s Text to print.
   */
  void eprint(std::string_view s);

  /**
   * @brief Prints text to stderr and appends a newline.
   *
   * @param s Text to print before the newline.
   */
  void eprintln(std::string_view s = {});

  /**
   * @brief Prints an integer to stdout without adding a newline.
   *
   * @param value Integer value to print.
   */
  void print_int(long long value);

  /**
   * @brief Prints an integer to stdout and appends a newline.
   *
   * @param value Integer value to print.
   */
  void println_int(long long value);

  /**
   * @brief Reads one line from stdin.
   *
   * @return The read line, or std::nullopt on EOF.
   */
  std::optional<std::string> readln();

  /**
   * @brief Clears the terminal screen.
   */
  void clear();

  /**
   * @brief Returns the current working directory.
   *
   * @return Current working directory path.
   */
  std::filesystem::path pwd();

  /**
   * @brief Changes the current working directory.
   *
   * @param path Target directory.
   * @param err  Optional output string receiving the error message on failure.
   * @return True on success, false on failure.
   */
  bool cd(const std::filesystem::path &path, std::string *err = nullptr);
}

#endif
