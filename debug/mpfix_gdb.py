# GDB pretty printers for mpfix::Float, mpfix::Complex and the Boost backend.
#
# Load them with
#     source /path/to/mpfix/debug/mpfix_gdb.py
# in ~/.gdbinit or the session. A Float prints as the text to_string gives; the
# sign, exponent and significand are its children, which `print/r x` and the
# MI front ends show.

import os
import struct
import sys

import gdb
import gdb.printing

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mpfix_format  # noqa: E402


def _float_parameters(value):
    """(P, E) of a Float value or type."""
    type_ = value.type if hasattr(value, "type") else value
    type_ = type_.strip_typedefs()
    try:
        return int(type_.template_argument(0)), int(type_.template_argument(1))
    except (RuntimeError, gdb.error):
        parsed = mpfix_format.parse_float_type(type_.name or str(type_))
        if parsed is None:
            raise
        return parsed


def _read_float(value):
    """The Decoded of a Float, read as bytes, which are the same whatever the
    standard library's std::array is made of."""
    precision, exponent_bits = _float_parameters(value)
    size = value.type.strip_typedefs().sizeof
    if value.address is not None:
        raw = bytes(gdb.selected_inferior().read_memory(value.address, size))
    else:
        # A value in a register or built by the debugger.
        limbs = value["limbs_"]
        field = limbs.type.fields()[0]
        count = mpfix_format.limb_count(precision)
        elements = limbs[field.name] if field.name else limbs
        raw = struct.pack(
            "<%dQII" % count,
            *[int(elements[i]) for i in range(count)],
            int(value["biased_exponent_"]), int(value["negative_"]))
    return mpfix_format.decode_bytes(raw, precision, exponent_bits)


class FloatPrinter:
    def __init__(self, value):
        self.value = value

    def to_string(self):
        return mpfix_format.decimal_text(_read_float(self.value))

    def children(self):
        for name, text in mpfix_format.describe(_read_float(self.value)):
            yield name, text


class ComplexPrinter:
    def __init__(self, value):
        self.value = value

    def to_string(self):
        return "(%s, %s)" % (self.value["re_"], self.value["im_"])


class BoostNumberPrinter:
    """boost::multiprecision::number<mpfix::boost_backend<P, E>>."""

    def __init__(self, value):
        self.value = value

    def to_string(self):
        return str(self.value["m_backend"]["value_"])


def build_printer():
    printer = gdb.printing.RegexpCollectionPrettyPrinter("mpfix")
    printer.add_printer("Float", r"^mpfix::Float<\s*\d+\s*,\s*\d+\s*>$",
                        FloatPrinter)
    printer.add_printer("Complex", r"^mpfix::Complex<.*>$", ComplexPrinter)
    printer.add_printer(
        "boost_number",
        r"^boost::multiprecision::number<mpfix::boost_backend<.*>.*>$",
        BoostNumberPrinter)
    return printer


def register(objfile=None):
    gdb.printing.register_pretty_printer(objfile, build_printer(),
                                         replace=True)


register(gdb.current_objfile())
