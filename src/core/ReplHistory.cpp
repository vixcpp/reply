/**
 *  @file ReplHistory.cpp
 *  @brief History storage and persistence implementation for the Vix Reply REPL.
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

#include <vix/reply/core/ReplHistory.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace vix::reply
{
  History::History(std::size_t maxItems)
      : max_(maxItems)
  {
  }

  void History::add(const std::string &line)
  {
    if (line.empty())
    {
      return;
    }

    if (!items_.empty() && items_.back() == line)
    {
      return;
    }

    items_.push_back(line);

    if (items_.size() > max_)
    {
      const std::size_t overflow = items_.size() - max_;

      items_.erase(
          items_.begin(),
          std::next(
              items_.begin(),
              static_cast<std::vector<std::string>::difference_type>(overflow)));
    }
  }

  void History::clear()
  {
    items_.clear();
  }

  const std::vector<std::string> &History::items() const noexcept
  {
    return items_;
  }

  bool History::loadFromFile(const std::filesystem::path &file, std::string *err)
  {
    if (err != nullptr)
    {
      err->clear();
    }

    std::ifstream in(file);

    if (!in.is_open())
    {
      return true;
    }

    std::string line;

    while (std::getline(in, line))
    {
      if (!line.empty())
      {
        add(line);
      }
    }

    return true;
  }

  bool History::saveToFile(const std::filesystem::path &file, std::string *err) const
  {
    if (err != nullptr)
    {
      err->clear();
    }

    std::ofstream out(file, std::ios::trunc);

    if (!out.is_open())
    {
      if (err != nullptr)
      {
        *err = "cannot write history file: " + file.string();
      }

      return false;
    }

    for (const auto &line : items_)
    {
      out << line << '\n';
    }

    return true;
  }
}
