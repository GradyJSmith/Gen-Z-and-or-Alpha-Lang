# BrainRot Interpreter v3.1

A dynamically-typed scripting language with Python-like semantics, written in C++17. Saves/loads `.aura` files. Includes a built-in terminal editor — no ncurses, raw ANSI/termios, works on Windows too.

---

## Installation

```bash
g++ -std=c++17 -O2 -o brainrot brainrot.cpp
```

---

## Usage

**This program runs best in powershell or bash**

```bash
brainrot                        # open editor (empty buffer)
brainrot edit <file.aura>       # open editor with existing file
brainrot run  <file.aura>       # run a source file directly
```

Source files use the `.aura` extension (plain UTF-8 text).

---

## Editor Keybindings

| Key | Action |
|-----|--------|
| Arrow keys / Home / End | Navigate |
| Backspace / Delete | Delete characters |
| Enter | New line |
| Ctrl-S | Save to `.aura` |
| Ctrl-R | Run current buffer |
| Ctrl-N | New file (prompts for name) |
| Ctrl-Q | Quit (prompts if unsaved) |

---

## Language Reference

| BrainRot | Python | Notes |
|----------|--------|-------|
| `rizz x = ...` | `x = ...` | Declare / assign variable |
| `yap(...)` | `print(...)` | Print to stdout |
| `slurp(...)` | `input(...)` | Read line from stdin |
| `rizzler(...)` | `int(...)` | Cast to integer |
| `unc` | — | Chaos: random type + value each time |
| `twin f(x) { }` | `def f(x):` | Define a function |
| `ghost <expr>` | `return` | Return from a function |
| `slay` / `nah` | `if` / `else` | Conditional |
| `skibidi` | `while` | While loop |
| `sigma x in range(0, 10) { }` | `for x in range(...)` | For loop |
| `bail` | `break` | Break out of loop |
| `ohio { } nah { }` | `try` / `except` | Error handling |
| `vibe check` | `assert` | Crash with message if condition is falsy |
| `low-key` | `exit()` | Terminate process immediately |

### Literals & Operators

| BrainRot | Meaning |
|----------|---------|
| `bussin` / `cap` | `True` / `False` |
| `npc` | `None` |
| `fr` / `no cap` | `==` / `!=` |
| `based` / `ratio` | `>=` / `<=` |
| `cooked` / `L` | `>` / `<` |
| `and` / `or` / `not` | Logical operators |
| `+ - * / %` | Arithmetic |

---

## Examples

### FizzBuzz

```
rizz i = 1
skibidi i ratio 100 {
    slay i % 15 fr 0 { yap("FizzBuzz") }
    nah slay i % 3 fr 0 { yap("Fizz") }
    nah slay i % 5 fr 0 { yap("Buzz") }
    nah { yap(i) }
    rizz i = i + 1
}
```

### Functions

```
twin greet(name) {
    ghost "yo " + name + " what is good"
}
yap(greet("world"))
```

### Error Handling

```
ohio {
    rizz x = 1 / 0
} nah {
    yap("that was ohio fr")
}
```

### The `unc` Keyword

`unc` evaluates to a completely random type and value every time it is read — integer, float, bool, or a string from a pool of certified brainrot phrases. Useful for chaos testing or just vibing.

```
rizz x = unc
yap(x)   # could be anything. literally.
```

---
## License

Do whatever. No cap.
