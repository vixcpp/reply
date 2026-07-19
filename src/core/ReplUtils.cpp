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

#include <cstdio>
#include <string_view>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
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

  bool terminal_colors_enabled()
  {
    if (get_env("NO_COLOR").has_value())
    {
      return false;
    }

    if (const auto term = get_env("TERM");
        term.has_value() && *term == "dumb")
    {
      return false;
    }

#if defined(_WIN32)
    if (_isatty(_fileno(stdout)) == 0)
    {
      return false;
    }

    const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);

    if (handle == INVALID_HANDLE_VALUE || handle == nullptr)
    {
      return false;
    }

    DWORD mode = 0;

    if (GetConsoleMode(handle, &mode) == 0)
    {
      return false;
    }

    return SetConsoleMode(
               handle,
               mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#else
    return ::isatty(STDOUT_FILENO) != 0;
#endif
  }

  std::string terminal_style(
      std::string_view text,
      std::string_view code)
  {
    if (!terminal_colors_enabled())
    {
      return std::string(text);
    }

    std::string output;
    output.reserve(text.size() + code.size() + 10);

    output += "\033[";
    output += code;
    output += "m";
    output += text;
    output += "\033[0m";

    return output;
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
    std::string location;

    const auto normalized = cwd.lexically_normal();
    const auto home = user_home_dir().lexically_normal();

    if (normalized == home)
    {
      location = "~";
    }
    else
    {
      const auto relative = normalized.lexically_relative(home);
      const std::string relativeText = relative.generic_string();

      if (!relativeText.empty() &&
          relativeText != "." &&
          !starts_with(relativeText, ".."))
      {
        location = "~/" + relativeText;
      }
      else
      {
        location = normalized.filename().string();

        if (location.empty())
        {
          location = normalized.root_path().string();
        }
      }
    }

    constexpr std::string_view brandColor =
        "1;38;2;243;119;38";

    return terminal_style("vix", brandColor) +
           " " +
           location +
           " " +
           terminal_style("❯", brandColor) +
           " ";
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
