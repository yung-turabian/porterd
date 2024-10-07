#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <fcntl.h>
#include <cstdlib>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <cstdio>
#include <cstring>
#include <cerrno>

#include "porterd.h"
#include "daemon_util.h"


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

  if(pid < 0) //error
    exit(EXIT_FAILURE);

  if(pid > 0) //parent terminate
    exit(EXIT_SUCCESS);



  if(setsid() < 0) // become leader of new session
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
  char strver[20];
  
  if(fexists(newpath) == 0) {
    strcpy(new_name, newpath);
    while(fexists(new_name) == 0) {
      strcpy(new_name, newpath); // reset
      sprintf(strver, "(%d) ", ++copy_num);
      strcat(new_name, strver);
    }

      if(copy_num != 0) {
      	fprintf(stdout, "Renaming file: %s\n", new_name);
	strcpy(newpath, new_name);
      }
  }
  
  if(rename(oldpath, newpath) == 0)
    return 0;
  else {
    perror("fmove() error\n");
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


DIR *userDir;
struct dirent *userDirEntry;

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
  while((userDirEntry = readdir(userDir)) != NULL) {
    if(strstr(userDirEntry->d_name, ".part") != NULL) {
      return 0;
    }
  }
  return -1;
}

int runScan(DIR *dirObj, UserPaths *paths, const char *dir)
{
  if((dirObj = opendir(dir)) == NULL) {
    perror("opendir() error");
    return 1;
  }
  
  moveToMusicDir(dirObj, paths);
  
  fprintf(stdout,"Delivered! \n");
  
  // cleanup
  closedir(dirObj);
  return 0;
}

int main(int argc, char **argv)
{

  UserPaths *paths = (UserPaths*)malloc(sizeof(UserPaths));

  findUserPaths(paths);

  // Single run
  if(argc > 1 && strcmp(argv[1], "-s") == 0 && strcmp(argv[1], "--single")) {
    runScan(userDir, paths, paths->Downloads);

    return 0;
  }
  
  int rc;
  const char *LOGNAME = "PORTERD";
  
  rc = transDaemon(0);
  if(rc) {
    syslog(LOG_USER | LOG_ERR, "error starting");
    closelog();
    return EXIT_FAILURE;
  }
  
  // daemon begunth
  
  openlog(LOGNAME, LOG_PID, LOG_USER);
  syslog(LOG_USER | LOG_INFO, "starting");

  while(1) {
    sleep(60);
    syslog(LOG_USER | LOG_INFO, "running a scan on ~/Downloads");
    if(checkForPARTFiles(userDir) != 0)
      runScan(userDir, paths, paths->Downloads);
    
  }

  
  return 0;
}
