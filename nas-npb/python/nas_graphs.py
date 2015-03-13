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
import matplotlib.ticker as ticker

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
    testlines.setdefault(test, [])
    testlines[test].append(lines[startline:])

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

def get_absolute_metrics(bdata, metric):
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
  absolute_metric = {}
  for n in avg_metrics:
    for test in avg_metrics[n]:
      t1 = avg_metrics[n][test]['upthread']
      t2 = avg_metrics[n][test]['native']
      absolute_metric.setdefault(n, {})
      absolute_metric[n][test] = [t1, t2]
  def keyfunc(x):
    t1 = absolute_metric[ncores[0]][x][0]
    t2 = absolute_metric[ncores[0]][x][1]
    s = t2/t1 - 1
    return s
  tests = sorted(absolute_metric[ncores[0]], key=keyfunc, reverse=True)
  absolute_metrics0 = [ absolute_metric[ncores[0]][t] for t in tests ]
  absolute_metrics1 = [ absolute_metric[ncores[1]][t] for t in tests ]
  return [tests, absolute_metrics0, absolute_metrics1]

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

def graph_runtime(bdata, config):
  tests, runtimes0, runtimes1 = get_absolute_metrics(bdata, 'time')

  margin = 0.2
  ind = np.arange(len(tests))
  colors = ["#396AB1", "#CC2529", "#3E9651", "#948B3D"]

  def autolabel(ax, y, factor, va):
    p = ax.text(0.5 + ind[i], y*(1 + factor), "%.1fs"%float(y), size="12", ha='center', va=va)
    return p

  fig, ax = plt.subplots()
  axes = [ax] + map(lambda x: ax.twinx(), range(len(tests) - 1))
  ax = axes[0]
  for i, t in enumerate(tests):
    p0 = axes[i].hlines(runtimes0[i][1], margin + ind[i], margin + ind[i] + (1 - 2*margin), linewidth=8, color=colors[0])
    p1 = axes[i].hlines(runtimes0[i][0], margin + ind[i], margin + ind[i] + (1 - 2*margin), linewidth=8, color=colors[1])
    p2 = axes[i].hlines(runtimes1[i][1], margin + ind[i], margin + ind[i] + (1 - 2*margin), linewidth=8, color=colors[2])
    p3 = axes[i].hlines(runtimes1[i][0], margin + ind[i], margin + ind[i] + (1 - 2*margin), linewidth=8, color=colors[3])
    maxy = max(runtimes0[i][0], runtimes0[i][1], runtimes1[i][0], runtimes1[i][1])
    miny = min(runtimes0[i][0], runtimes0[i][1], runtimes1[i][0], runtimes1[i][1])
    axes[i].set_ylim(0, 1.10*maxy)
    autolabel(axes[i], maxy, 0.03, 'bottom')
    autolabel(axes[i], miny, -0.03, 'top')

  axes[-1].yaxis.set_label_position("left")
  plt.title('Average Runtime of NAS Parallel Benchmarks', fontsize=20)
  plt.ylabel('Average Runtime (s)', fontsize=18)
  ax.xaxis.set_major_formatter(ticker.NullFormatter())
  ax.xaxis.set_minor_locator(ticker.FixedLocator(0.5 + np.arange(len(tests))))
  ax.xaxis.set_minor_formatter(ticker.FixedFormatter(tests))
  for tick in ax.xaxis.get_minor_ticks():
      tick.tick1line.set_markersize(0)
      tick.tick2line.set_markersize(0)
      tick.label.set_fontsize(18)
  for axis in axes[1:]:
    axis.get_yaxis().set_ticks([])
  ax.get_yaxis().set_ticklabels([0])

  ps = [p0, p1, p2, p3]
  labels = [
    "Linux-NPTL (32 cores)",
    "upthreads (32 cores)",
    "Linux-NPTL (16 cores)",
    "upthreads (16 cores)",
  ]
  leg = plt.legend(ps, labels, loc="lower center")
  for legobj in leg.legendHandles:
    legobj.set_linewidth(15.0)
  figname = config.output_folder + "/nas-runtimes.png"
  savefig(figname, bbox_inches="tight")
  clf()

def graph_speedup(bdata, config):
  tests, speedups0, speedups1 = get_relative_metrics(bdata, 'time')

  margin = 0.0
  width = 1.0
  ind = np.arange(len(tests))
  colors = ['#396AB1', '#CC2529']#, '#3E9651', 'yellow', '#DA7C30']

  def autolabel(ax, rects):
    for rect in rects:
      x = rect.get_x() + rect.get_width()/2.
      y = rect.get_height()
      offset = 0.005
      va = 'bottom'
      if rect.get_y() < 0:
        y = -y
        offset = -offset
        va = 'top'
      ax.text(x, y+offset, '%.1f%%'%(100*float(y)), size="14", ha='center', va=va)

  fig, ax = plt.subplots()
  p0 = ax.bar(margin + ind, speedups0, width, color=colors[0])
  p1 = ax.bar(margin + ind, speedups1, width, color=colors[1])
  autolabel(ax, p0)
  autolabel(ax, p1)
  ls = ['Full Hyperthreads (32 cores)', 'No Hyperthreads (16 cores)']
  title('Average Speedup of NAS Parallel Benchmarks', fontsize=20)
  ylabel('Percent Speedup (%)', fontsize=18)
  yticks(ax.get_yticks(), map(lambda x: "%s%%" % (x*100), ax.get_yticks()), fontsize=18)
  ax.xaxis.set_major_formatter(ticker.NullFormatter())
  ax.xaxis.set_minor_locator(ticker.FixedLocator(0.5 + np.arange(len(tests))))
  ax.xaxis.set_minor_formatter(ticker.FixedFormatter(tests))
  for tick in ax.xaxis.get_minor_ticks():
      tick.tick1line.set_markersize(0)
      tick.tick2line.set_markersize(0)
      tick.label.set_fontsize(18)
  x1,x2,y1,y2 = ax.axis()
  ax.axis((x1,x2,y1,0.85))
  ax.legend([p0, p1], ls, loc='best', fontsize=18)
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
  ls = ['Full Hyperthreads (32 cores)', 'No Hyperthreads (16 cores)']
  title('Average Speedup of NAS Parallel Benchmarks')
  ylabel('Percent Speedup (%)')
  yticks(ax.get_yticks(), map(lambda x: "%s%%" % (x*100), ax.get_yticks()))
  ax.xaxis.set_major_formatter(ticker.NullFormatter())
  ax.xaxis.set_minor_locator(ticker.FixedLocator(0.5 + np.arange(len(tests))))
  ax.xaxis.set_minor_formatter(ticker.FixedFormatter(tests))
  for tick in ax.xaxis.get_minor_ticks():
      tick.tick1line.set_markersize(0)
      tick.tick2line.set_markersize(0)
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
  ls = ['Full Hyperthreads (32 cores)', 'No Hyperthreads (16 cores)']
  title('Average Speedup of NAS Parallel Benchmarks')
  ylabel('Percent Speedup (%)')
  yticks(ax.get_yticks(), map(lambda x: "%s%%" % (x*100), ax.get_yticks()))
  ax.xaxis.set_major_formatter(ticker.NullFormatter())
  ax.xaxis.set_minor_locator(ticker.FixedLocator(0.5 + np.arange(len(tests))))
  ax.xaxis.set_minor_formatter(ticker.FixedFormatter(tests))
  for tick in ax.xaxis.get_minor_ticks():
      tick.tick1line.set_markersize(0)
      tick.tick2line.set_markersize(0)
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
  graph_runtime(bdata, config)
  graph_speedup(bdata, config)
  graph_mops(bdata, config)
  graph_mopspt(bdata, config)

