/**
 *  @file ReplLineEditor.cpp
 *  @brief Interactive line editor implementation for the Vix Reply REPL.
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

#include <vix/reply/console/ReplConsole.hpp>
#include <vix/reply/core/ReplLineEditor.hpp>

#include <iostream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace vix::reply
{
#ifndef _WIN32
  constexpr unsigned char KEY_CTRL_C = 0x03;
  constexpr unsigned char KEY_CTRL_D = 0x04;
  constexpr unsigned char KEY_CTRL_L = 0x0C;
  constexpr unsigned char KEY_BACKSPACE_1 = 0x08;
  constexpr unsigned char KEY_BACKSPACE_2 = 0x7F;
  constexpr unsigned char KEY_ENTER_1 = '\n';
  constexpr unsigned char KEY_ENTER_2 = '\r';
  constexpr unsigned char KEY_TAB = '\t';
  constexpr unsigned char KEY_ESC = 0x1B;

  static bool read_byte(unsigned char &out)
  {
    const ssize_t n = ::read(STDIN_FILENO, &out, 1);
    return n == 1;
  }

  static void redraw(const std::string &prompt, const std::string &line)
  {
    std::cout << "\r\033[2K" << prompt << line << std::flush;
  }

  struct RedrawCtx
  {
    const std::string *prompt = nullptr;
    const std::string *line = nullptr;
  };

  static void redraw_trampoline(void *ptr)
  {
    auto *ctx = static_cast<RedrawCtx *>(ptr);

    if (ctx == nullptr || ctx->prompt == nullptr || ctx->line == nullptr)
    {
      return;
    }

    redraw(*ctx->prompt, *ctx->line);
  }

  static void end_editing()
  {
    Console::set_redraw_callback(nullptr, nullptr);
    Console::set_editing(false);
  }

  static void print_suggestions_and_redraw(
      const std::vector<std::string> &suggestions,
      const std::string &prompt,
      const std::string &line)
  {
    std::cout << '\n';

    for (const auto &suggestion : suggestions)
    {
      std::cout << suggestion << "  ";
    }

    std::cout << '\n';
    redraw(prompt, line);
  }

  static bool read_escape_sequence(unsigned char &seq2, unsigned char &seq3)
  {
    if (!read_byte(seq2))
    {
      return false;
    }

    if (!read_byte(seq3))
    {
      return false;
    }

    return true;
  }

  ReadStatus read_line_edit(
      const std::string &prompt,
      std::string &outLine,
      const CompletionFn &completer,
      const HistoryNavFn &onHistoryUp,
      const HistoryNavFn &onHistoryDown)
  {
    outLine.clear();

    std::cout << prompt << std::flush;

    Console::set_editing(true);

    RedrawCtx ctx{&prompt, &outLine};
    Console::set_redraw_callback(&redraw_trampoline, &ctx);

    while (true)
    {
      unsigned char ch = 0;

      if (!read_byte(ch))
      {
        end_editing();
        std::cout << '\n';
        return ReadStatus::Eof;
      }

      if (ch == KEY_CTRL_C)
      {
        end_editing();
        std::cout << "^C\n";
        outLine.clear();
        return ReadStatus::Interrupted;
      }

      if (ch == KEY_CTRL_D)
      {
        if (outLine.empty())
        {
          end_editing();
          std::cout << '\n';
          return ReadStatus::Eof;
        }

        continue;
      }

      if (ch == KEY_CTRL_L)
      {
        end_editing();
        outLine.clear();
        std::cout << '\n';
        return ReadStatus::Clear;
      }

      if (ch == KEY_ENTER_1 || ch == KEY_ENTER_2)
      {
        end_editing();
        std::cout << '\n';
        return ReadStatus::Ok;
      }

      if (ch == KEY_BACKSPACE_1 || ch == KEY_BACKSPACE_2)
      {
        if (!outLine.empty())
        {
          outLine.pop_back();
          redraw(prompt, outLine);
        }

        continue;
      }

      if (ch == KEY_TAB)
      {
        if (completer)
        {
          auto result = completer(outLine);

          if (result.changed)
          {
            outLine = result.newLine;
            redraw(prompt, outLine);
            continue;
          }

          if (!result.suggestions.empty())
          {
            print_suggestions_and_redraw(result.suggestions, prompt, outLine);
            continue;
          }
        }

        continue;
      }

      if (ch == KEY_ESC)
      {
        unsigned char a = 0;
        unsigned char b = 0;

        if (!read_escape_sequence(a, b))
        {
          continue;
        }

        if (a == '[')
        {
          if (b == 'A')
          {
            if (onHistoryUp && onHistoryUp(outLine))
            {
              redraw(prompt, outLine);
            }

            continue;
          }

          if (b == 'B')
          {
            if (onHistoryDown && onHistoryDown(outLine))
            {
              redraw(prompt, outLine);
            }

            continue;
          }

          continue;
        }

        continue;
      }

      if (ch < 0x20)
      {
        continue;
      }

      outLine.push_back(static_cast<char>(ch));
      std::cout << static_cast<char>(ch) << std::flush;
    }
  }
#else
  ReadStatus read_line_edit(
      const std::string &prompt,
      std::string &outLine,
      const CompletionFn &,
      const HistoryNavFn &,
      const HistoryNavFn &)
  {
    std::cout << prompt << std::flush;

    if (!std::getline(std::cin, outLine))
    {
      std::cout << '\n';
      return ReadStatus::Eof;
    }

    return ReadStatus::Ok;
  }
#endif
}
