/**
 *  @file ReplApi.cpp
 *  @brief Basic public API helper implementation for the Vix Reply REPL.
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

#include <vix/reply/api/ReplApi.hpp>
#include <vix/reply/console/ReplConsole.hpp>
#include <vix/reply/core/ReplUtils.hpp>

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace vix::reply::api
{
  void print(std::string_view s)
  {
    Console::print_out(s);
  }

  void println(std::string_view s)
  {
    Console::println_out(s);
  }

  void eprint(std::string_view s)
  {
    Console::print_err(s);
  }

  void eprintln(std::string_view s)
  {
    Console::println_err(s);
  }

  void print_int(long long value)
  {
    Console::print_out(std::to_string(value));
  }

  void println_int(long long value)
  {
    Console::println_out(std::to_string(value));
  }

  std::optional<std::string> readln()
  {
    std::string line;

    if (!std::getline(std::cin, line))
    {
      return std::nullopt;
    }

    return trim_copy(line);
  }

  void clear()
  {
    clear_screen();
  }

  std::filesystem::path pwd()
  {
    return std::filesystem::current_path();
  }

  bool cd(const std::filesystem::path &path, std::string *err)
  {
    std::error_code ec;
    std::filesystem::current_path(path, ec);

    if (ec)
    {
      if (err != nullptr)
      {
        *err = ec.message();
      }

      return false;
    }

    return true;
  }
}
