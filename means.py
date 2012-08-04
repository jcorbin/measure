#!/usr/bin/python
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

import os
import sys
from operator import attrgetter
from measure import *

# dirt-simple means computing script, doesn't know nuthin' 'bout confidence or credibility ;-)

class Report:
    def __init__(self, collection):
        self.collection = collection

    def fields(self):
        yield 'Sample size', len(self.collection[0])
        for field, sample in zip(self.collection.fields, self.collection):
            mean = round(sum(sample) / len(sample), 2)
            mean = str(mean).rstrip('.0') or '0'
            yield field, mean

    def __str__(self):
        s = ''
        fields = list(self.fields())
        maxlen = max(len(label) for label, _ in fields)
        fmt = '%% %ds: %%s' % maxlen
        for pair in fields:
            s += fmt % pair + '\n'
        return s

usage_collector = Collector(
    Selector('cputime', lambda r: (r.utime - r.stime).asint()),
    Selector('maxrss'),
    Selector('minflt'),
    Selector('majflt'),
    Selector('nswap'),
    Selector('inblock'),
    Selector('oublock'),
    Selector('nvcsw'),
    Selector('nivcsw'))

result_collector = Collector(
    Selector('wallclock', lambda r: (r.end - r.start).asint()),
    Selector('cputime', lambda r: (r.utime - r.stime).asint()),
    Selector('maxrss'),
    Selector('minflt'),
    Selector('majflt'),
    Selector('nswap'),
    Selector('inblock'),
    Selector('oublock'),
    Selector('nvcsw'),
    Selector('nivcsw'),
    Selector('stdout_bytes', lambda r: os.path.getsize(r.stdout)),
    Selector('stderr_bytes', lambda r: os.path.getsize(r.stderr)))

records = named_records.read(sys.stdin)

has_usage = False # if --usage was used
if has_usage:
    colls = (usage_collector, result_collector)
    for i, record in enumerate(records):
        colls[i % 1].add(record)
    for label, ss in zip(('Usage', 'Results'), colls):
        print('==', label)
        print(Report(ss))
        print()
else:
    for record in records:
        result_collector.add(record)
    print('== Results')
    print(Report(result_collector))
