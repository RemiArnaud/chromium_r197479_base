#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o nounset
set -o errexit

PREFIX=${PREFIX:-}
VERIFY=${VERIFY:-yes}
EMU_HACK=${EMU_HACK:-yes}



rm -f  *.out words 2.1.dict
ln -s  data/all/input/words .
ln -s  data/all/input/2.1.dict .

${PREFIX} $1 ${DASHDASH} 2.1.dict -batch < data/train/input/train.in \
  > stdout.out 2> stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  cmp  stdout.out  data/train/output/train.out
fi
echo "OK"
