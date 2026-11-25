#!/usr/bin/env python3
"""grpn_py.py -- GRPN, a graphical RPN calculator, in Python and PyQt5.

A Python port of the GRPN C/Qt 5 program by Paul Wilkins.  It works with
real numbers, complex numbers and matrices; displays in binary, octal,
decimal or hexadecimal; uses degrees or radians; shows complex numbers in
rectangular or polar form; and supports named variables, undo, and typed
commands that may be abbreviated to their shortest unique prefix.

Arithmetic uses Python's decimal module with 77 significant digits, about
the same precision as the C version's 256-bit MPFR numbers.  The display
shows 12 significant digits by default, rounded half-up like HP
calculators, with optional HP 48 style engineering notation.

The calculation engine can be used without PyQt5:

    from grpn_py import Calculator, Matrix
    calc = Calculator()
    calc.type_text("2 3+")
    print(calc.format(calc.stack[-1]))        # 5

Run the GUI with:

    python3 grpn_py.py [-fn font] [-btn-fn font] [-disp-fn font]
                       [-rows n] [-cols n] [-b]

Copyright (C) 2002  Paul Wilkins (original C program).
This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your option)
any later version.
"""

import configparser
import math
import os
import sys
import decimal
from decimal import Decimal, localcontext
from fractions import Fraction

# ---------------------------------------------------------------------------
# Precision and decimal contexts
# ---------------------------------------------------------------------------

PREC = 77                 # significant digits of every stored number
PREC_BITS = 256           # the C version's MPFR precision; sets how many
                          # digits a fraction gets in binary/octal/hex
GUARD = 15                # extra digits for internal function evaluation
WP = PREC + GUARD         # working precision of the elementary functions
EXP_LIMIT = 323228496     # MPFR's default exponent range, in decimal
DISPLAY_GUARD = PREC - 5  # digits kept before display rounding (see below)

MIN_DISPLAY_DIGITS = 1
MAX_DISPLAY_DIGITS = 70
DEFAULT_DISPLAY_DIGITS = 12


def _context(prec, rounding=decimal.ROUND_HALF_EVEN):
    return decimal.Context(prec=prec, rounding=rounding, Emax=EXP_LIMIT,
                           Emin=-EXP_LIMIT, traps=[])


CTX = _context(PREC)


class CalcError(Exception):
    """An error shown on the top line of the display."""


# ---------------------------------------------------------------------------
# Elementary functions on Decimal (decimal has ln/log10/exp/sqrt/power only)
# ---------------------------------------------------------------------------

_pi_value = None
_pi_digits = 0


def _pi(digits):
    """pi to `digits` significant digits (Machin's formula, cached)."""
    global _pi_value, _pi_digits
    if digits > _pi_digits:
        n = digits + 10
        scale = 10 ** (n + 10)

        def arctan_inv(x):
            total = term = scale // x
            x2, k, sign = x * x, 3, -1
            while term:
                term //= x2
                total += sign * (term // k)
                sign, k = -sign, k + 2
            return total

        p = 16 * arctan_inv(5) - 4 * arctan_inv(239)
        with localcontext(_context(n)):
            _pi_value = Decimal(p).scaleb(-(n + 10))
        _pi_digits = n
    with localcontext(_context(digits)):
        return +_pi_value


def _sin_cos(x, wp=WP):
    """(sin x, cos x) for a finite Decimal x, good to about wp digits."""
    extra = max(0, x.adjusted()) + 5
    if extra > 20000:                  # argument reduction would be too slow
        nan = Decimal('NaN')
        return nan, nan
    with localcontext(_context(wp + extra)):
        half_pi = _pi(wp + extra + 5) / 2
        k = (x / half_pi).to_integral_value()
        r = x - k * half_pi            # |r| <= pi/4
    quadrant = int(k) % 4
    with localcontext(_context(wp + 5)):
        r = +r
        eps = Decimal(10) ** -(wp + 5)
        r2 = r * r
        s = term = r
        n = 1
        while term:
            term = -term * r2 / ((n + 1) * (n + 2))
            n += 2
            if abs(term) <= abs(s) * eps:
                break
            s += term
        c = term = Decimal(1)
        n = 0
        while True:
            term = -term * r2 / ((n + 1) * (n + 2))
            n += 2
            if abs(term) <= eps:
                break
            c += term
    return [(s, c), (c, -s), (-s, -c), (-c, s)][quadrant]


def _atan(x, wp=WP):
    """arctan of a Decimal x (finite or infinite)."""
    with localcontext(_context(wp + 5)):
        if not x:
            return Decimal(0)
        negative = x < 0
        x = abs(x)
        invert = x > 1
        if invert:
            x = 1 / x
        halvings = 0
        tenth = Decimal('0.1')
        while x > tenth:               # atan x = 2 atan(x / (1 + sqrt(1+x^2)))
            x = x / (1 + (1 + x * x).sqrt())
            halvings += 1
        eps = Decimal(10) ** -(wp + 5)
        x2 = x * x
        s = p = x
        k = 1
        while True:
            p = -p * x2
            k += 2
            t = p / k
            if abs(t) <= abs(s) * eps:
                break
            s += t
        s *= 2 ** halvings
        if invert:
            s = _pi(wp + 10) / 2 - s
        return -s if negative else s


def _atan2(y, x, wp=WP):
    with localcontext(_context(wp + 5)):
        if x > 0:
            return _atan(y / x, wp)
        if x < 0:
            a = _atan(y / x, wp)
            pi = _pi(wp + 10)
            return a + pi if y >= 0 else a - pi
        if y > 0:
            return _pi(wp + 10) / 2
        if y < 0:
            return -_pi(wp + 10) / 2
        return Decimal(0)


_bernoulli = []


def _bernoulli_b2k(count):
    """[B_2, B_4, ..., B_2count] as Fractions (Akiyama-Tanigawa)."""
    if len(_bernoulli) < count:
        n_max = 2 * count
        a = [Fraction(0)] * (n_max + 1)
        b = []
        for m in range(n_max + 1):
            a[m] = Fraction(1, m + 1)
            for j in range(m, 0, -1):
                a[j - 1] = j * (a[j - 1] - a[j])
            b.append(a[0])
        _bernoulli[:] = [b[2 * k] for k in range(1, count + 1)]
    return _bernoulli[:count]


_STIRLING_MIN = 200          # shift the argument up to here, then use Stirling


def _gamma(z):
    """Gamma of a finite Decimal z that is not 0 or a negative integer."""
    if z > 0 and z.adjusted() > 17:
        return Decimal('Infinity')             # far beyond the exponent range
    if z < Decimal('0.5'):
        # reflection: Gamma(z) = pi / (sin(pi z) Gamma(1-z))
        with localcontext(_context(WP + max(0, z.adjusted()) + 10)):
            t = z - 2 * (z / 2).to_integral_value(rounding=decimal.ROUND_FLOOR)
            one_minus_z = 1 - z
        with localcontext(_context(WP + 5)):
            sin_pz = _sin_cos(t * _pi(WP + 10))[0]
        g = _gamma(one_minus_z)
        with localcontext(_context(WP)):
            return _pi(WP + 5) / (sin_pz * g)
    if z == z.to_integral_value() and z <= 1001:
        with localcontext(_context(WP)):
            r = Decimal(1)
            for i in range(2, int(z)):
                r *= i
            return r
    with localcontext(_context(WP + max(0, z.adjusted()) + 10)):
        shift = 0
        prod = Decimal(1)
        while z + shift < _STIRLING_MIN:
            prod *= z + shift
            shift += 1
        x = z + shift
    wp2 = WP + max(0, x.adjusted()) + 3
    with localcontext(_context(wp2)):
        x = +x
        lg = (x - Decimal('0.5')) * x.ln() - x + (2 * _pi(wp2 + 5)).ln() / 2
        eps = Decimal(10) ** -wp2
        x2 = x * x
        xpow = x
        for k, b in enumerate(_bernoulli_b2k(40), start=1):
            term = Decimal(b.numerator) / (Decimal(b.denominator)
                                           * (2 * k) * (2 * k - 1) * xpow)
            lg += term
            if abs(term) < eps:
                break
            xpow *= x2
        g = lg.exp()
    with localcontext(_context(WP)):
        return g / prod


# ---------------------------------------------------------------------------
# Real numbers
# ---------------------------------------------------------------------------

OK, NAN, INF = 0, 1, 2


class Real:
    """An immutable real number: a 77-digit Decimal, NaN, or Infinity.

    As in the C version, NaN and Infinity are statuses rather than values;
    Infinity is unsigned and both display as words.
    """
    __slots__ = ('v', 'st')

    def __init__(self, v=Decimal(0), st=OK):
        self.v = v
        self.st = st

    @staticmethod
    def make(v):
        """A Real from a Decimal of any precision."""
        if v.is_nan():
            return NAN_REAL
        if v.is_infinite():
            return INF_REAL
        v = CTX.plus(v)
        if v.is_infinite():
            return INF_REAL
        return Real(v)

    @staticmethod
    def of(x):
        """A Real from an int, float, str or Decimal."""
        if isinstance(x, Real):
            return x
        if isinstance(x, float):
            x = repr(x)
        return Real.make(Decimal(x))

    def __repr__(self):
        if self.st == NAN:
            return 'Real(NaN)'
        if self.st == INF:
            return 'Real(Infinity)'
        return 'Real(%s)' % self.v

    def __float__(self):
        if self.st == NAN:
            return math.nan
        if self.st == INF:
            return math.inf
        return float(self.v)


NAN_REAL = Real(Decimal(0), NAN)
INF_REAL = Real(Decimal(0), INF)


def _status(a, b):
    return NAN_REAL if a.st == NAN or b.st == NAN else INF_REAL


def r_add(a, b):
    return _status(a, b) if a.st or b.st else Real.make(CTX.add(a.v, b.v))


def r_sub(a, b):
    return _status(a, b) if a.st or b.st else Real.make(CTX.subtract(a.v, b.v))


def r_mul(a, b):
    return _status(a, b) if a.st or b.st else Real.make(CTX.multiply(a.v, b.v))


def r_div(a, b):
    if a.st or b.st or not b.v:
        return NAN_REAL
    return Real.make(CTX.divide(a.v, b.v))


def r_neg(a):
    return a if a.st else Real(a.v.copy_negate())


def r_abs(a):
    return a if a.st else Real(a.v.copy_abs())


def r_inv(a):
    if a.st:
        return a
    if not a.v:
        return NAN_REAL
    return Real.make(CTX.divide(Decimal(1), a.v))


def r_pow(a, b):
    if a.st or b.st:
        return _status(a, b)
    if not b.v:
        return ONE
    if not a.v:
        return ZERO if b.v > 0 else INF_REAL
    return Real.make(CTX.power(a.v, b.v))


def _unary(a, f):
    if a.st:
        return a
    with localcontext(_context(WP)):
        return Real.make(f(a.v))


# decimal's own ln/log10/exp are correctly rounded in CTX
def r_ln(a):
    return _unary(a, CTX.ln)


def r_log(a):
    return _unary(a, CTX.log10)


def r_exp(a):
    return _unary(a, CTX.exp)


def r_sin(a):
    return _unary(a, lambda v: _sin_cos(v)[0])


def r_cos(a):
    return _unary(a, lambda v: _sin_cos(v)[1])


def r_tan(a):
    def tan(v):
        s, c = _sin_cos(v)
        return s / c if c else Decimal('Infinity')
    return _unary(a, tan)


def _sqrt_1_minus_sq(v):
    return ((1 - v) * (1 + v)).sqrt()


def r_asin(a):
    if a.st:
        return a
    if abs(a.v) > 1:
        return NAN_REAL
    return _unary(a, lambda v: _atan2(v, _sqrt_1_minus_sq(v)))


def r_acos(a):
    if a.st:
        return a
    if abs(a.v) > 1:
        return NAN_REAL
    return _unary(a, lambda v: _atan2(_sqrt_1_minus_sq(v), v))


def r_atan(a):
    return _unary(a, _atan)


def r_atan2(y, x):
    if y.st or x.st:
        return _status(y, x)
    return Real.make(_atan2(y.v, x.v))


def r_fact(a):
    """a! = Gamma(a+1); undefined (NaN) for negative integers."""
    if a.st:
        return a
    if a.v < 0 and a.v == a.v.to_integral_value():
        return NAN_REAL
    with localcontext(_context(PREC + 5)):
        z = a.v + 1
    return Real.make(_gamma(z))


def r_cmp(a, b):
    if not a.st and not b.st:
        return (a.v > b.v) - (a.v < b.v)
    return 0 if a.st == b.st else 1


def r_is_int(a):
    return not a.st and a.v == a.v.to_integral_value()


def r_to_int(a):
    """Truncate toward zero, like the C version's (int) cast."""
    return int(a.v) if not a.st else 0


ZERO = Real.of(0)
ONE = Real.of(1)
TWO = Real.of(2)
TEN = Real.of(10)
HALF = Real.of('0.5')
MONE = Real.of(-1)
PI_R = Real.make(_pi(PREC + 5))
PI2_R = Real.make(_pi(PREC + 5) / 2)
with localcontext(_context(WP)):
    R180PI = Real.make(Decimal(180) / _pi(WP))      # degrees per radian
LN10 = r_ln(TEN)

RADIANS, DEGREES = 'Radians', 'Degrees'
RECTANGULAR, POLAR = 'Rectangular', 'Polar'
BINARY, OCTAL, DECIMAL, HEXADECIMAL = 'Binary', 'Octal', 'Decimal', 'Hexadecimal'
BASE_RADIX = {BINARY: 2, OCTAL: 8, DECIMAL: 10, HEXADECIMAL: 16}


def r_to_radix(a, modes):
    """Radians to the angle mode for display."""
    return r_mul(a, R180PI) if modes.radix == DEGREES else a


def r_from_radix(a, modes):
    """An angle in the angle mode to radians."""
    return r_div(a, R180PI) if modes.radix == DEGREES else a


# ---------------------------------------------------------------------------
# Complex numbers (same formulas as the C version's complex.c)
# ---------------------------------------------------------------------------

class Cmplx:
    """An immutable complex number with Real parts, stored rectangular."""
    __slots__ = ('re', 'im')

    def __init__(self, re, im):
        self.re = Real.of(re)
        self.im = Real.of(im)

    def __repr__(self):
        return 'Cmplx(%r, %r)' % (self.re, self.im)


I_C = Cmplx(ZERO, ONE)


def c_neg(a):
    return Cmplx(r_neg(a.re), r_neg(a.im))


def c_add(a, b):
    return Cmplx(r_add(a.re, b.re), r_add(a.im, b.im))


def c_sub(a, b):
    return Cmplx(r_sub(a.re, b.re), r_sub(a.im, b.im))


def c_mul(a, b):
    return Cmplx(r_sub(r_mul(a.re, b.re), r_mul(a.im, b.im)),
                 r_add(r_mul(a.re, b.im), r_mul(a.im, b.re)))


def c_div(a, b):
    den = r_add(r_mul(b.re, b.re), r_mul(b.im, b.im))
    return Cmplx(r_div(r_add(r_mul(b.re, a.re), r_mul(b.im, a.im)), den),
                 r_div(r_sub(r_mul(b.re, a.im), r_mul(b.im, a.re)), den))


def c_inv(a):
    den = r_add(r_mul(a.re, a.re), r_mul(a.im, a.im))
    return Cmplx(r_div(a.re, den), r_neg(r_div(a.im, den)))


def c_add_r(a, r):
    return Cmplx(r_add(a.re, r), a.im)


def c_sub_r(a, r):
    return Cmplx(r_sub(a.re, r), a.im)


def c_r_sub(r, a):
    return Cmplx(r_sub(r, a.re), r_neg(a.im))


def c_mul_r(a, r):
    return Cmplx(r_mul(a.re, r), r_mul(a.im, r))


def c_div_r(a, r):
    return Cmplx(r_div(a.re, r), r_div(a.im, r))


def c_r_div(r, b):
    den = r_add(r_mul(b.re, b.re), r_mul(b.im, b.im))
    return Cmplx(r_div(r_mul(r, b.re), den), r_neg(r_div(r_mul(r, b.im), den)))


def c_abs(a):
    """|a| = big * sqrt(1 + (small/big)^2), avoiding overflow."""
    re, im = r_abs(a.re), r_abs(a.im)
    if not re.st and not im.st and not re.v and not im.v:
        return ZERO
    big, small = (re, im) if r_cmp(re, im) == 1 else (im, re)
    q = r_div(small, big)
    return r_mul(r_pow(r_add(r_mul(q, q), ONE), HALF), big)


def c_theta(a):
    sign_re = r_cmp(a.re, ZERO)
    if sign_re == 0:
        return r_neg(PI2_R) if r_cmp(a.im, ZERO) == -1 else PI2_R
    t = r_atan(r_div(a.im, a.re))
    if sign_re == -1:
        return r_sub(t, PI_R) if r_cmp(a.im, ZERO) == -1 else r_add(t, PI_R)
    return t


def c_polar(a):
    """(r, theta) packed in a Cmplx."""
    return Cmplx(c_abs(a), c_theta(a))


def c_rect(p):
    """(r, theta) packed in a Cmplx to rectangular."""
    return Cmplx(r_mul(r_cos(p.im), p.re), r_mul(r_sin(p.im), p.re))


def c_input(re, im, modes):
    """A complex number from two Reals entered in the current modes."""
    if modes.polar == POLAR:
        theta = r_div(im, R180PI) if modes.radix == DEGREES else im
        return c_rect(Cmplx(re, theta))
    return Cmplx(re, im)


def c_ln(a):
    p = c_polar(a)
    return Cmplx(r_ln(p.re), p.im)


def c_log(a):
    p = c_polar(a)
    return Cmplx(r_log(p.re), r_div(p.im, LN10))


def c_exp(a):
    e = r_exp(a.re)
    return Cmplx(r_mul(r_cos(a.im), e), r_mul(r_sin(a.im), e))


def c_pow(a, b):
    return c_exp(c_mul(c_ln(a), b))


def c_pow_r(a, r):
    return c_exp(c_mul_r(c_ln(a), r))


def c_r_pow(r, b):
    if r_cmp(r, ZERO) == -1:
        return c_exp(c_mul(b, c_ln(Cmplx(r, ZERO))))
    return c_exp(c_mul_r(b, r_ln(r)))


def c_sin(a):
    iz = c_mul(I_C, a)
    return c_div(c_sub(c_exp(iz), c_exp(c_neg(iz))), Cmplx(ZERO, TWO))


def c_cos(a):
    iz = c_mul(I_C, a)
    p = c_add(c_exp(iz), c_exp(c_neg(iz)))
    return Cmplx(r_mul(p.re, HALF), r_mul(p.im, HALF))


def c_tan(a):
    return c_div(c_sin(a), c_cos(a))


def c_asin(a):
    z = c_neg(a) if r_cmp(ZERO, a.re) == 1 else a
    sqzp1 = c_pow_r(c_add_r(z, ONE), HALF)
    sqzm1 = c_pow_r(c_sub_r(z, ONE), HALF)
    if r_cmp(ZERO, sqzp1.im) == 1:
        sqzp1 = c_neg(sqzp1)
    w = c_mul(c_ln(c_add(c_mul(sqzp1, sqzm1), z)), I_C)
    w = c_r_sub(PI2_R, w)
    if r_cmp(w.re, PI2_R) == 1:
        w = c_r_sub(PI_R, w)
    mpi = r_neg(PI_R)
    if r_cmp(w.re, mpi) == -1:
        w = c_r_sub(mpi, w)
    if r_cmp(ZERO, a.re) == 1:
        w = c_neg(w)
    return w


def c_acos(a):
    return c_r_sub(PI2_R, c_asin(a))


def c_atan(a):
    x, y = a.re, a.im
    r = c_abs(a)
    rsq = r_mul(r, r)
    re = r_mul(r_atan2(r_mul(x, TWO), r_sub(ONE, rsq)), HALF)
    r2 = r_add(rsq, ONE)
    r1 = r_mul(y, TWO)
    im = r_mul(r_ln(r_div(r_add(r2, r1), r_sub(r2, r1))), r_mul(HALF, HALF))
    return Cmplx(re, im)


# ---------------------------------------------------------------------------
# Matrices
# ---------------------------------------------------------------------------

class Matrix:
    """An immutable matrix whose entries are Real, Cmplx or Matrix."""
    __slots__ = ('data', 'rows', 'cols')

    def __init__(self, data):
        self.data = tuple(tuple(Real.of(x) if isinstance(x, (int, float, str, Decimal))
                                else x for x in row) for row in data)
        self.rows = len(self.data)
        self.cols = len(self.data[0]) if self.data else 0
        if any(len(row) != self.cols for row in self.data):
            raise ValueError('matrix rows must have the same length')

    def __getitem__(self, ij):
        i, j = ij
        return self.data[i][j]

    def __repr__(self):
        return 'Matrix(%r)' % (self.data,)


def _m_map(a, f):
    return Matrix([[f(x) for x in row] for row in a.data])


def _m_zip(a, b, f, name):
    if a.rows != b.rows or a.cols != b.cols:
        raise CalcError('%s Error: Invalid dimension.' % name)
    return Matrix([[f(x, y) for x, y in zip(ra, rb)]
                   for ra, rb in zip(a.data, b.data)])


def m_mul(a, b):
    if a.cols != b.rows:
        raise CalcError('* Error: Invalid dimension.')
    out = []
    for i in range(a.rows):
        row = []
        for j in range(b.cols):
            s = ZERO
            for k in range(a.cols):
                s = n_add(s, n_mul(a.data[i][k], b.data[k][j]))
            row.append(s)
        out.append(row)
    return Matrix(out)


def _is_zero(x):
    if isinstance(x, Real):
        return not x.st and not x.v
    if isinstance(x, Cmplx):
        return _is_zero(x.re) and _is_zero(x.im)
    return False


def _is_finite(x):
    if isinstance(x, Real):
        return not x.st
    return not x.re.st and not x.im.st


def _magnitude(x):
    """|x| of a finite real or complex entry, as a Decimal (NaN if the
    value went non-finite during elimination)."""
    m = r_abs(x) if isinstance(x, Real) else c_abs(x)
    return m.v if not m.st else Decimal('NaN')


# A pivot no bigger than this fraction of the largest entry is treated as
# zero: values carry 77 digits, so a singular matrix leaves pivots of
# rounding residue around 1e-77 of the entries' size.  Matrices whose
# condition number exceeds about 1e70 are thus also reported singular.
INV_PIVOT_TOLERANCE = Decimal('1e-70')


def m_inv(a):
    """Inverse by Gauss-Jordan elimination with partial pivoting."""
    n = a.rows
    if n != a.cols or n == 0:
        raise CalcError('Inv Error: Invalid dimension.')
    entries = [x for row in a.data for x in row]
    if any(isinstance(x, Matrix) for x in entries):
        raise CalcError('Inv Error: Invalid Type.')
    if not all(_is_finite(x) for x in entries):
        raise CalcError('Inv Error: Non-finite entry.')
    tol = CTX.multiply(max(_magnitude(x) for x in entries), INV_PIVOT_TOLERANCE)
    m = [list(row) + [ONE if i == j else ZERO for j in range(n)]
         for i, row in enumerate(a.data)]
    for col in range(n):
        # the largest entry in this column; if even that is within rounding
        # residue of zero, the matrix is singular
        sizes = [(_magnitude(m[r][col]), r) for r in range(col, n)]
        sizes = [(s, r) for s, r in sizes if not s.is_nan() and s > tol]
        if not sizes:
            raise CalcError('Inv Error: Singular matrix.')
        pivot = max(sizes, key=lambda sr: sr[0])[1]
        m[col], m[pivot] = m[pivot], m[col]
        p = m[col][col]
        m[col] = [n_div(x, p) for x in m[col]]
        for r in range(n):
            if r != col and not _is_zero(m[r][col]):
                f = m[r][col]
                m[r] = [n_sub(x, n_mul(f, y)) for x, y in zip(m[r], m[col])]
    return Matrix([row[n:] for row in m])


# ---------------------------------------------------------------------------
# Operations on any calculator value (C number.c)
# ---------------------------------------------------------------------------

def _invalid(name):
    return CalcError('%s Error: Invalid Type.' % name)


def n_neg(a):
    if isinstance(a, Real):
        return r_neg(a)
    if isinstance(a, Cmplx):
        return c_neg(a)
    return _m_map(a, n_neg)


def n_inv(a):
    if isinstance(a, Real):
        return r_inv(a)
    if isinstance(a, Cmplx):
        return c_inv(a)
    return m_inv(a)


def n_fact(a):
    if isinstance(a, Real):
        return r_fact(a)
    raise _invalid('!')


def _real_or_complex(a, name, real_ok, real_f, cmplx_f):
    if isinstance(a, Real):
        return real_f(a) if real_ok(a) else cmplx_f(Cmplx(a, ZERO))
    if isinstance(a, Cmplx):
        return cmplx_f(a)
    raise _invalid(name)


def n_ln(a):
    return _real_or_complex(a, 'ln', lambda x: r_cmp(x, ZERO) != -1, r_ln, c_ln)


def n_log(a):
    return _real_or_complex(a, 'log', lambda x: r_cmp(x, ZERO) != -1, r_log, c_log)


def n_exp(a):
    return _real_or_complex(a, 'exp', lambda x: True, r_exp, c_exp)


# Complex trigonometry is always in radians (as in the C version).
def n_sin(a, modes):
    return _real_or_complex(a, 'sin', lambda x: True,
                            lambda x: r_sin(r_from_radix(x, modes)), c_sin)


def n_cos(a, modes):
    return _real_or_complex(a, 'cos', lambda x: True,
                            lambda x: r_cos(r_from_radix(x, modes)), c_cos)


def n_tan(a, modes):
    return _real_or_complex(a, 'tan', lambda x: True,
                            lambda x: r_tan(r_from_radix(x, modes)), c_tan)


def _in_unit_range(x):
    return r_cmp(x, ONE) != 1 and r_cmp(x, MONE) != -1


def n_asin(a, modes):
    return _real_or_complex(a, 'asin', _in_unit_range,
                            lambda x: r_to_radix(r_asin(x), modes), c_asin)


def n_acos(a, modes):
    return _real_or_complex(a, 'acos', _in_unit_range,
                            lambda x: r_to_radix(r_acos(x), modes), c_acos)


def n_atan(a, modes):
    return _real_or_complex(a, 'atan', lambda x: True,
                            lambda x: r_to_radix(r_atan(x), modes), c_atan)


def n_pow(a, b):
    if isinstance(a, Matrix) or isinstance(b, Matrix):
        raise _invalid('pow')
    if isinstance(a, Real):
        negative = r_cmp(a, ZERO) == -1
        if isinstance(b, Real):
            if negative and not r_is_int(b):
                return c_pow_r(Cmplx(a, ZERO), b)
            return r_pow(a, b)
        return c_pow(Cmplx(a, ZERO), b) if negative else c_r_pow(a, b)
    if isinstance(b, Real):
        return c_pow_r(a, b)
    return c_pow(a, b)


def n_add(a, b):
    if isinstance(a, Real):
        if isinstance(b, Real):
            return r_add(a, b)
        if isinstance(b, Cmplx):
            return c_add_r(b, a)
        return _m_map(b, lambda x: n_add(x, a))
    if isinstance(a, Cmplx):
        if isinstance(b, Cmplx):
            return c_add(a, b)
        if isinstance(b, Real):
            return c_add_r(a, b)
        return _m_map(b, lambda x: n_add(x, a))
    if isinstance(b, Matrix):
        return _m_zip(a, b, n_add, '+')
    return _m_map(a, lambda x: n_add(x, b))


def n_sub(a, b):
    if isinstance(a, Real):
        if isinstance(b, Real):
            return r_sub(a, b)
        if isinstance(b, Cmplx):
            return c_r_sub(a, b)
        raise _invalid('-')
    if isinstance(a, Cmplx):
        if isinstance(b, Cmplx):
            return c_sub(a, b)
        if isinstance(b, Real):
            return c_sub_r(a, b)
        raise _invalid('-')
    if isinstance(b, Matrix):
        return _m_zip(a, b, n_sub, '-')
    return _m_map(a, lambda x: n_sub(x, b))


def n_mul(a, b):
    if isinstance(a, Real):
        if isinstance(b, Real):
            return r_mul(a, b)
        if isinstance(b, Cmplx):
            return c_mul_r(b, a)
        return _m_map(b, lambda x: n_mul(x, a))
    if isinstance(a, Cmplx):
        if isinstance(b, Cmplx):
            return c_mul(a, b)
        if isinstance(b, Real):
            return c_mul_r(a, b)
        return _m_map(b, lambda x: n_mul(x, a))
    if isinstance(b, Matrix):
        return m_mul(a, b)
    return _m_map(a, lambda x: n_mul(x, b))


def n_div(a, b):
    if isinstance(b, Matrix):
        raise _invalid('/')
    if isinstance(a, Real):
        return r_div(a, b) if isinstance(b, Real) else c_r_div(a, b)
    if isinstance(a, Cmplx):
        return c_div_r(a, b) if isinstance(b, Real) else c_div(a, b)
    return _m_map(a, lambda x: n_div(x, b))


def n_rip(a, b):
    """Resistors in parallel: a*b / (a+b)."""
    if isinstance(a, Matrix) or isinstance(b, Matrix):
        raise _invalid('rip')
    return n_div(n_mul(a, b), n_add(a, b))


def n_db(a, scale):
    if isinstance(a, Matrix):
        raise _invalid('db')
    return n_mul(n_log(a), Real.of(scale))


# ---------------------------------------------------------------------------
# Number formatting
# ---------------------------------------------------------------------------

class Modes:
    """Display and angle settings (the Calculator is also one of these)."""
    radix = RADIANS
    polar = RECTANGULAR
    base = DECIMAL
    digits = DEFAULT_DISPLAY_DIGITS
    eng = False
    separators = True


def _round_decimal(v, n):
    """Round a finite nonzero Decimal to n significant digits, halves away
    from zero (as HP calculators do).

    Returns (digits, exp10, negative): exactly n digit characters, and the
    power of ten of the first digit.  The value is rounded to a few digits
    fewer than the stored precision first, so a computed 0.99999...9 or a
    tie such as 1.2345 displays the way an HP calculator would show it.
    """
    with localcontext(_context(max(DISPLAY_GUARD, n + 1))):
        v = +v
    with localcontext(_context(n, decimal.ROUND_HALF_UP)):
        v = +v
    sign, digs, exp = v.as_tuple()
    d = ''.join(map(str, digs))
    exp10 = len(d) + exp - 1
    return (d + '0' * n)[:n], exp10, bool(sign)


def _print_std(v, digits):
    """Like printf %.<digits>g: trailing zeros removed, exponent form when
    the exponent is below -4 or at least the number of digits."""
    if not v:
        return '-0' if v.is_signed() else '0'
    d, x, negative = _round_decimal(v, digits)
    out = '-' if negative else ''
    sci = x < -4 or x >= digits
    if sci:
        out += d[0] + '.' + d[1:]
    elif x < 0:
        out += '0.' + '0' * (-x - 1) + d
    else:
        out += d[:x + 1] + '.' + d[x + 1:]
    out = out.rstrip('0').rstrip('.')
    if sci:
        out += 'e%s%02d' % ('-' if x < 0 else '+', abs(x))
    return out


def _print_eng(v, digits):
    """HP 48 ENG mode: `digits` significant digits (trailing zeros of the
    fraction removed), exponent a multiple of 3, 1 to 3 digits before the
    point."""
    if not v:
        d, x, negative = '0' * digits, 0, False
    else:
        d, x, negative = _round_decimal(v, digits)
    eng_exp = (x // 3) * 3
    int_digits = x - eng_exp + 1
    out = '-' if negative else ''
    out += ''.join(d[i] if i < digits else '0' for i in range(int_digits))
    frac = d[int_digits:digits].rstrip('0')
    if frac:
        out += '.' + frac
    return out + 'E%d' % eng_exp


def _group(number):
    """Insert thousands separators into the integer part."""
    start = 1 if number[:1] in '+-' else 0
    end = start
    while end < len(number) and number[end] not in '.eE':
        end += 1
    integer = number[start:end]
    if len(integer) <= 3:
        return number
    groups = []
    while integer:
        groups.insert(0, integer[-3:])
        integer = integer[:-3]
    return number[:start] + ','.join(groups) + number[end:]


_DIGIT_CHARS = '0123456789abcdef'


def _to_base(n, base):
    if n == 0:
        return '0'
    out = []
    while n:
        n, r = divmod(n, base)
        out.append(_DIGIT_CHARS[r])
    return ''.join(reversed(out))


def _print_base(v, base):
    prefix = {16: '0x', 8: '0', 2: ''}[base]
    negative = v.is_signed() and bool(v)
    sign = '-' if negative else ''
    if v == v.to_integral_value():
        return sign + prefix + _to_base(abs(int(v)), base)
    if abs(v.adjusted()) > 5000:                 # impractically long
        return _print_std(v, PREC)
    # mantissa digits as MPFR chooses them: 1 + ceil(bits * log(2)/log(base))
    m = int(PREC * math.log(10) / math.log(base))   # digits the precision supports
    f = abs(Fraction(v))
    e = int(v.adjusted() * math.log(10) / math.log(base))
    while f >= Fraction(base) ** e:
        e += 1
    while f < Fraction(base) ** (e - 1):
        e -= 1
    n = round(f * Fraction(base) ** (m - e))
    if n >= base ** m:
        n //= base
        e += 1
    d = _to_base(n, base).rjust(m, '0').rstrip('0') or '0'
    mant = d[0] + ('.' + d[1:] if len(d) > 1 else '')
    return '%s%s%s@%d' % (sign, prefix, mant, e - 1)


def format_real(a, modes=Modes):
    if a.st == NAN:
        return 'NaN'
    if a.st == INF:
        return 'Infinity'
    if modes.base != DECIMAL:
        return _print_base(a.v, BASE_RADIX[modes.base])
    if modes.eng:
        return _print_eng(a.v, modes.digits)
    out = _print_std(a.v, modes.digits)
    return _group(out) if modes.separators else out


def format_cmplx(c, modes=Modes, width=None):
    """(re, im), or (r, theta) in polar mode.  With a display width, a pair
    too long for one line is split after the comma."""
    if modes.polar == POLAR:
        p = c_polar(c)
        p1 = format_real(p.re, modes)
        theta = r_mul(p.im, R180PI) if modes.radix == DEGREES else p.im
        p2 = format_real(theta, modes)
    else:
        p1 = format_real(c.re, modes)
        p2 = format_real(c.im, modes)
    s = '(%s, %s)' % (p1, p2)
    if width is not None and len(p1) + len(p2) + 4 > width - 4:
        s = s[:len(p1) + 2] + '\n' + s[len(p1) + 3:]
    return s


def format_short(n, modes=Modes):
    if isinstance(n, Real):
        return format_real(n, modes)
    if isinstance(n, Cmplx):
        return format_cmplx(n, modes)
    return '[%d x %d Matrix]' % (n.rows, n.cols)


def format_matrix(m, modes=Modes):
    if not m.data:
        return ''
    rows = [''.join(format_short(x, modes) + ' ' for x in row) for row in m.data]
    return '[ ' + ' \n'.join(rows) + ']'


def format_number(n, modes=Modes, width=None):
    """The display text of a value; matrices and long complex numbers
    span several lines separated by newlines."""
    if isinstance(n, Real):
        return format_real(n, modes)
    if isinstance(n, Cmplx):
        return format_cmplx(n, modes, width)
    return format_matrix(n, modes)


# ---------------------------------------------------------------------------
# Number input (MPFR's mpfr_strtofr syntax)
# ---------------------------------------------------------------------------

def _digit_value(ch):
    i = _DIGIT_CHARS.find(ch.lower())
    return i


def parse_real(line, base):
    """Read a number from the start of line.  Returns (Real, chars used)
    or None.  Syntax: [sign] digits [. digits] [exponent]; the exponent is
    e/E (bases up to 10) or @ (any base) followed by a power of the base,
    or p/P (bases 2 and 16) followed by a power of two."""
    i = 0
    negative = False
    if line[i:i + 1] in ('+', '-'):
        negative = line[i] == '-'
        i += 1
    if base == 16 and line[i:i + 2].lower() == '0x' and \
            0 <= _digit_value(line[i + 2:i + 3] or 'z') < 16:
        i += 2
    elif base == 2 and line[i:i + 2].lower() == '0b' and line[i + 2:i + 3] in ('0', '1'):
        i += 2

    def digits_at(j):
        k = j
        while k < len(line) and 0 <= _digit_value(line[k]) < base:
            k += 1
        return k

    j = digits_at(i)
    int_digits = line[i:j]
    frac_digits = ''
    if j < len(line) and line[j] == '.':
        k = digits_at(j + 1)
        frac_digits = line[j + 1:k]
        if int_digits or frac_digits:
            j = k
    if not int_digits and not frac_digits:
        return None
    end = j
    exp, exp_base = 0, base
    markers = '@' + ('eE' if base <= 10 else '') + ('pP' if base in (2, 16) else '')
    if end < len(line) and line[end] in markers:
        k = end + 1
        if line[k:k + 1] in ('+', '-'):
            k += 1
        m = k
        while m < len(line) and line[m].isdigit():
            m += 1
        if m > k:
            exp = int(line[end + 1:m])
            exp_base = 2 if line[end] in 'pP' else base
            end = m
    if base == 10 and exp_base == 10:
        text = (int_digits or '0') + '.' + (frac_digits or '0') + 'E%d' % exp
        value = Decimal(text)
    else:
        mant = int((int_digits + frac_digits) or '0', base)
        with localcontext(_context(PREC + 10)):
            value = Decimal(mant) / Decimal(base) ** len(frac_digits)
            value *= Decimal(exp_base) ** exp
    if negative:
        value = value.copy_negate()
    return Real.make(value), end


# ---------------------------------------------------------------------------
# Buttons and commands (C button_def.c)
# ---------------------------------------------------------------------------

# (label, command, help, kind, argument)
#   kind 'op': call Calculator.<argument>(); 'char': type the character;
#   'pm': the +/- key; 'base': set the display base.
ROW2 = [
    ('Enter', 'dup', 'Copy the number on the top of the stack.', 'op', 'push_stack'),
    ('+/-', 'neg', 'Change sign.', 'pm', None),
    ('DRG', 'degrad', 'Toggle degrees/radians.', 'op', 'deg_rad'),
    ('HXD', 'hexdec', 'Toggle hex/decimal', 'op', 'hex_dec'),
    ('ENG', 'eng', 'Toggle engineering notation.', 'op', 'toggle_eng'),
    ('SEP', 'sep', 'Toggle thousands separators.', 'op', 'toggle_separators'),
    ('EEX', None, 'Mouse input: enter an exponent.', 'char', 'e'),
    ('DEL', None, 'Mouse input: backspace.', 'op', 'delete_editor'),
]
ROW3 = [
    ('Drop', 'drop', 'Delete the number on the top of the stack.', 'op', 'drop'),
    ('Swap', 'swap', 'Swap 2 numbers on the top of the stack.', 'op', 'swap'),
    ('Sto', 'sto', 'Store the top number under the entered variable name.', 'op',
     'store_variable'),
    ('Rcl', 'rcl', 'Recall the entered variable onto the stack.', 'op', 'recall_variable'),
    ('Purge', 'purge', 'Delete the entered variable.', 'op', 'purge_variable'),
    ('!', 'fact', 'Factorial (Gamma(x+1) for non-integers).', 'op', 'fact'),
]
NUM789 = [
    ('7', None, None, 'char', '7'), ('8', None, None, 'char', '8'),
    ('9', None, None, 'char', '9'), ('/', None, None, 'op', 'div'),
    ('Pi', 'pi', 'The constant PI.', 'op', 'pi'),
    ('Sin', 'sin', 'Trigonometric function Sin.', 'op', 'sin'),
    ('Cos', 'cos', 'Trigonometric function Cos.', 'op', 'cos'),
    ('Tan', 'tan', 'Trigonometric function Tan.', 'op', 'tan'),
]
NUM456 = [
    ('4', None, None, 'char', '4'), ('5', None, None, 'char', '5'),
    ('6', None, None, 'char', '6'), ('x', None, None, 'op', 'mul'),
    ('1/x', 'inv', 'Inverse.', 'op', 'inv'),
    ('Asin', 'asin', 'Trigonometric function Arc-sin.', 'op', 'asin'),
    ('Acos', 'acos', 'Trigonometric function Arc-cos.', 'op', 'acos'),
    ('Atan', 'atan', 'Trigonometric function Arc-tan.', 'op', 'atan'),
]
NUM123 = [
    ('1', None, None, 'char', '1'), ('2', None, None, 'char', '2'),
    ('3', None, None, 'char', '3'), ('-', None, None, 'op', 'sub'),
    ('Sqrt', 'sqrt', 'Square root.', 'op', 'sqrt'),
    ('Nroot', 'nroot', 'N-th root.', 'op', 'nroot'),
    ('Log', 'log', 'Log base 10.', 'op', 'log'),
    ('ln', 'ln', 'Natural log (log base e).', 'op', 'ln'),
]
NUM0 = [
    ('0', None, None, 'char', '0'), ('.', None, None, 'char', '.'),
    ('undo', 'undo', 'Undo last function', 'op', 'undo'),
    ('+', None, None, 'op', 'add'),
    ('x²', 'sqr', 'Square.', 'op', 'sqr'),
    ('yˣ', 'pow', 'Power.', 'op', 'pow'),
    ('10ˣ', 'tenx', 'Ten to the power.', 'op', 'tenx'),
    ('eˣ', 'exp', 'e to the power.', 'op', 'exp'),
]
HIDDEN = [
    ('Hex', 'hex', 'Hexadecimal display mode.', 'base', HEXADECIMAL),
    ('Dec', 'dec', 'Decimal display mode.', 'base', DECIMAL),
    ('Oct', 'oct', 'Octal display mode.', 'base', OCTAL),
    ('Bin', 'bin', 'Binary display mode.', 'base', BINARY),
    ('Db10', 'db10', 'Convert to dB (Power).', 'op', 'db10'),
    ('Db20', 'db20', 'Convert to dB (Voltage).', 'op', 'db20'),
    ('Rip', 'rip', 'Resistors in parallel.', 'op', 'rip'),
    ('Cplx', 'cplx', 'Create a complex number.', 'op', 'cplx'),
    ('Cplx', 'complex', 'Create a complex number.', 'op', 'cplx'),
    ('Mtrx', 'mtrx', 'Create or decompose a matrix.', 'op', 'mtrx'),
    ('Mtrx', 'matrix', 'Create or decompose a matrix.', 'op', 'mtrx'),
    ('Undo', 'undo', 'Undo last command.  Up to 10 commands can be undone.', 'op', 'undo'),
    ('Clear', 'clear', 'Clears and removes all numbers on the stack.', 'op', 'clear'),
    ('Quit', 'quit', 'Quit GRPN.', 'op', 'quit'),
]
BUTTON_ROWS = [ROW2, ROW3, NUM789, NUM456, NUM123, NUM0]
COMMAND_ROWS = BUTTON_ROWS + [HIDDEN]

MEMORY_BUTTON_COUNT = 10
MAX_UNDO_LEVELS = 10
EDIT_BUF_LEN = 1022

HELP_TEXT = """
GRPN  v1.1.2  (Python version)

By: Paul Wilkins
    paul.wilkins@analog.com

GRPN is a graphical reverse polish notation (RPN) calculator.

GRPN works with real numbers, complex numbers, matrices, and
complex matrices.  Numbers can be displayed in 4 different
radix modes, and complex numbers can be displayed in either
Cartesian or polar form.

GRPN uses a stack and reverse polish notation to evaluate
expressions.  The stack in GRPN is infinite and limited only
by available memory.

Expressions are evaluated in reverse polish notation.  Unlike
conventional (prefix) notation, RPN requires that all arguments
to a command are entered prior to execution of the command.  For
example to add 6.7 to 3.2 you would push both numbers onto the
stack, then press the add (+) button:

3.2<enter>
6.7<enter>
+

The result is left on the stack.

Note that GRPN uses a shorthand for all commands that causes
an automatic <enter> before executing a command.  The previous
example could then be done as:

3.2<enter>
6.7+

Input is accepted from both the keyboard and the mouse.  When
entering a command from the keyboard, the command may be
abbreviated to the shortest set of letters which uniquely
identifies the command.

Input may be forced to be interpreted as a command by prefixing
the command with double or single quotes.  This is useful, for
example when in Hexadecimal mode and you would like to switch to
decimal mode by typing:
dec

Mouse input simply requires pressing the button with the desired
command.

Available commands:

+           Add.
-           Subtract.
*           Multiply.
/           Divide.
^           Power.
!           Factorial.
<enter>     Push a number onto the top of the stack.
<space>     Push a number onto the top of the stack.
"""

LICENSE_TEXT = """

Copyright (C) 2002  Paul Wilkins

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.

"""


def help_text():
    lines = [HELP_TEXT]
    for row in COMMAND_ROWS:
        for label, cmd, hlp, kind, arg in row:
            if cmd:
                lines.append('%-12s%s\n' % (cmd, hlp))
    return ''.join(lines)


# ---------------------------------------------------------------------------
# The calculator: stack, input line, undo, variables, commands
# ---------------------------------------------------------------------------

def valid_variable_name(name):
    """Variable names are non-empty and have no whitespace or control
    characters."""
    return bool(name) and not any(ch.isspace() or ord(ch) < 32 or ord(ch) == 127
                                  for ch in name)


class Calculator(Modes):
    """The GRPN engine.  stack[-1] is the top of the stack (level 1)."""

    def __init__(self):
        self.stack = []
        self.error = ''
        self.edit = ''            # the input line
        self.cursor = 0
        self.variables = {}
        self.lcd_width = 30
        self._undo = []
        # Callbacks for a user interface (all optional).
        self.on_modes_changed = None      # f()
        self.on_variables_changed = None  # f(), after a variable is added or purged
        self.on_variables_edited = None    # f(), after Sto or Purge changes a variable
        self.on_quit = None               # f()

    # --- public conveniences -------------------------------------------------

    def format(self, value, width=None):
        """Display text of a value with the current modes."""
        return format_number(value, self, width)

    def type_text(self, text):
        """Feed characters as if typed (or pasted) into the display."""
        for ch in text:
            if ch in ' \n\t\r':
                self.key(' ')
            elif ' ' <= ch <= '~':
                self.key(ch)

    def press(self, label):
        """Press the keypad button with this label."""
        for row in BUTTON_ROWS:
            for entry in row:
                if entry[0] == label:
                    self.invoke(entry)
                    return
        raise KeyError(label)

    def invoke(self, entry):
        """Run a button/command table entry as its button does."""
        label, cmd, hlp, kind, arg = entry
        if kind == 'op':
            self.error = ''
            getattr(self, arg)()
        elif kind == 'char':
            self.error = ''
            self.insert_editor(arg)
        elif kind == 'pm':
            self.plus_minus()
        elif kind == 'base':
            self.set_base(arg)

    def key(self, k, ctrl=False):
        """Process one key: a printable character, or one of 'Return',
        'Tab', 'Home', 'Up', 'End', 'Down', 'Left', 'Right', 'BackSpace',
        'Delete', 'Escape'.  Control-C quits; other control keys are
        ignored."""
        if ctrl:
            if k in ('c', 'C'):
                self.quit()
            return
        self.error = ''
        if k in (' ', 'Return', 'Tab', 'KP_Enter', 'Linefeed'):
            self.push_stack()
        elif k in ('Home', 'Up'):
            self.cursor = 0
        elif k in ('End', 'Down'):
            self.cursor = len(self.edit)
        elif k == 'Right':
            self.cursor = min(self.cursor + 1, len(self.edit))
        elif k == 'Left':
            self.cursor = max(self.cursor - 1, 0)
        elif k in ('BackSpace', 'Delete'):
            if self.edit:
                self.delete_editor()
            else:
                self.drop()
        elif k == 'Escape':
            self.cancel_editor()
        elif k in ('+', '-'):
            if self._prev_char() in ('e', 'E'):
                self.insert_editor(k)
            elif k == '+':
                self.add()
            else:
                self.sub()
        elif k == '*':
            self.mul()
        elif k == '/':
            self.div()
        elif k == '^':
            self.pow()
        elif k == '!':
            self.fact()
        elif len(k) == 1 and ' ' <= k <= '~':
            self.insert_editor(k)

    def display(self, width, height):
        """The display as `height` strings of `width` characters, plus the
        input line's cursor column (None when not editing).  The error
        message is on the top line, the input line at the bottom, and the
        stack fills upward from the bottom, values right-aligned."""
        self.lcd_width = width
        grid = [[' '] * width for _ in range(height)]

        def put(row, col, text):
            for i, ch in enumerate(text[:max(0, width - col)]):
                grid[row][col + i] = ch

        editing = bool(self.edit)
        usable = height - 1 if editing else height
        if usable > 0:
            top = 0
            if self.error:
                put(0, 0, self.error)
                top = 1
            bottom = usable
            index = 0
            while index < len(self.stack):
                parts = self.format(self.stack[-1 - index], width).split('\n')
                row = max(bottom - len(parts), top)
                label = ('%d: ' % (index + 1))[:width]
                put(row, 0, label)
                shown = 0
                for i in range(row, bottom):
                    if shown >= len(parts):
                        break
                    text = parts[shown]
                    put(i, max(width - len(text), len(label)), text)
                    shown += 1
                bottom -= shown
                if bottom == top:
                    break
                index += 1
            j = bottom
            while j > top:
                put(j - 1, 0, ('%d: ' % (index + 1))[:width])
                index += 1
                j -= 1
        cursor = None
        if editing and height > 0:
            text = self.edit
            cursor = self.cursor
            if cursor > width:
                text = text[cursor - width:]
            put(height - 1, 0, text)
            cursor = min(cursor, width)
        return [''.join(r) for r in grid], cursor

    def mode_string(self):
        return ' %s   %s   %s' % (self.radix, self.polar, self.base)

    # --- modes ---------------------------------------------------------------

    def _modes_changed(self):
        if self.on_modes_changed:
            self.on_modes_changed()

    def set_base(self, base):
        self.base = base
        self._modes_changed()

    def set_radix(self, radix):
        self.radix = radix
        self._modes_changed()

    def set_polar(self, polar):
        self.polar = polar
        self._modes_changed()

    def set_digits(self, n):
        self.digits = max(MIN_DISPLAY_DIGITS, min(MAX_DISPLAY_DIGITS, int(n)))

    def deg_rad(self):
        self.set_radix(DEGREES if self.radix == RADIANS else RADIANS)

    def hex_dec(self):
        nxt = {BINARY: DECIMAL, DECIMAL: HEXADECIMAL, HEXADECIMAL: BINARY}
        self.set_base(nxt.get(self.base, self.base))

    def toggle_eng(self):
        self.eng = not self.eng
        self._modes_changed()

    def toggle_separators(self):
        self.separators = not self.separators
        self._modes_changed()

    # --- input line ----------------------------------------------------------

    def _prev_char(self):
        return self.edit[self.cursor - 1] if self.cursor > 0 else ''

    def insert_editor(self, ch):
        if len(self.edit) >= EDIT_BUF_LEN:
            return
        self.edit = self.edit[:self.cursor] + ch + self.edit[self.cursor:]
        self.cursor += 1

    def delete_editor(self):
        """Backspace over the character before the cursor."""
        if self.cursor == 0:
            return
        self.edit = self.edit[:self.cursor - 1] + self.edit[self.cursor:]
        self.cursor -= 1

    def cancel_editor(self):
        self.edit = ''
        self.cursor = 0

    def finish_editor(self):
        """Process anything on the input line."""
        if self.edit:
            line = self.edit
            self.cancel_editor()
            self.read_line(line)

    def plus_minus(self):
        """+/-: change the sign of the exponent (if any) or mantissa being
        typed, otherwise negate the top of the stack."""
        self.error = ''
        if not self.edit:
            self.neg()
            return
        line = self.edit
        e = next((i for i, ch in enumerate(line) if ch in 'eE'), None)
        pos = e + 1 if e is not None else 0
        if line[pos:pos + 1] in ('+', '-'):
            sign = '-' if line[pos] == '+' else '+'
            self.edit = line[:pos] + sign + line[pos + 1:]
        else:
            self.edit = line[:pos] + '-' + line[pos:]
            if self.cursor >= pos:
                self.cursor += 1

    def read_line(self, line):
        """A number (optionally followed by a command), or a command."""
        first = line[:1]
        if first not in ('"', "'") and (self._is_digit_base(first) or first in ('-', '+', '.')):
            line = line.replace(',', '')
            parsed = parse_real(line, BASE_RADIX[self.base])
            if parsed is None or parsed[0].st != OK:
                self.error = 'Error: Cannot read number.'
                return
            value, used = parsed
            snap = self._snapshot()
            self.stack.append(value)
            self._commit(snap)
            if used < len(line):
                self.run_command(line[used:])
        else:
            self.run_command(line)

    def _is_digit_base(self, ch):
        return len(ch) == 1 and 0 <= _digit_value(ch) < BASE_RADIX[self.base]

    def run_command(self, line):
        """Run a typed command; any unique prefix of a command works."""
        if line[:1] in ('"', "'"):
            line = line[1:]
        exact = None
        matches = []
        for row in COMMAND_ROWS:
            for entry in row:
                cmd = entry[1]
                if cmd and cmd.startswith(line):
                    if cmd == line:
                        exact = entry
                        break
                    if all(m[3:] != entry[3:] for m in matches):
                        matches.append(entry)
            if exact:
                break
        if exact is None and len(matches) == 1:
            exact = matches[0]
        if exact is not None:
            self.cancel_editor()
            self.invoke(exact)
        elif matches:
            self.error = '%s: Ambiguous command name.' % line
        else:
            self.error = '%s: Unrecognized command.' % line

    # --- undo ----------------------------------------------------------------

    def _snapshot(self):
        return list(self.stack)

    def _commit(self, snapshot):
        self._undo.append(snapshot)
        if len(self._undo) > MAX_UNDO_LEVELS:
            del self._undo[0]

    def undo(self):
        if not self._undo:
            self.error = 'Undo Error: Nothing to Undo.'
            return
        self.stack = self._undo.pop()

    # --- stack operations ----------------------------------------------------

    def _check_args(self, name, n):
        if len(self.stack) < n:
            self.error = '%s Error: Too few Arguments.' % name
            return True
        return False

    def _apply(self, name, nargs, fn):
        """Replace the top nargs values by fn(*values)."""
        self.finish_editor()
        if self._check_args(name, nargs):
            return
        args = self.stack[len(self.stack) - nargs:]
        try:
            result = fn(*args)
        except CalcError as e:
            self.error = str(e)
            return
        snap = self._snapshot()
        del self.stack[len(self.stack) - nargs:]
        self.stack.append(result)
        self._commit(snap)

    def push(self, value):
        """Push a value (Real, Cmplx, Matrix, or int/float/str) as an
        undoable step."""
        snap = self._snapshot()
        self.stack.append(Real.of(value) if not isinstance(value, (Real, Cmplx, Matrix))
                          else value)
        self._commit(snap)

    def push_stack(self):
        if self.edit:
            self.finish_editor()
        else:
            self.dup()

    def dup(self):
        self.finish_editor()
        if self._check_args('Dup', 1):
            return
        snap = self._snapshot()
        self.stack.append(self.stack[-1])
        self._commit(snap)

    def drop(self):
        if self._check_args('Drop', 1):
            return
        snap = self._snapshot()
        self.stack.pop()
        self._commit(snap)

    def swap(self):
        self.finish_editor()
        if self._check_args('swap', 2):
            return
        snap = self._snapshot()
        self.stack[-1], self.stack[-2] = self.stack[-2], self.stack[-1]
        self._commit(snap)

    def pi(self):
        self.finish_editor()
        snap = self._snapshot()
        self.stack.append(PI_R)
        self._commit(snap)

    def neg(self):
        self._apply('+/-', 1, n_neg)

    def inv(self):
        self._apply('Inv', 1, n_inv)

    def fact(self):
        self._apply('!', 1, n_fact)

    def ln(self):
        self._apply('ln', 1, n_ln)

    def log(self):
        self._apply('log', 1, n_log)

    def exp(self):
        self._apply('exp', 1, n_exp)

    def tenx(self):
        self._apply('10^x', 1, lambda a: n_pow(TEN, a))

    def pow(self):
        self._apply('pow', 2, n_pow)

    def nroot(self):
        def root(x, n):
            # matrix roots are not defined; say so before inverting one
            if isinstance(x, Matrix) or isinstance(n, Matrix):
                raise _invalid('nroot')
            return n_pow(x, n_inv(n))
        self._apply('nroot', 2, root)

    def sqr(self):
        self._apply('sqr', 1, lambda a: n_pow(a, TWO))

    def sqrt(self):
        self._apply('sqrt', 1, lambda a: n_pow(a, HALF))

    def db10(self):
        self._apply('db10', 1, lambda a: n_db(a, 10))

    def db20(self):
        self._apply('db20', 1, lambda a: n_db(a, 20))

    def rip(self):
        self._apply('rip', 2, n_rip)

    def sin(self):
        self._apply('sin', 1, lambda a: n_sin(a, self))

    def cos(self):
        self._apply('cos', 1, lambda a: n_cos(a, self))

    def tan(self):
        self._apply('tan', 1, lambda a: n_tan(a, self))

    def asin(self):
        self._apply('asin', 1, lambda a: n_asin(a, self))

    def acos(self):
        self._apply('acos', 1, lambda a: n_acos(a, self))

    def atan(self):
        self._apply('atan', 1, lambda a: n_atan(a, self))

    def add(self):
        self._apply('+', 2, n_add)

    def sub(self):
        self._apply('-', 2, n_sub)

    def mul(self):
        self._apply('*', 2, n_mul)

    def div(self):
        self._apply('/', 2, n_div)

    def cplx(self):
        """Make a complex number from the top two reals (in polar mode,
        level 2 is the magnitude and level 1 the angle)."""
        def make(re, im):
            if not isinstance(re, Real) or not isinstance(im, Real):
                raise CalcError('-> complex Error: Bad Argument Type.')
            return c_input(re, im, self)
        self._apply('cplx', 2, make)

    def mtrx(self):
        """Build a matrix (entries row by row, then columns, then rows on
        top) or take one apart (the reverse)."""
        self.finish_editor()
        if self._check_args('matrix', 1):
            return
        top = self.stack[-1]
        if isinstance(top, Cmplx):
            self.error = 'matrix Error: Bad Argument Type.'
        elif isinstance(top, Matrix):
            snap = self._snapshot()
            self.stack.pop()
            for row in top.data:
                self.stack.extend(row)
            self.stack.append(Real.of(top.cols))
            self.stack.append(Real.of(top.rows))
            self._commit(snap)
        else:
            self._create_matrix()

    def _create_matrix(self):
        if self._check_args('matrix', 2):
            return
        n_rows, n_cols = self.stack[-1], self.stack[-2]
        if not isinstance(n_rows, Real) or not isinstance(n_cols, Real):
            self.error = 'matrix Error: Bad Argument Type.'
            return
        rows, cols = r_to_int(n_rows), r_to_int(n_cols)
        if rows <= 0 or cols <= 0:
            self.error = 'matrix Error: Bad Argument Type.'
            return
        if len(self.stack) - 2 < rows * cols:
            self.error = 'matrix Error: Too few Arguments.'
            return
        snap = self._snapshot()
        del self.stack[-2:]
        entries = self.stack[len(self.stack) - rows * cols:]
        del self.stack[len(self.stack) - rows * cols:]
        self.stack.append(Matrix([entries[i * cols:(i + 1) * cols] for i in range(rows)]))
        self._commit(snap)

    def clear(self):
        self.stack = []
        self.error = ''
        self.cancel_editor()
        self._undo = []

    def quit(self):
        if self.on_quit:
            self.on_quit()

    # --- variables -----------------------------------------------------------

    def _variable_name(self):
        text = self.edit
        if not text.startswith("'") or len(text) < 2:
            self.error = "Variable Error: Enter a name beginning with '."
            return None
        if not valid_variable_name(text[1:]):
            self.error = 'Variable Error: Names cannot contain whitespace.'
            return None
        return text[1:]

    def _variables_changed(self, added_or_purged):
        if added_or_purged and self.on_variables_changed:
            self.on_variables_changed()
        if self.on_variables_edited:
            self.on_variables_edited()

    def store_variable(self):
        """Sto: copy the top value into the variable named on the input
        line ('name)."""
        name = self._variable_name()
        if name is None:
            return
        if not self.stack:
            self.error = 'Sto Error: Too few Arguments.'
            return
        is_new = name not in self.variables
        self.variables[name] = self.stack[-1]
        self._variables_changed(is_new)
        self.cancel_editor()

    def recall_variable(self):
        name = self._variable_name()
        if name is not None and self.recall_named(name):
            self.cancel_editor()

    def recall_named(self, name):
        if name not in self.variables:
            self.error = 'Rcl Error: Unknown variable.'
            return False
        snap = self._snapshot()
        self.stack.append(self.variables[name])
        self._commit(snap)
        return True

    def purge_variable(self):
        """Purge: delete the variable named on the input line ('name)."""
        name = self._variable_name()
        if name is None:
            return
        if name not in self.variables:
            self.error = 'Purge Error: Unknown variable.'
            return
        del self.variables[name]
        self._variables_changed(True)
        self.cancel_editor()


# ---------------------------------------------------------------------------
# PyQt5 user interface
# ---------------------------------------------------------------------------

try:
    from PyQt5 import QtCore, QtGui, QtWidgets
except ImportError:            # the engine above works without PyQt5
    QtWidgets = None

DEFAULT_LCD_FAMILY = 'DejaVu Sans Mono'


def _parse_font(spec, default_size=None):
    """A QFont from a Pango-style name such as 'DejaVu Sans Mono Bold 12'."""
    words = spec.split()
    size = None
    if words:
        try:
            size = float(words[-1])
            words = words[:-1]
        except ValueError:
            pass
    font = QtGui.QFont()
    styles = {'bold': lambda: font.setBold(True), 'italic': lambda: font.setItalic(True),
              'oblique': lambda: font.setItalic(True)}
    while words and words[-1].lower() in styles:
        styles[words.pop().lower()]()
    if words:
        font.setFamily(' '.join(words))
    if size or default_size:
        font.setPointSizeF(size or default_size)
    return font


if QtWidgets is not None:
    Qt = QtCore.Qt

    class KeyButton(QtWidgets.QPushButton):
        """A keypad button only as wide as its label plus a small margin."""

        def __init__(self, text=''):
            super().__init__(text)
            self.setFocusPolicy(Qt.NoFocus)

        def natural_width(self):
            fm = self.fontMetrics()
            return max(fm.horizontalAdvance(t) for t in self.text().split('\n')) + 10

        def sizeHint(self):
            return QtCore.QSize(self.natural_width(), super().sizeHint().height())

        def minimumSizeHint(self):
            return self.sizeHint()

    class LcdWidget(QtWidgets.QWidget):
        """The stack display: a grid of fixed-width characters."""
        BORDER = 4

        def __init__(self, window, calc, rows, cols, font):
            super().__init__()
            self.window_ = window
            self.calc = calc
            self.lcd_font = font
            self.lines = []
            self.anchor = None
            self.selection = None          # (x1, y1, x2, y2), inclusive
            self.dragging = False
            self.setFocusPolicy(Qt.StrongFocus)
            self.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
            self._measure()
            self._set_minimum(cols, rows)

        def _measure(self):
            fm = QtGui.QFontMetrics(self.lcd_font)
            self.fw = max(1, fm.horizontalAdvance('M'))
            self.fh = max(1, fm.height())
            self.ascent = fm.ascent()

        def _set_minimum(self, cols, rows):
            self.setMinimumSize(2 * self.BORDER + cols * self.fw, 2 * self.BORDER + rows * self.fh)

        def grid_size(self):
            return (max(1, (self.width() - 2 * self.BORDER) // self.fw),
                    max(1, (self.height() - 2 * self.BORDER) // self.fh))

        def font_size(self):
            return int(round(self.lcd_font.pointSizeF()))

        def set_font_size(self, size):
            cols, rows = self.grid_size()
            self.lcd_font.setPointSizeF(size)
            self._measure()
            self._set_minimum(cols, rows)
            self.updateGeometry()
            self.update()

        def paintEvent(self, event):
            p = QtGui.QPainter(self)
            p.fillRect(self.rect(), Qt.white)
            p.setFont(self.lcd_font)
            cols, rows = self.grid_size()
            self.lines, cursor = self.calc.display(cols, rows)
            b = self.BORDER
            p.setPen(Qt.black)
            for i, line in enumerate(self.lines):
                p.drawText(b, b + i * self.fh + self.ascent, line)
            if self.selection:
                x1, y1, x2, y2 = self.selection
                x2, y2 = min(x2, cols - 1), min(y2, rows - 1)
                for y in range(y1, y2 + 1):
                    p.fillRect(b + x1 * self.fw, b + y * self.fh,
                               (x2 - x1 + 1) * self.fw, self.fh, QtGui.QColor.fromRgbF(0.15, 0.25, 0.45))
                    p.setPen(Qt.white)
                    p.drawText(b + x1 * self.fw, b + y * self.fh + self.ascent,
                               self.lines[y][x1:x2 + 1])
            if cursor is not None:
                p.setPen(Qt.black)
                x = b + cursor * self.fw - 1
                p.drawLine(x, b + (rows - 1) * self.fh, x, b + rows * self.fh - 1)

        # keyboard
        KEY_NAMES = {
            Qt.Key_Return: 'Return', Qt.Key_Enter: 'Return', Qt.Key_Tab: 'Tab',
            Qt.Key_Home: 'Home', Qt.Key_Up: 'Up', Qt.Key_End: 'End', Qt.Key_Down: 'Down',
            Qt.Key_Left: 'Left', Qt.Key_Right: 'Right', Qt.Key_Backspace: 'BackSpace',
            Qt.Key_Delete: 'Delete', Qt.Key_Escape: 'Escape',
        }

        def event(self, event):
            # keep Tab for ourselves instead of moving the focus
            if event.type() == QtCore.QEvent.KeyPress and event.key() == Qt.Key_Tab:
                self.keyPressEvent(event)
                return True
            return super().event(event)

        def keyPressEvent(self, event):
            k = event.key()
            if event.modifiers() & Qt.ControlModifier:
                if k == Qt.Key_C:
                    self.calc.key('c', ctrl=True)       # Control-C quits
                return
            name = self.KEY_NAMES.get(k)
            if k == Qt.Key_Insert and event.modifiers() & Qt.KeypadModifier:
                name = 'Return'
            if name is None:
                text = event.text()
                if len(text) == 1 and ' ' <= text <= '~':
                    name = text
                else:
                    event.ignore()
                    return
            self.calc.key(name)
            self.window_.refresh()

        # mouse: drag to select and copy, middle button to paste
        def _cell(self, pos):
            cols, rows = self.grid_size()
            x = (pos.x() - self.BORDER) // self.fw
            y = (pos.y() - self.BORDER) // self.fh
            return min(max(x, 0), cols - 1), min(max(y, 0), rows - 1)

        @staticmethod
        def _clipboard():
            cb = QtWidgets.QApplication.clipboard()
            mode = QtGui.QClipboard.Selection if cb.supportsSelection() else QtGui.QClipboard.Clipboard
            return cb, mode

        def mousePressEvent(self, event):
            self.setFocus()
            if event.button() == Qt.LeftButton:
                self.anchor = self._cell(event.pos())
                self.selection = None
                self.dragging = True
                self.update()
            elif event.button() == Qt.MiddleButton:
                cb, mode = self._clipboard()
                self.calc.type_text(cb.text(mode))
                self.window_.refresh()

        def mouseMoveEvent(self, event):
            if self.dragging:
                x, y = self._cell(event.pos())
                ax, ay = self.anchor
                self.selection = (min(ax, x), min(ay, y), max(ax, x), max(ay, y))
                self.update()

        def mouseReleaseEvent(self, event):
            if event.button() == Qt.LeftButton and self.dragging:
                self.dragging = False
                if self.selection and self.lines:
                    x1, y1, x2, y2 = self.selection
                    text = '\n'.join(self.lines[y][x1:x2 + 1]
                                     for y in range(y1, min(y2, len(self.lines) - 1) + 1))
                    cb, mode = self._clipboard()
                    cb.setText(text, mode)

    class TextDialog(QtWidgets.QDialog):
        def __init__(self, parent, title, text):
            super().__init__(parent)
            self.setWindowTitle(title)
            self.resize(470, 470)
            layout = QtWidgets.QVBoxLayout(self)
            view = QtWidgets.QPlainTextEdit(text)
            view.setReadOnly(True)
            view.setFont(QtGui.QFontDatabase.systemFont(QtGui.QFontDatabase.FixedFont))
            layout.addWidget(view)
            button = QtWidgets.QPushButton('Dismiss')
            button.clicked.connect(self.close)
            layout.addWidget(button)

    class OptionsDialog(QtWidgets.QDialog):
        def __init__(self, window):
            super().__init__(window)
            self.window_ = window
            calc = window.calc
            self.setWindowTitle('Options')
            layout = QtWidgets.QVBoxLayout(self)
            layout.setSizeConstraint(QtWidgets.QLayout.SetFixedSize)

            def section(title):
                box = QtWidgets.QGroupBox(title)
                row = QtWidgets.QHBoxLayout(box)
                layout.addWidget(box)
                return row

            def radios(title, choices, setter):
                row = section(title)
                group = QtWidgets.QButtonGroup(self)
                buttons = {}
                for label, value in choices:
                    b = QtWidgets.QRadioButton(label)
                    group.addButton(b)
                    row.addWidget(b)
                    b.toggled.connect(lambda on, v=value: on and self._set(setter, v))
                    buttons[value] = b
                return buttons

            self.base_buttons = radios('Base', [('Bin', BINARY), ('Oct', OCTAL), ('Dec', DECIMAL),
                                                ('Hex', HEXADECIMAL)], calc.set_base)
            self.radix_buttons = radios('Radix', [('Degrees', DEGREES), ('Radians', RADIANS)],
                                        calc.set_radix)
            self.polar_buttons = radios('Coordinate Mode', [('Rectangular', RECTANGULAR),
                                                            ('Polar', POLAR)], calc.set_polar)
            row = section('Number Formatting')
            self.separators = QtWidgets.QCheckBox('Thousands separators')
            self.separators.toggled.connect(lambda on: self._set(setattr, calc, 'separators', on))
            row.addWidget(self.separators)
            self.eng = QtWidgets.QCheckBox('Engineering notation')
            self.eng.toggled.connect(lambda on: self._set(setattr, calc, 'eng', on))
            row.addWidget(self.eng)
            row = section('Maximum Displayed Digits')
            self.digits = QtWidgets.QSpinBox()
            self.digits.setRange(MIN_DISPLAY_DIGITS, MAX_DISPLAY_DIGITS)
            self.digits.valueChanged.connect(lambda n: self._set(calc.set_digits, n))
            row.addWidget(self.digits)
            row.addStretch()
            row = section('Display Font Size')
            self.font_size = QtWidgets.QSpinBox()
            self.font_size.setRange(8, 36)
            self.font_size.valueChanged.connect(lambda n: self._set(window.lcd.set_font_size, n))
            row.addWidget(self.font_size)
            row.addStretch()
            dismiss = QtWidgets.QPushButton('Dismiss')
            dismiss.clicked.connect(self.close)
            layout.addWidget(dismiss)
            self.sync()

        def _set(self, setter, *args):
            setter(*args)
            self.window_.refresh()

        def sync(self):
            """Show the calculator's current settings."""
            calc = self.window_.calc
            widgets = [self.separators, self.eng, self.digits, self.font_size]
            widgets += list(self.base_buttons.values()) + list(self.radix_buttons.values())
            widgets += list(self.polar_buttons.values())
            for w in widgets:
                w.blockSignals(True)
            self.base_buttons[calc.base].setChecked(True)
            self.radix_buttons[calc.radix].setChecked(True)
            self.polar_buttons[calc.polar].setChecked(True)
            self.separators.setChecked(calc.separators)
            self.eng.setChecked(calc.eng)
            self.digits.setValue(calc.digits)
            self.font_size.setValue(self.window_.lcd.font_size())
            for w in widgets:
                w.blockSignals(False)

    class MainWindow(QtWidgets.QMainWindow):
        def __init__(self, calc, rows=8, cols=30, draw_buttons=True,
                     button_font=None, display_font=None, font_size=None):
            super().__init__()
            self.calc = calc
            self.options = self.help_dialog = self.license_dialog = None
            self.memory_buttons = []
            self.setWindowTitle('grpn')

            menu = self.menuBar()
            file_menu = menu.addMenu('File')
            file_menu.addAction('Clear', self.clear)
            file_menu.addAction('Options', self.show_options)
            file_menu.addAction('Quit', QtWidgets.QApplication.quit)
            help_menu = menu.addMenu('Help')
            help_menu.addAction('Help', self.show_help)
            help_menu.addAction('License', self.show_license)

            central = QtWidgets.QWidget()
            vbox = QtWidgets.QVBoxLayout(central)
            vbox.setContentsMargins(0, 0, 0, 0)
            vbox.setSpacing(0)
            self.mode_label = QtWidgets.QLabel()
            self.mode_label.setAlignment(Qt.AlignCenter)
            vbox.addWidget(self.mode_label)
            # a size given with -disp-fn wins over the saved font size
            lcd_font = _parse_font(display_font or DEFAULT_LCD_FAMILY, font_size or 12)
            lcd_font.setStyleHint(QtGui.QFont.Monospace)
            lcd_font.setFixedPitch(True)
            self.lcd = LcdWidget(self, calc, rows, cols, lcd_font)
            vbox.addWidget(self.lcd, 1)
            if button_font:
                font = _parse_font(button_font)
                menu.setFont(font)
            if draw_buttons:
                keypad = QtWidgets.QWidget()
                if button_font:
                    keypad.setFont(font)
                self._build_keypad(keypad)
                vbox.addWidget(keypad)
            self.setCentralWidget(central)

            calc.on_modes_changed = self.modes_changed
            calc.on_variables_changed = self.variables_changed
            calc.on_quit = QtWidgets.QApplication.quit
            self.modes_changed()
            self.lcd.setFocus()

        def _build_keypad(self, keypad):
            layout = QtWidgets.QVBoxLayout(keypad)
            layout.setContentsMargins(0, 0, 0, 0)
            layout.setSpacing(0)
            rows = [[KeyButton() for _ in range(MEMORY_BUTTON_COUNT)]]
            for b in rows[0]:
                b.setEnabled(False)
                b.clicked.connect(lambda checked=False, b=b: self.recall(b.property('variable')))
            self.memory_buttons = rows[0]
            for row in BUTTON_ROWS:
                buttons = []
                for entry in row:
                    b = KeyButton(entry[0])
                    b.clicked.connect(lambda checked=False, e=entry: self.button(e))
                    buttons.append(b)
                rows.append(buttons)
            self.key_rows = rows
            height = max(b.sizeHint().height() for row in rows for b in row)
            for row in rows:
                box = QtWidgets.QHBoxLayout()
                box.setSpacing(0)
                for b in row:
                    b.setSizePolicy(QtWidgets.QSizePolicy.Ignored, QtWidgets.QSizePolicy.Fixed)
                    b.setFixedHeight(height)
                    box.addWidget(b, 1)
                layout.addLayout(box)
                self._equalize(row)
            self.variables_changed()

        @staticmethod
        def _equalize(row):
            width = max(b.natural_width() for b in row)
            for b in row:
                b.setMinimumWidth(width)

        def closeEvent(self, event):
            QtWidgets.QApplication.quit()       # also closes any open dialogs
            super().closeEvent(event)

        def button(self, entry):
            self.calc.invoke(entry)
            self.refresh()

        def variables_changed(self):
            """The memory buttons show the first variables, oldest first."""
            if not self.memory_buttons:
                return
            names = list(self.calc.variables)
            for i, b in enumerate(self.memory_buttons):
                name = names[i] if i < len(names) else ''
                b.setText(name.replace('&', '&&'))    # '&' is not a shortcut
                b.setProperty('variable', name)
                b.setEnabled(bool(name))
            self._equalize(self.memory_buttons)

        def recall(self, name):
            self.calc.recall_named(name)
            self.refresh()

        def modes_changed(self):
            self.mode_label.setText(self.calc.mode_string())
            if self.options:
                self.options.sync()
            self.refresh()

        def refresh(self):
            self.lcd.update()

        def clear(self):
            self.calc.clear()
            self.refresh()

        def show_options(self):
            if self.options is None:
                self.options = OptionsDialog(self)
            self.options.sync()
            self.options.show()
            self.options.raise_()

        def show_help(self):
            if self.help_dialog is None:
                self.help_dialog = TextDialog(self, 'Help', help_text())
            self.help_dialog.show()
            self.help_dialog.raise_()

        def show_license(self):
            if self.license_dialog is None:
                self.license_dialog = TextDialog(self, 'License', LICENSE_TEXT)
            self.license_dialog.show()
            self.license_dialog.raise_()


USAGE = 'Usage: grpn [-fn font] [-btn-fn font] [-disp-fn font] [-rows n] [-cols n] [-b]'


def parse_args(argv):
    opts = {'rows': 8, 'cols': 30, 'draw_buttons': True, 'button_font': None,
            'display_font': None}

    def fail(msg):
        sys.stderr.write('Error: %s\n%s\n' % (msg, USAGE))
        return None

    args = list(argv[1:])
    while args:
        a = args.pop(0)
        if a == '-b':
            opts['draw_buttons'] = False
        elif a in ('-fn', '-btn-fn', '-disp-fn'):
            if not args:
                return fail('Missing font argument.')
            font = args.pop(0)
            if a in ('-fn', '-btn-fn'):
                opts['button_font'] = font
            if a in ('-fn', '-disp-fn'):
                opts['display_font'] = font
        elif a in ('-rows', '-cols'):
            try:
                n = int(args.pop(0))
            except (IndexError, ValueError):
                n = 0
            if n < 1:
                return fail('Invalid display size.')
            opts[a[1:]] = n
        else:
            return fail('Unknown argument.')
    return opts


# ---------------------------------------------------------------------------
# Saved settings, shared with the C version: $XDG_CONFIG_HOME/grpn/grpn.conf
# ---------------------------------------------------------------------------

SETTINGS_GROUP = 'Options'
_SETTING_CHOICES = {'base': (BINARY, OCTAL, DECIMAL, HEXADECIMAL),
                    'radix': (DEGREES, RADIANS),
                    'coordinates': (RECTANGULAR, POLAR)}
_SETTING_ATTRS = {'coordinates': 'polar'}


def settings_path():
    config = os.environ.get('XDG_CONFIG_HOME') or os.path.join(os.path.expanduser('~'), '.config')
    return os.path.join(config, 'grpn', 'grpn.conf')


def load_settings(calc, path=None):
    """Apply saved Options settings to calc and return the saved display
    font size (None if there is none).  A missing file or a missing or
    invalid value leaves the default in place."""
    path = path or settings_path()
    cp = configparser.ConfigParser(interpolation=None)
    try:
        if not cp.read(path, encoding='utf-8'):
            return None
    except (configparser.Error, OSError, UnicodeError) as e:
        sys.stderr.write('grpn_py: could not read %s: %s\n' % (path, e))
        return None
    if not cp.has_section(SETTINGS_GROUP):
        return None
    opts = cp[SETTINGS_GROUP]
    for key, choices in _SETTING_CHOICES.items():
        value = opts.get(key, '').strip().lower()
        for c in choices:
            if value == c.lower():
                setattr(calc, _SETTING_ATTRS.get(key, key), c)
    for key, attr in (('thousands_separators', 'separators'), ('engineering_notation', 'eng')):
        try:
            value = opts.getboolean(key)
        except ValueError:
            value = None
        if value is not None:
            setattr(calc, attr, value)
    font_size = None
    try:
        if 'display_digits' in opts:
            calc.set_digits(opts.getint('display_digits'))
        if 'font_size' in opts:
            font_size = max(8, min(36, opts.getint('font_size')))
    except ValueError:
        pass
    return font_size


def save_settings(calc, font_size, path=None):
    path = path or settings_path()
    cp = configparser.ConfigParser(interpolation=None)
    cp[SETTINGS_GROUP] = {
        'base': calc.base, 'radix': calc.radix, 'coordinates': calc.polar,
        'thousands_separators': 'true' if calc.separators else 'false',
        'engineering_notation': 'true' if calc.eng else 'false',
        'display_digits': str(calc.digits), 'font_size': str(font_size)}
    try:
        os.makedirs(os.path.dirname(path), mode=0o700, exist_ok=True)
        with open(path, 'w', encoding='utf-8') as f:
            cp.write(f, space_around_delimiters=False)
    except OSError as e:
        sys.stderr.write('grpn_py: could not save settings to %s: %s\n' % (path, e))


# ---------------------------------------------------------------------------
# Saved variables, shared with the C version: $XDG_CONFIG_HOME/grpn/variables
#
# Each line is a name followed by its value:
#    value := r REAL | c REAL REAL | m ROWS COLS value...
#    REAL  := a decimal number such as -1.25e-3, or nan or inf
# 'c' is a complex number (real part, imaginary part); 'm' is a matrix whose
# ROWS*COLS entries follow in row order.  Blank lines and lines starting
# with # are ignored.
# ---------------------------------------------------------------------------

MAX_MATRIX_NESTING = 16
MAX_MATRIX_ENTRIES = 1000000


def variables_path():
    return os.path.join(os.path.dirname(settings_path()), 'variables')


def _real_text(r):
    if r.st == NAN:
        return 'nan'
    if r.st == INF:
        return 'inf'
    return str(r.v)


def value_text(value):
    """A Real, Cmplx or Matrix in the saved-variables format."""
    if isinstance(value, Real):
        return 'r ' + _real_text(value)
    if isinstance(value, Cmplx):
        return 'c %s %s' % (_real_text(value.re), _real_text(value.im))
    if isinstance(value, Matrix):
        return ' '.join(['m %d %d' % (value.rows, value.cols)] +
                        [value_text(x) for row in value.data for x in row])
    raise TypeError('cannot save %r' % (value,))


def _read_real(tokens):
    t = next(tokens)
    if t == 'nan':
        return NAN_REAL
    if t == 'inf':
        return INF_REAL
    if '_' in t:                    # Decimal accepts 1_000; the C version doesn't
        raise ValueError(t)
    try:
        v = Decimal(t)
    except decimal.InvalidOperation:
        raise ValueError(t)
    r = Real.make(v)
    if not v.is_finite() or r.st != OK:     # NaN and Inf only as words
        raise ValueError(t)
    return r


def _read_value(tokens, depth=0):
    if depth > MAX_MATRIX_NESTING:
        raise ValueError('matrix nested too deeply')
    kind = next(tokens)
    if kind == 'r':
        return _read_real(tokens)
    if kind == 'c':
        return Cmplx(_read_real(tokens), _read_real(tokens))
    if kind == 'm':
        rows, cols = int(next(tokens)), int(next(tokens))
        if rows < 1 or cols < 1 or rows * cols > MAX_MATRIX_ENTRIES:
            raise ValueError('bad matrix size')
        return Matrix([[_read_value(tokens, depth + 1) for _ in range(cols)]
                       for _ in range(rows)])
    raise ValueError(kind)


def parse_value(text):
    """The value written by value_text()."""
    tokens = iter(text.split())
    value = _read_value(tokens)
    if next(tokens, None) is not None:
        raise ValueError('extra text')
    return value


def load_variables(calc, path=None):
    """Add the saved variables to calc.  A missing file means there are
    none; unreadable lines are skipped."""
    path = path or variables_path()
    try:
        with open(path, encoding='utf-8') as f:
            lines = f.read().splitlines()
    except FileNotFoundError:
        return
    except (OSError, UnicodeError) as e:
        sys.stderr.write('grpn_py: could not read %s: %s\n' % (path, e))
        return
    for lineno, line in enumerate(lines, 1):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        name, rest = (line.split(None, 1) + [''])[:2]
        try:
            if not valid_variable_name(name):
                raise ValueError(name)
            calc.variables[name] = parse_value(rest)
        except (ValueError, StopIteration):
            sys.stderr.write('grpn_py: %s:%d: ignoring unreadable variable\n' % (path, lineno))
    if calc.on_variables_changed:
        calc.on_variables_changed()


def save_variables(calc, path=None):
    """Rewrite the file.  It is written beside the old one and renamed over
    it, so a failed write leaves the old file in place."""
    path = path or variables_path()
    tmp = path + '.tmp'
    try:
        os.makedirs(os.path.dirname(path), mode=0o700, exist_ok=True)
        with open(tmp, 'w', encoding='utf-8') as f:
            f.write('# GRPN variables (see variables.h in the grpn sources)\n')
            for name, value in calc.variables.items():
                f.write('%s %s\n' % (name, value_text(value)))
        os.replace(tmp, path)
    except OSError as e:
        sys.stderr.write('grpn_py: could not save variables to %s: %s\n' % (path, e))
        try:
            os.remove(tmp)
        except OSError:
            pass


def main(argv=None):
    argv = sys.argv if argv is None else argv
    opts = parse_args(argv)
    if opts is None:
        return 1
    if QtWidgets is None:
        sys.stderr.write('grpn_py: PyQt5 is required for the GUI '
                         '(Debian/Ubuntu: sudo apt install python3-pyqt5).\n')
        return 1
    app = QtWidgets.QApplication(argv[:1])
    app.setApplicationName('grpn')
    calc = Calculator()
    font_size = load_settings(calc)
    load_variables(calc)
    calc.on_variables_edited = lambda: save_variables(calc)
    window = MainWindow(calc, font_size=font_size, **opts)
    window.show()
    status = app.exec_()
    save_settings(calc, window.lcd.font_size())
    return status


if __name__ == '__main__':
    sys.exit(main())
