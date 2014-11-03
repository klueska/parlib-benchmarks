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
      test, num_cores, tpc, duration = lines[i]
      num_cores = int(num_cores)
      tpc = int(tpc)
      duration = int(duration)
      t = TestData(test, num_cores, tpc, duration, lines[i+1 : i+1 + num_cores])
      bdata.data.setdefault(self.label, {})
      bdata.data[self.label].setdefault(test, {})
      bdata.data[self.label][test].setdefault(num_cores, {})
      bdata.data[self.label][test][num_cores][tpc] = t
      i += 1 + num_cores

class TestData:
  def __init__(self, name, num_cores, tpc, duration, lines):
    self.name = name
    self.num_cores = num_cores
    self.tpc = tpc 
    self.duration = duration
    self.vstats = map(lambda x: VcoreStats(x), lines)

class VcoreStats:
  def __init__(self, line):
    self.vcoreid = int(line[0])
    self.tsc_freq = int(line[1])
    self.beg_time = int(line[2])
    self.end_time = int(line[3])
    self.count = int(line[4])

def data_transform(bdata, config, transform):
  test = 'ctxswitch'
  lats = {}
  for l in bdata.data:
    for n in bdata.data[l][test]:
      for tpc in bdata.data[l][test][n]:
        lat = map(transform, bdata.data[l][test][n][tpc].vstats)
        lats.setdefault(l, {})
        lats[l].setdefault(n, {})
        lats[l][n][tpc] = lat
  return lats

def relative_transform(bdata, config, transform):
  lats = data_transform(bdata, config, transform)
  rlats = {}
  for l in [l for l in bdata.data if l != 'native-pthread']:
    for n in lats[l]:
      for tpc in lats[l][n]:
        rlats.setdefault(l, {})
        rlats[l].setdefault(n, {})
        if l in config.measurement_order:
          curr = config.measurement_order.index(l)
          if curr > 0:
            lprev = config.measurement_order[curr - 1]
            rlats[l][n][tpc] = [i - j for i, j in zip(lats[l][n][tpc], lats[lprev][n][tpc])]
          else:
            rlats[l][n][tpc] = lats[l][n][tpc]
  rlats['native-pthread'] = lats['native-pthread']
  return rlats

def graph_stacked(bdata, config):
  libs = ['raw-ctxswitch', 'handle-events', 'use-queue-locks',
          'swap-tls (wrfsbase)', 'save-fpstate']
  num_cores = [1, 6, 12, 24]
  tpc = 1

  margin = 0.10
  width = 0.40
  ind = np.arange(len(num_cores))
  colors = ['#396AB1', '#CC2529', '#3E9651', 'yellow', '#DA7C30']

  def autolabel(rects, es, offset=0.2):
    for rect, y in zip(rects, es.get_children()[2].get_ydata()):
      plt.text(rect.get_x()+rect.get_width()/2., y + offset,
               '%dns'%int(y), size="10", ha='center', va='bottom')

  #cyclecalc = lambda x: 1.0*(x.end_time - x.beg_time)/(x.count)
  latcalc = lambda x: 1e9/x.tsc_freq*(x.end_time - x.beg_time)/(x.count)
  rdata = relative_transform(bdata, config, latcalc)

  ps = []
  labels = []
  for i, l in enumerate(libs):
    avgs = []
    vars = []
    for n in num_cores:
      lats = rdata[l][n][tpc]
      avgs.append(np.mean(lats))
      vars.append(np.std(lats))

    bottoms = np.sum(np.array(map(lambda p: map(lambda r: r.get_height(), p), ps)), axis=0) if len(ps) else None
    p = plt.bar(margin + ind, avgs, width, bottom=bottoms, label=l, color=colors[i])
    if i > 2:
      e = plt.errorbar(margin + ind + width/2, avgs+bottoms, yerr=vars, fmt=None, ecolor='k', lw=2, capsize=5, capthick=2)
      autolabel(p, e, 0)
    ps.append(p)
    labels.append(l)
  leg = legend(reversed(ps), reversed(labels), title="upthread", loc="upper left")
  legtitle = leg.get_title()
  legtitle.set_fontsize(14)
  gca().add_artist(leg)

  l = 'native-pthread'
  colors = ["#EEE8AA", '#C0C0C0']
  tpcs = [2, 1]
  ps = []
  labels = []
  for i, tpc in enumerate(tpcs):
    avgs = []
    vars = []
    for n in num_cores:
      lats = rdata[l][n][tpc]
      avgs.append(np.mean(lats))
      vars.append(np.std(lats))
    p = plt.bar(margin + ind + width, avgs, width, label=l, color=colors[i])
    e = plt.errorbar(margin + ind + width + width/2, avgs, yerr=vars, fmt=None, ecolor='k', lw=2, capsize=5, capthick=2)
    ps.append(p)
    labels.append("%d thread%s/core" % (tpc, 's' if tpc > 1 else ''))
    autolabel(p, e, 0)
  leg = legend(ps, labels, title="native-pthread", bbox_to_anchor=[0.85, 1])
  legtitle = leg.get_title()
  legtitle.set_fontsize(14)
  x1,x2,y1,y2 = plt.axis()
  plt.axis((x1,x2,y1,1250))

  title('Average Context Switch Latency (Per Core)')
  ylabel('Context Switch Latency (ns)')
  xticks(margin + ind + 2*width/2, ("Single Core", "1 Socket\nAll cores\nNo SMT",
                     "2 Sockets\nAll Cores\nNo SMT", "2 Sockets\nAll Cores\nFull SMT"))
  figname = config.output_folder + "/ctxswitch-stacked.png"
  savefig(figname)
  clf()

def graph_tpceffect(bdata, config):
  tpcalc = lambda x: 1.0*x.count*x.tsc_freq/(x.end_time - x.beg_time)/1e6
  tdata = data_transform(bdata, config, tpcalc)
  upthread_data = tdata['full-long']
  pthread_data = tdata['native-pthread']
  data = {
    'upthread': upthread_data,
    'native-pthread': pthread_data
  }
  colors = ["#396AB1", "#CC2529", "#3E9651", "#948B3D",
            "#DA7C30", "#535154", "#922428"]

  sums = OrderedDict()
  for l in data:
    for n in sorted(data[l].keys()):
      for tpc in sorted(data[l][n].keys()):
        lats = data[l][n][tpc]
        sums.setdefault(l, OrderedDict())
        sums[l].setdefault(tpc, OrderedDict())
        sums[l][tpc][n] = np.sum(lats)

  legs = []
  leg_position = {
    'upthread': [1,0.75],
    'native-pthread': [1,0.25]
  }
  for i, l in enumerate(sums):
    ps = []
    labels = []
    for j, tpc in enumerate(sums[l]):
      if tpc < 128:
        p = plot(sums[l][tpc].keys(), sums[l][tpc].values(), linewidth=6, color=colors[j])
        ps.append(p[0])
        labels.append("%d thread%s/core" % (tpc, 's' if tpc > 1 else ''))
    leg = legend(ps, labels, loc='center left', title=l, bbox_to_anchor=leg_position[l])
    for legobj in leg.legendHandles:
      legobj.set_linewidth(10.0)
    legtitle = leg.get_title()
    legtitle.set_fontsize(14)
    legs.append(leg)
    if i == 0:
      gca().add_artist(leg)

  title('Total Context Switches / Second ')
  ylabel('Millions of Context Switches / Second')
  xlabel('Number of Cores')
  figname = config.output_folder + "/ctxswitch-tpceffects.png"
  savefig(figname, bbox_extra_artists=legs, bbox_inches="tight")
  clf()

def ctxswitch_graphs(parser, args):
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
  graph_stacked(bdata, config)
  graph_tpceffect(bdata, config)

