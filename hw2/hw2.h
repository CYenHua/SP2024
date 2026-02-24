#define PARENT_READ_FD 3
#define PARENT_WRITE_FD 4
#define MAX_CHILDREN 8
#define MAX_FIFO_NAME_LEN 9
#define MAX_FRIEND_INFO_LEN 12
#define MAX_FRIEND_NAME_LEN 9
#define MAX_CMD_LEN 200
#define MAX_CHECK_LEN 400
#define MAX_FIFO_CMD 1024
#include <sys/types.h>
typedef struct friend{
    pid_t pid;
    int read_fd;
    int write_fd;
    char info[MAX_FRIEND_INFO_LEN];
    char name[MAX_FRIEND_NAME_LEN];
    int value;
    struct friend *next;
} friend;
