#ifndef COMMON_H
#define COMMON_H

#define CMD_BUFFER_SIZE 128
#define PID_BUFFER_SIZE 16

#define LOGNAME "PORTERD"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>

typedef uint64_t u64;
typedef uint8_t u8;

extern char *configDirName;



#endif
