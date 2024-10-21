#include "DaemonUtil.h"

#include "common.h"

static int
closeNonStdFds(void)
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


static int
resetSignalHandlers(void)
{
#if defined _NSIG
  
  for(unsigned int i = 1; i < _NSIG; i++) {
    if(i != SIGKILL && i != SIGSTOP && i != SIGTERM)
      signal(i, SIG_DFL);
  }
#endif

  return 0;
}

static int
clearSignalMask(void)
{
  sigset_t sset;

  return ((sigemptyset(&sset) == 0)
	  && (sigprocmask(SIG_SETMASK, &sset, NULL) == 0));
}

static int
attachStdFdsToNull(void)
{
  int null_fd_read, null_fd_write;

  return(((null_fd_read = open("/dev/null", O_RDONLY)) != -1)
	 && (dup2(null_fd_read, STDIN_FILENO) != -1)
	 && ((null_fd_write = open("/dev/null", O_WRONLY)) != -1)
	 && (dup2(null_fd_write, STDOUT_FILENO) != -1)
	 && (dup2(null_fd_write, STDERR_FILENO) != -1));
}

static int
createPIDFile(const char *pid_file)
{
  pid_t my_pid = getpid();
  char my_pid_str[10];
  int fd;

  sprintf(my_pid_str, "%d", my_pid);

  if((fd = open(pid_file, O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR)) == -1) {
    perror("open pid");
    return -1;
  }

  if(write(fd, my_pid_str, strlen(my_pid_str)) == -1) {
    perror("write pid");
    return -1;
  }

  close(fd);

  return 0;
}

int
transDaemon(int flags)
{
  int rc;
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

  attachStdFdsToNull();


  if(!(flags & BD_NO_UMASK0))
    umask(0);

  if(!(flags & BD_NO_CHDIR))
    chdir("/");

  // HAVE TO MOVE WHERE STARTUP occurs
  /*char *pidLoc = (char*)malloc(sizeof(char) * (strlen(configDirName) + 12));

  strcpy(pidLoc, configDirName);
  strcat(pidLoc, "porterd.pid");

  createPIDFile(pidLoc);

  free(pidLoc);*/

  return 0;
}

