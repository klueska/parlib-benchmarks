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
    m = re.match('(?P<lib>.*).dat', self.file_name)
    self.lib = m.group('lib')
    lines = file(f).readlines()

    # Set the data in the parent object
    bdata.data.setdefault(self.lib, {})

    # Build a regex for breaking the test into parts
    regex = re.compile("^Num Vcores: (?P<ncores>\d+)$")

    # Break the file into individual tests by ncores
    testlines = {}
    m = regex.search(lines[0])
    ncores = int(m.group('ncores'))
    startline = 1
    for i, l in enumerate(lines):
      m = regex.search(l)
      if m:
        data = map(lambda x: int(x.split()[0]), lines[startline:i])
        bdata.data[self.lib][ncores] = data
        ncores = int(m.group('ncores'))
        startline = i+1
    data = map(lambda x: int(x.split()[0]), lines[startline:])
    bdata.data[self.lib][ncores] = data

def graph_linux_throughput(bdata, config):
  colors = {
      "nginx" : "#396AB1",
      "linux-custom-sched" : "#CC2529",
      "linux-native" : "#3E9651",
      "linux-upthread" : "#948B3D",
  }

  libs = ['nginx', 'linux-custom-sched', 'linux-upthread', 'linux-native']
  for lib in libs:
    data = bdata.data[lib]
    ncores = range(1,33)
    rps_avg = [0]*32
    rps_std = [0]*32
    for n in data:
      rps_avg[n-1] = np.mean(data[n][-10:])
      rps_std[n-1] = np.var(data[n][-10:])
    label = lib
    if label == 'linux-native':
      label = 'linux-NPTL'
    label = "%s%s" % ("" if label == 'nginx' else 'kweb-', label)
    plot(ncores, rps_avg, label=label, linewidth=4, color=colors[lib])

  title('Average Webserver Throughput')
  ylabel('Requests / Second')
  xlabel('Number of Cores')
  leg = legend(loc='center right')
  for legobj in leg.legendHandles:
    legobj.set_linewidth(10.0)
  leg.get_title().set_fontsize(14)
  figname = config.output_folder + "/linux-throughput.png"
  savefig(figname, bbox_inches="tight")
  clf()

def graph_akaros_throughput(bdata, config):
  colors = {
      "akaros" : "#396AB1",
      "akaros-custom-sched" : "#CC2529",
      "linux-native" : "#3E9651",
  }

  libs = ['akaros', 'akaros-custom-sched', 'linux-native']
  for lib in libs:
    data = bdata.data[lib]
    ncores = range(1,17)
    rps_avg = [0]*16
    rps_std = [0]*16
    for n in data:
      if lib == 'akaros-custom-sched':
        __n = n/2 - 1
      else :
        __n = n - 1
      if __n >= 16:
        continue
      rps_avg[__n] = np.mean(data[n][-10:])
      rps_std[__n] = np.var(data[n][-10:])
    label = lib
    if label == 'linux-native':
      label = 'linux-NPTL'
    label = "%s%s" % ('kweb-', label)
    plot(ncores, rps_avg, label=label, linewidth=4, color=colors[lib])

  title('Average Webserver Throughput')
  ylabel('Requests / Second')
  xlabel('Number of Cores')
  leg = legend(loc='lower right')
  for legobj in leg.legendHandles:
    legobj.set_linewidth(10.0)
  leg.get_title().set_fontsize(14)
  figname = config.output_folder + "/akaros-throughput.png"
  savefig(figname, bbox_inches="tight")
  clf()

def kweb_graphs(parser, args):
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
  graph_linux_throughput(bdata, config)
  graph_akaros_throughput(bdata, config)
  #graph_speedup(bdata, config)
  #graph_mops(bdata, config)
  #graph_mopspt(bdata, config)

