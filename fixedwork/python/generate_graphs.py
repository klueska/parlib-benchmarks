#!/usr/bin/arch -i386 python2.7
#
# Author: Kevin Klues <klueska@cs.berkeley.edu>

import os
import re
import glob
import matplotlib
import matplotlib as mpl
matplotlib.use('Agg')
from pylab import *
import numpy as np

class BenchmarkData:
  def __init__(self, input_folder):
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

def graph_compute_times(bdata, folder):
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
  legend()
  figname = folder + "/compute_times.png"
  savefig(figname)
  clf()

def graph_start_times(bdata, folder):
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
  legend()
  figname = folder + "/start_times.png"
  savefig(figname)
  clf()

def graph_completion_times(bdata, folder):
  title("Completion Time Per Thread")
  xlabel("Thread Number (Ordered by CompletionTime)")
  ylabel("Completion Time (s)")
  for lib in bdata.data.keys():
    ticks = []
    for i in bdata.data[lib].keys():
      prog_start = bdata.data[lib][i].prog_start
      tsc_freq = bdata.data[lib][i].tsc_freq
      t = map(lambda x: x.end_time - prog_start, bdata.data[lib][i].tstats)
      ticks.append(sorted(t))
    avg_ticks = map(lambda x: np.mean(x), np.transpose(ticks))
    avg_time = map(lambda x: x/tsc_freq, avg_ticks)
    plot(range(len(avg_time)), avg_time, label=lib)
  legend(loc='center right')
  figname = folder + "/completion_times.png"
  savefig(figname)
  clf()

def generate_graphs(args):
  try:
    os.mkdir(args.output_folder)
  except:
    pass
  bdata = BenchmarkData(args.input_folder)
  graph_compute_times(bdata, args.output_folder)
  graph_start_times(bdata, args.output_folder)
  graph_completion_times(bdata, args.output_folder)

