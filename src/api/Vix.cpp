/**
 *  @file Vix.cpp
 *  @brief Runtime helper object implementation for the Vix Reply REPL.
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

#include <vix/reply/api/Vix.hpp>
#include <vix/reply/core/ReplHistory.hpp>
#include <vix/reply/core/ReplUtils.hpp>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#else
#include <process.h>
#endif

namespace vix::reply::api
{
  Vix::Vix(vix::reply::History *history)
      : history_(history)
  {
  }

  int Vix::pid() const
  {
#ifndef _WIN32
    return static_cast<int>(::getpid());
#else
    return static_cast<int>(_getpid());
#endif
  }

  void Vix::exit(int code)
  {
    exitRequested_ = true;
    exitCode_ = code;
  }

  bool Vix::exit_requested() const
  {
    return exitRequested_;
  }

  int Vix::exit_code() const
  {
    return exitCode_;
  }

  std::filesystem::path Vix::cwd() const
  {
    return std::filesystem::current_path();
  }

  VixResult Vix::cd(const std::string &path)
  {
    std::error_code ec;
    std::filesystem::current_path(path, ec);

    if (ec)
    {
      return {1, "cd: " + ec.message()};
    }

    return {0, ""};
  }

  VixResult Vix::mkdir(const std::string &path, bool recursive)
  {
    std::error_code ec;

    if (recursive)
    {
      std::filesystem::create_directories(path, ec);
    }
    else
    {
      std::filesystem::create_directory(path, ec);
    }

    if (ec)
    {
      return {1, "mkdir: " + ec.message()};
    }

    return {0, ""};
  }

  std::optional<std::string> Vix::env(const std::string &key) const
  {
    return vix::reply::get_env(key);
  }

  const std::vector<std::string> &Vix::args() const
  {
    return args_;
  }

  void Vix::set_args(std::vector<std::string> args)
  {
    args_ = std::move(args);
  }

  VixResult Vix::history()
  {
    if (history_ == nullptr)
    {
      return {1, "history not available"};
    }

    return {0, ""};
  }

  VixResult Vix::history_clear()
  {
    if (history_ == nullptr)
    {
      return {1, "history not available"};
    }

    history_->clear();
    return {0, ""};
  }
}
