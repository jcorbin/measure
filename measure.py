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
import re
from collections import namedtuple
from operator import add, sub, mul, floordiv, truediv, attrgetter, itemgetter

__all__ = ('named_records', 'Selector', 'Collector')

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

def componentwise_op(op):
    def componentwise(a, b):
        cls = a.__class__
        if not isinstance(b, cls):
            if isinstance(b, Number):
                return cls(*(op(ai, b) for ai in a))
            else:
                return NotImplemented
        return cls(*map(op, a, b))
    componentwise.__name__ += '_' + op.__name__
    return componentwise

def componentwise_rop(op):
    def componentwise(a, b):
        cls = a.__class__
        if not isinstance(b, cls):
            if isinstance(b, Number):
                return cls(*(op(b, ai) for ai in a))
            else:
                return NotImplemented
        return cls(*map(op, b, a))
    componentwise.__name__ += '_' + op.__name__
    return componentwise

def scalar_op(op):
    def scalarr(x, s):
        cls = x.__class__
        return cls(*(xi * s for xi in x))
    scalarr.__name__ += '_' + op.__name__
    return scalarr

class timeval(namedtuple('timeval', 's us')):
    __add__ = componentwise_op(add)
    __radd__ = componentwise_rop(add)
    __sub__ = componentwise_op(sub)
    __rsub__ = componentwise_op(sub)
    __mul__ = scalar_op(mul)
    __pow__ = scalar_op(pow)
    __truediv__ = scalar_op(truediv)
    __floordiv__ = scalar_op(floordiv)

    def __round__(self, prec):
        return timeval(*(round(i, prec) for i in self))

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
    __add__ = componentwise_op(add)
    __radd__ = componentwise_rop(add)
    __sub__ = componentwise_op(sub)
    __rsub__ = componentwise_op(sub)
    __mul__ = scalar_op(mul)
    __pow__ = scalar_op(pow)
    __truediv__ = scalar_op(truediv)
    __floordiv__ = scalar_op(floordiv)

    def __round__(self, prec):
        return timeval(*(round(i, prec) for i in self))

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
    @classmethod
    def read(cls, lines):
        runinfo = {
            'samplename': lines.name
        }
        for line in lines:
            if '=' not in line: break
            line = line.rstrip('\r\n')
            key, val = line.split('=', 1)
            armatch = re.match(r'(\w+)\[(\d+)\]$', key)
            if armatch:
                key, i = armatch.groups()
                i = int(i)
                ar = runinfo.setdefault(key, [])
                while len(ar) < i+1:
                    ar.append(None)
                ar[i] = val
            else:
                runinfo[key] = val

        self = cls(lines, initial_line=line)
        self.runinfo = runinfo
        return self

    def __init__(self, lines, initial_line=None):
        if initial_line is None:
            initial_line = next(lines)
        self.record_class = create_record_class(initial_line)
        self.lines = lines

    def __getattr__(self, name):
        try:
            return self.runinfo[name]
        except KeyError:
            pass
        raise AttributeError('no %s in %s' % (
            name, self.__class__.__name__))

    @property
    def fields(self):
        return self.record_class._fields

    def __iter__(self):
        return self

    def __next__(self):
        return self.record_class(next(self.lines))

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
    records = named_records.read(sys.stdin)
    pprint(records.runinfo)
    pprint(list(records))
