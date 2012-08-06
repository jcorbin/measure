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

from math import modf
from measure import *

# dirt-simple means computing script, doesn't know nuthin' 'bout confidence or credibility ;-)

def run_stats(run):
    collection = run.results()
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

import argparse
parser = argparse.ArgumentParser()
parser.add_argument('--table', '-t', action='store_true',
    help='Output in space-delimited table format')
parser.add_argument('files', metavar='FILE',
    type=argparse.FileType('r'), nargs='*',
    help='Sample files to read, use STDIN if none given')
args = parser.parse_args()

runs = map(Run, args.files)

if args.table:
    fields = None
    for i, run in enumerate(runs):
        runfields, stats = zip(*run_stats(run))
        if i == 0:
            fields = runfields
            print('samplename', *fields)
        else:
            assert runfields == fields
        print(run.samplename, *stats)

else:
    for i, run in enumerate(runs):
        if i > 0:
            print()
        print('== Results')
        fields = list(run_stats(run))
        maxlen = max(len(label) for label, _ in fields) + 2
        for field, mean in fields:
            print((field + ':').ljust(maxlen) + str(mean))
