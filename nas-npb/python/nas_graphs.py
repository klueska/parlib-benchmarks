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
import pprint

class BenchmarkData:
  def __init__(self, config):
    input_folder = config.input_folder
    self.files = glob.glob(input_folder + '/*.dat')
    self.data = {}
    map(lambda x: FileData(x, self), self.files)

class FileData:
  def __init__(self, f, bdata):
    self.file_name = os.path.basename(f)
    m = re.match('(?P<lib>.*)-(?P<test>.*)-(?P<class>.*)-(?P<ncores>.*)-(?P<iters>.*).dat', self.file_name)
    self.lib = m.group('lib')
    self.ncores = int(m.group('ncores'))
    lines = file(f).readlines()

    # Set the data in the parent object
    bdata.data.setdefault(self.lib, {})
    bdata.data[self.lib].setdefault(self.ncores, {})

    # Build a regex for breaking the test into parts
    regex = re.compile("^bin\/(?P<test>.*)\.(?P<class>.*)\..*$")

    # Strip out all the compile stuff
    startline = 0
    test = ""
    for i, l in enumerate(lines):
      m = regex.search(l)
      if m:
        test = m.group('test')
        startline = i
        break

    # Break the file into individual tests
    testlines = {}
    for i, l in enumerate(lines):
      m = regex.search(l)
      if m:
        testlines.setdefault(test, [])
        testlines[test].append(lines[startline:i])
        test = m.group('test')
        startline = i

    # Parse each set of tests
    tregex = re.compile("^\s*Time in seconds\s*=\s*(?P<time>.*)$")
    mregex = re.compile("^\s*Mop/s total\s*=\s*(?P<mops>.*)$")
    mptregex = re.compile("^\s*Mop/s/thread\s*=\s*(?P<mopspt>.*)$")
    tdata = bdata.data[self.lib][self.ncores]
    for t in testlines.keys():
        for test in testlines[t]:
            tdata.setdefault(t, {})
            for l in test:
                m = tregex.search(l)
                if m:
                    tdata[t].setdefault('time', [])
                    tdata[t]['time'].append(float(m.group('time')))
                m = mregex.search(l)
                if m:
                    tdata[t].setdefault('mops', [])
                    tdata[t]['mops'].append(float(m.group('mops')))
                m = mptregex.search(l)
                if m:
                    tdata[t].setdefault('mopspt', [])
                    tdata[t]['mopspt'].append(float(m.group('mopspt')))

def get_relative_metrics(bdata, metric):
  avg_metrics = {}
  ncores = []
  for lib in bdata.data:
    for n in bdata.data[lib]:
      ncores.append(n) if n not in ncores else None
      for test in bdata.data[lib][n]:
        data = bdata.data[lib][n][test][metric]
        avg_metrics.setdefault(n, {})
        avg_metrics[n].setdefault(test, {})
        avg_metrics[n][test][lib] = np.mean(data)
  ncores.sort()
  relative_metric = {}
  for n in avg_metrics:
    for test in avg_metrics[n]:
      t1 = avg_metrics[n][test]['upthread']
      t2 = avg_metrics[n][test]['native']
      s = t2/t1 - 1
      relative_metric.setdefault(n, {})
      relative_metric[n][test] = s
  tests = sorted(relative_metric[ncores[0]], key=relative_metric[ncores[0]].__getitem__, reverse=True)
  relative_metrics0 = [ relative_metric[ncores[0]][t] for t in tests ]
  relative_metrics1 = [ relative_metric[ncores[1]][t] for t in tests ]
  return [tests, relative_metrics0, relative_metrics1]


def graph_speedup(bdata, config):
  tests, speedups0, speedups1 = get_relative_metrics(bdata, 'time')

  margin = 0.0
  width = 1.0
  ind = np.arange(len(tests))
  colors = ['#396AB1', '#CC2529']#, '#3E9651', 'yellow', '#DA7C30']

  fig, ax = plt.subplots()
  p0 = ax.bar(margin + ind, speedups0, width, color=colors[0])
  p1 = ax.bar(margin + ind, speedups1, width, color=colors[1])
  ls = ['Full SMT', 'No SMT']
  title('Average Speedup of NAS Parallel Benchmarks (20 runs)') 
  ylabel('Percent Speedup (%)')
  xticks(margin + ind + width/2, tests)
  yticks(ax.get_yticks(), map(lambda x: "%s%%" % (x*100), ax.get_yticks()))
  ax.legend([p0, p1], ls, loc='best')
  figname = config.output_folder + "/nas-speedup.png"
  savefig(figname, bbox_inches="tight")
  clf()

def graph_mops(bdata, config):
  tests, speedups0, speedups1 = get_relative_metrics(bdata, 'mops')

  margin = 0.0
  width = 1.0
  ind = np.arange(len(tests))
  colors = ['#396AB1', '#CC2529']#, '#3E9651', 'yellow', '#DA7C30']

  fig, ax = plt.subplots()
  p0 = ax.bar(margin + ind, speedups0, width, color=colors[0])
  p1 = ax.bar(margin + ind, speedups1, width, color=colors[1])
  ls = ['Full SMT', 'No SMT']
  title('Average Speedup of NAS Parallel Benchmarks (20 runs)') 
  ylabel('Percent Speedup (%)')
  xticks(margin + ind + width/2, tests)
  yticks(ax.get_yticks(), map(lambda x: "%s%%" % (x*100), ax.get_yticks()))
  ax.legend([p0, p1], ls, loc='best')
  figname = config.output_folder + "/nas-mops.png"
  savefig(figname, bbox_inches="tight")
  clf()

def graph_mopspt(bdata, config):
  tests, speedups0, speedups1 = get_relative_metrics(bdata, 'mopspt')

  margin = 0.0
  width = 1.0
  ind = np.arange(len(tests))
  colors = ['#396AB1', '#CC2529']#, '#3E9651', 'yellow', '#DA7C30']

  fig, ax = plt.subplots()
  p0 = ax.bar(margin + ind, speedups0, width, color=colors[0])
  p1 = ax.bar(margin + ind, speedups1, width, color=colors[1])
  ls = ['Full SMT', 'No SMT']
  title('Average Speedup of NAS Parallel Benchmarks (20 runs)') 
  ylabel('Percent Speedup (%)')
  xticks(margin + ind + width/2, tests)
  yticks(ax.get_yticks(), map(lambda x: "%s%%" % (x*100), ax.get_yticks()))
  ax.legend([p0, p1], ls, loc='best')
  figname = config.output_folder + "/nas-mopspt.png"
  savefig(figname, bbox_inches="tight")
  clf()

def nas_graphs(parser, args):
  config = lambda:None
  if args.config_file:
    config.__dict__ = json.load(file(args.config_file), object_pairs_hook=OrderedDict)
  else:
    parser.print_help()
    exit(1)

  try:
    os.mkdir(config.output_folder)
  except:
    pass
  bdata = BenchmarkData(config)
  graph_speedup(bdata, config)
  graph_mops(bdata, config)
  graph_mopspt(bdata, config)

