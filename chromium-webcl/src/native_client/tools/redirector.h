/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TOOLS_REDIRECTOR_H_
#define NATIVE_CLIENT_TOOLS_REDIRECTOR_H_

#include <wchar.h>

typedef struct {
  const wchar_t *from;
  const wchar_t *to;
  const wchar_t *args;
} redirect_t;

const redirect_t redirects[] = {
  {L"/bin/i686-nacl-addr2line.exe",   L"/libexec/x86_64-nacl-addr2line.exe", L""},
  {L"/bin/i686-nacl-ar.exe",          L"/libexec/x86_64-nacl-ar.exe",        L""},
  {L"/bin/i686-nacl-as.exe",          L"/libexec/x86_64-nacl-as.exe",        L"--32"},
  {L"/bin/i686-nacl-c++.exe",         L"/libexec/x86_64-nacl-c++.exe",       L"-m32"},
  {L"/bin/i686-nacl-c++filt.exe",     L"/libexec/x86_64-nacl-c++filt.exe",   L""},
  {L"/bin/i686-nacl-cpp.exe",         L"/libexec/x86_64-nacl-cpp.exe",       L""},
  {L"/bin/i686-nacl-g++.exe",         L"/libexec/x86_64-nacl-g++.exe",       L"-m32"},
  {L"/bin/i686-nacl-gcc.exe",         L"/libexec/x86_64-nacl-gcc.exe",       L"-m32"},
  {L"/bin/i686-nacl-gcc-4.4.3.exe",   L"/libexec/x86_64-nacl-gcc-4.4.3.exe", L"-m32"},
  {L"/bin/i686-nacl-gccbug.exe",      L"/libexec/x86_64-nacl-gccbug.exe",    L""},
  {L"/bin/i686-nacl-gcov.exe",        L"/libexec/x86_64-nacl-gcov.exe",      L""},
  {L"/bin/i686-nacl-gfortran.exe",    L"/libexec/x86_64-nacl-gfortran.exe",  L"-m32"},
  {L"/bin/i686-nacl-gprof.exe",       L"/libexec/x86_64-nacl-gprof.exe",     L""},
  {L"/bin/i686-nacl-ld.exe",          L"/libexec/x86_64-nacl-ld.exe",        L"-melf_nacl"},
  {L"/bin/i686-nacl-nm.exe",          L"/libexec/x86_64-nacl-nm.exe",        L""},
  {L"/bin/i686-nacl-objcopy.exe",     L"/libexec/x86_64-nacl-objcopy.exe",   L""},
  {L"/bin/i686-nacl-objdump.exe",     L"/libexec/x86_64-nacl-objdump.exe",   L""},
  {L"/bin/i686-nacl-ranlib.exe",      L"/libexec/x86_64-nacl-ranlib.exe",    L""},
  {L"/bin/i686-nacl-readelf.exe",     L"/libexec/x86_64-nacl-readelf.exe",   L""},
  {L"/bin/i686-nacl-size.exe",        L"/libexec/x86_64-nacl-size.exe",      L""},
  {L"/bin/i686-nacl-strings.exe",     L"/libexec/x86_64-nacl-strings.exe",   L""},
  {L"/bin/i686-nacl-strip.exe",       L"/libexec/x86_64-nacl-strip.exe",     L""},
  {L"/bin/x86_64-nacl-addr2line.exe", L"/libexec/x86_64-nacl-addr2line.exe", L""},
  {L"/bin/x86_64-nacl-ar.exe",        L"/libexec/x86_64-nacl-ar.exe",        L""},
  {L"/bin/x86_64-nacl-as.exe",        L"/libexec/x86_64-nacl-as.exe",        L""},
  {L"/bin/x86_64-nacl-c++.exe",       L"/libexec/x86_64-nacl-c++.exe",       L"-m64"},
  {L"/bin/x86_64-nacl-c++filt.exe",   L"/libexec/x86_64-nacl-c++filt.exe",   L""},
  {L"/bin/x86_64-nacl-cpp.exe",       L"/libexec/x86_64-nacl-cpp.exe",       L""},
  {L"/bin/x86_64-nacl-g++.exe",       L"/libexec/x86_64-nacl-g++.exe",       L"-m64"},
  {L"/bin/x86_64-nacl-gcc.exe",       L"/libexec/x86_64-nacl-gcc.exe",       L"-m64"},
  {L"/bin/x86_64-nacl-gcc-4.4.3.exe", L"/libexec/x86_64-nacl-gcc-4.4.3.exe", L"-m64"},
  {L"/bin/x86_64-nacl-gccbug.exe",    L"/libexec/x86_64-nacl-gccbug.exe",    L""},
  {L"/bin/x86_64-nacl-gcov.exe",      L"/libexec/x86_64-nacl-gcov.exe",      L""},
  {L"/bin/x86_64-nacl-gfortran.exe",  L"/libexec/x86_64-nacl-gfortran.exe",  L"-m64"},
  {L"/bin/x86_64-nacl-gprof.exe",     L"/libexec/x86_64-nacl-gprof.exe",     L""},
  {L"/bin/x86_64-nacl-ld.exe",        L"/libexec/x86_64-nacl-ld.exe",        L""},
  {L"/bin/x86_64-nacl-nm.exe",        L"/libexec/x86_64-nacl-nm.exe",        L""},
  {L"/bin/x86_64-nacl-objcopy.exe",   L"/libexec/x86_64-nacl-objcopy.exe",   L""},
  {L"/bin/x86_64-nacl-objdump.exe",   L"/libexec/x86_64-nacl-objdump.exe",   L""},
  {L"/bin/x86_64-nacl-ranlib.exe",    L"/libexec/x86_64-nacl-ranlib.exe",    L""},
  {L"/bin/x86_64-nacl-readelf.exe",   L"/libexec/x86_64-nacl-readelf.exe",   L""},
  {L"/bin/x86_64-nacl-size.exe",      L"/libexec/x86_64-nacl-size.exe",      L""},
  {L"/bin/x86_64-nacl-strings.exe",   L"/libexec/x86_64-nacl-strings.exe",   L""},
  {L"/bin/x86_64-nacl-strip.exe",     L"/libexec/x86_64-nacl-strip.exe",     L""},
  {L"/x86_64-nacl/bin/ar.exe",        L"/libexec/x86_64-nacl-ar.exe",        L""},
  {L"/x86_64-nacl/bin/as.exe",        L"/libexec/x86_64-nacl-as.exe",        L""},
  {L"/x86_64-nacl/bin/c++.exe",       L"/libexec/x86_64-nacl-c++.exe",       L"-m64"},
  {L"/x86_64-nacl/bin/gcc.exe",       L"/libexec/x86_64-nacl-gcc.exe",       L"-m64"},
  {L"/x86_64-nacl/bin/g++.exe",       L"/libexec/x86_64-nacl-g++.exe",       L"-m64"},
  {L"/x86_64-nacl/bin/gfortran.exe",  L"/libexec/x86_64-nacl-gfortran.exe",  L"-m64"},
  {L"/x86_64-nacl/bin/ld.exe",        L"/libexec/x86_64-nacl-ld.exe",        L""},
  {L"/x86_64-nacl/bin/nm.exe",        L"/libexec/x86_64-nacl-nm.exe",        L""},
  {L"/x86_64-nacl/bin/objcopy.exe",   L"/libexec/x86_64-nacl-objcopy.exe",   L""},
  {L"/x86_64-nacl/bin/objdump.exe",   L"/libexec/x86_64-nacl-objdump.exe",   L""},
  {L"/x86_64-nacl/bin/ranlib.exe",    L"/libexec/x86_64-nacl-ranlib.exe",    L""},
  {L"/x86_64-nacl/bin/strip.exe",     L"/libexec/x86_64-nacl-strip.exe",     L""},
};

#endif
