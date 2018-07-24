#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/syscall.h>

static void usage(const char *argv0) {
  printf("usage: %s "
         "flock|lockf|fcntl_posix|fcntl_linux same_fd|dup_fd|two_fds threads|processes\n",
         argv0);
  exit(1);
}

static void message(const char *msg) {
  time_t ts;
  time(&ts);
  struct tm *tm = localtime(&ts);
  char buf[32] = {};
  strftime(buf, sizeof(buf), "%H:%M:%S", tm);
  printf("%s pid=%d tid=%d %s\n", buf, (int)getpid(), (int)syscall(SYS_gettid), msg);
}

static void delay() {
  usleep(1000000);
}

static void test_flock(int fd) {
  message("lock");
  if (flock(fd, LOCK_EX) == -1) {
    perror("flock");
    abort();
  }

  message("sleep");
  delay();

  message("unlock");
  if (flock(fd, LOCK_UN) == -1) {
    perror("flock");
    abort();
  }
}

static void test_lockf(int fd) {
  message("lock");
  if (lockf(fd, F_LOCK, 0) == -1) {
    perror("lockf");
    abort();
  }

  message("sleep");
  delay();

  message("unlock");
  if (lockf(fd, F_ULOCK, 0) == -1) {
    perror("lockf");
    abort();
  }
}

static void test_fcntl_posix(int fd) {
  message("lock");
  struct flock fl;
  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;
  if (fcntl(fd, F_SETLKW, &fl) == -1) {
    perror("fcntl");
    abort();
  }

  message("sleep");
  delay();

  message("unlock");
  fl.l_type = F_UNLCK;
  if (fcntl(fd, F_SETLK, &fl) == -1) {
    perror("fcntl");
    abort();
  }
}

static void test_fcntl_linux(int fd) {
  message("lock");
  struct flock fl;
  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;
  if (fcntl(fd, F_OFD_SETLKW, &fl) == -1) {
    perror("fcntl");
    abort();
  }

  message("sleep");
  delay();

  message("unlock");
  fl.l_type = F_UNLCK;
  if (fcntl(fd, F_OFD_SETLK, &fl) == -1) {
    perror("fcntl");
    abort();
  }
}

struct params {
  char **argv;
  int fd;
};

static void *test(void *arg) {
  struct params *p = (struct params *)arg;

  if (strcmp(p->argv[1], "flock") == 0) {
    test_flock(p->fd);
  } else if (strcmp(p->argv[1], "lockf") == 0) {
    test_lockf(p->fd);
  } else if (strcmp(p->argv[1], "fcntl_posix") == 0) {
    test_fcntl_posix(p->fd);
  } else if (strcmp(p->argv[1], "fcntl_linux") == 0) {
    test_fcntl_linux(p->fd);
  } else {
    usage(p->argv[0]);
  }
}

int main(int argc, char **argv) {
  if (argc != 4) {
    usage(argv[0]);
  }

  struct params p1;
  struct params p2;

  p1.argv = argv;
  p2.argv = argv;

  p1.fd = open("lockfile", O_RDWR | O_CREAT, 0666);

  if (strcmp(argv[2], "same_fd") == 0) {
    p2.fd = p1.fd;
  } else if (strcmp(argv[2], "dup_fd") == 0) {
    p2.fd = dup(p1.fd);
  } else if (strcmp(argv[2], "two_fds") == 0) {
    p2.fd = open("lockfile", O_RDWR);
  } else {
    usage(argv[0]);
  }

  pthread_t t;

  if (strcmp(argv[3], "threads") == 0) {
    int ret = pthread_create(&t, NULL, test, &p1);
    if (ret != 0) {
      abort();
    }
  } else if (strcmp(argv[3], "processes") == 0) {
    if (fork() == 0) {
      test(&p1);
      return 0;
    }
  } else {
    usage(argv[0]);
  }

  test(&p2);

  if (strcmp(argv[3], "threads") == 0) {
    pthread_join(t, NULL);
  } else {
    wait(NULL);
  }

  return 0;
}
