/**
 *  @file ReplHistory.hpp
 *  @brief History storage and persistence for the Vix Reply REPL.
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

#ifndef VIX_REPLY_CORE_REPL_HISTORY_HPP
#define VIX_REPLY_CORE_REPL_HISTORY_HPP

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace vix::reply
{
  /**
   * @brief Stores REPL input history and supports file persistence.
   *
   * History keeps the most recent REPL lines up to a configured maximum.
   * It can load entries from a history file and save the current history
   * back to disk.
   */
  class History
  {
  public:
    /**
     * @brief Creates a history container with a maximum number of entries.
     *
     * @param maxItems Maximum number of history entries to keep.
     */
    explicit History(std::size_t maxItems);

    /**
     * @brief Adds a line to the history.
     *
     * @param line Input line to store.
     */
    void add(const std::string &line);

    /**
     * @brief Removes all history entries.
     */
    void clear();

    /**
     * @brief Returns all stored history entries.
     *
     * @return Read-only reference to the stored history entries.
     */
    const std::vector<std::string> &items() const noexcept;

    /**
     * @brief Loads history entries from a file.
     *
     * @param file Path to the history file.
     * @param err  Optional output string receiving the error message on failure.
     * @return True on success, false on failure.
     */
    bool loadFromFile(const std::filesystem::path &file, std::string *err = nullptr);

    /**
     * @brief Saves history entries to a file.
     *
     * @param file Path to the history file.
     * @param err  Optional output string receiving the error message on failure.
     * @return True on success, false on failure.
     */
    bool saveToFile(const std::filesystem::path &file, std::string *err = nullptr) const;

  private:
    /**
     * @brief Maximum number of history entries.
     */
    std::size_t max_;

    /**
     * @brief Stored history entries.
     */
    std::vector<std::string> items_;
  };
}

#endif
