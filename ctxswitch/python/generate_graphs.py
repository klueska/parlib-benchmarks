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
import numpy as np

class BenchmarkData:
  def __init__(self, config):
    input_folder = config.input_folder
    self.dir_name = os.path.basename(input_folder)
    m = re.match('ctxswitch-out-(?P<num_threads>.*)-(?P<num_loops>.*)', self.dir_name)
    self.num_threads = int(m.group('num_threads'))
    self.num_loops = int(m.group('num_loops'))
    self.files = glob.glob(input_folder + '/*')
    self.data = {}
    map(lambda x: FileData(x, self), self.files)

class FileData:
  def __init__(self, f, bdata):
    self.file_name = os.path.basename(f)
    m = re.match('(?P<lib>.*)-ctxswitch-out-(?P<iter>.*).dat', self.file_name)
    self.lib = m.group('lib')
    self.iteration = int(m.group('iter'))
    lines = map(lambda x: x.strip().split(':'), file(f).readlines())
    self.vstats = map(lambda x: VcoreStats(x), lines)
    bdata.data.setdefault(self.lib, {})
    bdata.data[self.lib][self.iteration] = self

class VcoreStats:
  def __init__(self, line):
    self.id = int(line[0])
    self.start_time = int(line[1])
    self.end_time = int(line[2])

def graph_ctxsps(bdata, config):
  title("Average Context Switches Per Second (%d runs)" % len(bdata.data.itervalues().next()))
  xlabel("Number of Vcores in Use")
  ylabel("Context Switches Per Second")
  for lib in bdata.data.keys():
    ctxsps = []
    for i in bdata.data[lib].keys():
      ctxs = bdata.num_threads * bdata.num_loops
      c = map(lambda x: ctxs / (x.end_time - x.start_time) * 1000000, bdata.data[lib][i].vstats)
      ctxsps.append(c)
    avg_ctxsps = map(lambda x: np.mean(x), np.transpose(ctxsps))
    plot(range(1, len(avg_ctxsps)+1), avg_ctxsps, label=lib)
  legend(framealpha=0.5, loc='best')
  figname = config.output_folder + "/ctxs_per_sec.png"
  savefig(figname)
  clf()

def generate_graphs(parser, args):
  config = lambda:None
  if args.config_file:
    config.__dict__ = json.load(file(args.config_file))
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

