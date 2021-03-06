#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "../bgzf.h"
#include "block.h"
#include "queue.h"
#include "reader.h"
#include "consumer.h"
#include "writer.h"
#include "util.h"
#include "pbgzf.h"

int 
write_open(const char *fn, int is_forced)
{
  int fd = -1;
  char c;
  if (!is_forced) {
      if ((fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC | O_EXCL, 0666)) < 0 && errno == EEXIST) {
          fprintf(stderr, "[bgzip] %s already exists; do you wish to overwrite (y or n)? ", fn);
          scanf("%c", &c);
          if (c != 'Y' && c != 'y') {
              fprintf(stderr, "[bgzip] not overwritten\n");
              exit(1);
          }
      }
  }
  if (fd < 0) {
      if ((fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) {
          fprintf(stderr, "[bgzip] %s: Fail to write\n", fn);
          exit(1);
      }
  }
  return fd;
}

static int 
pbgzip_main_usage()
{
  fprintf(stderr, "\n");
  fprintf(stderr, "Usage:   pbgzip [options] [file] ...\n\n");
  fprintf(stderr, "Options: -c        write on standard output, keep original files unchanged\n");
  fprintf(stderr, "         -d        decompress\n");
  fprintf(stderr, "         -f        overwrite files without asking\n");
  fprintf(stderr, "         -n INT    number of threads [%d]\n", detect_cpus());
#ifndef DISABLE_BZ2
  fprintf(stderr, "         -t INT    the compress type (0 - gz, 1 - bz2) [%d]\n", 0);
#endif
  fprintf(stderr, "         -1 .. -9  the compression level [%d]\n", Z_DEFAULT_COMPRESSION);
  fprintf(stderr, "         -h        give this help\n");
  fprintf(stderr, "\n");
  return 1;
}


int
main(int argc, char *argv[])
{
  int opt, f_src, f_dst;
  int32_t compress, compress_level, compress_type, pstdout, is_forced, queue_size, n_threads;

  compress = 1; compress_level = -1; compress_type = 0;
  pstdout = 0; is_forced = 0; queue_size = 1000; n_threads = detect_cpus();
#ifndef DISABLE_BZ2
  while((opt = getopt(argc, argv, "cdhfn:t:q:0123456789")) >= 0){
#else
  while((opt = getopt(argc, argv, "cdhfn:q:0123456789")) >= 0){
#endif
      if('0' <= opt && opt <= '9') {
          compress_level = opt - '0'; 
          continue;
      }
      switch(opt){
        case 'd': compress = 0; break;
        case 'c': pstdout = 1; break;
        case 'f': is_forced = 1; break;
        case 'q': queue_size = atoi(optarg); break;
        case 'n': n_threads = atoi(optarg); break;
#ifndef DISABLE_BZ2
        case 't': compress_type = atoi(optarg); break;
#endif
        case 'h': 
        default:
                  return pbgzip_main_usage();
      }
  }

  if(argc <= 1) return pbgzip_main_usage();

  if(pstdout) {
      f_dst = fileno(stdout);
  }
  else {
      if(1 == compress) {
          char *name = malloc(strlen(argv[optind]) + 5);
          strcpy(name, argv[optind]);
#ifndef DISABLE_BZ2
          if(0 == compress_type) strcat(name, ".gz");
          else strcat(name, ".bz2");
#else
          strcat(name, ".gz");
#endif
          f_dst = write_open(name, is_forced);
          free(name);
          if (f_dst < 0) return 1;
      }
      else {
          char *name = strdup(argv[optind]);
#ifndef DISABLE_BZ2
          if(0 == compress_type) {
              if(strlen(name) < 3 || 0 != strcmp(name + (strlen(name)-3), ".gz")) {
                  fprintf(stderr, "Error: the input file did not end in .gz");
                  return 1;
              }
              name[strlen(name) - 3] = '\0';
          }
          else {
              if(strlen(name) < 4 || 0 != strcmp(name + (strlen(name)-4), ".bz2")) {
                  fprintf(stderr, "Error: the input file did not end in .bz2");
                  return 1;
              }
              name[strlen(name) - 4] = '\0';
          }
#else 
          if(strlen(name) < 3 || 0 != strcmp(name + (strlen(name)-3), ".gz")) {
              fprintf(stderr, "Error: the input file did not end in .gz");
              return 1;
          }
          name[strlen(name) - 3] = '\0';
#endif
          f_dst = write_open(name, is_forced);
          free(name);
      }
  }

  if ((f_src = open(argv[optind], O_RDONLY)) < 0) {
      fprintf(stderr, "[pbgzip] %s: %s\n", strerror(errno), argv[optind]);
      return 1;
  }

  pbgzf_main(f_src, f_dst, compress, compress_level, compress_type, queue_size, n_threads);

  if(!pstdout) unlink(argv[optind]);

  return 0;
}
