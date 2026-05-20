# Vix Reply

Interactive REPL engine for Vix.

Vix Reply powers the interactive `vix` and `vix repl` experience. It provides a fast shell for expressions, variables, JSON values, runtime helpers, and real C++ snippets powered by the Vix run pipeline.

## Usage

Start the REPL:

```bash
vix
```

Or start it explicitly:

```bash
vix repl
```

Pass arguments to the REPL:

```bash
vix repl -- --port 8080 --mode dev
```

Inside the REPL:

```text
Vix.args()
```

## Example

```text
>>> println("Hello from Vix Reply")
Hello from Vix Reply

>>> 1 + 2 * 3
7

>>> name = "Vix"
name = "Vix"

>>> type(name)
string
```

## C++ snippets

Vix Reply can run real C++ snippets through `vix run`.

```text
>>> :cpp
C++ mode. Type :run to execute or :cancel to exit.
cpp> #include <vix/print.hpp>
...   int main() {
...     vix::print("Hello from C++");
...   }
Hello from C++
```

You can also start a small Vix HTTP server from the REPL:

```text
>>> :cpp
cpp> #include <vix.hpp>
...   using namespace vix;
...
...   int main() {
...     App app;
...
...     app.get("/", [](Request&, Response& res) {
...       res.send("Hello, world");
...     });
...
...     app.run(8080);
...   }
```

Then test it:

```bash
curl http://localhost:8080
```

## Features

- Interactive prompt
- Persistent history
- Line editing
- History navigation
- Basic math evaluation
- Variables and JSON values
- Function-style calls
- Runtime helpers through `Vix`
- Real C++ snippets powered by `vix run`

## Built-in commands

```text
help
version
pwd
cd <dir>
clear
history
history clear
exit
```

## C++ mode commands

```text
:cpp       Enter C++ snippet mode
:run       Run the current C++ snippet
:cancel    Cancel C++ snippet mode
```

## Important note

Vix Reply is not a full C++ interpreter.

C++ snippet mode writes the snippet to a temporary `.cpp` file and runs it through `vix run`. The code is validated by the real C++ compiler and uses the normal Vix build, diagnostics, and runtime behavior.

## Documentation

Full documentation is available here:

```text
https://docs.vixcpp.com/cli/repl
```

## Module layout

```text
include/vix/reply/
  api/
  console/
  core/

src/
  api/
  console/
  core/
```

## Public entry point

```cpp
#include <vix/reply/core/ReplFlow.hpp>

int repl_flow_run(const std::vector<std::string>& replArgs);
```

The Vix CLI uses this entry point to implement:

```bash
vix repl
```

## Roadmap

- [x] Interactive REPL prompt
- [x] Persistent history
- [x] Line editing
- [x] History navigation
- [x] Math evaluation
- [x] Variables and JSON values
- [x] Runtime helpers through `Vix`
- [x] C++ snippet mode powered by `vix run`
- [x] C++ snippets with normal Vix diagnostics
- [x] Clean Ctrl+C behavior for long-running snippets
- [ ] Better autocomplete for variables and helpers
- [ ] Better multiline editing
- [ ] Snippet cache reuse
- [ ] Better C++ snippet session management
- [ ] More structured REPL diagnostics
- [ ] More runtime helpers

## License

MIT
