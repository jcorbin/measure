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
from math import modf
import os
import re
import sys
from operator import attrgetter
from measure import *

# dirt-simple means computing script, doesn't know nuthin' 'bout confidence or credibility ;-)

def collection_stats(collection):
    yield 'samples', len(collection[0])
    for field, sample in zip(collection.fields, collection):
        sample = [x for x in sample if x is not None]
        if sample:
            mean = round(sum(sample) / len(sample), 2)
            if isinstance(mean, float):
                f, i = modf(mean)
                if f == 0:
                    mean = int(i)
        else:
            mean = None
        yield field, mean

def collection_report(collection):
    fields = list(collection_stats(collection))
    maxlen = max(len(label) for label, _ in fields)
    maxlen += 2
    return '\n'.join(
        (field + ':').ljust(maxlen) + str(mean)
        for field, mean in fields)

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

def run_collections(run):
    usage = usage_extractors
    results = result_extractors

    # TODO: support compressed output

    if not re.match('<.+>$', run.samplename):
        basedir = os.path.dirname(os.path.realpath(run.samplename))
        stdout_bytes = lambda r: os.path.join(basedir, r.stdout)
        stderr_bytes = lambda r: os.path.join(basedir, r.stderr)
    else:
        stdout_bytes = attrgetter('stdout')
        stderr_bytes = attrgetter('stderr')

    stdout_bytes = maybe_path_exists(
        compose(os.path.getsize, stdout_bytes))
    stderr_bytes = maybe_path_exists(
        compose(os.path.getsize, stderr_bytes))

    results += (
        Selector('stdout_bytes', stdout_bytes),
        Selector('stderr_bytes', stderr_bytes))

    results = Collector(*results)
    colls = [results]
    if getattr(run, 'hasusage', 'false').lower() == 'true':
        usage = Collector(*usage)
        colls.append(usage)
    else:
        usage = None
    nc = len(colls)
    for i, record in enumerate(run):
        colls[i % nc].add(record)
    return results, usage

class RunReport:
    def __init__(self, run):
        self.run = run
        self.results, self.usage = run_collections(run)

    def __str__(self):
        s = ''
        if self.usage is not None:
            s += '== Usage\n%s\n\n' % ollection_report(self.usage)
        s += '== Results\n%s' % collection_report(self.results)
        return s

import argparse
parser = argparse.ArgumentParser()
parser.add_argument('files', metavar='FILE',
    type=argparse.FileType('r'), nargs='*',
    help='Sample files to read, use STDIN if none given')
args = parser.parse_args()

runs = map(named_records.read, args.files)

for i, run in enumerate(runs):
    if i > 0:
        print()
    print(RunReport(run))
