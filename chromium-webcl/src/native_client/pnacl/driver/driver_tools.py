#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  pnacl/build.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.
#

import hashlib
import platform
import os
import re
import shlex
import signal
import subprocess
import struct
import sys
import tempfile

import artools
import ldtools
import pathtools

from driver_env import env
# TODO: import driver_log and change these references from 'foo' to
# 'driver_log.foo', or split driver_log further
from driver_log import Log, DriverOpen, DriverClose, StringifyCommand, TempFiles, DriverExit
from shelltools import shell

LLVM_BITCODE_MAGIC = 'BC\xc0\xde'
LLVM_WRAPPER_MAGIC = '\xde\xc0\x17\x0b'

def ParseError(s, leftpos, rightpos, msg):
  Log.Error("Parse Error: %s", msg)
  Log.Error('  ' + s)
  Log.Error('  ' + (' '*leftpos) + ('^'*(rightpos - leftpos + 1)))
  DriverExit(1)


# Run a command with extra environment settings
def RunWithEnv(cmd, **kwargs):
  env.push()
  env.setmany(**kwargs)
  ret = Run(cmd)
  env.pop()
  return ret


def SetExecutableMode(path):
  if os.name == "posix":
    realpath = pathtools.tosys(path)
    # os.umask gets and sets at the same time.
    # There's no way to get it without setting it.
    umask = os.umask(0)
    os.umask(umask)
    os.chmod(realpath, 0755 & ~umask)


def FilterOutArchArgs(args):
  while '-arch' in args:
    i = args.index('-arch')
    args = args[:i] + args[i+2:]
  return args


def RunDriver(invocation, args, suppress_inherited_arch_args=False):
  """
  RunDriver() is used to invoke "driver" tools, e.g.
  those prefixed  with "pnacl-"

  It automatically appends some additional flags to the invocation
  which were inherited from the current invocation.
  Those flags were preserved by ParseArgs
  """

  if isinstance(args, str):
    args = shell.split(env.eval(args))

  module_name = 'pnacl-%s' % invocation
  script = env.eval('${DRIVER_BIN}/%s' % module_name)
  script = shell.unescape(script)

  inherited_driver_args = env.get('INHERITED_DRIVER_ARGS')
  if suppress_inherited_arch_args:
    inherited_driver_args = FilterOutArchArgs(inherited_driver_args)

  script = pathtools.tosys(script)
  cmd = [script] + args + inherited_driver_args
  Log.Info('Driver invocation: %s', repr(cmd))

  module = __import__(module_name)
  # Save the environment, reset the environment, run
  # the driver module, and then restore the environment.
  env.push()
  env.reset()
  DriverMain(module, cmd)
  env.pop()

def memoize(f):
  """ Memoize a function with no arguments """
  saved = {}
  def newf():
    if len(saved) == 0:
      saved[None] = f()
    return saved[None]
  newf.__name__ = f.__name__
  return newf


@env.register
@memoize
def GetBuildOS():
  name = platform.system().lower()
  if name.startswith('cygwin_nt') or 'windows' in name:
    name = 'windows'
  if name not in ('linux', 'darwin', 'windows'):
    Log.Fatal("Unsupported platform '%s'", name)
  return name

@env.register
@memoize
def GetBuildArch():
  m = platform.machine()

  # Windows is special
  if m == 'x86':
    m = 'i686'

  if m not in ('i386', 'i686', 'x86_64'):
    Log.Fatal("Unsupported architecture '%s'", m)
  return m

# Crawl backwards, starting from the directory containing this script,
# until we find a directory satisfying a filter function.
def FindBaseDir(function):
  Depth = 0
  cur = env.getone('DRIVER_BIN')
  while not function(cur) and Depth < 16:
    cur = pathtools.dirname(cur)
    Depth += 1
  if function(cur):
    return cur
  return None

@env.register
@memoize
def FindBaseNaCl():
  """ Find native_client/ directory """
  dir = FindBaseDir(lambda cur: pathtools.basename(cur) == 'native_client')
  if dir is None:
    Log.Fatal("Unable to find 'native_client' directory")
  return shell.escape(dir)

@env.register
@memoize
def FindBaseToolchain():
  """ Find toolchain/ directory """
  dir = FindBaseDir(lambda cur: pathtools.basename(cur) == 'toolchain')
  if dir is None:
    Log.Fatal("Unable to find 'toolchain' directory")
  return shell.escape(dir)

@env.register
@memoize
def FindBasePNaCl():
  """ Find the base directory of the PNaCl toolchain """
  # The bin/ directory is one of:
  # <base>/bin if <base> is /path/to/pnacl_translator
  # <base>/newlib/bin or <base>/glibc/bin otherwise
  bindir = env.getone('DRIVER_BIN')

  # If this is a translator dir, use pnacl_translator
  translator_dir = FindBaseDir(
      lambda cur: pathtools.basename(cur) == 'pnacl_translator')
  if translator_dir is not None:
    return shell.escape(translator_dir)
  # Else use ../..
  basedir = pathtools.dirname(pathtools.dirname(bindir))
  return shell.escape(basedir)

def ReadConfig():
  # Mock out ReadConfig if running unittests.  Settings are applied directly
  # by DriverTestEnv rather than reading this configuration file.
  if env.has('PNACL_RUNNING_UNITTESTS'):
    return
  driver_bin = env.getone('DRIVER_BIN')
  driver_conf = pathtools.join(driver_bin, 'driver.conf')
  fp = DriverOpen(driver_conf, 'r')
  linecount = 0
  for line in fp:
    linecount += 1
    line = line.strip()
    if line == '' or line.startswith('#'):
      continue
    sep = line.find('=')
    if sep < 0:
      Log.Fatal("%s: Parse error, missing '=' on line %d",
                pathtools.touser(driver_conf), linecount)
    keyname = line[:sep].strip()
    value = line[sep+1:].strip()
    env.setraw(keyname, value)
  DriverClose(fp)

  if env.getone('LIBMODE') not in ('newlib', 'glibc'):
    Log.Fatal('Invalid LIBMODE in %s', pathtools.touser(driver_conf))


@env.register
def AddPrefix(prefix, varname):
  values = env.get(varname)
  return ' '.join([prefix + shell.escape(v) for v in values ])

######################################################################
#
# Argument Parser
#
######################################################################

DriverArgPatterns = [
  ( '--pnacl-driver-verbose',             "env.set('LOG_VERBOSE', '1')"),
  ( ('-arch', '(.+)'),                 "SetArch($0)"),
  ( '--pnacl-sb',                      "env.set('SANDBOXED', '1')"),
  ( '--pnacl-use-emulator',            "env.set('USE_EMULATOR', '1')"),
  ( '--dry-run',                       "env.set('DRY_RUN', '1')"),
  ( '--pnacl-arm-bias',                "env.set('BIAS', 'ARM')"),
  ( '--pnacl-mips-bias',               "env.set('BIAS', 'MIPS32')"),
  ( '--pnacl-i686-bias',               "env.set('BIAS', 'X8632')"),
  ( '--pnacl-x86_64-bias',             "env.set('BIAS', 'X8664')"),
  ( '--pnacl-bias=(.+)',               "env.set('BIAS', FixArch($0))"),
  ( '--pnacl-default-command-line',    "env.set('USE_DEFAULT_CMD_LINE', '1')"),
  ( '-save-temps',                     "env.set('SAVE_TEMPS', '1')"),
  ( '-no-save-temps',                  "env.set('SAVE_TEMPS', '0')"),
 ]

DriverArgPatternsNotInherited = [
  ( '--pnacl-driver-set-([^=]+)=(.*)',    "env.set($0, $1)"),
  ( '--pnacl-driver-append-([^=]+)=(.*)', "env.append($0, $1)"),
]


def ShouldExpandCommandFile(arg):
  """ We may be given files with commandline arguments.
  Read in the arguments so that they can be handled as usual. """
  if arg.startswith('@'):
    possible_file = pathtools.normalize(arg[1:])
    return pathtools.isfile(possible_file)
  else:
    return False


def DoExpandCommandFile(argv, i):
  arg = argv[i]
  fd = DriverOpen(pathtools.normalize(arg[1:]), 'r')
  more_args = []

  # Use shlex here to process the response file contents.
  # This ensures that single and double quoted args are
  # handled correctly.  Since this file is very likely
  # to contain paths with windows path seperators we can't
  # use the normal shlex.parse() since we need to disable
  # disable '\' (the default escape char).
  for line in fd:
    lex = shlex.shlex(line, posix=True)
    lex.escape = ''
    lex.whitespace_split = True
    more_args += list(lex)

  fd.close()
  argv = argv[:i] + more_args + argv[i+1:]
  return argv


def ParseArgs(argv,
              patternlist,
              driver_patternlist=DriverArgPatterns,
              driver_patternlist_not_inherited=DriverArgPatternsNotInherited):
  """Parse argv using the patterns in patternlist
     Also apply the built-in DriverArgPatterns unless instructed otherwise.
     This function must be called by all (real) drivers.
  """
  if driver_patternlist:
    driver_args, argv = ParseArgsBase(argv, driver_patternlist)

    # TODO(robertm): think about a less obscure mechanism to
    #                replace the inherited args feature
    assert not env.get('INHERITED_DRIVER_ARGS')
    env.append('INHERITED_DRIVER_ARGS', *driver_args)

  _, argv = ParseArgsBase(argv, driver_patternlist_not_inherited)

  _, unmatched = ParseArgsBase(argv, patternlist)
  if unmatched:
    for u in unmatched:
      Log.Error('Unrecognized argument: ' + u)
    Log.Fatal('unknown arguments')


def ParseArgsBase(argv, patternlist):
  """ Parse argv using the patterns in patternlist
      Returns: (matched, unmatched)
  """
  matched = []
  unmatched = []
  i = 0
  while i < len(argv):
    if ShouldExpandCommandFile(argv[i]):
      argv = DoExpandCommandFile(argv, i)
    num_matched, action, groups = MatchOne(argv, i, patternlist)
    if num_matched == 0:
      unmatched.append(argv[i])
      i += 1
      continue
    matched += argv[i:i+num_matched]
    if isinstance(action, str):
      # Perform $N substitution
      for g in xrange(0, len(groups)):
        action = action.replace('$%d' % g, 'groups[%d]' % g)
    try:
      if isinstance(action, str):
        # NOTE: this is essentially an eval for python expressions
        # which does rely on the current environment for unbound vars
        # Log.Info('about to exec [%s]', str(action))
        exec(action)
      else:
        action(*groups)
    except Exception, err:
      Log.Fatal('ParseArgs action [%s] failed with: %s', action, err)
    i += num_matched
  return (matched, unmatched)


def MatchOne(argv, i, patternlist):
  """Find a pattern which matches argv starting at position i"""
  for (regex, action) in patternlist:
    if isinstance(regex, str):
      regex = [regex]
    j = 0
    matches = []
    for r in regex:
      if i+j < len(argv):
        match = re.compile('^'+r+'$').match(argv[i+j])
      else:
        match = None
      matches.append(match)
      j += 1
    if None in matches:
      continue
    groups = [ list(m.groups()) for m in matches ]
    groups = reduce(lambda x,y: x+y, groups, [])
    return (len(regex), action, groups)
  return (0, '', [])

def UnrecognizedOption(*args):
  Log.Fatal("Unrecognized option: " + ' '.join(args) + "\n" +
            "Use '--help' for more information.")

######################################################################
# File Type Tools
######################################################################

def SimpleCache(f):
  """ Cache results of a function using a dictionary """
  cache = dict()
  def wrapper(*args):
    if args in cache:
      return cache[args]
    else:
      result = f(*args)
      cache[args] = result
      return result
  wrapper.__name__ = f.__name__
  wrapper.__cache = cache
  return wrapper

@SimpleCache
def IsNative(filename):
  return (IsNativeObject(filename) or
          IsNativeDSO(filename) or
          IsNativeArchive(filename))

@SimpleCache
def IsNativeObject(filename):
  return FileType(filename) == 'o'

@SimpleCache
def IsNativeDSO(filename):
  return FileType(filename) == 'so'

@SimpleCache
def IsBitcodeDSO(filename):
  return FileType(filename) == 'pso'

@SimpleCache
def IsBitcodeObject(filename):
  return FileType(filename) == 'po'

def IsBitcodeWrapperHeader(data):
  return data[:4] == LLVM_WRAPPER_MAGIC

@SimpleCache
def IsWrappedBitcode(filename):
  fp = DriverOpen(filename, 'rb')
  header = fp.read(4)
  iswbc = IsBitcodeWrapperHeader(header)
  DriverClose(fp)
  return iswbc

@SimpleCache
def IsBitcode(filename):
  fp = DriverOpen(filename, 'rb')
  header = fp.read(4)
  DriverClose(fp)
  # Raw bitcode
  if header == LLVM_BITCODE_MAGIC:
    return True
  # Wrapped bitcode
  return IsBitcodeWrapperHeader(header)

@SimpleCache
def IsArchive(filename):
  return artools.IsArchive(filename)

@SimpleCache
def IsBitcodeArchive(filename):
  filetype = FileType(filename)
  return filetype == 'archive-bc'

@SimpleCache
def IsNativeArchive(filename):
  return IsArchive(filename) and not IsBitcodeArchive(filename)

class ELFHeader(object):
  ELF_MAGIC = '\x7fELF'
  ELF_TYPES = { 1: 'REL',  # .o
                2: 'EXEC', # .exe
                3: 'DYN' } # .so
  ELF_MACHINES = {  3: '386',
                    8: 'MIPS',
                   40: 'ARM',
                   62: 'X86_64' }
  ELF_OSABI = { 0: 'UNIX',
                3: 'LINUX',
                123: 'NACL' }
  ELF_ABI_VER = { 0: 'NONE',
                  7: 'NACL' }

  def __init__(self, e_type, e_machine, e_osabi, e_abiver):
    self.type = self.ELF_TYPES[e_type]
    self.machine = self.ELF_MACHINES[e_machine]
    self.osabi = self.ELF_OSABI[e_osabi]
    self.abiver = self.ELF_ABI_VER[e_abiver]
    self.arch = FixArch(self.machine)  # For convenience

# If the file is not ELF, returns None.
# Otherwise, returns an ELFHeader object.
@SimpleCache
def GetELFHeader(filename):
  fp = DriverOpen(filename, 'rb')
  header = fp.read(16 + 2 + 2)
  DriverClose(fp)
  return DecodeELFHeader(header, filename)

def DecodeELFHeader(header, filename):
  # Pull e_ident, e_type, e_machine
  if header[0:4] != ELFHeader.ELF_MAGIC:
    return None

  e_osabi = DecodeLE(header[7])
  e_abiver = DecodeLE(header[8])
  e_type = DecodeLE(header[16:18])
  e_machine = DecodeLE(header[18:20])

  if e_osabi not in ELFHeader.ELF_OSABI:
    Log.Fatal('%s: ELF file has unknown OS ABI (%d)', filename, e_osabi)
  if e_abiver not in ELFHeader.ELF_ABI_VER:
    Log.Fatal('%s: ELF file has unknown ABI version (%d)', filename, e_abiver)
  if e_type not in ELFHeader.ELF_TYPES:
    Log.Fatal('%s: ELF file has unknown type (%d)', filename, e_type)
  if e_machine not in ELFHeader.ELF_MACHINES:
    Log.Fatal('%s: ELF file has unknown machine type (%d)', filename, e_machine)

  eh = ELFHeader(e_type, e_machine, e_osabi, e_abiver)
  return eh

def IsELF(filename):
  return GetELFHeader(filename) is not None

# Decode Little Endian bytes into an unsigned value
def DecodeLE(bytes):
  value = 0
  for b in reversed(bytes):
    value *= 2
    value += ord(b)
  return value

# Sandboxed LLC returns the metadata after translation. Use FORCED_METADATA
# if available instead of llvm-dis to avoid building llvm-dis on arm, and
# to make sure this interface gets tested
FORCED_METADATA = {}
@SimpleCache
def GetBitcodeMetadata(filename, assume_pexe):
  assert(IsBitcode(filename))
  # The argument |assume_pexe| helps break a dependency on bitcode metadata,
  # as the shared library metadata is not finalized yet.
  if assume_pexe:
    return { 'OutputFormat': 'executable',
             'SOName'      : '',
             'NeedsLibrary': [] }
  global FORCED_METADATA
  if filename in FORCED_METADATA:
    return FORCED_METADATA[filename]

  llvm_dis = env.getone('LLVM_DIS')
  args = [ llvm_dis, '-dump-metadata', filename ]
  _, stdout_contents, _  = Run(args, redirect_stdout=subprocess.PIPE)

  metadata = { 'OutputFormat': '',
               'SOName'      : '',
               'NeedsLibrary': [] }
  for line in stdout_contents.split('\n'):
    if not line.strip():
      continue
    k, v = line.split(':')
    k = k.strip()
    v = v.strip()
    if k.startswith('NeededRecord_'):
      metadata[k] = ''
    assert(k in metadata)
    if isinstance(metadata[k], list):
      metadata[k].append(v)
    else:
      metadata[k] = v

  return metadata

def SetBitcodeMetadata(filename, is_shared, soname, needed_libs):
  metadata = {}
  metadata['OutputFormat'] = 'shared' if is_shared else 'executable'
  metadata['NeedsLibrary'] = list(needed_libs)
  metadata['SOName'] = soname
  global FORCED_METADATA
  FORCED_METADATA[filename] = metadata

# If FORCED_FILE_TYPE is set, FileType() will return FORCED_FILE_TYPE for all
# future input files. This is useful for the "as" incarnation, which
# needs to accept files of any extension and treat them as ".s" (or ".ll")
# files. Also useful for gcc's "-x", which causes all files to be treated
# in a certain way.
FORCED_FILE_TYPE = None
def SetForcedFileType(t):
  global FORCED_FILE_TYPE
  FORCED_FILE_TYPE = t

def GetForcedFileType():
  return FORCED_FILE_TYPE

def ForceFileType(filename, newtype = None):
  if newtype is None:
    if FORCED_FILE_TYPE is None:
      return
    newtype = FORCED_FILE_TYPE
  FileType.__cache[filename] = newtype

# File Extension -> Type string
# TODO(pdox): Add types for sources which should not be preprocessed.
ExtensionMap = {
  'c'   : 'c',
  'i'   : 'c',    # C, but should not be preprocessed.

  'cc'  : 'c++',
  'cp'  : 'c++',
  'cxx' : 'c++',
  'cpp' : 'c++',
  'CPP' : 'c++',
  'c++' : 'c++',
  'C'   : 'c++',
  'ii'  : 'c++',  # C++, but should not be preprocessed.

  'm'   : 'objc',  # .m = "Objective-C source file"

  'll'  : 'll',
  'bc'  : 'po',
  'po'  : 'po',   # .po = "Portable object file"
  'pexe': 'pexe', # .pexe = "Portable executable"
  'pso' : 'pso',  # .pso = "Portable Shared Object"
  'asm' : 'S',
  'S'   : 'S',
  'sx'  : 'S',
  's'   : 's',
  'o'   : 'o',
  'os'  : 'o',
  'so'  : 'so',
  'nexe': 'nexe',
}

def IsSourceType(filetype):
  return filetype in ('c','c++','objc')

# The SimpleCache decorator is required for correctness, due to the
# ForceFileType mechanism.
@SimpleCache
def FileType(filename):
  # Auto-detect bitcode files, since we can't rely on extensions
  ext = filename.split('.')[-1]

  # TODO(pdox): We open and read the the first few bytes of each file
  #             up to 4 times, when we only need to do it once. The
  #             OS cache prevents us from hitting the disk, but this
  #             is still slower than it needs to be.
  if IsArchive(filename):
    return artools.GetArchiveType(filename)

  if IsELF(filename):
    return GetELFType(filename)

  if IsBitcode(filename):
    return GetBitcodeType(filename, False)

  if (ext in ('o','so','a','po','pso','pa','x') and
      ldtools.IsLinkerScript(filename)):
    return 'ldscript'

  # Use the file extension if it is recognized
  if ext in ExtensionMap:
    return ExtensionMap[ext]

  Log.Fatal('%s: Unrecognized file type', filename)


@SimpleCache
def GetELFType(filename):
  """ ELF type as determined by ELF metadata """
  assert(IsELF(filename))
  elfheader = GetELFHeader(filename)
  elf_type_map = {
    'EXEC': 'nexe',
    'REL' : 'o',
    'DYN' : 'so'
  }
  return elf_type_map[elfheader.type]

@SimpleCache
def GetBitcodeType(filename, assume_pexe):
  """ Bitcode type as determined by bitcode metadata """
  assert(IsBitcode(filename))
  metadata = GetBitcodeMetadata(filename, assume_pexe)
  format_map = {
    'object': 'po',
    'shared': 'pso',
    'executable': 'pexe'
  }
  return format_map[metadata['OutputFormat']]

def GetBasicHeaderData(data):
  """ Extract bitcode offset and size from LLVM bitcode wrapper header.
      Format is 4-bytes each of [ magic, version, bc offset, bc size, ...
      (documented at http://llvm.org/docs/BitCodeFormat.html)
  """
  if not IsBitcodeWrapperHeader(data):
    raise ValueError('Data is not a bitcode wrapper')
  magic, llvm_bcversion, offset, size = struct.unpack('<IIII', data)
  if llvm_bcversion != 0:
    raise ValueError('Data is not a valid bitcode wrapper')
  return offset, size

def WrapBitcode(output):
  """ Hash the bitcode and insert a wrapper header with the sha value.
      If the bitcode is already wrapped, the old hash is overwritten.
  """
  fd = DriverOpen(output, 'rb')
  maybe_header = fd.read(16)
  if IsBitcodeWrapperHeader(maybe_header):
    offset, bytes_left = GetBasicHeaderData(maybe_header)
    fd.seek(offset)
  else:
    offset = 0
    fd.seek(0, os.SEEK_END)
    bytes_left = fd.tell()
    fd.seek(0)
  # get the hash
  sha = hashlib.sha256()
  while bytes_left:
    block = fd.read(min(bytes_left, 4096))
    sha.update(block)
    bytes_left -= len(block)
  DriverClose(fd)
  # run bc-wrap
  Run(' '.join(['${LLVM_BCWRAP}', '-hash', sha.hexdigest(), '"%s"' % output]))


######################################################################
#
# File Naming System (Temp files & Output files)
#
######################################################################

def DefaultOutputName(filename, outtype):
  if outtype in ('pp','dis'): return '-' # stdout

  base = pathtools.basename(filename)
  base = RemoveExtension(base)
  if outtype in ('po'): return base + '.o'

  assert(outtype in ExtensionMap.values())
  assert(not IsSourceType(outtype))

  return base + '.' + outtype

def RemoveExtension(filename):
  if filename.endswith('.opt.bc'):
    return filename[0:-len('.opt.bc')]

  name, ext = pathtools.splitext(filename)
  return name

def PathSplit(f):
  paths = []
  cur = f
  while True:
    cur, piece = pathtools.split(cur)
    if piece == '':
      break
    paths.append(piece)
  paths.reverse()
  return paths

# Generate a unique identifier for each input file.
# Start with the basename, and if that is not unique enough,
# add parent directories. Rinse, repeat.
class TempNameGen(object):
  def __init__(self, inputs, output):
    inputs = [ pathtools.abspath(i) for i in inputs ]
    output = pathtools.abspath(output)

    self.TempBase = output + '---linked'

    # TODO(pdox): Figure out if there's a less confusing way
    #             to simplify the intermediate filename in this case.
    #if len(inputs) == 1:
    #  # There's only one input file, don't bother adding the source name.
    #  TempMap[inputs[0]] = output + '---'
    #  return

    # Build the initial mapping
    self.TempMap = dict()
    for f in inputs:
      if f.startswith('-'):
        continue
      path = PathSplit(f)
      self.TempMap[f] = [1, path]

    while True:
      # Find conflicts
      ConflictMap = dict()
      Conflicts = set()
      for (f, [n, path]) in self.TempMap.iteritems():
        candidate = output + '---' + '_'.join(path[-n:]) + '---'
        if candidate in ConflictMap:
          Conflicts.add(ConflictMap[candidate])
          Conflicts.add(f)
        else:
          ConflictMap[candidate] = f

      if len(Conflicts) == 0:
        break

      # Resolve conflicts
      for f in Conflicts:
        n = self.TempMap[f][0]
        if n+1 > len(self.TempMap[f][1]):
          Log.Fatal('Unable to resolve naming conflicts')
        self.TempMap[f][0] = n+1

    # Clean up the map
    NewMap = dict()
    for (f, [n, path]) in self.TempMap.iteritems():
      candidate = output + '---' + '_'.join(path[-n:]) + '---'
      NewMap[f] = candidate
    self.TempMap = NewMap
    return

  def TempNameForOutput(self, imtype):
    temp = self.TempBase + '.' + imtype
    if not env.getbool('SAVE_TEMPS'):
      TempFiles.add(temp)
    return temp

  def TempNameForInput(self, input, imtype):
    fullpath = pathtools.abspath(input)
    # If input is already a temporary name, just change the extension
    if fullpath.startswith(self.TempBase):
      temp = self.TempBase + '.' + imtype
    else:
      # Source file
      temp = self.TempMap[fullpath] + '.' + imtype

    if not env.getbool('SAVE_TEMPS'):
      TempFiles.add(temp)
    return temp

# (Invoked from loader.py)
# If the driver is waiting on a background process in RunWithLog()
# and the user Ctrl-C's or kill's the driver, it may leave
# the child process (such as llc) running. To prevent this,
# the code below sets up a signal handler which issues a kill to
# the currently running child processes.
CleanupProcesses = []
def SetupSignalHandlers():
  global CleanupProcesses
  def signal_handler(unused_signum, unused_frame):
    for p in CleanupProcesses:
      try:
        p.kill()
      except BaseException:
        pass
    os.kill(os.getpid(), signal.SIGKILL)
    return 0
  if os.name == "posix":
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGHUP, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)


def ArgsTooLongForWindows(args):
  """ Detect when a command line might be too long for Windows.  """
  if not IsWindowsPython():
    return False
  else:
    return len(' '.join(args)) > 8191


def ConvertArgsToFile(args):
  fd, outfile = tempfile.mkstemp()
  # Remember to delete this file afterwards.
  TempFiles.add(outfile)
  cmd = args[0]
  other_args = args[1:]
  os.write(fd, ' '.join(other_args))
  os.close(fd)
  return [cmd, '@' + outfile]

# Note:
# The redirect_stdout and redirect_stderr is only used a handful of times
#
# The stdin_contents feature is currently only used by:
#  sel_universal invocations in the translator
#
def Run(args,
        errexit=True,
        stdin_contents=None,
        redirect_stdout=None,
        redirect_stderr=None):
  """ Run: Run a command.
      Returns: return_code, stdout, stderr

      Run() is used to invoke "other" tools, e.g.
      those NOT prefixed with "pnacl-"

      stdout and stderr only contain meaningful data if
          redirect_{stdout,stderr} == subprocess.PIPE

      Run will terminate the program upon failure unless errexit == False
      TODO(robertm): errexit == True has not been tested and needs more work

      redirect_stdout and redirect_stderr are passed straight
      to subprocess.Popen
      stdin_contents is an optional string used as stdin
  """

  result_stdout = None
  result_stderr = None
  if isinstance(args, str):
    args = shell.split(env.eval(args))

  args = [pathtools.tosys(args[0])] + args[1:]

  Log.Info('Running: ' + StringifyCommand(args))
  if stdin_contents:
    Log.Info('--------------stdin: begin')
    Log.Info(stdin_contents)
    Log.Info('--------------stdin: end')

  if env.getbool('DRY_RUN'):
    if redirect_stderr or redirect_stdout:
      # TODO(pdox): Prevent this from happening, so that
      # dry-run is more useful.
      Log.Fatal("Unhandled dry-run case.")
    return 0, None, None

  try:
    # If we have too long of a cmdline on windows, running it would fail.
    # Attempt to use a file with the command line options instead in that case.
    if ArgsTooLongForWindows(args):
      actual_args = ConvertArgsToFile(args)
      Log.Info('Wrote long commandline to file for Windows: ' +
               StringifyCommand(actual_args))

    else:
      actual_args = args

    redirect_stdin = None
    if stdin_contents:
      redirect_stdin = subprocess.PIPE

    p = subprocess.Popen(actual_args,
                         stdin=redirect_stdin,
                         stdout=redirect_stdout,
                         stderr=redirect_stderr)
    result_stdout, result_stderr = p.communicate(input=stdin_contents)
  except Exception, e:
    msg =  '%s\nCommand was: %s' % (str(e),
                                    StringifyCommand(args, stdin_contents))
    print msg
    DriverExit(1)

  Log.Info('Return Code: ' + str(p.returncode))

  if errexit and p.returncode != 0:
    if redirect_stdout == subprocess.PIPE:
        Log.Error('--------------stdout: begin')
        Log.Error(result_stdout)
        Log.Error('--------------stdout: end')

    if redirect_stderr == subprocess.PIPE:
        Log.Error('--------------stderr: begin')
        Log.Error(result_stderr)
        Log.Error('--------------stderr: end')
    DriverExit(p.returncode)

  return p.returncode, result_stdout, result_stderr


def FixArch(arch):
  arch = arch.lower()
  archfix = { 'x86-32': 'X8632',
              'x86_32': 'X8632',
              'x8632' : 'X8632',
              'i686'  : 'X8632',
              'ia32'  : 'X8632',
              '386'   : 'X8632',
              '686'   : 'X8632',

              'amd64' : 'X8664',
              'x86_64': 'X8664',
              'x86-64': 'X8664',
              'x8664' : 'X8664',

              'arm'   : 'ARM',
              'armv7' : 'ARM',
              'arm-thumb2' : 'ARM',

              'mips32': 'MIPS32',
              'mips'  : 'MIPS32',
              'mipsel': 'MIPS32',
              }
  if arch not in archfix:
    Log.Fatal('Unrecognized arch "%s"!', arch)
  return archfix[arch]

def IsWindowsPython():
  return 'windows' in platform.system().lower()

def SetupCygwinLibs():
  bindir = env.getone('DRIVER_BIN')
  os.environ['PATH'] += os.pathsep + pathtools.tosys(bindir)

# Map from GCC's -x file types and this driver's file types.
FILE_TYPE_MAP = {
    'c'                 : 'c',
    'c++'               : 'c++',
    'assembler'         : 's',
    'assembler-with-cpp': 'S',
}
FILE_TYPE_MAP_REVERSE = dict([reversed(_tmp) for _tmp in FILE_TYPE_MAP.items()])

def FileTypeToGCCType(filetype):
  return FILE_TYPE_MAP_REVERSE[filetype]

def GCCTypeToFileType(gcctype):
  if gcctype not in FILE_TYPE_MAP:
    Log.Fatal('language "%s" not recognized' % gcctype)
  return FILE_TYPE_MAP[gcctype]

def HelpNotAvailable():
  return 'Help text not available'

def DriverMain(module, argv):
  # TODO(robertm): this is ugly - try to get rid of this
  if '--pnacl-driver-verbose' in argv:
    Log.IncreaseVerbosity()
    env.set('LOG_VERBOSE', '1')

  # driver_path has the form: /foo/bar/pnacl_root/newlib/bin/pnacl-clang
  driver_path = pathtools.abspath(pathtools.normalize(argv[0]))
  driver_bin = pathtools.dirname(driver_path)
  script_name = pathtools.basename(driver_path)
  env.set('SCRIPT_NAME', script_name)
  env.set('DRIVER_PATH', driver_path)
  env.set('DRIVER_BIN', driver_bin)

  Log.SetScriptName(script_name)

  ReadConfig()

  if IsWindowsPython():
    SetupCygwinLibs()

  # skip tool name
  argv = argv[1:]

  # Handle help info
  if ('--help' in argv or
      '-h' in argv or
      '-help' in argv or
      '--help-full' in argv):
    help_func = getattr(module, 'get_help', None)
    if not help_func:
      Log.Fatal(HelpNotAvailable())
    helpstr = help_func(argv)
    print helpstr
    return 0

  return module.main(argv)


def SetArch(arch):
  env.set('ARCH', FixArch(arch))

def GetArch(required = False):
  arch = env.getone('ARCH')
  if arch == '':
    arch = None

  if required and not arch:
    Log.Fatal('Missing -arch!')

  return arch

# Read an ELF file to determine the machine type. If ARCH is already set,
# make sure the file has the same architecture. If ARCH is not set,
# set the ARCH to the file's architecture.
#
# Returns True if the file matches ARCH.
#
# Returns False if the file doesn't match ARCH. This only happens when
# must_match is False. If must_match is True, then a fatal error is generated
# instead.
def ArchMerge(filename, must_match):
  filetype = FileType(filename)
  if filetype in ('o','so'):
    elfheader = GetELFHeader(filename)
    if not elfheader:
      Log.Fatal("%s: Cannot read ELF header", filename)
    new_arch = elfheader.arch
  elif IsNativeArchive(filename):
    new_arch = filetype[len('archive-'):]
  else:
    Log.Fatal('%s: Unexpected file type in ArchMerge', filename)

  existing_arch = GetArch()

  if not existing_arch:
    SetArch(new_arch)
    return True
  elif new_arch != existing_arch:
    if must_match:
      msg = "%s: Incompatible object file (%s != %s)"
      logfunc = Log.Fatal
    else:
      msg = "%s: Skipping incompatible object file (%s != %s)"
      logfunc = Log.Warning
    logfunc(msg, filename, new_arch, existing_arch)
    return False
  else: # existing_arch and new_arch == existing_arch
    return True

def CheckTranslatorPrerequisites():
  """ Assert that the scons artifacts for running the sandboxed translator
      exist: sel_universal, and sel_ldr. """
  reqs = ['SEL_UNIVERSAL', 'SEL_LDR']
  # Linux also requires the nacl bootstrap helper.
  if GetBuildOS() == 'linux':
    reqs.append('BOOTSTRAP_LDR')
  for var in reqs:
    needed_file = env.getone(var)
    if not pathtools.exists(needed_file):
      Log.Fatal('Could not find %s [%s]', var, needed_file)

class DriverChain(object):
  """ The DriverChain class takes one or more input files,
      an output file, and a sequence of steps. It executes
      those steps, using intermediate files in between,
      to generate the final outpu.
  """

  def __init__(self, input, output, namegen):
    self.input = input
    self.output = output
    self.steps = []
    self.namegen = namegen

    # "input" can be a list of files or a single file.
    # If we're compiling for a single file, then we use
    # TempNameForInput. If there are multiple files
    # (e.g. linking), then we use TempNameForOutput.
    self.use_names_for_input = isinstance(input, str)

  def add(self, callback, output_type, **extra):
    step = (callback, output_type, extra)
    self.steps.append(step)

  def run(self):
    step_input = self.input
    for (i, (callback, output_type, extra)) in enumerate(self.steps):
      if i == len(self.steps)-1:
        # Last step
        step_output = self.output
      else:
        # Intermediate step
        if self.use_names_for_input:
          step_output = self.namegen.TempNameForInput(self.input, output_type)
        else:
          step_output = self.namegen.TempNameForOutput(output_type)
      callback(step_input, step_output, **extra)
      step_input = step_output
