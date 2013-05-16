int mynfs_open(char *fname);
int mynfs_read(int fd, char buf[], int n);
int mynfs_write(int fd, char buf[], int n);
int mynfs_version(int fd);
int mynfs_seek(int fd, int pos);
int mynfs_close(int fd);
int mynfs_get_pos(int fp);

