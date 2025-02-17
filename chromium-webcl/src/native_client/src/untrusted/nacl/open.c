/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>

#include "native_client/src/untrusted/nacl/nacl_irt.h"

int open(char const *pathname, int flags, ...) {
  int error;
  int fd;
  mode_t mode = 0;

  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }

  error = __libnacl_irt_filename.open(pathname, flags, mode, &fd);
  if (error) {
    errno = error;
    return -1;
  }

  return fd;
}
