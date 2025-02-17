// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "string_helpers.h"

#include <base/string_util.h>

namespace mtpd {

std::string EnsureUTF8String(const std::string& str) {
  return IsStringUTF8(str) ? str : "";
}

}  // namespace
