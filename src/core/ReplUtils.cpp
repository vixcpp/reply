/**
 *  @file ReplUtils.cpp
 *  @brief Utility helper implementation for the Vix Reply REPL.
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

#include <vix/reply/core/ReplUtils.hpp>

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <cstdlib>
#endif

namespace vix::reply
{
  namespace
  {
    const char *reply_getenv(const char *name) noexcept
    {
#if defined(_WIN32)
      static thread_local std::string value;
      value.clear();

      char *buffer = nullptr;
      std::size_t length = 0;

      if (_dupenv_s(&buffer, &length, name) != 0 || buffer == nullptr)
      {
        return nullptr;
      }

      value.assign(buffer);
      std::free(buffer);

      return value.empty() ? nullptr : value.c_str();
#else
      return std::getenv(name);
#endif
    }
  }

  std::optional<std::string> get_env(const std::string &name)
  {
    const char *value = reply_getenv(name.c_str());

    if (value == nullptr || *value == '\0')
    {
      return std::nullopt;
    }

    return std::string(value);
  }

  std::string trim_copy(std::string s)
  {
    auto is_space = [](unsigned char c)
    {
      return std::isspace(c) != 0;
    };

    while (!s.empty() && is_space(static_cast<unsigned char>(s.front())))
    {
      s.erase(s.begin());
    }

    while (!s.empty() && is_space(static_cast<unsigned char>(s.back())))
    {
      s.pop_back();
    }

    return s;
  }

  bool starts_with(const std::string &s, const std::string &prefix)
  {
    return s.size() >= prefix.size() &&
           s.compare(0, prefix.size(), prefix) == 0;
  }

  std::filesystem::path user_home_dir()
  {
#if defined(_WIN32)
    if (auto home = get_env("USERPROFILE"))
    {
      return std::filesystem::path(*home);
    }

    auto drive = get_env("HOMEDRIVE");
    auto path = get_env("HOMEPATH");

    if (drive && path)
    {
      return std::filesystem::path(*drive + *path);
    }

    return std::filesystem::current_path();
#else
    if (auto home = get_env("HOME"))
    {
      return std::filesystem::path(*home);
    }

    return std::filesystem::current_path();
#endif
  }

  std::string make_prompt(const std::filesystem::path &cwd)
  {
    (void)cwd;
    return ">>> ";
  }

  void clear_screen()
  {
#if defined(_WIN32)
    const int rc = std::system("cls");
#else
    const int rc = std::system("clear");
#endif

    (void)rc;
  }

  std::vector<std::string> split_command_line(const std::string &line)
  {
    std::vector<std::string> out;
    std::string current;
    current.reserve(line.size());

    bool inQuotes = false;
    char quoteChar = '\0';
    bool escaping = false;

    auto push_current = [&]()
    {
      if (!current.empty())
      {
        out.push_back(current);
        current.clear();
      }
    };

    for (std::size_t i = 0; i < line.size(); ++i)
    {
      const char c = line[i];

      if (escaping)
      {
        current.push_back(c);
        escaping = false;
        continue;
      }

      if (c == '\\')
      {
        escaping = true;
        continue;
      }

      if (inQuotes)
      {
        if (c == quoteChar)
        {
          inQuotes = false;
          quoteChar = '\0';
          continue;
        }

        current.push_back(c);
        continue;
      }

      if (c == '"' || c == '\'')
      {
        inQuotes = true;
        quoteChar = c;
        continue;
      }

      if (std::isspace(static_cast<unsigned char>(c)))
      {
        push_current();
        continue;
      }

      current.push_back(c);
    }

    push_current();
    return out;
  }
}
