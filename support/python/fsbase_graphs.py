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
    self.files = config.input_files
    self.data = {}
    map(lambda x: FileData(x, self), self.files.items())

class FileData:
  def __init__(self, f, bdata):
    self.label = os.path.basename(f[0])
    self.file_name = os.path.basename(f[1])
    lines = map(lambda x: x.strip().split(':'), file(f[1]).readlines())
    i = 0
    while (i < len(lines)):
      test, num_cores, duration = lines[i]
      num_cores = int(num_cores)
      duration = int(duration)
      t = TestData(test, num_cores, duration, lines[i+1 : i+1 + num_cores*2])
      bdata.data.setdefault(self.label, {})
      bdata.data[self.label].setdefault(test, {})
      bdata.data[self.label][test][num_cores] = t
      i += 1 + num_cores*2

class TestData:
  def __init__(self, name, num_cores, duration, lines):
    self.name = name
    self.num_cores = num_cores
    self.duration = duration
    self.rdstats = map(lambda x: VcoreStats(x), lines[:num_cores])
    self.wrstats = map(lambda x: VcoreStats(x), lines[num_cores:])

class VcoreStats:
  def __init__(self, line):
    self.scmc = line[0]
    self.rdwr = line[1]
    self.vcoreid = int(line[2])
    self.tsc_freq = int(line[3])
    self.beg_time = int(line[4])
    self.end_time = int(line[5])
    self.count = int(line[6])

def graph_rdwrfsbase_helper(bdata, config, which):
  margin = 0.05
  width = 0.30
  ind = np.arange(4)
  colors = ['#CC2529', '#396AB1', '#3E9651']

  def autolabel(rects, es, offset=0.2):
    for rect, y in zip(rects, es.get_children()[2].get_ydata()):
      plt.text(rect.get_x()+rect.get_width()/2., y + offset,
               '%d'%int(y), size="10", ha='center', va='bottom')

  i = 0
  order = [0, 2, 1]
  ps = [0]*3
  labels = [0]*3
  for l in bdata.data.keys():
    for t in bdata.data[l].keys():
      avgs = []
      vars = []
      for v in sorted(bdata.data[l][t].keys()):
        data = bdata.data[l][t][v]
        lat = map(lambda x: 1.0*(x.end_time - x.beg_time)
		                    /(x.count), eval('data.' + which))
        avgs.append(np.mean(lat))
        vars.append(np.std(lat))
      p = plt.bar(margin + ind + order[i]*width, avgs, width, color=colors[i], label=t)
      e = plt.errorbar(margin + ind + order[i]*width + width/2, avgs, yerr=vars, fmt=None, ecolor='k', lw=2, capsize=5, capthick=2)
      autolabel(p, e, 5)
      ps[order[i]] = p
      labels[order[i]] = t
      i += 1

  ylabel('Cycles per operation')
  xticks(margin + ind + 3.0/2*width, ("Single Core", "1 Socket\nAll cores\nNo SMT",
                     "2 Sockets\nAll Cores\nNo SMT", "2 Sockets\nAll Cores\nFull SMT"))
  return ps, labels

def graph_rdfsbase(bdata, config):
  ps, labels = graph_rdwrfsbase_helper(bdata, config, 'rdstats')
  labels[0] = "rdfsbase"
  labels[1] += "-3.2.0-49-generic"
  labels[2] += "-3.13.0-32-generic"

  title('Average Cycles to Read the TLS Base Register')
  legend(ps, labels, loc="upper left")
  figname = config.output_folder + "/rdfsbase.png"
  savefig(figname)
  clf()

def graph_wrfsbase(bdata, config):
  ps, labels = graph_rdwrfsbase_helper(bdata, config, 'wrstats')
  labels[0] = "wrfsbase"
  labels[1] += "-3.2.0-49-generic"
  labels[2] += "-3.13.0-32-generic"

  title('Average Cycles to Write the TLS Base Register')
  legend(ps, labels, loc="upper left")
  figname = config.output_folder + "/wrfsbase.png"
  savefig(figname)
  clf()

def fsbase_graphs(parser, args):
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
  graph_rdfsbase(bdata, config)
  graph_wrfsbase(bdata, config)

