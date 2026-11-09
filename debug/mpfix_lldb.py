# LLDB summaries for mpfix::Float, mpfix::Complex and the Boost backend.
#
# Load them with
#     command script import /path/to/mpfix/debug/mpfix_lldb.py
# in ~/.lldbinit or the session. A Float's summary is the text to_string gives;
# its sign, class, exponent and significand are shown as children.

import os
import sys

import lldb

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mpfix_format  # noqa: E402

_FLOAT_REGEX = r"^mpfix::Float<[0-9]+, ?[0-9]+>$"
_COMPLEX_REGEX = r"^mpfix::Complex<.*>$"
_BOOST_REGEX = (r"^boost::multiprecision::number<mpfix::boost_backend<.*>.*>$")


def _decode(valobj):
    """The Decoded of a Float, or None if its bytes cannot be read."""
    valobj = valobj.GetNonSyntheticValue()
    type_ = valobj.GetType().GetCanonicalType()
    parameters = mpfix_format.parse_float_type(type_.GetName())
    if parameters is None:
        return None
    precision, exponent_bits = parameters
    error = lldb.SBError()
    size = valobj.GetByteSize()
    raw = valobj.GetData().ReadRawData(error, 0, size)
    if error.Fail() or raw is None or len(raw) < size:
        return None
    return mpfix_format.decode_bytes(bytes(raw), precision, exponent_bits)


def float_summary(valobj, internal_dict):
    decoded = _decode(valobj)
    if decoded is None:
        return "<unreadable>"
    return mpfix_format.decimal_text(decoded)


class FloatChildren:
    """Synthetic children: sign, class, exponent and significand."""

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.fields = []

    def update(self):
        decoded = _decode(self.valobj)
        self.fields = [] if decoded is None else mpfix_format.describe(decoded)
        return False

    def num_children(self):
        return len(self.fields)

    def has_children(self):
        return True

    def get_child_at_index(self, index):
        if index < 0 or index >= len(self.fields):
            return None
        name, text = self.fields[index]
        # A char array built from data, not from an expression: that would
        # need a running process, and fail on a core file.
        target = self.valobj.GetTarget()
        data = lldb.SBData.CreateDataFromCString(
            target.GetByteOrder(), target.GetAddressByteSize(), text)
        char_array = target.GetBasicType(lldb.eBasicTypeChar).GetArrayType(
            len(text) + 1)
        return self.valobj.CreateValueFromData(name, data, char_array)

    def get_child_index(self, name):
        for index, (field, _) in enumerate(self.fields):
            if field == name:
                return index
        return -1


def complex_summary(valobj, internal_dict):
    valobj = valobj.GetNonSyntheticValue()
    real = valobj.GetChildMemberWithName("re_")
    imaginary = valobj.GetChildMemberWithName("im_")
    return "(%s, %s)" % (real.GetSummary(), imaginary.GetSummary())


def boost_summary(valobj, internal_dict):
    value = valobj.GetNonSyntheticValue().GetChildMemberWithName(
        "m_backend").GetChildMemberWithName("value_")
    return value.GetSummary()


def __lldb_init_module(debugger, internal_dict):
    module = __name__
    category = "mpfix"
    debugger.HandleCommand(
        'type summary add -x "%s" -F %s.float_summary -w %s' %
        (_FLOAT_REGEX, module, category))
    debugger.HandleCommand(
        'type synthetic add -x "%s" --python-class %s.FloatChildren -w %s' %
        (_FLOAT_REGEX, module, category))
    debugger.HandleCommand(
        'type summary add -x "%s" -F %s.complex_summary -w %s' %
        (_COMPLEX_REGEX, module, category))
    debugger.HandleCommand(
        'type summary add -x "%s" -F %s.boost_summary -w %s' %
        (_BOOST_REGEX, module, category))
    debugger.HandleCommand("type category enable %s" % category)
