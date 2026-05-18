/**
 *  @file ReplConsole.hpp
 *  @brief Console output and redraw helpers for the Vix Reply REPL.
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

#ifndef VIX_REPLY_CONSOLE_REPL_CONSOLE_HPP
#define VIX_REPLY_CONSOLE_REPL_CONSOLE_HPP

#include <string>
#include <string_view>

namespace vix::reply
{
  /**
   * @brief Provides console output helpers used by the interactive REPL.
   *
   * The Console structure centralizes stdout/stderr printing and allows
   * the REPL line editor to temporarily redraw the current input line when
   * output is emitted while the user is editing.
   */
  struct Console
  {
    /**
     * @brief Enables or disables editing mode.
     *
     * Editing mode tells the console that the user is currently typing in
     * the interactive REPL prompt.
     *
     * @param editing True when the REPL is editing a line, false otherwise.
     */
    static void set_editing(bool editing);

    /**
     * @brief Checks whether the REPL is currently in editing mode.
     *
     * @return True if editing mode is active, false otherwise.
     */
    static bool is_editing();

    /**
     * @brief Registers a callback used to redraw the current REPL input line.
     *
     * The callback is called when console output needs to be printed while
     * the user is editing a line. This allows the REPL to restore the prompt
     * and current input after printing external output.
     *
     * @param fn  Function pointer used to redraw the current input line.
     * @param ctx User-provided context passed to the redraw callback.
     */
    static void set_redraw_callback(void (*fn)(void *ctx), void *ctx);

    /**
     * @brief Prints text to the standard output stream.
     *
     * @param s Text to print.
     */
    static void print_out(std::string_view s);

    /**
     * @brief Prints text to the standard error stream.
     *
     * @param s Text to print.
     */
    static void print_err(std::string_view s);

    /**
     * @brief Prints a line to the standard output stream.
     *
     * @param s Text to print before the newline.
     */
    static void println_out(std::string_view s = {});

    /**
     * @brief Prints a line to the standard error stream.
     *
     * @param s Text to print before the newline.
     */
    static void println_err(std::string_view s = {});
  };
}

#endif
