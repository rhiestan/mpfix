# Decoding and formatting of mpfix values, shared by the gdb and lldb scripts.
# Pure Python, no debugger module, so that it can be tested on its own.
#
# A mpfix::Float<P, E> is stored as
#     std::uint64_t limbs_[kLimbs];     kLimbs = ceil(P / 64), little endian
#     std::uint32_t biased_exponent_;   the exponent XOR 0x80000000
#     std::uint32_t negative_;
# The significand is normalized: its top bit is the highest bit of the last
# limb, and the value is 1.f * 2^exponent, also for subnormals of the IEEE
# format, which have an exponent below Emin here. Zero, infinity and NaN are
# marked by the exponent: INT32_MIN, INT32_MAX - 1 and INT32_MAX.

import decimal
import math
import re
import struct

INT32_MIN = -(1 << 31)
EXP_ZERO = INT32_MIN
EXP_INFINITY = (1 << 31) - 2
EXP_NAN = (1 << 31) - 1

_FLOAT_RE = re.compile(r"mpfix::Float<\s*(\d+)\s*,\s*(\d+)\s*>")


def parse_float_type(type_name):
    """(P, E) from a type name such as 'mpfix::Float<113, 15>', or None."""
    match = _FLOAT_RE.search(type_name)
    if match is None:
        return None
    return int(match.group(1)), int(match.group(2))


def limb_count(precision):
    return (precision + 63) // 64


def unpack(raw, precision):
    """(limbs, biased_exponent, negative) from the bytes of a Float."""
    count = limb_count(precision)
    fields = struct.unpack_from("<%dQII" % count, raw)
    return list(fields[:count]), fields[count], fields[count + 1]


class Decoded:
    """What a Float holds. `kind` is 'zero', 'inf', 'nan' or 'finite'."""

    def __init__(self, precision, exponent_bits, limbs, biased_exponent,
                 negative):
        self.precision = precision
        self.exponent_bits = exponent_bits
        self.limbs = limbs
        self.negative = negative != 0
        exponent = biased_exponent ^ 0x80000000
        self.exponent = exponent - (1 << 32) if exponent >= (1 << 31) \
            else exponent
        self.significand = 0
        for i, limb in enumerate(limbs):
            self.significand |= limb << (64 * i)
        top_bit = 64 * len(limbs) - 1
        if self.exponent == EXP_ZERO:
            self.kind = "zero"
        elif self.exponent == EXP_INFINITY:
            self.kind = "inf"
        elif self.exponent == EXP_NAN:
            self.kind = "nan"
        else:
            self.kind = "finite"
        # The quiet bit is the one below the top bit, as in the IEEE format.
        self.quiet = (self.significand >> (top_bit - 1)) & 1 != 0
        emax = (1 << (exponent_bits - 1)) - 1
        self.subnormal = self.kind == "finite" and self.exponent < 1 - emax


def decode(precision, exponent_bits, limbs, biased_exponent, negative):
    return Decoded(precision, exponent_bits, limbs, biased_exponent, negative)


def decode_bytes(raw, precision, exponent_bits):
    return decode(precision, exponent_bits, *unpack(raw, precision))


def _context(digits):
    context = decimal.Context(prec=digits, rounding=decimal.ROUND_HALF_EVEN,
                              Emax=decimal.MAX_EMAX, Emin=decimal.MIN_EMIN)
    context.traps[decimal.Overflow] = False
    context.traps[decimal.Underflow] = False
    return context


def max_digits10(precision):
    return 2 + int(precision * math.log10(2.0))


def _shortest(value, decoded, context_digits):
    """The digits and exponent of the shortest decimal that reads back."""
    precision = decoded.precision
    wide = _context(context_digits)
    # A Float of precision P has P bits: its unit in the last place is
    # 2^(exponent - P + 1), and for a subnormal that of the smallest normal.
    emax = (1 << (decoded.exponent_bits - 1)) - 1
    ulp = wide.power(decimal.Decimal(2),
                     max(decoded.exponent, 1 - emax) - precision + 1)
    only_top = decoded.significand == 1 << (64 * len(decoded.limbs) - 1)
    # Below a power of two the spacing halves, except at the smallest normal.
    below = ulp
    if only_top and decoded.exponent > 1 - emax:
        below = wide.divide(ulp, 2)
    low = wide.subtract(value, wide.divide(below, 2))
    high = wide.add(value, wide.divide(ulp, 2))
    digits = max_digits10(precision)
    for count in range(1, digits + 1):
        round_context = _context(count)
        best = None
        for mode in (decimal.ROUND_FLOOR, decimal.ROUND_CEILING):
            round_context.rounding = mode
            candidate = round_context.plus(value)
            if low <= candidate <= high:
                distance = wide.subtract(candidate, value).copy_abs()
                # The closest; of two as close, the one with the even digit.
                key = (distance, candidate.as_tuple().digits[-1] % 2)
                if best is None or key < best[0]:
                    best = (key, candidate)
        if best is not None:
            return best[1]
    return _context(digits).plus(value)


def _notation(sign, digits, exponent, exact_integer):
    """The text std::to_chars and mpfix's to_string give: fixed or scientific
    notation, whichever is shorter. `digits` are the shortest digits, with
    the decimal point after the first, times 10^exponent. Where the fixed text
    has no fraction, the integer is printed exactly rather than padded with
    zeros, which `exact_integer` supplies."""
    length = len(digits)
    scientific = digits[0]
    if length > 1:
        scientific += "." + digits[1:]
    scientific += "e%s%02d" % ("-" if exponent < 0 else "+", abs(exponent))
    fixed_length = exponent + 1
    if exponent < 0:
        fixed_length = 1 - exponent + length
    elif length > exponent + 1:
        fixed_length = length + 1
    if len(scientific) < fixed_length:
        return sign + scientific
    if exponent < 0:
        return sign + "0." + "0" * (-exponent - 1) + digits
    if length > exponent + 1:
        return sign + digits[:exponent + 1] + "." + digits[exponent + 1:]
    return sign + exact_integer()


def decimal_text(decoded):
    """The shortest decimal text that reads back as the same Float."""
    sign = "-" if decoded.negative else ""
    if decoded.kind == "zero":
        return sign + "0"
    if decoded.kind == "inf":
        return sign + "inf"
    if decoded.kind == "nan":
        return sign + ("nan" if decoded.quiet else "snan")
    # Without its trailing zero bits the significand is exact in a Decimal of
    # a few more digits than it has, so that values of small formats, and ties
    # between two decimals, are exact.
    significand = decoded.significand
    shift = decoded.exponent - (64 * len(decoded.limbs) - 1)
    trailing = (significand & -significand).bit_length() - 1
    significand >>= trailing
    shift += trailing
    context_digits = max_digits10(decoded.precision) + 12 +         len(str(significand))
    wide = _context(context_digits)
    value = wide.multiply(decimal.Decimal(significand),
                          wide.power(decimal.Decimal(2), shift))
    candidate = _shortest(value, decoded, context_digits)
    _, digit_tuple, exponent = candidate.as_tuple()
    digit_string = "".join(map(str, digit_tuple)).rstrip("0") or "0"
    # d.ddd * 10^scientific
    scientific = len(digit_tuple) + exponent - 1

    def exact_integer():
        return str(wide.quantize(value, decimal.Decimal(1)))

    return _notation(sign, digit_string, scientific, exact_integer)


def significand_text(decoded):
    """The significand in hexadecimal, 1.xxxx with the hidden bit shown."""
    return "0x%0*x" % (16 * len(decoded.limbs), decoded.significand)


def describe(decoded):
    """Fields shown as children in the debuggers: (name, text) pairs."""
    fields = [("sign", "-" if decoded.negative else "+"),
              ("class", decoded.kind +
               (" (subnormal)" if decoded.subnormal else ""))]
    if decoded.kind == "finite":
        fields.append(("exponent", str(decoded.exponent)))
    if decoded.kind in ("finite", "nan"):
        fields.append(("significand", significand_text(decoded)))
    return fields
