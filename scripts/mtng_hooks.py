
import sim
import os, sys

# Snapshot marked by markers
class Snapshot:
  def __init__(self,ncore):
    self.marker = -1
    self.cpufreqs = [0]*ncore

class Mtng:
  def __init__(self):
    self.snapshots = []
    self.isMarkerStarted = False
    # self.curSnapshot = Snapshot(sim.config.ncore)
    self.regionCnt = 0

  def setup(self, args):
    args = (args or '').split(':')
    self.write_terminal = 'verbose' in args
    self.write_markers = 'markers' in args
    self.write_stats = 'stats' in args
    self.outputFile = open(args[-1],'w')


  # When sift frontend calls a cpufreq change, the backend will call pyHook.
  def hook_cpufreq_change(self, thread):
    print ("[MTNG PY_HOOK] hook_cpufreq_change called")
    # print thread

    if self.write_terminal:
      print '[SCRIPT] Magic marker from thread %d: a = %d,' % (thread, a),
      if s:
        print 'str = %s' % s
      else:
        print 'b = %d' % b

    if self.write_markers:
      sim.stats.marker(thread, core, a, b, s)

    # Pass in 'stats' as option to write out statistics at each magic marker
    # $ run-sniper -s markers:stats
    # This will allow for e.g. partial CPI stacks of specific code regions:
    # $ tools/cpistack.py --partial=marker-1-4:marker-2-4

    # if self.write_stats:
      # sim.stats.write('mtng-cpufreq-%d-%d' % (a, b)) 
        

  # When sift frontend calls a marker, the backend will call pyHook.
  def hook_magic_marker(self, thread, core, a, b, s):
    if self.write_terminal:
      print '[SCRIPT] Magic marker from thread %d: a = %d,' % (thread, a),
      if s:
        print 'str = %s' % s
      else:
        print 'b = %d' % b

    if self.write_markers:
      sim.stats.marker(thread, core, a, b, s)

    # Pass in 'stats' as option to write out statistics at each magic marker
    # $ run-sniper -s markers:stats
    # This will allow for e.g. partial CPI stacks of specific code regions:
    # $ tools/cpistack.py --partial=marker-1-4:marker-2-4

    # if self.write_stats:
      # sim.stats.write('marker-%d-%d' % (a, b))


    self.isMarkerStarted = 1 - self.isMarkerStarted # toggle

    # Read cpu freq
    if (not self.isMarkerStarted):
      # This is an end marker, save the last snapshot
      snapshot = Snapshot(sim.config.ncores)
      for i in range(sim.config.ncores):
        snapshot.cpufreqs[i] = sim.dvfs.get_frequency(i)
      snapshot.marker = self.regionCnt
      self.snapshots.append(snapshot)
      print "Pushing snapshot of region ", self.regionCnt
      print snapshot.cpufreqs
      self.regionCnt += 1

      # For now, write to a separate outputfile
      self.outputFile.write("%d " % snapshot.marker)
      for f in snapshot.cpufreqs:
        self.outputFile.write("%d " % f)
      self.outputFile.write('\n')





sim.util.register(Mtng())
