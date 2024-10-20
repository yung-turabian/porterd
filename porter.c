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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>

#include "porter.h"
#include "daemon_util.h"
#include "util.h"

#define CMD_BUFFER_SIZE 128
#define PID_BUFFER_SIZE 16

typedef uint64_t u64;

u64 numOfDeliveries = 0;

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
  int rc;
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
		
	// sets daemon name, as opposed to exe name
	rc = prctl(PR_SET_NAME, "porterd", 0, 0, 0);
  if(rc != 0) {
			perror("prctl()");
			exit(1);
	}


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
		struct dirent *entry;
		rewinddir(userDir);

  while((entry = readdir(userDir)) != NULL) {
    if(strstr(entry->d_name, ".mp3") != NULL) {
      fprintf(stdout, "[%s]\n", entry->d_name);

      char oldpath[100];
      char newpath[100];
      
      strcpy(oldpath, paths->Downloads);
      strcat(oldpath, entry->d_name);

      strcpy(newpath, paths->Music);

      strcat(newpath, entry->d_name);

      fmove(oldpath, newpath);
			numOfDeliveries++;
      
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


// Return -1 on failure, gets the PID of the 'porterd' daemon
pid_t getPID()
{
		DIR *dir;
		struct dirent *entry;
		pid_t pid = -1;

		if((dir = opendir("/proc")) != NULL) {
				while((entry = readdir(dir)) != NULL) {
						if(isNum(entry->d_name)) {
								char path[512];
								snprintf(path, sizeof(path), "/proc/%s/comm", entry->d_name);

								FILE *fptr = fopen(path, "r");
								if(fptr) {
										char comm[256];

										if(fgets(comm, sizeof(comm), fptr)) {
												comm[strcspn(comm, "\n")] = 0;

												if(strcmp(comm, "porterd") == 0) {
														pid = atoi(entry->d_name);
														break;
												}
										}
										fclose(fptr);
								}
						}
				
				}
				closedir(dir);
		}
		
		return pid;
}

int8_t parseCmdOptions(int argc, char **argv)
{

		struct option longOptions[] ={
				{"help", no_argument, 0, 'h'},
				{"daemon", no_argument, 0, 'd'},
				{"single", no_argument, 0, 's'},
				{"block", no_argument, 0, 'b'},
				{"logs", no_argument, 0, 'l'},
				{0, 0, 0, 0}
		};

		int opt;
		int option_index = 0;
		int rc;
		int singleMode, daemonMode, blockMode = 0;
		int showLogs, showHelp = 0;

		while((opt = getopt_long(argc, argv, "hsdbl", longOptions, &option_index)) != -1)
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
						
						case 'b':
								blockMode = 1;
								break;

						case 'l':
								showLogs = 1;
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

		if(showLogs == 1) {
				// Chill haha
				system("cat /var/log/syslog | grep PORTERD | less +G");
		}


		if(singleMode == 1 && daemonMode == 1) {
				fprintf(stderr, "Error: --standard and --daemon cannot be used together.\n");
        return 1;
		}
		
		// Blocking mode, really for testing purposes
		if(blockMode == 1) {
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
				
				pid_t pid;

				if((pid = getPID()) != -1) {
						if(promptYesOrNo("Instance running, terminate and start anew?")) {
								int rc = kill(pid, SIGTERM);
								if(rc == 0) {
										fprintf(stdout, "Killed process 🪦\n");
								} else {
										perror("Error killing process");
								}
						} else {
								return 0;
						}
				}

				fprintf(stdout, "Starting daemon `porterd` 😈\n");

				const char *LOGNAME = "PORTERD";
							
				rc = transDaemon(0);
				if(rc) {
						syslog(LOG_USER | LOG_ERR, "Error starting");
						closelog();
						return 1;
				}
							
				// daemon begunth
							
				openlog(LOGNAME, LOG_PID, LOG_USER);
				syslog(LOG_USER | LOG_INFO, "Starting process");

				while(1) {
						syslog(LOG_USER | LOG_INFO, "Running a scan on ~/Downloads");

						if(userDir == NULL) {
								if((userDir = opendir(paths->Downloads)) == NULL) {
										perror("opendir() error when attempt to open Downloads");
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

		} else {

				if(getPID() == -1) {
						fprintf(stdout, "Nothing to show :) [try running `porterd -d`]\n");
						return 0;
				}
				
				char pid[30];
				snprintf(pid, sizeof(pid), "%d", getPID());
				
				
				char ps_cmd[CMD_BUFFER_SIZE];
				snprintf(ps_cmd, sizeof(ps_cmd), "ps -p %s -o pid,ppid,cmd,%%mem,%%cpu,start_time", pid);

				FILE *ps_output = popen(ps_cmd, "r");
				if(!ps_output) {
						perror("popen");    
						return 1;
				}

				char line[CMD_BUFFER_SIZE];
				while(fgets(line, sizeof(line), ps_output)) {
						fprintf(stdout, "%s", line);
				}
				fflush(stdout);
				pclose(ps_output);



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
