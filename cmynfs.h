int cmynfs_open(char *fname);
int cmynfs_read(int fd, char buf[], int n);
int cmynfs_write(int fd, char buf[], int n);
int cmynfs_seek(int fd, int pos);
int cmynfs_close(int fd);