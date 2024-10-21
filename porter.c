#include "common.h"

#include "ConfParser.h"

#include "extio.h"

#include "porter.h"
#include "DaemonUtil.h"
#include "util.h"


// Config
int shouldCreateNewArrivalDirectory = 0;
char **dirsToDeliverTo = NULL;



// Info
u64 numOfDeliveries = 0;
u64 numOfCycles = 0;


// Directories
DIR *userDir = NULL;

char *configDirName = NULL;


// Booleans
volatile sig_atomic_t shouldQuit = 0;

void
cleanup() {
  if(paths != NULL) {
    free(paths);
    paths = NULL;
  }

  if(userDir != NULL) {
    closedir(userDir);
    userDir = NULL;
  }

  if(configDirName != NULL) {
    free(configDirName);
    configDirName = NULL;
  }

}


void
signalHandler(int sig)
{
  switch(sig)
    {
    case SIGTERM:
      cleanup();
      syslog(LOG_USER | LOG_INFO, "Terminating...");
      exit(EXIT_SUCCESS);

    case SIGINT:
      cleanup();
      exit(EXIT_SUCCESS);

    default:
      break;
    }
}



int findUserPaths(UserPaths *paths)
{
  char *homeDir;
  
  if((homeDir = getenv("HOME")) != NULL) {
    if(strlen(homeDir) + 2 <= sizeof(paths->Home)) {
      snprintf(paths->Home, sizeof(paths->Home), "%s/", homeDir);
    } else {
      fprintf(stderr, "Err: HOME directory path is too long.\n");
      return -1;
    }
  }

  strncpy(paths->Downloads, paths->Home, strlen(paths->Home) + 1);
  strncat(paths->Downloads, "Downloads/", 12);

  strncpy(paths->Music, paths->Home, strlen(paths->Home) + 1);
  strncat(paths->Music, "Music/", 8);
  
  strncpy(paths->Documents, paths->Home, strlen(paths->Home) + 1);
  strncat(paths->Documents, "Documents/", 12);


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

int
checkForPARTFiles(DIR *userDir)
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
pid_t
getPID()
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
	      fclose(fptr);
	      break;
	    }
	  }
	  fclose(fptr);
	}
      }
				
    }
    closedir(dir);
  }

  /* USE THIS INSTEAD of /comm, save to config and check config before creation
     char *pidLoc = (char*)malloc(sizeof(char) * (strlen(confDir) + 12));

     strcpy(pidLoc, confDir);
     strcat(pidLoc, "porterd.pid");

     FILE *fptr = fopen(pidLoc, "r");
     if(fptr) {
     char buf[256];

     if(fgets(buf, sizeof(buf), fptr)) {
     buf[strcspn(buf, "\n")] = 0;
     pid = atoi(buf);
     }
     fclose(fptr);
     } 


     free(pidLoc);*/
		
  return pid;
}

void
checkConfig()
{
  char *confFile;
  confFile = (char*)malloc(sizeof(char) * (strlen(configDirName) + 12));

  snprintf(confFile, sizeof(char) * (strlen(configDirName) + 12), "%sporter.conf", configDirName);

  if(fexists(confFile) == 0) {
    parseConfFile(confFile);
  }
  free(confFile);

}

void
startup()
{
  paths = (UserPaths*)malloc(sizeof(UserPaths));

  findUserPaths(paths);

  configDirName = (char*)malloc(sizeof(char) * (strlen(paths->Home) + 16));
  strcpy(configDirName, paths->Home);
  strcat(configDirName, ".config/porter/");

  mkdir(configDirName, 0700);


  char *confFile;
  confFile = (char*)malloc(sizeof(char) * (strlen(configDirName) + 12));

  snprintf(confFile, sizeof(char) * (strlen(configDirName) + 12), "%sporter.conf", configDirName);

  if(fexists(confFile) == 0) {
    parseConfFile(confFile);
  }
  free(confFile);
 
}

u8
parseCmdOptions(int argc, char **argv)
{

  struct option longOptions[] ={
    {"help", no_argument, 0, 'h'},
    {"daemon", no_argument, 0, 'd'},
    {"single", no_argument, 0, 's'},
    {"block", no_argument, 0, 'b'},
    {"logs", no_argument, 0, 'l'},
    {"kill", no_argument, 0, 'k'},
    {"version", no_argument, 0, 'v'},
    {0, 0, 0, 0}
  };

  int opt;
  int option_index = 0;
  u8 rc;
  u8 singleMode = 0;
  u8 daemonMode = 0;
  u8 blockMode = 0;
  u8 showLogs = 0;
  u8 showHelp = 0;
  u8 shouldKill = 0;

  while((opt = getopt_long(argc, argv, "khsdblv", longOptions, &option_index)) != -1)
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

	case 'k':
	  shouldKill = 1;
	  break;

	case 'v':
	  fprintf(stdout, "porter: version 0.1.0\n");
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

  if(shouldKill == 1) {
    pid_t pid;
    if((pid = getPID()) != -1) {
      if(kill(pid, SIGTERM) == 0) {
	fprintf(stdout, "Killed process: %d 🪦\n", pid);
	return 0;
      } else {
	perror("Error killing process");
	return 1;
      }
    } else {
      fprintf(stdout, "Nothing to do\n");
      return 0;
    }
  }

  if(showLogs == 1) {
    // Chill haha
    system("cat /var/log/syslog | grep PORTERD --text | less +G");
  }


  if(singleMode == 1 && daemonMode == 1) {
    fprintf(stderr, "Error: --standard and --daemon cannot be used together.\n");
    return 1;
  }
		
  // Blocking mode, really for testing purposes
  if(blockMode == 1) {
    
    startup();

    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if(sigaction(SIGINT, &sa, NULL) == -1) {
      perror("Error registering SIGINT handler");
      return 1;
    }

    
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
						
      sleep(1);

      printf("Checking ~/.config/porter/porter.conf for updates\n");
      checkConfig();
    }
  } 

  if(singleMode == 1) {

    startup();
    
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
      if(promptYesOrNo("Instance running, terminate and start anew?") == 0) {
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

							
    rc = transDaemon(0);
    if(rc) {
      openlog(LOGNAME, LOG_PID, LOG_USER);
      syslog(LOG_USER | LOG_ERR, "Error starting");
      closelog();
      return 1;
    }

    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if(sigaction(SIGTERM, &sa, NULL) == -1) {
      perror("Error registering SIGTERM handler");
      return 1;
    }

							
    // daemon begunth

    startup();
							
    openlog(LOGNAME, LOG_PID, LOG_USER);
    syslog(LOG_USER | LOG_INFO, "Starting process");

    while(!shouldQuit) {
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

      syslog(LOG_USER | LOG_INFO, "Checking ~/.config/porter/porter.conf for updates");
      checkConfig();
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

    fprintf(stdout, "Number of deliveries made: %ld\n", numOfDeliveries);
  }

  return 0;
}

int main(int argc, char **argv)
{



  parseCmdOptions(argc, argv);

  cleanup();
  
  return 0;
}
