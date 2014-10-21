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
from collections import OrderedDict

class BenchmarkData:
  def __init__(self, config):
    input_folder = config.input_folder
    self.dir_name = os.path.basename(input_folder)
    m = re.match('fixedwork-out-(?P<num_threads>.*)-(?P<num_loops>.*)-(?P<fake_work>.*)', self.dir_name)
    self.num_threads = int(m.group('num_threads'))
    self.num_loops = int(m.group('num_loops'))
    self.fake_work = int(m.group('fake_work'))
    self.files = glob.glob(input_folder + '/*')
    self.data = {}
    map(lambda x: FileData(x, self), self.files)

class FileData:
  def __init__(self, f, bdata):
    self.file_name = os.path.basename(f)
    m = re.match('(?P<lib>.*)-fixedwork-out-(?P<iter>.*).dat', self.file_name)
    self.lib = m.group('lib')
    self.iteration = int(m.group('iter'))
    lines = map(lambda x: x.strip().split(':'), file(f).readlines())
    self.tsc_freq = int(lines[0][0])
    self.prog_start = int(lines[0][1])
    self.prog_end = int(lines[0][2])
    self.tstats = map(lambda x: ThreadStats(x), lines[1:])
    bdata.data.setdefault(self.lib, {})
    bdata.data[self.lib][self.iteration] = self

class ThreadStats:
  def __init__(self, line):
    self.id = int(line[0])
    self.create_time = int(line[1])
    self.start_time = int(line[2])
    self.end_time = int(line[3])
    self.join_time = int(line[4])

def graph_compute_times(bdata, config):
  title("Total Compute Time Per Thread")
  xlabel("Thread Number (Ordered by Compute Time)")
  ylabel("Total Compute Time (ms)")
  for lib in bdata.data.keys():
    ticks = []
    for i in bdata.data[lib].keys():
      tsc_freq = bdata.data[lib][i].tsc_freq
      t = map(lambda x: x.end_time - x.start_time, bdata.data[lib][i].tstats)
      ticks.append(sorted(t))
    avg_ticks = map(lambda x: np.mean(x), np.transpose(ticks))
    avg_time = map(lambda x: x/tsc_freq*1000, avg_ticks)
    plot(range(len(avg_time)), avg_time, label=lib)
  legend(framealpha=0.5, loc='best')
  figname = config.output_folder + "/compute_times.png"
  savefig(figname)
  clf()

def graph_start_times(bdata, config):
  title("Start Time Per Thread")
  xlabel("Thread Number (Ordered by Start Time)")
  ylabel("Start Time (s)")
  for lib in bdata.data.keys():
    ticks = []
    for i in bdata.data[lib].keys():
      prog_start = bdata.data[lib][i].prog_start
      tsc_freq = bdata.data[lib][i].tsc_freq
      t = map(lambda x: x.start_time - prog_start, bdata.data[lib][i].tstats)
      ticks.append(sorted(t))
    avg_ticks = map(lambda x: np.mean(x), np.transpose(ticks))
    avg_time = map(lambda x: x/tsc_freq, avg_ticks)
    plot(range(len(avg_time)), avg_time, label=lib)
  legend(framealpha=0.5, loc='best')
  figname = config.output_folder + "/start_times.png"
  savefig(figname)
  clf()

def graph_completion_times(bdata, config):
  t = "Average Completion Time Per Thread (%d runs)\n" \
    + "%d Threads Running %d Million Iterations Each" 
  title(t % (len(bdata.data.itervalues().next()),
             bdata.num_threads,
             bdata.num_loops * bdata.fake_work / 1000000))
       
  xlabel("Thread Number")
  ylabel("Completion Time (s)")
  for lib in config.libs.keys():
    if 'alias' in config.libs[lib].keys():
      libname = config.libs[lib]['alias']
    else:
      libname = lib
    ticks = []
    for i in bdata.data[lib].keys():
      prog_start = bdata.data[lib][i].prog_start
      tsc_freq = bdata.data[lib][i].tsc_freq
      t = map(lambda x: x.end_time - prog_start, bdata.data[lib][i].tstats)
      ticks.append(sorted(t))
    avg_ticks = map(lambda x: np.mean(x), np.transpose(ticks))
    avg_time = map(lambda x: x/tsc_freq, avg_ticks)
    plot(range(len(avg_time)), avg_time, label=libname, linewidth=2,
         color=config.libs[lib]['color'])
    leg = legend(framealpha=0.5, prop={'size': 10}, 
                 loc=config.graphs['completion_times']['legend_loc'],
                 bbox_to_anchor=config.graphs['completion_times']['legend_bbox_to_anchor'])
  for legobj in leg.legendHandles:
    legobj.set_linewidth(6.0)
  figname = config.output_folder + "/completion_times.png"
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
  graph_compute_times(bdata, config)
  graph_start_times(bdata, config)
  graph_completion_times(bdata, config)

