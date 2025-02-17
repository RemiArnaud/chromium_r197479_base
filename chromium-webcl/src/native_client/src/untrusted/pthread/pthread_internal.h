/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_PTHREAD_PTHREAD_INTERNAL_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_PTHREAD_PTHREAD_INTERNAL_H_ 1

#include "native_client/src/untrusted/irt/irt.h"

struct nc_combined_tdb;

extern struct nacl_irt_mutex __nc_irt_mutex;
extern struct nacl_irt_cond __nc_irt_cond;
extern struct nacl_irt_sem __nc_irt_sem;

extern int __nc_thread_initialized;

void __nc_initialize_globals(void);

void __nc_initialize_unjoinable_thread(struct nc_combined_tdb *tdb);

void __nc_initialize_interfaces(struct nacl_irt_thread *irt_thread);

void __nc_tsd_exit(void);

#endif
