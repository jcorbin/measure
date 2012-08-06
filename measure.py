# Copyright (C) 2012, Joshua T Corbin <jcorbin@wunjo.org>
#
# This file is part of measure, a program to measure programs.
#
# Measure is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Measure is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Measure.  If not, see <http://www.gnu.org/licenses/>.

import collections
import errno
import os
import re
from collections import namedtuple
from functools import wraps
from math import modf
from operator import attrgetter, itemgetter

__all__ = ('Run', 'Selector', 'Collector')

# TODO: docstrings? comments? examples?

class RecordMetaClass(type):
    def __new__(mcs, name, bases, dict):
        try:
            fields = dict['_fields']
        except KeyError:
            pass
        else:
            for i, field in enumerate(fields):
                dict[field] = property(itemgetter(i))
        return type.__new__(mcs, name, bases, dict)

from numbers import Number

def scalar_op(op):
    def scalarr(x, s):
        cls = x.__class__
        return cls(*(xi * s for xi in x))
    scalarr.__name__ += '_' + op.__name__
    return scalarr

class timeval(namedtuple('timeval', 's us')):
    def __add__(a, b):
        if isinstance(b, Number):
            s, us = a.s + b, a.us + b
        elif isinstance(b, timeval):
            s, us = a.s + b.s, a.us + b.us
        else:
            return NotImplemented
        while us > 10**6:
            s, us = s + 1, us - 10**6
        return timeval(s, us)

    __radd__ = __add__

    def __sub__(a, b):
        if isinstance(b, Number):
            s, us = a.s - b, a.us - b
        elif isinstance(b, timeval):
            s, us = a.s - b.s, a.us - b.us
        else:
            return NotImplemented
        while us < 0:
            s, us = s - 1, us + 10**6
        return timeval(s, us)

    def __rsub__(b, a):
        if isinstance(a, Number):
            s, us = a - b.s, a - b.us
        elif isinstance(b, timeval):
            s, us = a.s - b.s, a.us - b.us
        else:
            return NotImplemented
        while us < 0:
            s, us = s - 1, us + 10**6
        return timeval(s, us)

    def __mul__(a, b):
        if isinstance(b, Number):
            s, us = a.s * b, a.us * b
        else:
            return NotImplemented
        if us > 10**6:
            q, r = divmod(us, 10**6)
            s, us = s + q, r
        return timeval(s, us)

    __rmul__ = __mul__

    def __truediv__(a, b):
        if isinstance(b, Number):
            s, us = a.s / b, int(round(a.us / b))
        else:
            return NotImplemented
        f, i = modf(s)
        s = int(i)
        us += int(round(10**6 * f))
        if us > 10**6:
            q, r = divmod(us, 10**6)
            s, us = s + q, r
        return timeval(s, us)

    __floordiv__ = __truediv__

    def __pow__(a, b):
        if isinstance(b, Number):
            s, us = a.s ** b, a.us ** b
        else:
            return NotImplemented
        if us > 10**6:
            q, r = divmod(us, 10**6)
            s, us = s + q, r
        return timeval(s, us)

    def __round__(self, prec):
        return timeval(
            int(round(self.s,  prec)),
            int(round(self.us, prec)))

    def asint(self):
        return self.s * 10**6 + self.us

    def asfloat(self):
        return self.s + self.us / 10**6

    def __str__(self):
        return '%ds,%dus' % self

    re = re.compile(r'(\d+)s,(\d+)us')

    @classmethod
    def match(cls, value):
        m = cls.re.match(value)
        if m:
            return cls(*(map(int, m.groups())))

class timespec(namedtuple('timespec', 's ns')):
    def __add__(a, b):
        if isinstance(b, Number):
            s, ns = a.s + b, a.ns + b
        elif isinstance(b, timespec):
            s, ns = a.s + b.s, a.ns + b.ns
        else:
            return NotImplemented
        while ns > 10**9:
            s, ns = s + 1, ns - 10**9
        return timespec(s, ns)

    __radd__ = __add__

    def __sub__(a, b):
        if isinstance(b, Number):
            s, ns = a.s - b, a.ns - b
        elif isinstance(b, timespec):
            s, ns = a.s - b.s, a.ns - b.ns
        else:
            return NotImplemented
        while ns < 0:
            s, ns = s - 1, ns + 10**9
        return timespec(s, ns)

    def __rsub__(b, a):
        if isinstance(a, Number):
            s, ns = a - b.s, a - b.ns
        elif isinstance(b, timespec):
            s, ns = a.s - b.s, a.ns - b.ns
        else:
            return NotImplemented
        while ns < 0:
            s, ns = s - 1, ns + 10**9
        return timespec(s, ns)

    def __mul__(a, b):
        if isinstance(b, Number):
            s, ns = a.s * b, a.ns * b
        else:
            return NotImplemented
        if ns > 10**9:
            q, r = divmod(ns, 10**9)
            s, ns = s + q, r
        return timespec(s, ns)

    __rmul__ = __mul__

    def __truediv__(a, b):
        if isinstance(b, Number):
            s, ns = a.s / b, int(round(a.ns / b))
        else:
            return NotImplemented
        f, i = modf(s)
        s = int(i)
        ns += int(round(10**9 * f))
        if ns > 10**9:
            q, r = divmod(ns, 10**9)
            s, ns = s + q, r
        return timespec(s, ns)

    __floordiv__ = __truediv__

    def __pow__(a, b):
        if isinstance(b, Number):
            s, ns = a.s ** b, a.ns ** b
        else:
            return NotImplemented
        if ns > 10**9:
            q, r = divmod(ns, 10**9)
            s, ns = s + q, r
        return timespec(s, ns)

    def __round__(self, prec):
        return timespec(
            int(round(self.s,  prec)),
            int(round(self.ns, prec)))

    def asint(self):
        return self.s * 10**9 + self.ns

    def asfloat(self):
        return self.s + self.ns / 10**9

    def __str__(self):
        return '%ds,%dns' % self

    re = re.compile(r'(\d+)s,(\d+)ns')

    @classmethod
    def match(cls, value):
        m = cls.re.match(value)
        if m:
            return cls(*(map(int, m.groups())))

rdigit = re.compile(r'\d+$')
def match_digit(value):
    m = rdigit.match(value)
    if m:
        return int(m.group(0))

value_matchers = (match_digit, timeval.match, timespec.match)

def parse_value(value):
    for matcher in value_matchers:
        match = matcher(value)
        if match is not None:
            return match
    return value

class Record(tuple, metaclass=RecordMetaClass):
    def __new__(cls, record):
        if isinstance(record, str):
            record = record.split()
        try:
            fields = cls._fields
        except AttributeError:
            pass
        else:
            if len(record) != len(fields):
                raise ValueError("Expecting %d fields got %d" %
                    (len(fields), len(record)))
        record = map(parse_value, record)
        return tuple.__new__(cls, record)

    def __repr__(self):
        try:
            fields = self._fields
        except AttributeError:
            args = map(repr, self)
        else:
            args = ('%s=%r' % pair for pair in zip(fields, self))
        return self.__class__.__name__ + '(' + ', '.join(args) + ')'

def create_record_class(fields):
    if isinstance(fields, str):
        fields = fields.split()
    fields = tuple(fields)
    class NamedRecord(Record):
        _fields = fields
    return NamedRecord

class named_records(object):
    def __init__(self, lines, initial_line=None):
        if initial_line is None:
            initial_line = next(lines)
        self.record_class = create_record_class(initial_line)
        self.lines = lines

    @property
    def fields(self):
        return self.record_class._fields

    def __iter__(self):
        return self

    def __next__(self):
        return self.record_class(next(self.lines))

def maybe_path_exists(f):
    @wraps(f)
    def wrapper(x):
        try:
            return f(x)
        except OSError as e:
            if e.errno == errno.ENOENT:
                return None
            raise
    return wrapper

compose = lambda f, g: lambda x: f(g(x))

class Run(named_records):
    def __init__(self, lines):
        self.runinfo = {}
        self.runinfo['samplename'] = lines.name
        for line in lines:
            if '=' not in line: break
            line = line.rstrip('\r\n')
            key, val = line.split('=', 1)
            armatch = re.match(r'(\w+)\[(\d+)\]$', key)
            if armatch:
                key, i = armatch.groups()
                i = int(i)
                ar = self.runinfo.setdefault(key, [])
                while len(ar) < i+1:
                    ar.append(None)
                ar[i] = val
            else:
                self.runinfo[key] = val
        super(Run, self).__init__(lines, initial_line=line)

    def __getattr__(self, name):
        try:
            return self.runinfo[name]
        except KeyError:
            pass
        raise AttributeError('no %s in %s' % (
            name, self.__class__.__name__))

    def results(self):
        # TODO: support compressed output

        if not re.match('<.+>$', self.samplename):
            basedir = os.path.dirname(os.path.realpath(self.samplename))
            stdout_bytes = lambda r: os.path.join(basedir, r.stdout)
            stderr_bytes = lambda r: os.path.join(basedir, r.stderr)
        else:
            stdout_bytes = attrgetter('stdout')
            stderr_bytes = attrgetter('stderr')

        stdout_bytes = maybe_path_exists(
            compose(os.path.getsize, stdout_bytes))
        stderr_bytes = maybe_path_exists(
            compose(os.path.getsize, stderr_bytes))

        results = Collector(
            Selector('wallclock', lambda r: (r.end - r.start)),
            Selector('cputime', lambda r: (r.utime - r.stime)),
            Selector('maxrss'),
            Selector('minflt'),
            Selector('majflt'),
            Selector('nswap'),
            Selector('inblock'),
            Selector('oublock'),
            Selector('nvcsw'),
            Selector('nivcsw'),
            Selector('stdout_bytes', stdout_bytes),
            Selector('stderr_bytes', stderr_bytes))
        for record in self:
            results.add(record)
        return results

class Selector(object):
    def __init__(self, name, f=None):
        self.name = name
        if f is None:
            f = name
        if isinstance(f, str):
            f = attrgetter(f)
        self.f = f

    def __call__(self, r):
        return self.f(r)

class Collector(tuple):
    def __new__(cls, *selectors, container=list):
        self = super(Collector, cls).__new__(cls, (
            container() for field in selectors))
        self.fields = tuple(s.name for s in selectors)
        self.selectors = selectors
        return self

    def __getattr__(self, name):
        try:
            i = self.fields.index(name)
        except ValueError:
            i = None
        else:
            return self[i]
        raise AttributeError('no %s in %s' % (
            name, self.__class__.__name__))

    def add(self, value):
        for sample, selector in zip(self, self.selectors):
            sample.append(selector(value))

if __name__ == '__main__':
    from pprint import pprint
    import sys
    records = Run(sys.stdin)
    pprint(records.runinfo)
    pprint(list(records))
