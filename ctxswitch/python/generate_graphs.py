#!/usr/bin/arch -i386 python2.7
#
# Author: Kevin Klues <klueska@cs.berkeley.edu>

import os
import re
import glob
import json
import matplotlib
import matplotlib as mpl
matplotlib.use('Agg')
from pylab import *
from collections import OrderedDict
import numpy as np

class BenchmarkData:
  def __init__(self, config):
    input_folder = config.input_folder
    self.dir_name = os.path.basename(input_folder)
    m = re.match('ctxswitch-out-(?P<lpt>.*)', self.dir_name)
    self.loops_per_thread = int(m.group('lpt'))
    self.files = glob.glob(input_folder + '/*')
    self.data = {}
    map(lambda x: FileData(x, self), self.files)

class FileData:
  def __init__(self, f, bdata):
    self.file_name = os.path.basename(f)
    m = re.match('(?P<lib>.*)-ctxswitch-out-(?P<tpc>.*)-(?P<iter>.*).dat', self.file_name)
    self.lib = m.group('lib')
    self.threads_per_core = int(m.group('tpc'))
    self.iteration = int(m.group('iter'))
    lines = map(lambda x: x.strip().split(':'), file(f).readlines())
    self.vstats = map(lambda x: VcoreStats(x), lines)
    bdata.data.setdefault(self.lib, {})
    bdata.data[self.lib].setdefault(self.threads_per_core, {})
    bdata.data[self.lib][self.threads_per_core][self.iteration] = self

class VcoreStats:
  def __init__(self, line):
    self.num_vcores = int(line[0])
    self.tsc_freq = int(line[1])
    self.start_time = int(line[2])
    self.end_time = int(line[3])

def graph_ctxsps(bdata, config):
  title("Average Context Switches / Second (%d runs)" \
       % len(bdata.data.itervalues().next().itervalues().next()))
  xlabel("Number of Vcores in Use")
  ylabel("Million Context Switches / Second")
  for lib in config.libs.keys():
    if lib not in bdata.data.keys():
        continue
    if 'alias' in config.libs[lib]:
        libname = config.libs[lib]['alias']
    else:
        libname = lib
    ctxsps = []

    for tpc in sorted(bdata.data[lib]):
      for i in bdata.data[lib][tpc].keys():
        lpc = bdata.loops_per_thread / tpc
        c = map(lambda x: ((tpc * lpc * x.num_vcores**2) * x.tsc_freq) / (x.end_time - x.start_time), bdata.data[lib][tpc][i].vstats)
        ctxsps.append(c)
      avg_ctxsps = map(lambda x: np.mean(x) / 1000000, np.transpose(ctxsps))
      plot(range(1, len(avg_ctxsps)+1), avg_ctxsps, label=libname+'-%d-tpc' % tpc)
  legend(framealpha=0.5, loc='best')
  figname = config.output_folder + "/ctxs_per_sec.png"
  savefig(figname)
  clf()

def generate_graphs(parser, args):
  config = lambda:None
  if args.config_file:
    config.__dict__ = json.load(file(args.config_file), object_pairs_hook=OrderedDict)
    if args.input_folder:
      config.input_folder = args.input_folder
    if args.output_folder:
      config.output_folder = args.output_folder
  elif args.input_folder and args.output_folder:
    config.input_folder = args.input_folder
    config.output_folder = args.output_folder
  else:
    parser.print_help()
    exit(1)

  try:
    os.mkdir(config.output_folder)
  except:
    pass
  bdata = BenchmarkData(config)
  graph_ctxsps(bdata, config)

