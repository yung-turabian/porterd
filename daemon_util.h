#ifndef DAEMON_UTIL_H
#define DAEMON_UTIL_H

#define BD_NO_CHDIR           01 // dont chdir("/")
#define BD_NO_CLOSE_FILES     02 // dont close all open files
#define BD_NO_REOPEN_STD_FDS  04 // dont reopen stdin/out/err

#define BD_NO_UMASK0          010 // dont do umask(0)
#define BD_MAX_CLOSE          8192 // max fds

// 0 on success; -1 on error
int transDaemon(int flags);

#endif
