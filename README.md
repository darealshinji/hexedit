Small utility that helps to view and edit bytes in binary files from command line.

```
usage:
    hexedit --help
    hexedit read <offset> <length> <file>
    hexedit write <offset> <data> <file>
    hexedit memset <offset> <length> <char> <file>

    Offset and length may be hexadecimal prefixed with `0x'/`\x' or decimal.
    Data must be hexadecimal without prefixes.
    Char can be a literal character, escaped control character, hexadecimal value
    prefixed with `0x'/`\x' or a decimal number prefixed with `\'.
```
