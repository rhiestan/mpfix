#!/usr/bin/env python3
# Checks the decoding of the debugger scripts against the library: runs
# debug_dump, which prints the bytes of many Floats with the library's own
# shortest text, and decodes the bytes with mpfix_format.
#
#   test_mpfix_format.py path/to/debug_dump

import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mpfix_format  # noqa: E402


def main():
    output = subprocess.run([sys.argv[1]], check=True, capture_output=True,
                            text=True).stdout
    checked = 0
    failures = []
    for line in output.splitlines():
        precision, exponent_bits, hex_bytes, expected = line.split(" ")
        raw = bytes.fromhex(hex_bytes)
        decoded = mpfix_format.decode_bytes(raw, int(precision),
                                            int(exponent_bits))
        actual = mpfix_format.decimal_text(decoded)
        if decoded.kind == "finite":
            same = actual == expected
        else:
            # The library does not tell a signaling NaN from a quiet one.
            same = actual.replace("snan", "nan") == expected
        checked += 1
        if not same:
            failures.append("Float<%s,%s> %s: decoded %s, library %s" %
                            (precision, exponent_bits, hex_bytes, actual,
                             expected))
    for failure in failures[:20]:
        print(failure)
    print("%d values checked, %d differ" % (checked, len(failures)))
    return 1 if failures or checked == 0 else 0


if __name__ == "__main__":
    sys.exit(main())
