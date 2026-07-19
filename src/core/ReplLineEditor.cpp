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
#include <vix/reply/core/ReplUtils.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#ifndef _WIN32
#include <sys/select.h>
#include <unistd.h>
#endif

namespace vix::reply
{
  namespace
  {
    constexpr std::string_view colorPreprocessor =
        "1;38;2;243;119;38";

    constexpr std::string_view colorKeyword =
        "1;38;2;255;156;92";

    constexpr std::string_view colorType =
        "38;2;86;182;194";

    constexpr std::string_view colorString =
        "38;2;152;195;121";

    constexpr std::string_view colorNumber =
        "38;2;198;120;221";

    constexpr std::string_view colorComment =
        "38;2;130;140;145";

    constexpr std::string_view colorFunction =
        "38;2;97;175;239";

    constexpr std::string_view colorNamespace =
        "38;2;97;175;239";

    bool is_identifier_start(char value)
    {
      const auto character =
          static_cast<unsigned char>(value);

      return std::isalpha(character) != 0 ||
             value == '_';
    }

    bool is_identifier_character(char value)
    {
      const auto character =
          static_cast<unsigned char>(value);

      return std::isalnum(character) != 0 ||
             value == '_';
    }

    bool is_space(char value)
    {
      return std::isspace(
                 static_cast<unsigned char>(value)) != 0;
    }

    bool is_digit(char value)
    {
      return std::isdigit(
                 static_cast<unsigned char>(value)) != 0;
    }

    bool is_cpp_keyword(std::string_view value)
    {
      static const std::unordered_set<std::string_view> keywords = {
          "alignas",
          "alignof",
          "and",
          "and_eq",
          "asm",
          "atomic_cancel",
          "atomic_commit",
          "atomic_noexcept",
          "auto",
          "bitand",
          "bitor",
          "break",
          "case",
          "catch",
          "class",
          "compl",
          "concept",
          "const",
          "consteval",
          "constexpr",
          "constinit",
          "const_cast",
          "continue",
          "co_await",
          "co_return",
          "co_yield",
          "decltype",
          "default",
          "delete",
          "do",
          "dynamic_cast",
          "else",
          "enum",
          "explicit",
          "export",
          "extern",
          "final",
          "for",
          "friend",
          "goto",
          "if",
          "inline",
          "mutable",
          "namespace",
          "new",
          "noexcept",
          "not",
          "not_eq",
          "operator",
          "or",
          "or_eq",
          "override",
          "private",
          "protected",
          "public",
          "register",
          "reinterpret_cast",
          "requires",
          "return",
          "sizeof",
          "static",
          "static_assert",
          "static_cast",
          "struct",
          "switch",
          "template",
          "this",
          "thread_local",
          "throw",
          "try",
          "typedef",
          "typeid",
          "typename",
          "union",
          "using",
          "virtual",
          "volatile",
          "while",
          "xor",
          "xor_eq"};

      return keywords.contains(value);
    }

    bool is_cpp_type(std::string_view value)
    {
      static const std::unordered_set<std::string_view> types = {
          "bool",
          "char",
          "char8_t",
          "char16_t",
          "char32_t",
          "double",
          "float",
          "int",
          "long",
          "short",
          "signed",
          "unsigned",
          "void",
          "wchar_t",

          "size_t",
          "ptrdiff_t",
          "int8_t",
          "int16_t",
          "int32_t",
          "int64_t",
          "uint8_t",
          "uint16_t",
          "uint32_t",
          "uint64_t",

          "string",
          "string_view",
          "vector",
          "array",
          "deque",
          "list",
          "map",
          "set",
          "unordered_map",
          "unordered_set",
          "optional",
          "variant",
          "tuple",
          "pair",
          "unique_ptr",
          "shared_ptr",
          "weak_ptr",
          "filesystem",
          "path",
          "exception",
          "error_code"};

      return types.contains(value);
    }

    bool is_cpp_literal(std::string_view value)
    {
      static const std::unordered_set<std::string_view> literals = {
          "true",
          "false",
          "nullptr",
          "NULL"};

      return literals.contains(value);
    }

    bool is_cpp_namespace(std::string_view value)
    {
      static const std::unordered_set<std::string_view> namespaces = {
          "std",
          "vix",
          "filesystem",
          "chrono",
          "ranges",
          "views"};

      return namespaces.contains(value);
    }

    bool is_macro_name(std::string_view value)
    {
      if (value.size() < 2)
      {
        return false;
      }

      bool hasLetter = false;

      for (const char character : value)
      {
        if (character >= 'A' && character <= 'Z')
        {
          hasLetter = true;
          continue;
        }

        if (character >= '0' && character <= '9')
        {
          continue;
        }

        if (character == '_')
        {
          continue;
        }

        return false;
      }

      return hasLetter;
    }

    std::size_t next_non_space(
        const std::string &line,
        std::size_t position)
    {
      while (position < line.size() &&
             is_space(line[position]))
      {
        ++position;
      }

      return position;
    }

    std::size_t scan_quoted_literal(
        const std::string &line,
        std::size_t start)
    {
      const char quote = line[start];

      std::size_t position = start + 1;
      bool escaping = false;

      while (position < line.size())
      {
        const char character = line[position];

        if (escaping)
        {
          escaping = false;
          ++position;
          continue;
        }

        if (character == '\\')
        {
          escaping = true;
          ++position;
          continue;
        }

        ++position;

        if (character == quote)
        {
          break;
        }
      }

      return position;
    }

    std::size_t scan_raw_string(
        const std::string &line,
        std::size_t start)
    {
      const std::size_t delimiterStart = start + 2;
      const std::size_t contentStart =
          line.find('(', delimiterStart);

      if (contentStart == std::string::npos)
      {
        return line.size();
      }

      const std::string delimiter =
          line.substr(
              delimiterStart,
              contentStart - delimiterStart);

      const std::string closing =
          ")" + delimiter + "\"";

      const std::size_t closingPosition =
          line.find(
              closing,
              contentStart + 1);

      if (closingPosition == std::string::npos)
      {
        return line.size();
      }

      return closingPosition + closing.size();
    }

    std::size_t scan_number(
        const std::string &line,
        std::size_t start)
    {
      std::size_t position = start;

      while (position < line.size())
      {
        const char character = line[position];

        const bool normalNumberCharacter =
            std::isalnum(
                static_cast<unsigned char>(character)) != 0 ||
            character == '.' ||
            character == '\'' ||
            character == '_';

        if (normalNumberCharacter)
        {
          ++position;
          continue;
        }

        if ((character == '+' || character == '-') &&
            position > start)
        {
          const char previous = line[position - 1];

          if (previous == 'e' ||
              previous == 'E' ||
              previous == 'p' ||
              previous == 'P')
          {
            ++position;
            continue;
          }
        }

        break;
      }

      return position;
    }

    std::string colorize(
        std::string_view text,
        std::string_view color)
    {
      return terminal_style(text, color);
    }

    std::string highlight_cpp_line(
        const std::string &line)
    {
      if (!terminal_colors_enabled())
      {
        return line;
      }

      std::string output;
      output.reserve(line.size() * 2);

      std::size_t position = 0;
      bool includeDirective = false;

      while (position < line.size())
      {
        const char character = line[position];

        if (is_space(character))
        {
          output.push_back(character);
          ++position;
          continue;
        }

        /*
         * Single-line comment.
         */
        if (character == '/' &&
            position + 1 < line.size() &&
            line[position + 1] == '/')
        {
          output += colorize(
              std::string_view(
                  line.data() + position,
                  line.size() - position),
              colorComment);

          break;
        }

        /*
         * Block comment contained on the current line.
         */
        if (character == '/' &&
            position + 1 < line.size() &&
            line[position + 1] == '*')
        {
          const std::size_t closing =
              line.find("*/", position + 2);

          const std::size_t end =
              closing == std::string::npos
                  ? line.size()
                  : closing + 2;

          output += colorize(
              std::string_view(
                  line.data() + position,
                  end - position),
              colorComment);

          position = end;
          continue;
        }

        /*
         * Preprocessor directive:
         * #include, #define, #if, ...
         */
        if (character == '#')
        {
          const std::size_t start = position;

          ++position;

          while (position < line.size() &&
                 is_space(line[position]))
          {
            ++position;
          }

          const std::size_t nameStart = position;

          while (position < line.size() &&
                 is_identifier_character(line[position]))
          {
            ++position;
          }

          const std::string_view directiveName(
              line.data() + nameStart,
              position - nameStart);

          output += colorize(
              std::string_view(
                  line.data() + start,
                  position - start),
              colorPreprocessor);

          includeDirective =
              directiveName == "include" ||
              directiveName == "include_next";

          continue;
        }

        /*
         * Header path:
         * <iostream>
         */
        if (includeDirective &&
            character == '<')
        {
          const std::size_t closing =
              line.find('>', position + 1);

          const std::size_t end =
              closing == std::string::npos
                  ? line.size()
                  : closing + 1;

          output += colorize(
              std::string_view(
                  line.data() + position,
                  end - position),
              colorString);

          position = end;
          includeDirective = false;
          continue;
        }

        /*
         * Raw string:
         * R"(text)"
         */
        if (character == 'R' &&
            position + 1 < line.size() &&
            line[position + 1] == '"')
        {
          const std::size_t end =
              scan_raw_string(line, position);

          output += colorize(
              std::string_view(
                  line.data() + position,
                  end - position),
              colorString);

          position = end;
          continue;
        }

        /*
         * String and character literals.
         */
        if (character == '"' ||
            character == '\'')
        {
          const std::size_t end =
              scan_quoted_literal(
                  line,
                  position);

          output += colorize(
              std::string_view(
                  line.data() + position,
                  end - position),
              colorString);

          position = end;
          includeDirective = false;
          continue;
        }

        /*
         * Numbers:
         * 42, 3.14, 0xff, 10ULL, 1.5e10
         */
        if (is_digit(character) ||
            (character == '.' &&
             position + 1 < line.size() &&
             is_digit(line[position + 1])))
        {
          const std::size_t end =
              scan_number(line, position);

          output += colorize(
              std::string_view(
                  line.data() + position,
                  end - position),
              colorNumber);

          position = end;
          continue;
        }

        /*
         * Identifiers, keywords, types and function names.
         */
        if (is_identifier_start(character))
        {
          const std::size_t start = position;

          ++position;

          while (position < line.size() &&
                 is_identifier_character(line[position]))
          {
            ++position;
          }

          const std::string_view token(
              line.data() + start,
              position - start);

          if (is_cpp_keyword(token))
          {
            output += colorize(
                token,
                colorKeyword);

            continue;
          }

          if (is_cpp_type(token))
          {
            output += colorize(
                token,
                colorType);

            continue;
          }

          if (is_cpp_literal(token))
          {
            output += colorize(
                token,
                colorNumber);

            continue;
          }

          if (is_cpp_namespace(token))
          {
            output += colorize(
                token,
                colorNamespace);

            continue;
          }

          if (is_macro_name(token))
          {
            output += colorize(
                token,
                colorPreprocessor);

            continue;
          }

          const std::size_t next =
              next_non_space(
                  line,
                  position);

          if (next < line.size() &&
              line[next] == '(')
          {
            output += colorize(
                token,
                colorFunction);

            continue;
          }

          const bool namespaceMember =
              start >= 2 &&
              line[start - 1] == ':' &&
              line[start - 2] == ':';

          if (namespaceMember)
          {
            output += colorize(
                token,
                colorNamespace);

            continue;
          }

          output.append(token);
          continue;
        }

        output.push_back(character);
        ++position;
      }

      return output;
    }

    std::string render_line(
        const std::string &line,
        const LineEditorOptions &options)
    {
      if (!options.codeMode)
      {
        return line;
      }

      return highlight_cpp_line(line);
    }

    static bool prefix_contains_only_indentation(
        const std::string &line,
        std::size_t cursor)
    {
      if (cursor > line.size())
      {
        return false;
      }

      return std::all_of(
          line.begin(),
          line.begin() +
              static_cast<std::string::difference_type>(cursor),
          [](char value)
          {
            return value == ' ' ||
                   value == '\t';
          });
    }

    static void remove_indentation_before_cursor(
        std::string &line,
        std::size_t &cursor,
        std::size_t indentSize)
    {
      if (cursor == 0 ||
          indentSize == 0)
      {
        return;
      }

      if (line[cursor - 1] == '\t')
      {
        line.erase(cursor - 1, 1);
        --cursor;
        return;
      }

      std::size_t count = 0;

      while (cursor > 0 &&
             count < indentSize &&
             line[cursor - 1] == ' ')
      {
        line.erase(cursor - 1, 1);
        --cursor;
        ++count;
      }
    }

    static std::size_t first_non_space(
        const std::string &line)
    {
      const auto position =
          line.find_first_not_of(" \t");

      return position == std::string::npos
                 ? line.size()
                 : position;
    }

    static std::size_t next_word_position(
        const std::string &line,
        std::size_t cursor)
    {
      while (cursor < line.size() &&
             !std::isspace(
                 static_cast<unsigned char>(line[cursor])))
      {
        ++cursor;
      }

      while (cursor < line.size() &&
             std::isspace(
                 static_cast<unsigned char>(line[cursor])))
      {
        ++cursor;
      }

      return cursor;
    }

    static std::size_t previous_word_position(
        const std::string &line,
        std::size_t cursor)
    {
      while (cursor > 0 &&
             std::isspace(
                 static_cast<unsigned char>(line[cursor - 1])))
      {
        --cursor;
      }

      while (cursor > 0 &&
             !std::isspace(
                 static_cast<unsigned char>(line[cursor - 1])))
      {
        --cursor;
      }

      return cursor;
    }

  }

#ifndef _WIN32
  constexpr unsigned char KEY_CTRL_A = 0x01;
  constexpr unsigned char KEY_CTRL_C = 0x03;
  constexpr unsigned char KEY_CTRL_D = 0x04;
  constexpr unsigned char KEY_CTRL_E = 0x05;
  constexpr unsigned char KEY_CTRL_K = 0x0B;
  constexpr unsigned char KEY_CTRL_L = 0x0C;
  constexpr unsigned char KEY_CTRL_U = 0x15;

  constexpr unsigned char KEY_BACKSPACE_1 = 0x08;
  constexpr unsigned char KEY_BACKSPACE_2 = 0x7F;
  constexpr unsigned char KEY_ENTER_1 = '\n';
  constexpr unsigned char KEY_ENTER_2 = '\r';
  constexpr unsigned char KEY_TAB = '\t';
  constexpr unsigned char KEY_ESC = 0x1B;

  static bool read_byte(unsigned char &out)
  {
    const ssize_t count =
        ::read(
            STDIN_FILENO,
            &out,
            1);

    return count == 1;
  }

  static bool read_byte_timeout(
      unsigned char &out,
      int timeoutMilliseconds)
  {
    fd_set descriptors;
    FD_ZERO(&descriptors);
    FD_SET(STDIN_FILENO, &descriptors);

    timeval timeout{};
    timeout.tv_sec =
        timeoutMilliseconds / 1000;

    timeout.tv_usec =
        (timeoutMilliseconds % 1000) * 1000;

    const int result =
        ::select(
            STDIN_FILENO + 1,
            &descriptors,
            nullptr,
            nullptr,
            &timeout);

    if (result <= 0)
    {
      return false;
    }

    return read_byte(out);
  }

  static void redraw(
      const std::string &prompt,
      const std::string &line,
      std::size_t cursor,
      const LineEditorOptions &options)
  {
    std::cout
        << "\r\033[2K"
        << prompt
        << render_line(line, options);

    const std::size_t charactersAfterCursor =
        line.size() - cursor;

    if (charactersAfterCursor > 0)
    {
      std::cout
          << "\033["
          << charactersAfterCursor
          << "D";
    }

    std::cout << std::flush;
  }

  struct RedrawCtx
  {
    const std::string *prompt = nullptr;
    const std::string *line = nullptr;
    const std::size_t *cursor = nullptr;
    const LineEditorOptions *options = nullptr;
  };

  static void redraw_trampoline(void *ptr)
  {
    auto *ctx =
        static_cast<RedrawCtx *>(ptr);

    if (ctx == nullptr ||
        ctx->prompt == nullptr ||
        ctx->line == nullptr ||
        ctx->cursor == nullptr ||
        ctx->options == nullptr)
    {
      return;
    }

    redraw(
        *ctx->prompt,
        *ctx->line,
        *ctx->cursor,
        *ctx->options);
  }

  static void end_editing()
  {
    Console::set_redraw_callback(
        nullptr,
        nullptr);

    Console::set_editing(false);
  }

  static void print_suggestions_and_redraw(
      const std::vector<std::string> &suggestions,
      const std::string &prompt,
      const std::string &line,
      std::size_t cursor,
      const LineEditorOptions &options)
  {
    std::cout << '\n';

    for (const auto &suggestion : suggestions)
    {
      std::cout
          << suggestion
          << "  ";
    }

    std::cout << '\n';

    redraw(
        prompt,
        line,
        cursor,
        options);
  }

  ReadStatus read_line_edit(
      const std::string &prompt,
      std::string &outLine,
      const CompletionFn &completer,
      const HistoryNavFn &onHistoryUp,
      const HistoryNavFn &onHistoryDown,
      const LineEditorOptions &options)
  {
    outLine = options.initialLine;

    std::size_t cursor =
        outLine.size();

    enum class ViMode
    {
      Insert,
      Normal
    };

    ViMode viMode =
        ViMode::Insert;

    std::cout
        << prompt
        << render_line(outLine, options)
        << std::flush;

    Console::set_editing(true);

    RedrawCtx ctx{
        &prompt,
        &outLine,
        &cursor,
        &options};

    Console::set_redraw_callback(
        &redraw_trampoline,
        &ctx);

    while (true)
    {
      unsigned char character = 0;

      if (!read_byte(character))
      {
        end_editing();
        std::cout << '\n';

        return ReadStatus::Eof;
      }

      if (character == KEY_CTRL_C)
      {
        end_editing();
        std::cout << "^C\n";

        outLine.clear();
        return ReadStatus::Interrupted;
      }

      if (character == KEY_CTRL_D)
      {
        if (outLine.empty())
        {
          end_editing();
          std::cout << '\n';

          return ReadStatus::Eof;
        }

        continue;
      }

      if (character == KEY_CTRL_L)
      {
        end_editing();

        outLine.clear();
        std::cout << '\n';

        return ReadStatus::Clear;
      }

      if (character == KEY_CTRL_A)
      {
        cursor = 0;

        redraw(
            prompt,
            outLine,
            cursor,
            options);

        continue;
      }

      if (character == KEY_CTRL_E)
      {
        cursor = outLine.size();

        redraw(
            prompt,
            outLine,
            cursor,
            options);

        continue;
      }

      if (character == KEY_CTRL_U)
      {
        outLine.erase(0, cursor);
        cursor = 0;

        redraw(
            prompt,
            outLine,
            cursor,
            options);

        continue;
      }

      if (character == KEY_CTRL_K)
      {
        outLine.erase(cursor);

        redraw(
            prompt,
            outLine,
            cursor,
            options);

        continue;
      }

      if (character == KEY_ENTER_1 ||
          character == KEY_ENTER_2)
      {
        end_editing();
        std::cout << '\n';

        return ReadStatus::Ok;
      }

      if (character == KEY_BACKSPACE_1 ||
          character == KEY_BACKSPACE_2)
      {
        if (cursor == 0)
        {
          continue;
        }

        if (options.codeMode &&
            prefix_contains_only_indentation(
                outLine,
                cursor))
        {
          remove_indentation_before_cursor(
              outLine,
              cursor,
              options.indentSize);
        }
        else
        {
          outLine.erase(cursor - 1, 1);
          --cursor;
        }

        redraw(
            prompt,
            outLine,
            cursor,
            options);

        continue;
      }

      if (character == KEY_TAB)
      {
        if (options.codeMode)
        {
          outLine.insert(
              cursor,
              options.indentSize,
              ' ');

          cursor += options.indentSize;

          redraw(
              prompt,
              outLine,
              cursor,
              options);

          continue;
        }

        if (completer)
        {
          auto result =
              completer(outLine);

          if (result.changed)
          {
            outLine = result.newLine;
            cursor = outLine.size();

            redraw(
                prompt,
                outLine,
                cursor,
                options);

            continue;
          }

          if (!result.suggestions.empty())
          {
            print_suggestions_and_redraw(
                result.suggestions,
                prompt,
                outLine,
                cursor,
                options);

            continue;
          }
        }

        continue;
      }

      if (character == KEY_ESC)
      {
        unsigned char sequence2 = 0;

        /*
         * Esc seul : passe en mode normal Vim.
         */
        if (!read_byte_timeout(sequence2, 40))
        {
          if (options.keymap == LineEditorKeymap::Vi)
          {
            viMode = ViMode::Normal;
          }

          continue;
        }

        unsigned char sequence3 = 0;

        if (!read_byte_timeout(sequence3, 40))
        {
          continue;
        }

        if (sequence2 != '[')
        {
          continue;
        }

        // Arrow Up
        if (sequence3 == 'A')
        {
          if (onHistoryUp &&
              onHistoryUp(outLine))
          {
            cursor = outLine.size();

            redraw(
                prompt,
                outLine,
                cursor,
                options);
          }

          continue;
        }

        // Arrow Down
        if (sequence3 == 'B')
        {
          if (onHistoryDown &&
              onHistoryDown(outLine))
          {
            cursor = outLine.size();

            redraw(
                prompt,
                outLine,
                cursor,
                options);
          }

          continue;
        }

        // Arrow Right
        if (sequence3 == 'C')
        {
          if (cursor < outLine.size())
          {
            ++cursor;

            redraw(
                prompt,
                outLine,
                cursor,
                options);
          }

          continue;
        }

        // Arrow Left
        if (sequence3 == 'D')
        {
          if (cursor > 0)
          {
            --cursor;

            redraw(
                prompt,
                outLine,
                cursor,
                options);
          }

          continue;
        }

        // Home
        if (sequence3 == 'H')
        {
          cursor = 0;

          redraw(
              prompt,
              outLine,
              cursor,
              options);

          continue;
        }

        // End
        if (sequence3 == 'F')
        {
          cursor = outLine.size();

          redraw(
              prompt,
              outLine,
              cursor,
              options);

          continue;
        }

        // Shift+Tab
        if (sequence3 == 'Z' &&
            options.codeMode &&
            prefix_contains_only_indentation(
                outLine,
                cursor))
        {
          remove_indentation_before_cursor(
              outLine,
              cursor,
              options.indentSize);

          redraw(
              prompt,
              outLine,
              cursor,
              options);

          continue;
        }

        // Delete: ESC [ 3 ~
        if (sequence3 == '3')
        {
          unsigned char sequence4 = 0;

          if (read_byte_timeout(sequence4, 40) &&
              sequence4 == '~' &&
              cursor < outLine.size())
          {
            outLine.erase(cursor, 1);

            redraw(
                prompt,
                outLine,
                cursor,
                options);
          }

          continue;
        }

        continue;
      }

      if (options.keymap == LineEditorKeymap::Vi &&
          viMode == ViMode::Normal)
      {
        if (character == 'h')
        {
          if (cursor > 0)
          {
            --cursor;
          }
        }
        else if (character == 'l')
        {
          if (cursor < outLine.size())
          {
            ++cursor;
          }
        }
        else if (character == '0')
        {
          cursor = 0;
        }
        else if (character == '$')
        {
          cursor = outLine.size();
        }
        else if (character == 'w')
        {
          cursor =
              next_word_position(
                  outLine,
                  cursor);
        }
        else if (character == 'b')
        {
          cursor =
              previous_word_position(
                  outLine,
                  cursor);
        }
        else if (character == 'i')
        {
          viMode = ViMode::Insert;
          continue;
        }
        else if (character == 'a')
        {
          if (cursor < outLine.size())
          {
            ++cursor;
          }

          viMode = ViMode::Insert;
          continue;
        }
        else if (character == 'I')
        {
          cursor =
              first_non_space(outLine);

          viMode = ViMode::Insert;
          continue;
        }
        else if (character == 'A')
        {
          cursor = outLine.size();
          viMode = ViMode::Insert;
          continue;
        }
        else if (character == 'x')
        {
          if (cursor < outLine.size())
          {
            outLine.erase(cursor, 1);
          }
        }

        redraw(
            prompt,
            outLine,
            cursor,
            options);

        continue;
      }

      /*
       * Automatically dedent closing braces.
       */
      if (options.codeMode &&
          character == '}' &&
          prefix_contains_only_indentation(
              outLine,
              cursor))
      {
        remove_indentation_before_cursor(
            outLine,
            cursor,
            options.indentSize);

        outLine.insert(cursor, 1, '}');
        ++cursor;

        redraw(
            prompt,
            outLine,
            cursor,
            options);

        continue;
      }

      if (character < 0x20)
      {
        continue;
      }

      outLine.insert(
          cursor,
          1,
          static_cast<char>(character));

      ++cursor;
      if (options.codeMode)
      {
        /*
         * Redraw the complete line so syntax colors are
         * recalculated after every typed character.
         */
        redraw(
            prompt,
            outLine,
            cursor,
            options);
      }
      else
      {
        std::cout
            << static_cast<char>(character)
            << std::flush;
      }
    }
  }
#else
  ReadStatus read_line_edit(
      const std::string &prompt,
      std::string &outLine,
      const CompletionFn &,
      const HistoryNavFn &,
      const HistoryNavFn &,
      const LineEditorOptions &options)
  {
    std::cout
        << prompt
        << options.initialLine
        << std::flush;

    std::string input;

    if (!std::getline(
            std::cin,
            input))
    {
      std::cout << '\n';
      return ReadStatus::Eof;
    }

    outLine =
        options.initialLine +
        input;

    return ReadStatus::Ok;
  }
#endif
}
