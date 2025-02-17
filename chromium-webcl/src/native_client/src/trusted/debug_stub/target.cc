/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <algorithm>

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/debug_stub/abi.h"
#include "native_client/src/trusted/debug_stub/packet.h"
#include "native_client/src/trusted/debug_stub/platform.h"
#include "native_client/src/trusted/debug_stub/session.h"
#include "native_client/src/trusted/debug_stub/target.h"
#include "native_client/src/trusted/debug_stub/thread.h"
#include "native_client/src/trusted/debug_stub/util.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/thread_suspension.h"

#if NACL_WINDOWS
#define snprintf sprintf_s
#endif

using std::string;

using port::IPlatform;
using port::IThread;
using port::MutexLock;

namespace gdb_rsp {


Target::Target(struct NaClApp *nap, const Abi* abi)
  : nap_(nap),
    abi_(abi),
    session_(NULL),
    initial_breakpoint_addr_(0),
    ctx_(NULL),
    cur_signal_(0),
    sig_thread_(0),
    reg_thread_(0),
    step_over_breakpoint_thread_(0) {
  if (NULL == abi_) abi_ = Abi::Get();
}

Target::~Target() {
  Destroy();
}

bool Target::Init() {
  string targ_xml = "l<target><architecture>";

  targ_xml += abi_->GetName();
  targ_xml += "</architecture><osabi>NaCl</osabi>";
  targ_xml += abi_->GetTargetXml();
  targ_xml += "</target>";

  // Set a more specific result which won't change.
  properties_["target.xml"] = targ_xml;
  properties_["Supported"] =
    "PacketSize=1000;qXfer:features:read+";

  NaClXMutexCtor(&mutex_);
  ctx_ = new uint8_t[abi_->GetContextSize()];

  if (NULL == ctx_) {
    Destroy();
    return false;
  }
  initial_breakpoint_addr_ = (uint32_t) nap_->initial_entry_pt;
  if (!AddBreakpoint(initial_breakpoint_addr_))
    return false;
  return true;
}

void Target::Destroy() {
  NaClMutexDtor(&mutex_);

  delete[] ctx_;
}

bool Target::AddBreakpoint(uint32_t user_address) {
  const Abi::BPDef *bp = abi_->GetBreakpointDef();

  // If we already have a breakpoint here then don't add it
  if (breakpoint_map_.find(user_address) != breakpoint_map_.end())
    return false;

  uintptr_t sysaddr = NaClUserToSysAddrRange(nap_, user_address, bp->size_);
  if (sysaddr == kNaClBadAddress)
    return false;
  // We allow setting breakpoints in the code area but not the data area.
  if (user_address + bp->size_ > nap_->dynamic_text_end)
    return false;

  // We add the breakpoint by overwriting the start of an instruction
  // with a breakpoint instruction.  (At least, we assume that we have
  // been given the address of the start of an instruction.)  In order
  // to be able to remove the breakpoint later, we save a copy of the
  // locations we are overwriting into breakpoint_map_.
  uint8_t *data = new uint8_t[bp->size_];
  if (NULL == data)
    return false;

  // Copy the old code from here
  if (!IPlatform::GetMemory(sysaddr, bp->size_, data)) {
    delete[] data;
    return false;
  }
  if (!IPlatform::SetMemory(nap_, sysaddr, bp->size_, bp->code_)) {
    delete[] data;
    return false;
  }

  breakpoint_map_[user_address] = data;
  return true;
}

bool Target::RemoveBreakpoint(uint32_t user_address) {
  const Abi::BPDef *bp_def = abi_->GetBreakpointDef();

  BreakpointMap_t::iterator iter = breakpoint_map_.find(user_address);
  if (iter == breakpoint_map_.end())
    return false;

  uintptr_t sysaddr = NaClUserToSys(nap_, user_address);
  uint8_t *data = iter->second;
  // Copy back the old code, and free the data
  if (!IPlatform::SetMemory(nap_, sysaddr, bp_def->size_, data)) {
    NaClLog(LOG_ERROR, "Failed to undo breakpoint.\n");
    return false;
  }
  delete[] data;
  breakpoint_map_.erase(iter);
  return true;
}

void Target::CopyFaultSignalFromAppThread(IThread *thread) {
  if (thread->GetFaultSignal() == 0 && thread->HasThreadFaulted()) {
    int signal =
        IThread::ExceptionToSignal(thread->GetAppThread()->fault_signal);
    // If a thread hits a breakpoint, we want to ensure that it is
    // reported as SIGTRAP rather than SIGSEGV.  This is necessary
    // because we use HLT (which produces SIGSEGV) rather than the
    // more usual INT3 (which produces SIGTRAP) on x86, in order to
    // work around a Mac OS X bug.
    //
    // We need to check each thread to see whether it hit a
    // breakpoint.  We record this on the thread object because:
    //  * We need to check the threads before accepting any commands
    //    from GDB which might remove breakpoints from
    //    breakpoint_map_, which would remove our ability to tell
    //    whether a thread hit a breakpoint.
    //  * Although we deliver fault events to GDB one by one, we might
    //    have multiple threads that have hit breakpoints.
    if (signal == NACL_ABI_SIGSEGV) {
      // Casting to uint32_t is necessary to drop the top 32 bits of
      // %rip on x86-64.
      uint32_t prog_ctr = (uint32_t) thread->GetContext()->prog_ctr;
      if (breakpoint_map_.find(prog_ctr) != breakpoint_map_.end()) {
        signal = NACL_ABI_SIGTRAP;
      }
    }
    thread->SetFaultSignal(signal);
  }
}

void Target::RemoveInitialBreakpoint() {
  if (initial_breakpoint_addr_ != 0) {
    if (!RemoveBreakpoint(initial_breakpoint_addr_)) {
      NaClLog(LOG_FATAL,
              "RemoveInitialBreakpoint: Failed to remove breakpoint\n");
    }
    initial_breakpoint_addr_ = 0;
  }
}

// When the debugger reads memory, we want to report the original
// memory contents without the modifications we made to add
// breakpoints.  This function undoes the modifications from a copy of
// memory.
void Target::EraseBreakpointsFromCopyOfMemory(uint32_t user_address,
                                              uint8_t *data, uint32_t size) {
  uint32_t user_end = user_address + size;
  const Abi::BPDef *bp = abi_->GetBreakpointDef();
  for (BreakpointMap_t::iterator iter = breakpoint_map_.begin();
       iter != breakpoint_map_.end();
       ++iter) {
    uint32_t breakpoint_address = iter->first;
    uint32_t breakpoint_end = breakpoint_address + bp->size_;
    uint8_t *original_data = iter->second;

    uint32_t overlap_start = std::max(user_address, breakpoint_address);
    uint32_t overlap_end = std::min(user_end, breakpoint_end);
    if (overlap_start < overlap_end) {
      uint8_t *dest = data + (overlap_start - user_address);
      uint8_t *src = original_data + (overlap_start - breakpoint_address);
      size_t copy_size = overlap_end - overlap_start;
      // Sanity check: do some bounds checks.
      CHECK(data <= dest && dest + copy_size <= data + size);
      CHECK(original_data <= src
            && src + copy_size <= original_data + bp->size_);
      memcpy(dest, src, copy_size);
    }
  }
}

void Target::Run(Session *ses) {
  bool initial_breakpoint_seen = false;
  NaClXMutexLock(&mutex_);
  session_ = ses;
  NaClXMutexUnlock(&mutex_);
  do {
    bool ignore_input_from_gdb = step_over_breakpoint_thread_ != 0 ||
        !initial_breakpoint_seen;
    ses->WaitForDebugStubEvent(nap_, ignore_input_from_gdb);

    // Lock to prevent anyone else from modifying threads
    // or updating the signal information.
    MutexLock lock(&mutex_);
    Packet recv, reply;

    if (step_over_breakpoint_thread_ != 0) {
      // We are waiting for a specific thread to fault while all other
      // threads are suspended.  Note that faulted_thread_count might
      // be >1, because multiple threads can fault simultaneously
      // before the debug stub gets a chance to suspend all threads.
      // This is why we must check the status of a specific thread --
      // we cannot call UnqueueAnyFaultedThread() and expect it to
      // return step_over_breakpoint_thread_.
      IThread *thread = threads_[step_over_breakpoint_thread_];
      if (!thread->HasThreadFaulted()) {
        // The thread has not faulted.  Nothing to do, so try again.
        // Note that we do not respond to input from GDB while in this
        // state.
        // TODO(mseaborn): We should allow GDB to interrupt execution.
        continue;
      }
      // All threads but one are already suspended.  We only need to
      // suspend the single thread that we allowed to run.
      thread->SuspendThread();
      CopyFaultSignalFromAppThread(thread);
      cur_signal_ = thread->GetFaultSignal();
      thread->UnqueueFaultedThread();
      sig_thread_ = step_over_breakpoint_thread_;
      reg_thread_ = step_over_breakpoint_thread_;
      step_over_breakpoint_thread_ = 0;
    } else if (nap_->faulted_thread_count != 0) {
      // At least one untrusted thread has got an exception.  First we
      // need to ensure that all threads are suspended.  Then we can
      // retrieve a thread from the set of faulted threads.
      SuspendAllThreads();
      UnqueueAnyFaultedThread(&sig_thread_, &cur_signal_);
      reg_thread_ = sig_thread_;
    } else {
      // Otherwise look for messages from GDB.  To fix a potential
      // race condition, we don't do this on the first run, because in
      // that case we are waiting for the initial breakpoint to be
      // reached.  We don't want GDB to observe states where the
      // (internal) initial breakpoint is still registered or where
      // the initial thread is suspended in NaClStartThreadInApp()
      // before executing its first untrusted instruction.
      if (!initial_breakpoint_seen || !ses->IsDataAvailable()) {
        // No input from GDB.  Nothing to do, so try again.
        continue;
      }
      // GDB should have tried to interrupt the target.
      // See http://sourceware.org/gdb/current/onlinedocs/gdb/Interrupts.html
      // TODO(eaeltsin): should we verify the interrupt sequence?

      // Indicate we have no current thread.
      // TODO(eaeltsin): or pick any thread? Add a test.
      // See http://code.google.com/p/nativeclient/issues/detail?id=2743
      sig_thread_ = 0;
      SuspendAllThreads();
    }

    if (sig_thread_ != 0) {
      // Reset single stepping.
      threads_[sig_thread_]->SetStep(false);
      RemoveInitialBreakpoint();
    }

    // Next update the current thread info
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "QC%x", sig_thread_);
    properties_["C"] = tmp;

    if (!initial_breakpoint_seen) {
      // First time on a connection, we don't send the signal
      initial_breakpoint_seen = true;
    } else {
      // All other times, send the signal that triggered us
      Packet pktOut;
      SetStopReply(&pktOut);
      ses->SendPacketOnly(&pktOut);
    }

    // Now we are ready to process commands
    // Loop through packets until we process a continue
    // packet.
    do {
      if (ses->GetPacket(&recv)) {
        reply.Clear();
        if (ProcessPacket(&recv, &reply)) {
          // If this is a continue command, break out of this loop
          break;
        } else {
          // Othwerise send the reponse
          ses->SendPacket(&reply);
        }
      }
    } while (ses->Connected());

    // Reset the signal value
    cur_signal_ = 0;

    // TODO(eaeltsin): it might make sense to resume signaled thread before
    // others, though it is not required by GDB docs.
    if (step_over_breakpoint_thread_ == 0) {
      ResumeAllThreads();
    } else {
      // Resume one thread while leaving all others suspended.
      threads_[step_over_breakpoint_thread_]->ResumeThread();
    }

    // Continue running until the connection is lost.
  } while (ses->Connected());
  NaClXMutexLock(&mutex_);
  session_ = NULL;
  NaClXMutexUnlock(&mutex_);
}


void Target::SetStopReply(Packet *pktOut) const {
  pktOut->AddRawChar('T');
  pktOut->AddWord8(cur_signal_);

  // gdbserver handles GDB interrupt by sending SIGINT to the debuggee, thus
  // GDB interrupt is also a case of a signalled thread.
  // At the moment we handle GDB interrupt differently, without using a signal,
  // so in this case sig_thread_ is 0.
  // This might seem weird to GDB, so at least avoid reporting tid 0.
  // TODO(eaeltsin): http://code.google.com/p/nativeclient/issues/detail?id=2743
  if (sig_thread_ != 0) {
    // Add 'thread:<tid>;' pair. Note terminating ';' is required.
    pktOut->AddString("thread:");
    pktOut->AddNumberSep(sig_thread_, ';');
  }
}


bool Target::GetFirstThreadId(uint32_t *id) {
  threadItr_ = threads_.begin();
  return GetNextThreadId(id);
}

bool Target::GetNextThreadId(uint32_t *id) {
  if (threadItr_ == threads_.end()) return false;

  *id = (*threadItr_).first;
  threadItr_++;

  return true;
}


uint64_t Target::AdjustUserAddr(uint64_t addr) {
  // On x86-64, GDB sometimes uses memory addresses with the %r15
  // sandbox base included, so we must accept these addresses.
  // TODO(eaeltsin): Fix GDB to not use addresses with %r15 added.
  if (NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64 &&
      NaClIsUserAddr(nap_, (uintptr_t) addr)) {
    return NaClSysToUser(nap_, (uintptr_t) addr);
  }
  // Otherwise, we expect an untrusted address.
  return addr;
}

bool Target::ProcessPacket(Packet* pktIn, Packet* pktOut) {
  char cmd;
  int32_t seq = -1;
  ErrDef  err = NONE;

  // Clear the outbound message
  pktOut->Clear();

  // Pull out the sequence.
  pktIn->GetSequence(&seq);
  if (seq != -1) pktOut->SetSequence(seq);

  // Find the command
  pktIn->GetRawChar(&cmd);

  switch (cmd) {
    // IN : $?
    // OUT: $Sxx
    case '?':
      SetStopReply(pktOut);
      break;

    case 'c':
      return true;

    // IN : $d
    // OUT: -NONE-
    case 'd':
      Detach();
      break;

    // IN : $g
    // OUT: $xx...xx
    case 'g': {
      IThread *thread = GetRegThread();
      if (NULL == thread) {
        err = BAD_ARGS;
        break;
      }

      // Copy OS preserved registers to GDB payload
      for (uint32_t a = 0; a < abi_->GetRegisterCount(); a++) {
        const Abi::RegDef *def = abi_->GetRegisterDef(a);
        thread->GetRegister(a, &ctx_[def->offset_], def->bytes_);
      }

      pktOut->AddBlock(ctx_, abi_->GetContextSize());
      break;
    }

    // IN : $Gxx..xx
    // OUT: $OK
    case 'G': {
      IThread *thread = GetRegThread();
      if (NULL == thread) {
        err = BAD_ARGS;
        break;
      }

      pktIn->GetBlock(ctx_, abi_->GetContextSize());

      // GDB payload to OS registers
      for (uint32_t a = 0; a < abi_->GetRegisterCount(); a++) {
        const Abi::RegDef *def = abi_->GetRegisterDef(a);
        thread->SetRegister(a, &ctx_[def->offset_], def->bytes_);
      }

      pktOut->AddString("OK");
      break;
    }

    // IN : $H(c/g)(-1,0,xxxx)
    // OUT: $OK
    case 'H': {
        char type;
        uint64_t id;

        if (!pktIn->GetRawChar(&type)) {
          err = BAD_FORMAT;
          break;
        }
        if (!pktIn->GetNumberSep(&id, 0)) {
          err = BAD_FORMAT;
          break;
        }

        if (threads_.begin() == threads_.end()) {
            err = BAD_ARGS;
            break;
        }

        // If we are using "any" get the first thread
        if (id == static_cast<uint64_t>(-1)) id = threads_.begin()->first;

        // Verify that we have the thread
        if (threads_.find(static_cast<uint32_t>(id)) == threads_.end()) {
          err = BAD_ARGS;
          break;
        }

        pktOut->AddString("OK");
        switch (type) {
          case 'g':
            reg_thread_ = static_cast<uint32_t>(id);
            break;

          case 'c':
            // 'c' is deprecated in favor of vCont.
          default:
            err = BAD_ARGS;
            break;
        }
        break;
      }

    // IN : $maaaa,llll
    // OUT: $xx..xx
    case 'm': {
        uint64_t user_addr;
        uint64_t wlen;
        uint32_t len;
        if (!pktIn->GetNumberSep(&user_addr, 0)) {
          err = BAD_FORMAT;
          break;
        }
        if (!pktIn->GetNumberSep(&wlen, 0)) {
          err = BAD_FORMAT;
          break;
        }
        user_addr = AdjustUserAddr(user_addr);
        uint64_t sys_addr = NaClUserToSysAddrRange(nap_, (uintptr_t) user_addr,
                                                   (size_t) wlen);
        if (sys_addr == kNaClBadAddress) {
          err = FAILED;
          break;
        }

        len = static_cast<uint32_t>(wlen);
        nacl::scoped_array<uint8_t> block(new uint8_t[len]);
        if (!port::IPlatform::GetMemory(sys_addr, len, block.get())) {
          err = FAILED;
          break;
        }
        EraseBreakpointsFromCopyOfMemory((uint32_t) user_addr,
                                         block.get(), len);

        pktOut->AddBlock(block.get(), len);
        break;
      }

    // IN : $Maaaa,llll:xx..xx
    // OUT: $OK
    case 'M':  {
        uint64_t user_addr;
        uint64_t wlen;
        uint32_t len;

        if (!pktIn->GetNumberSep(&user_addr, 0)) {
          err = BAD_FORMAT;
          break;
        }
        if (!pktIn->GetNumberSep(&wlen, 0)) {
          err = BAD_FORMAT;
          break;
        }
        user_addr = AdjustUserAddr(user_addr);
        uint64_t sys_addr = NaClUserToSysAddrRange(nap_, (uintptr_t) user_addr,
                                                   (size_t) wlen);
        if (sys_addr == kNaClBadAddress) {
          err = FAILED;
          break;
        }
        len = static_cast<uint32_t>(wlen);
        // We disallow the debugger from modifying code.
        if (user_addr < nap_->dynamic_text_end) {
          err = FAILED;
          break;
        }

        nacl::scoped_array<uint8_t> block(new uint8_t[len]);
        pktIn->GetBlock(block.get(), len);

        if (!port::IPlatform::SetMemory(nap_, sys_addr, len, block.get())) {
          err = FAILED;
          break;
        }

        pktOut->AddString("OK");
        break;
      }

    case 'q': {
      string tmp;
      const char *str = &pktIn->GetPayload()[1];
      stringvec toks = StringSplit(str, ":;");
      PropertyMap_t::const_iterator itr = properties_.find(toks[0]);

      // If this is a thread query
      if (!strcmp(str, "fThreadInfo") || !strcmp(str, "sThreadInfo")) {
        uint32_t curr;
        bool more = false;
        if (str[0] == 'f') {
          more = GetFirstThreadId(&curr);
        } else {
          more = GetNextThreadId(&curr);
        }

        if (!more) {
          pktOut->AddString("l");
        } else {
          pktOut->AddString("m");
          pktOut->AddNumberSep(curr, 0);
        }
        break;
      }

      // Check for architecture query
      tmp = "Xfer:features:read:target.xml";
      if (!strncmp(str, tmp.data(), tmp.length())) {
        stringvec args = StringSplit(&str[tmp.length()+1], ",");
        if (args.size() != 2) break;

        const char *out = properties_["target.xml"].data();
        int offs = strtol(args[0].data(), NULL, 16);
        int max  = strtol(args[1].data(), NULL, 16) + offs;
        int len  = static_cast<int>(strlen(out));

        if (max >= len) max = len;

        while (offs < max) {
          pktOut->AddRawChar(out[offs]);
          offs++;
        }
        break;
      }

      // Check the property cache
      if (itr != properties_.end()) {
        pktOut->AddString(itr->second.data());
      }
      break;
    }

    case 's': {
      IThread *thread = GetRunThread();
      if (thread) thread->SetStep(true);
      return true;
    }

    case 'T': {
      uint64_t id;
      if (!pktIn->GetNumberSep(&id, 0)) {
        err = BAD_FORMAT;
        break;
      }

      if (GetThread(static_cast<uint32_t>(id)) == NULL) {
        err = BAD_ARGS;
        break;
      }

      pktOut->AddString("OK");
      break;
    }

    case 'v': {
      const char *str = pktIn->GetPayload() + 1;

      if (strncmp(str, "Cont", 4) == 0) {
        // vCont
        const char *subcommand = str + 4;

        if (strcmp(subcommand, "?") == 0) {
          // Report supported vCont actions. These 4 are required.
          pktOut->AddString("vCont;s;S;c;C");
          break;
        }

        if (strcmp(subcommand, ";c") == 0) {
          // Continue all threads.
          return true;
        }

        if (strncmp(subcommand, ";s:", 3) == 0) {
          // Single step one thread and optionally continue all other threads.
          char *end;
          uint32_t thread_id = static_cast<uint32_t>(
              strtol(subcommand + 3, &end, 16));
          if (end == subcommand + 3) {
            err = BAD_ARGS;
            break;
          }

          ThreadMap_t::iterator it = threads_.find(thread_id);
          if (it == threads_.end()) {
            err = BAD_ARGS;
            break;
          }

          if (*end == 0) {
            // Single step one thread and keep other threads stopped.
            // GDB uses this to continue from a breakpoint, which works by:
            // - replacing trap instruction with the original instruction;
            // - single-stepping through the original instruction. Other threads
            //   must remain stopped, otherwise they might execute the code at
            //   the same address and thus miss the breakpoint;
            // - replacing the original instruction with trap instruction;
            // - continuing all threads;
            if (thread_id != sig_thread_) {
              err = BAD_ARGS;
              break;
            }
            step_over_breakpoint_thread_ = sig_thread_;
          } else if (strcmp(end, ";c") == 0) {
            // Single step one thread and continue all other threads.
          } else {
            // Unsupported combination of single step and other args.
            err = BAD_ARGS;
            break;
          }

          it->second->SetStep(true);
          return true;
        }

        // Continue one thread and keep other threads stopped.
        //
        // GDB sends this for software single step, which is used:
        // - on Win64 to step over rsp modification and subsequent rsp
        //   sandboxing at once. For details, see:
        //     http://code.google.com/p/nativeclient/issues/detail?id=2903
        // - TODO: on ARM, which has no hardware support for single step
        // - TODO: to step over syscalls
        //
        // Unfortunately, we can't make this just Win-specific. We might
        // use Linux GDB to connect to Win debug stub, so even Linux GDB
        // should send software single step. Vice versa, software single
        // step-enabled Win GDB might be connected to Linux debug stub,
        // so even Linux debug stub should accept software single step.
        if (strncmp(subcommand, ";c:", 3) == 0) {
          char *end;
          uint32_t thread_id = static_cast<uint32_t>(
              strtol(subcommand + 3, &end, 16));
          if (end != subcommand + 3 && *end == 0) {
            if (thread_id == sig_thread_) {
              step_over_breakpoint_thread_ = sig_thread_;
              return true;
            }
          }

          err = BAD_ARGS;
          break;
        }

        // Unsupported form of vCont.
        err = BAD_FORMAT;
        break;
      }

      NaClLog(LOG_ERROR, "Unknown command: %s\n", pktIn->GetPayload());
      return false;
    }

    case 'Z': {
      uint64_t breakpoint_type;
      uint64_t breakpoint_address;
      uint64_t breakpoint_kind;
      if (!pktIn->GetNumberSep(&breakpoint_type, 0) ||
          breakpoint_type != 0 ||
          !pktIn->GetNumberSep(&breakpoint_address, 0) ||
          !pktIn->GetNumberSep(&breakpoint_kind, 0)) {
        err = BAD_FORMAT;
        break;
      }
      if (breakpoint_address != (uint32_t) breakpoint_address ||
          !AddBreakpoint((uint32_t) breakpoint_address)) {
        err = FAILED;
        break;
      }
      pktOut->AddString("OK");
      break;
    }

    case 'z': {
      uint64_t breakpoint_type;
      uint64_t breakpoint_address;
      uint64_t breakpoint_kind;
      if (!pktIn->GetNumberSep(&breakpoint_type, 0) ||
          breakpoint_type != 0 ||
          !pktIn->GetNumberSep(&breakpoint_address, 0) ||
          !pktIn->GetNumberSep(&breakpoint_kind, 0)) {
        err = BAD_FORMAT;
        break;
      }
      if (breakpoint_address != (uint32_t) breakpoint_address ||
          !RemoveBreakpoint((uint32_t) breakpoint_address)) {
        err = FAILED;
        break;
      }
      pktOut->AddString("OK");
      break;
    }

    default: {
      // If the command is not recognzied, ignore it by sending an
      // empty reply.
      string str;
      pktIn->GetString(&str);
      NaClLog(LOG_ERROR, "Unknown command: %s\n", pktIn->GetPayload());
      return false;
    }
  }

  // If there is an error, return the error code instead of a payload
  if (err) {
    pktOut->Clear();
    pktOut->AddRawChar('E');
    pktOut->AddWord8(err);
  }
  return false;
}


void Target::TrackThread(struct NaClAppThread *natp) {
  // natp->thread_num values are 0-based indexes, but we treat 0 as
  // "not a thread ID", so we add 1.
  uint32_t id = natp->thread_num + 1;
  MutexLock lock(&mutex_);
  CHECK(threads_[id] == 0);
  threads_[id] = IThread::Create(id, natp);
}

void Target::IgnoreThread(struct NaClAppThread *natp) {
  uint32_t id = natp->thread_num + 1;
  MutexLock lock(&mutex_);
  ThreadMap_t::iterator iter = threads_.find(id);
  CHECK(iter != threads_.end());
  delete iter->second;
  threads_.erase(iter);
}

void Target::Exit() {
  MutexLock lock(&mutex_);
  if (session_ != NULL) {
    Packet exit_packet;
    if (NACL_ABI_WIFSIGNALED(nap_->exit_status)) {
      exit_packet.AddRawChar('X');
      exit_packet.AddWord8(NACL_ABI_WTERMSIG(nap_->exit_status));
    } else {
      exit_packet.AddRawChar('W');
      exit_packet.AddWord8(NACL_ABI_WEXITSTATUS(nap_->exit_status));
    }
    session_->SendPacket(&exit_packet);
  }
}

void Target::Detach() {
  NaClLog(LOG_INFO, "Requested Detach.\n");
}


IThread* Target::GetRegThread() {
  ThreadMap_t::const_iterator itr;

  switch (reg_thread_) {
    // If we want "any" then try the signal'd thread first
    case 0:
    case 0xFFFFFFFF:
      itr = threads_.begin();
      break;

    default:
      itr = threads_.find(reg_thread_);
      break;
  }

  if (itr == threads_.end()) return 0;

  return itr->second;
}

IThread* Target::GetRunThread() {
  // This is used to select a thread for "s" (step) command only.
  // For multi-threaded targets, "s" is deprecated in favor of "vCont", which
  // always specifies the thread explicitly when needed. However, we want
  // to keep backward compatibility here, as using "s" when debugging
  // a single-threaded program might be a popular use case.
  if (threads_.size() == 1) {
    return threads_.begin()->second;
  }
  return NULL;
}

IThread* Target::GetThread(uint32_t id) {
  ThreadMap_t::const_iterator itr;
  itr = threads_.find(id);
  if (itr != threads_.end()) return itr->second;

  return NULL;
}

void Target::SuspendAllThreads() {
  NaClUntrustedThreadsSuspendAll(nap_, /* save_registers= */ 1);
  for (ThreadMap_t::const_iterator iter = threads_.begin();
       iter != threads_.end();
       ++iter) {
    IThread *thread = iter->second;
    thread->CopyRegistersFromAppThread();
    CopyFaultSignalFromAppThread(thread);
  }
}

void Target::ResumeAllThreads() {
  for (ThreadMap_t::const_iterator iter = threads_.begin();
       iter != threads_.end();
       ++iter) {
    iter->second->CopyRegistersToAppThread();
  }
  NaClUntrustedThreadsResumeAll(nap_);
}

// UnqueueAnyFaultedThread() picks a thread that has been blocked as a
// result of faulting and unblocks it.  It returns the thread's ID via
// |thread_id| and the type of fault via |signal|.  As a precondition,
// all threads must be currently suspended.
void Target::UnqueueAnyFaultedThread(uint32_t *thread_id, int8_t *signal) {
  for (ThreadMap_t::const_iterator iter = threads_.begin();
       iter != threads_.end();
       ++iter) {
    IThread *thread = iter->second;
    if (thread->GetFaultSignal() != 0) {
      *signal = thread->GetFaultSignal();
      *thread_id = thread->GetId();
      thread->UnqueueFaultedThread();
      return;
    }
  }
  NaClLog(LOG_FATAL, "UnqueueAnyFaultedThread: No threads queued\n");
}

}  // namespace gdb_rsp
