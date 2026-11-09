# Debugger visualizers

A `Float` is a few integers in memory, and a debugger shows them as such. These
files show the value instead.

| Debugger | File | Shows |
| --- | --- | --- |
| Visual Studio, VS Code (cppvsdbg), CLion, WinDbg | `mpfix.natvis` | the value as a double (about 17 digits), or `mantissa * 2^exponent` outside the double range |
| gdb | `mpfix_gdb.py` | the exact value, as the text `to_string` gives |
| lldb | `mpfix_lldb.py` | the exact value, as the text `to_string` gives |

All three cover `Float<P, E>` (so `binary128`, `binary256`, `binary512`),
`Complex<F>`, and Boost's `number<boost_backend<P, E>>`. Zero, infinity and NaN
(quiet or signaling) are shown by name. Each value also expands to its sign,
exponent and significand.

## Visual Studio

With MSVC and CMake, nothing to do: linking `mpfix::mpfix` puts
`mpfix.natvis` in the PDB of your program. Otherwise, either

- link with `/NATVIS:path\to\mpfix.natvis`,
- copy `mpfix.natvis` to the `Visualizers` folder under your Visual Studio
  documents folder, such as `%USERPROFILE%\Documents\Visual Studio 2022\Visualizers`,
  or
- in a project, add it with *Add > Existing Item*.

A natvis file cannot compute with 113 or more bits, so the summary is the value
as a `double`, prefixed with `~`. For the digits, call `to_string(x)` in the
program, or use gdb or lldb.

## gdb

In `~/.gdbinit`, or in a session:

    source /path/to/mpfix/debug/mpfix_gdb.py

then `print x` shows the value, and `print/r x` the raw members. The script
reads the bytes of the value, so it does not depend on libstdc++'s or libc++'s
`std::array`. An installed mpfix has the files in `share/mpfix/debug`.

## lldb

In `~/.lldbinit`, or in a session:

    command script import /path/to/mpfix/debug/mpfix_lldb.py

then `frame variable x` or `p x` shows the value; `frame variable -R x` the raw
members.

## Tests

The decoding of both scripts is in `mpfix_format.py`, which does not need a
debugger. The ctest `debug_visualizer_format` decodes the bytes of about 2900
values, in six formats, and compares the text with the library's `to_string`.
The natvis file is checked against Visual Studio's schema, and its output with
WinDbg's `dx`. The debugger-facing parts of the gdb and lldb scripts, the
registration and reading the memory, are not yet tested against the debuggers.
