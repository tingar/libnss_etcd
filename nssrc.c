/*
 * NSS plugin for looking up by extra nameservers
 */

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nss.h>
#include <netdb.h>
#include <arpa/inet.h>

#define ALIGN(a) (((a+sizeof(void*)-1)/sizeof(void*))*sizeof(void*))

/**
 * Packs the data from a name/value string into a hostent return
 * struct. "result" must be previously initialized.
 */
static void pack_hostent(/* OUT */ struct hostent *result, char *buffer,
    size_t buflen, const char *name, const void *addr) {
  char *aliases, *r_addr, *addrlist;
  size_t l, idx;

  /* we can't allocate any memory, the buffer is where we need to
   * return things we want to use
   *
   * 1st, the hostname */
  l = strlen(name);
  result->h_name = buffer;
  memcpy(result->h_name, name, l);
  buffer[l] = '\0';

  idx = ALIGN (l+1);

  /* 2nd, the empty aliases array */
  aliases = buffer + idx;
  *(char **) aliases = NULL;
  idx += sizeof (char*);

  result->h_aliases = (char **) aliases;

  result->h_addrtype = AF_INET;
  result->h_length = sizeof (struct in_addr);

  /* 3rd, address */
  r_addr = buffer + idx;
  inet_pton(AF_INET, addr, r_addr);
  idx += ALIGN(result->h_length);

  /* 4th, the addresses ptr array */
  addrlist = buffer + idx;
  ((char **) addrlist)[0] = r_addr;
  ((char **) addrlist)[1] = NULL;

  result->h_addr_list = (char **) addrlist;
}


/**
 * Resolves the hostname into an IP address. Not really re-entrant. This
 * function will be called multiple times by the GNU C library to get the
 * entire list of addresses.
 * This function spec is defined at http://www.gnu.org/software/libc/manual/html_node/NSS-Module-Function-Internals.html
 */
enum nss_status _nss_etcd_gethostbyname2_r (const char *name, int af,
    /* OUT */ struct hostent *result, char *buffer, size_t buflen,
    /* OUT */ int *errnop, /* OUT */ int *h_errnop) {

  int pid, pipes[2], rv;  /* For hardcore forking action later. */
  char addr[256];
  int last_err;  /* Just in case we need to perror(3). */
  char **args;

  /* Only IPv4 addresses make sense for this resolver. */
  if (af != AF_INET) {
    *errnop = EAFNOSUPPORT;
    *h_errnop = NO_DATA;
    return NSS_STATUS_UNAVAIL;
  }

  pipe(pipes);
  if (0 == (pid = fork())) {
    args = (char **) calloc(sizeof(char *), 4);
    if (args < 0) goto CHILD_ERR;  /** Dijkstra can suck it. */

    args[0] = (char *) calloc(sizeof(char), 8);
    strcpy(args[0], "etcdctl");
    args[1] = (char *) calloc(sizeof(char), 4);
    strcpy(args[1], "get");
    args[2] = (char *) calloc(sizeof(char), strlen(name) + 7);
    strcpy(args[2], "/hosts/");
    strcpy(args[2] + 7, name);
    args[3] = NULL;

    /* Child code */
    close(pipes[0]);
    close(0);
    close(2);
    dup2(pipes[1], 1);
    execvp("etcdctl", args);

    CHILD_ERR:
      last_err = errno;
      perror("etcdctl");
      exit(last_err);  /* Couldn't exec. */
  } else if (pid > 0) {
    /* Parent code */
    close(pipes[1]);

    if (0 > read(pipes[0], addr, 255)) {
      last_err = errno;
      perror("read");
      *errnop = last_err;
      *h_errnop = NO_DATA;
      return NSS_STATUS_NOTFOUND;
    } else {
      int len = strlen(addr);
      addr[len - 1] = '\0';
    }

    waitpid(pid, &rv, 0);

    if (rv) {
      /* Host wasn't found or etcdctl failed spectacularly. */
      *errnop = ENOENT;
      *h_errnop = HOST_NOT_FOUND;
      return NSS_STATUS_NOTFOUND;
    }
  } else {
    /* Error forking. */
    last_err = errno;
    perror("fork");
    *errnop = last_err;
    *h_errnop = NO_DATA;
    return NSS_STATUS_UNAVAIL;
  }

  pack_hostent(result, buffer, buflen, name, addr);

  return NSS_STATUS_SUCCESS;
}


/**
 * Resolves a given hostname. This function just piggybacks off the
 * re-entrant version.
 */
enum nss_status _nss_etcd_gethostbyname_r (const char *name,
    /* OUT */ struct hostent *result, char *buffer, size_t buflen,
    /* OUT */ int *errnop, /* OUT */ int *h_errnop) {
  return _nss_etcd_gethostbyname2_r(name, AF_INET, result, buffer, buflen,
    errnop, h_errnop);
}


/**
 * Handles the reverse name lookup. Not currently supported.
 */
enum nss_status _nss_etcd_gethostbyaddr_r (const void *addr, socklen_t len,
    int af, /* OUT */ struct hostent *result, char *buffer, size_t buflen,
    /* OUT */ int *errnop, /* OUT */ int *h_errnop) {
  if (af != AF_INET) {
    *errnop = EAFNOSUPPORT;
    *h_errnop = NO_DATA;
    return NSS_STATUS_UNAVAIL;
  }

  if (len != sizeof (struct in_addr)) {
    *errnop = EINVAL;
    *h_errnop = NO_RECOVERY;
    return NSS_STATUS_UNAVAIL;
  }

  *errnop = EAFNOSUPPORT;
  *h_errnop = NO_DATA;
  return NSS_STATUS_UNAVAIL;
}
