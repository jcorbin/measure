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

import errno
from functools import wraps
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
            sample = [x for x in sample if x is not None]
            if sample:
                mean = round(sum(sample) / len(sample), 2)
            else:
                mean = None
            yield field, mean

    def __str__(self):
        s = ''
        fields = list(self.fields())
        maxlen = max(len(label) for label, _ in fields)
        maxlen += 2
        for field, mean in fields:
            s += (field + ':').ljust(maxlen)
            if isinstance(mean, float):
                s += str(mean).rstrip('.0') or '0'
            else:
                s += str(mean)
            s += '\n'
        return s.rstrip('\n')

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

class RunReport:

    usage_extractors = (
        Selector('cputime', lambda r: (r.utime - r.stime)),
        Selector('maxrss'),
        Selector('minflt'),
        Selector('majflt'),
        Selector('nswap'),
        Selector('inblock'),
        Selector('oublock'),
        Selector('nvcsw'),
        Selector('nivcsw'))

    result_extractors = (
        Selector('wallclock', lambda r: (r.end - r.start)),
    ) + usage_extractors

    results = None
    usage = None

    def __init__(self, run):
        self.run = run

        result_extractors = self.result_extractors
        result_extractors += (
            Selector('stdout_bytes', maybe_path_exists(lambda r: os.path.getsize(r.stdout))),
            Selector('stderr_bytes', maybe_path_exists(lambda r: os.path.getsize(r.stderr))))

        self.results = Collector(*result_extractors)
        colls = [self.results,]
        if getattr(self.run, 'hasusage', 'false').lower() == 'true':
            self.usage = Collector(*self.usage_extractors)
            colls.append(self.usage)
        nc = len(colls)
        for i, record in enumerate(self.run):
            colls[i % nc].add(record)

    def __str__(self):
        s = ''
        if self.usage is not None:
            s += '== Usage\n%s\n\n' % Report(self.usage)
        s += '== Results\n%s' % Report(self.results)
        return s

print(RunReport(named_records.read(sys.stdin)))
