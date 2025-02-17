#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  pnacl/build.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.
#

import driver_env
import driver_tools

EXTRA_ENV = {
    'ARGS' : '',
}

PATTERNS = [
    ('(.*)',   "env.append('ARGS', $0)"),
]

def main(argv):
  driver_env.env.update(EXTRA_ENV)
  driver_tools.ParseArgs(argv, PATTERNS)

  driver_tools.Run('"${PNACL_ABICHECK}" ${ARGS}')
  return 0;

# Don't just call the binary with -help because most of those options are
# completely useless for this tool.
def get_help(unused_argv):
  return """
USAGE: pnacl-abicheck <input bitcode>
  If <input bitcode> is -, then standard input will be read.
"""
