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

struct child_comm {
    unsigned char id;
    size_t len;
    const void *data;
};

int child_comm_send(int fd, const void *buf, size_t len);

int child_comm_recv(int fd, void *buf, size_t len);

int child_comm_write(int fd, const struct child_comm *comm);

#define COMM_READ_ERR_HEADER -1
#define COMM_READ_ERR_MALLOC -2
#define COMM_READ_ERR_DATA   -3

int child_comm_read(int fd, struct child_comm *comm);

#define CHILD_COMM_ID_MESS 0x01
#define CHILD_COMM_ID_STARTTIME 0x02
#define CHILD_COMM_ID_FILEPATH 0x03

int child_comm_send_mess(int fd, const char *mess);

int child_comm_send_filepath(int fd, const char *name, const char *path);

#define CHILD_EXIT_COMMERROR 0xff
