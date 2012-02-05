/* Copyright (C) 2012, Joshua T Corbin <jcorbin@wunjo.org>
 *
 * This file is part of measure, a program to measure programs.
 *
 * Measure is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Measure is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Measure.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _CHILD_H
#define _CHILD_H

#include "program.h"

void child_run(struct program_result *res, int commfd);

#endif // _CHILD_H
