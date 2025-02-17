/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/load_file.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_copy.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_handlers.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/tests/minnacl/minimal_test_syscalls.h"

static int32_t NotImplementedDecoder(struct NaClAppThread *natp) {
  NaClCopyDropLock(natp->nap);
  printf("Error: entered an unexpected syscall!\n");
  fflush(stdout);
  _exit(1);
}

static int32_t MySyscallInvoke(struct NaClAppThread *natp) {
  NaClCopyDropLock(natp->nap);
  printf("Inside custom test 'invoke' syscall\n");
  fflush(stdout);
  /* Return a value that the test guest program checks for. */
  return 123;
}

static int32_t MySyscallExit(struct NaClAppThread *natp) {
  NaClCopyDropLock(natp->nap);
  printf("Inside custom test 'exit' syscall\n");
  fflush(stdout);
  _exit(0);
}

int main(int argc, char **argv) {
  struct NaClApp app;
  struct NaClApp *nap = &app;
  struct NaClSyscallTableEntry syscall_table[NACL_MAX_SYSCALLS] = {{0}};
  int index;
  int use_separate_thread = 0;

  NaClHandleBootstrapArgs(&argc, &argv);

  for (index = 0; index < NACL_MAX_SYSCALLS; index++) {
    syscall_table[index].handler = NotImplementedDecoder;
  }
  syscall_table[TEST_SYSCALL_INVOKE].handler = MySyscallInvoke;
  syscall_table[TEST_SYSCALL_EXIT].handler = MySyscallExit;

  if (argc >= 2 && strcmp(argv[1], "--use_separate_thread") == 0) {
    use_separate_thread = 1;
    argc--;
    argv++;
  }
  if (argc != 2) {
    NaClLog(LOG_FATAL, "Expected 1 argument: executable filename\n");
  }

  NaClAllModulesInit();

  CHECK(NaClAppWithSyscallTableCtor(&app, syscall_table));
  CHECK(NaClAppLoadFileFromFilename(&app, argv[1]) == LOAD_OK);
  CHECK(NaClAppPrepareToLaunch(&app) == LOAD_OK);

  /* These are examples of two different ways to run untrusted code. */
  if (use_separate_thread) {
    /* Create a new host thread that is managed by NaCl. */
    CHECK(NaClCreateMainThread(&app, 0, NULL, NULL));
    NaClWaitForMainThreadToExit(&app);

    NaClLog(LOG_FATAL, "The exit syscall is not supposed to be callable\n");
  } else {
    /* Reuse the existing host thread for running untrusted code. */
    struct NaClAppThread *natp;
    uintptr_t stack_ptr = NaClGetInitialStackTop(nap);
    /* Ensure the stack pointer is suitably aligned. */
    stack_ptr &= ~NACL_STACK_ALIGN_MASK;
    stack_ptr -= NACL_STACK_PAD_BELOW_ALIGN;
    /* TODO(mseaborn): Make this interface more straightforward to use. */
    stack_ptr = NaClSysToUserStackAddr(nap, NaClUserToSys(nap, stack_ptr));

    natp = NaClAppThreadMake(nap, nap->initial_entry_pt, stack_ptr, 0, 0);
    CHECK(natp != NULL);
    NaClAppThreadLauncher(natp);

    NaClLog(LOG_FATAL, "NaClThreadLauncher() should not return\n");
  }

  return 1;
}
