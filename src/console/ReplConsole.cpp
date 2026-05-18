/**
 *  @file ReplConsole.cpp
 *  @brief Console output and redraw implementation for the Vix Reply REPL.
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

#include <atomic>
#include <iostream>

namespace vix::reply
{
  namespace
  {
    std::atomic<bool> g_editing{false};
    void (*g_redraw_fn)(void *) = nullptr;
    void *g_redraw_ctx = nullptr;

    void before_print()
    {
      if (g_editing.load())
      {
        std::cout << "\r\n";
      }
    }

    void after_print()
    {
      if (g_editing.load() && g_redraw_fn != nullptr)
      {
        g_redraw_fn(g_redraw_ctx);
      }
    }
  }

  void Console::set_editing(bool editing)
  {
    g_editing.store(editing);
  }

  bool Console::is_editing()
  {
    return g_editing.load();
  }

  void Console::set_redraw_callback(void (*fn)(void *ctx), void *ctx)
  {
    g_redraw_fn = fn;
    g_redraw_ctx = ctx;
  }

  void Console::print_out(std::string_view s)
  {
    before_print();
    std::cout << s << std::flush;
    after_print();
  }

  void Console::print_err(std::string_view s)
  {
    before_print();
    std::cerr << s << std::flush;
    after_print();
  }

  void Console::println_out(std::string_view s)
  {
    before_print();

    if (!s.empty())
    {
      std::cout << s;
    }

    std::cout << '\n'
              << std::flush;
    after_print();
  }

  void Console::println_err(std::string_view s)
  {
    before_print();

    if (!s.empty())
    {
      std::cerr << s;
    }

    std::cerr << '\n'
              << std::flush;
    after_print();
  }
}
