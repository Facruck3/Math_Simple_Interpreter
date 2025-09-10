# 🧮 Math Interpreter

A simple math expression interpreter with variable support, written in C.

## Features

- **Basic Math**: `+`, `-`, `*`, `/`, `%`, `^` (power), `sqrt()`
- **Variables**: Create and use variables (`x = 5`)
- **High Precision**: Uses MPFR library for accurate calculations
- **Commands**: Built-in commands for control

## Quick Start

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install libmpfr-dev

# Compile
gcc -o math_interpreter *.c -lmpfr -lgmp

# Run
./math_interpreter
```

## Examples

```bash
>> 2 + 3 * 4
Result: 14

>> x = 5
Result: 5

>> x + 3
Result: 8

>> sqrt(16)
Result: 4
```

## Available Commands

- `-exit` - Quit the program
- `-clear` - Clear screen
- `-help` - Show help message
- `-show` - Display all variables
- `-clear-vars` - Delete all variables

## Project Structure

- `parser.[ch]` - Expression parsing and evaluation
- `symbolTable.[ch]` - Variable storage system
- `instructions.[ch]` - Command handling
- `debug.h` - Debugging system

## Requirements

- C compiler (GCC/Clang)
- MPFR library
- GMP library

## License

MIT License