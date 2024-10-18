#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "porterd.h"
#include "daemon_util.h"

DIR *userDir;


void signalHandler(int sig)
{
		switch(sig)
		{
				case SIGINT:
						if(paths != NULL)
								free(paths);

						if(userDir != NULL)
								closedir(userDir);

						exit(EXIT_SUCCESS);

				default:
						break;
		}
}


static int closeNonStdFds(void)
{
  unsigned int i;

  struct rlimit rlim;
  int num_of_fds = getrlimit(RLIMIT_NOFILE, &rlim);

  if(num_of_fds == -1)
    return 1;

  for(i = 3; i < num_of_fds; i++)
    close(i);

  return 0;
}

static int resetSignalHandlers(void)
{
  #if defined _NSIG
  
  for(unsigned int i = 1; i < _NSIG; i++) {
    if(i != SIGKILL && i != SIGSTOP)
      signal(i, SIG_DFL);
  }
  #endif

  return 0;
}

static int clearSignalMask(void)
{
  sigset_t sset;

  return ((sigemptyset(&sset) == 0)
	  && (sigprocmask(SIG_SETMASK, &sset, NULL) == 0));
}
/*
static int attachStdFdsToNull(void)
{
  int null_fd_read, null_fd_write;

  return(((null_fd_read = open(NULL_DEV_FILE, O_RDONY)) != -1)
	 && (dup2(null_fd_read, STDIN_FILENO) != -1)
	 && ((null_fd_writer = open(NULL_DEV_FILE, O_WRONLY)) != -1)
	 && (dup2(null_fd_write, STDOUT_FILENO) != -1)
	 && (dup2(null_fd_write, STDERR_FILENO) != -1));
	 }*/

static int createPIDFile(const char *pid_file)
{
  pid_t my_pid = getpid();
  char my_pid_str[10];
  int fd;

  sprintf(my_pid_str, "%d", my_pid);

  if((fd = open(pid_file, O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR)) == -1)
    return -1;

  if(write(fd, my_pid_str, strlen(my_pid_str)) == -1)
    return -1;

  close(fd);

  return 0;
}

int transDaemon(int flags)
{
  int maxfd, fd;
  pid_t pid;

  closeNonStdFds();

  resetSignalHandlers();

  clearSignalMask();


  // santize enviroment block

  pid = fork();

  if(pid == -1) //error
    exit(EXIT_FAILURE);
  else if(pid != 0) //parent terminate
    exit(EXIT_SUCCESS);

  if(setsid() == -1) // become leader of new session
    exit(EXIT_FAILURE);

  pid = fork();

  if(pid < 0) //error
    exit(EXIT_FAILURE);

  if(pid > 0) //parent terminate
    exit(EXIT_SUCCESS);



  if(!(flags & BD_NO_UMASK0))
    umask(0);

  if(!(flags & BD_NO_CHDIR))
    chdir("/");

  createPIDFile("/run/porterd.pid");


  if(!(flags & BD_NO_CLOSE_FILES)) {
    maxfd = sysconf(_SC_OPEN_MAX);
    if(maxfd == -1)
      maxfd = BD_MAX_CLOSE;
    for(fd = 0; fd < maxfd; fd++)
      close(fd);
  }

  if(!(flags & BD_NO_REOPEN_STD_FDS)) {
    //going dark
    close(STDIN_FILENO);

    fd = open("/dev/null", O_RDWR);
    if(fd != STDIN_FILENO)
      return -1;
    if(dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
      return -2;
    if(dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
      return -3;
  }

  return 0;
}

int fmove(const char *oldpath, char *newpath)
{
  char new_name[100];
  int copy_num = 0;
  
  if(fexists(newpath) == 0) {
    strcpy(new_name, newpath);
    while(fexists(new_name) == 0) {
		  strcpy(new_name, newpath);
      sprintf(new_name + strlen(new_name), " (%d)", ++copy_num);
    }

      if(copy_num != 0) {
      	fprintf(stdout, "Renaming file: '%s' -> '%s'\n", oldpath, new_name);
				strcpy(newpath, new_name);
      }
  }
  
  if(rename(oldpath, newpath) == 0) {
    return 0;
	} else {
    perror("fmove() error on attempts to rename");
    return -1;
  }
}

int fexists(const char *file)
{
  if(access(file, F_OK) == 0) {
    return 0;
  } else {
    return -1;
  }
}




int findUserPaths(UserPaths *paths)
{
  char *homeDir;
  
  if((homeDir = getenv("HOME")) != NULL) {
    strcpy(paths->Home, homeDir);
  }

  strncpy(paths->Downloads, paths->Home, strlen(paths->Home) + 1);
  strncat(paths->Downloads, "/Downloads/", 12);

  strncpy(paths->Music, paths->Home, strlen(paths->Home) + 1);
  strncat(paths->Music, "/Music/", 8);
  
  strncpy(paths->Documents, paths->Home, strlen(paths->Home) + 1);
  strncat(paths->Documents, "/Documents/", 12);


  return 0;
}

int moveToMusicDir(DIR *userDir, UserPaths *paths)
{
		struct dirent *userDirEntry;
		rewinddir(userDir);

  while((userDirEntry = readdir(userDir)) != NULL) {
    if(strstr(userDirEntry->d_name, ".mp3") != NULL) {
      fprintf(stdout, "[%s]\n", userDirEntry->d_name);

      char oldpath[100];
      char newpath[100];
      
      strcpy(oldpath, paths->Downloads);
      strcat(oldpath, userDirEntry->d_name);

      strcpy(newpath, paths->Music);

      strcat(newpath, userDirEntry->d_name);

      fmove(oldpath, newpath);
      
      }
  }

  return 0;
}

int checkForPARTFiles(DIR *userDir)
{
		struct dirent *userDirEntry;

		rewinddir(userDir);

  while((userDirEntry = readdir(userDir)) != NULL) {
    if(strstr(userDirEntry->d_name, ".part") != NULL) {
      return 0;
    }
  }
  return -1;
}

int runScan(DIR *dirObj, UserPaths *paths, const char *dir)
{
  moveToMusicDir(dirObj, paths);
  
  return 0;
}

int8_t parseCmdOptions(int argc, char **argv)
{

		struct option longOptions[] ={
				{"help", no_argument, 0, 'h'},
				{"daemon", no_argument, 0, 'd'},
				{"single", no_argument, 0, 's'},
				{0, 0, 0, 0}
		};

		int opt;
		int option_index = 0;
		int rc;
		int singleMode, daemonMode, showHelp = 0;

		while((opt = getopt_long(argc, argv, "hsd", longOptions, &option_index)) != -1)
		{
				switch(opt)
				{
						case 'h':
								showHelp = 1;
								break;

						case 's':
								singleMode = 1;
								break;

						case 'd':
								daemonMode = 1;
								break;

						case '?':
								return 1;

						default:
								fprintf(stderr, "Error parsing options\n");
								return 1;
				}
		}

		if(showHelp == 1) {
				printHelp();
				return 0;
		}

		if(singleMode == 1 && daemonMode == 1) {
				fprintf(stderr, "Error: --standard and --daemon cannot be used together.\n");
        return 1;
		}

		if(singleMode == 1) {
				if(userDir == NULL) {
						if((userDir = opendir(paths->Downloads)) == NULL) {
								perror("opendir() error");
								return 1;
						}

				}

				if(checkForPARTFiles(userDir) != 0) {
						runScan(userDir, paths, paths->Downloads);
				}
		} else if(daemonMode == 1) {
				const char *LOGNAME = "PORTERD";
							
				rc = transDaemon(0);
				if(rc) {
						syslog(LOG_USER | LOG_ERR, "error starting");
						closelog();
						return 1;
				}
							
				// daemon begunth
							
				openlog(LOGNAME, LOG_PID, LOG_USER);
				syslog(LOG_USER | LOG_INFO, "starting");

				while(1) {
						syslog(LOG_USER | LOG_INFO, "running a scan on ~/Downloads");

						if(userDir == NULL) {
								if((userDir = opendir(paths->Downloads)) == NULL) {
										perror("opendir() error when attempt to open Dowloads");
										return 1;
								}
									
						}

						if(checkForPARTFiles(userDir) != 0) {
									runScan(userDir, paths, paths->Downloads);
									syslog(LOG_USER | LOG_ERR, "Delivered files:");
						}
						else {
									syslog(LOG_USER | LOG_ERR, "Waiting for files to finish downloading");
						}

								sleep(60);
				}

		} else { // Standard mode, really for testing purposes
				while(1) {
						//fprintf(stdout, "running a scan on ~/Downloads\n");

						if(userDir == NULL) {
								if((userDir = opendir(paths->Downloads)) == NULL) {
										perror("opendir() error");
										return 1;
								}
									
						}
						

						if(checkForPARTFiles(userDir) != 0) {
								runScan(userDir, paths, paths->Downloads);
								//fprintf(stdout, "delivered\n");
						}
						else {
								fprintf(stdout, "waiting for files to finish downloading\n");
								sleep(1);
						}
						
						//sleep(1);
				}

		}

		return 0;
}

int main(int argc, char **argv)
{

		signal(SIGINT, signalHandler);

  paths = (UserPaths*)malloc(sizeof(UserPaths));

  findUserPaths(paths);

  parseCmdOptions(argc, argv);

	free(paths);
	closedir(userDir);
  
  return 0;
}
