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

#define child_die(mess) exit( \
    child_comm_send_mess(run->comm[1], mess) < 0 \
    ? CHILD_EXIT_COMMERROR : 1)
