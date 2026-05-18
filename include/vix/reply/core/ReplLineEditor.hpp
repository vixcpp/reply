/**
 *  @file ReplLineEditor.hpp
 *  @brief Interactive line editor for the Vix Reply REPL.
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

#ifndef VIX_REPLY_CORE_REPL_LINE_EDITOR_HPP
#define VIX_REPLY_CORE_REPL_LINE_EDITOR_HPP

#include <functional>
#include <string>
#include <vector>

namespace vix::reply
{
  /**
   * @brief Result status returned by the interactive line reader.
   */
  enum class ReadStatus
  {
    /**
     * @brief A line was read successfully.
     */
    Ok,

    /**
     * @brief End-of-file was reached.
     */
    Eof,

    /**
     * @brief Reading was interrupted by the user.
     */
    Interrupted,

    /**
     * @brief The user requested the screen to be cleared.
     */
    Clear
  };

  /**
   * @brief Result returned by a completion callback.
   */
  struct CompletionResult
  {
    /**
     * @brief True if the current input line was changed by completion.
     */
    bool changed = false;

    /**
     * @brief New input line produced by completion.
     */
    std::string newLine;

    /**
     * @brief Suggestions to display when multiple matches are available.
     */
    std::vector<std::string> suggestions;
  };

  /**
   * @brief Completion callback type used by the line editor.
   *
   * The callback receives the current input line and returns either a changed
   * line, a list of suggestions, or both.
   */
  using CompletionFn = std::function<CompletionResult(const std::string &currentLine)>;

  /**
   * @brief History navigation callback type.
   *
   * The callback can modify the current line in place.
   *
   * @return True if the line changed, false otherwise.
   */
  using HistoryNavFn = std::function<bool(std::string &line)>;

  /**
   * @brief Reads one line from the interactive REPL using line editing.
   *
   * This function handles the prompt, current input line, completion callback,
   * and history navigation callbacks.
   *
   * @param prompt        Prompt displayed before reading input.
   * @param outLine       Output string receiving the completed input line.
   * @param completer     Completion callback.
   * @param onHistoryUp   Callback used to navigate backward in history.
   * @param onHistoryDown Callback used to navigate forward in history.
   * @return Read status describing how the input operation ended.
   */
  ReadStatus read_line_edit(
      const std::string &prompt,
      std::string &outLine,
      const CompletionFn &completer,
      const HistoryNavFn &onHistoryUp,
      const HistoryNavFn &onHistoryDown);
}

#endif
