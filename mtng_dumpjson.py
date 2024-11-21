#!/usr/bin/env python2
import os, sys

from mtng_dumpstats import PerformStats
# class to hold representative stats
import json
import argparse
# dumpstats for Detailed Running
#from tqdm import tqdm
import numpy as np
import sys
from numpy import linalg as LA
#from multiprocessing import Pool, cpu_count
from multiprocessing import  cpu_count
from multiprocessing.pool import ThreadPool as Pool
FastForwardMode = 1
DetailMode = 0

clusters = dict()

#threshold_in = 0.1
threshold_in = 0.5

class LocalPerformStats:
    def __init__(self, time0,insts ):
        self.time0 = time0
        self.instruction_count_per_core = insts
class SegmentData:
    def __init__(self, record1,record2):
        ##( record type, sim mode, pc, sim time, instrs)
        self.sim_mode = record1[1]
        self.segmentType = record1[0]
        self.pc = record1[2]
        time0 = record2[3] - record1[3]
        insts = np.array(record2[4]) - np.array(record1[4])
        insts = list(insts)
        stats = LocalPerformStats( time0,insts )
        if( not self.sim_mode == FastForwardMode ) : 
            clusters[ self.pc ] = stats
        self.stats = stats
    def get_time(self):
        
        if self.sim_mode == FastForwardMode:
    #self.walltime = max(stats.walltime_per_core)/1e6
            stats_template = clusters[ self.pc ]
            stats = self.stats
#            spi = ( float(stats_template.time0)) / sum(stats_template.instruction_count_per_core)
#            time = spi * sum( stats.instruction_count_per_core )
            diff = float( sum( stats.instruction_count_per_core ) ) / float( sum(stats_template.instruction_count_per_core) )
            if diff <= 1.2:
                time = stats_template.time0 * diff
            else:
                time = stats_template.time0
#            time1=time/1e15
#            time2=stats_template.time0/1e15
#            if(time1 >= 0.01 or time2>=0.01):
#                print ("{:.2f}".format(time1) ,  "{:.2f}".format(time2) )

        else:
            stats = self.stats
            time = stats.time0
        return time

class MtngDumpStats:

  def __init__(self, config):
    self.config = config;
    sniper_lib_path = "%(sniper_root)s/tools" % self.config
    if ( sniper_lib_path not in sys.path):
      # so that we can import sniper_lib
      sys.path.insert(0, "%(sniper_root)s/tools" % self.config)
    if "only-full-time" in self.config:
      self.only_full_time = config["only-full-time"]
    else:
      self.only_full_time = False
    self.stats_per_region = []
    self.region_count = 0
    self.total_time = 0
    if "sniper_file_enable" in config:
        self.sniper_file_enable = config["sniper_file_enable"]
    else:
        self.sniper_file_enable = True
    self.populate_cluster_stats()
    self.time_each_feature_level = [ 0 ] * 5
    self.num_each_feature_level = [ 0 ] * 5
    return
  def populate_cluster_stats(self):
############init sim mode and segment type
    with open( "%(output_sniper_validate_dir)s/intra_mtng.json"%self.config ) as f:
        json_raw = f.read()
        record = json.loads(json_raw)
    ##( record type, sim mode, pc)
    records = record
    if "flush" in self.config and self.config["flush"]:
        self.record = record
    self.feature_stack=records['feature_stack']
    if "time_prediction" in records:
        self.predicttime=records['time_prediction']
    else:
        self.predicttime = 0
    if "time_cluster" in records:
        self.clustertime=records['time_cluster']
    else:
        self.clustertime = 0
    if "time_sum" in record:
        total_time = records['time_sum']
    else:
        total_time = 0

    import sniper_lib
    import sniper_stats
    jobid = 0
    resultsdir = "%(output_sniper_validate_dir)s/"%self.config
    if self.sniper_file_enable:
        stats = sniper_stats.SniperStats( resultsdir = resultsdir, jobid = jobid )
#    stats = sniper_lib.get_results( resultsdir=resultsdir,jobid=jobid)
############init marker:
        snapshots = stats.get_snapshots()
        markers = snapshots
        m1 = markers[0]
        m2 = markers[-1]
        stats = PerformStats(self.config,(m1,m2), dvfs=[],roi = False)
        self.walltime = max(stats.walltime_per_core)/1e6
    else:
        self.walltime = 0
    self.total_time = total_time/1e15
    return
  def getinsnum(self,feature):
#      print(feature)
      return max( np.array(feature["insnums"]))
      #return sum( np.array(feature["insnums"]))



  def extrapolate(self, feature, detailed_features,minimum_threshold,enable_marker,not_only_detailed,clusterid2fs,closed=False):
    bbv = np.array(feature["bbv"]) 
    pc = feature["pc"]
    distance = 100
    extrapolate_feature = None
    norm_version = 1
#    print(len(detailed_features))
    level = 0
    for detailed_feature in detailed_features:
        if ((not enable_marker) or pc == detailed_feature["pc"] ) and ( detailed_feature["feature_level"]==1):
            bbv2 = np.array(detailed_feature["bbv"])
            distance_tmp = LA.norm( bbv - bbv2,norm_version)

            if distance > distance_tmp:
                extrapolate_feature = detailed_feature
                distance = distance_tmp
    if(distance > minimum_threshold):
    #    print(feature)
        return 0,False
    if extrapolate_feature == None:
        return 0,False
    insnum = self.getinsnum(feature)
    insnum_extrapolate = self.getinsnum(extrapolate_feature)

    clusterid = extrapolate_feature["clusterid"]
#    if clusterid in clusterid2fs and (not closed):
#        return clusterid2fs[clusterid]  * insnum, True
 #   else:
    return float(extrapolate_feature["fs"])  / insnum_extrapolate * insnum, True
  def active_bbvthread_num( self,bbvs,insnums ,threadnum,bbvdim):
    active_num = 0
#    new_bbv = []
    for ti in range(threadnum):
        bbv_begin = ti * bbvdim
        bbv_end = bbv_begin + bbvdim
        active = False
        for i in range( bbv_begin,bbv_end ):
            if bbvs[i] != 0:
                active = True
                break
        if active:
#            print(ti)
            active_num += 1
#            new_bbv += bbvs[bbv_begin:bbv_end]
    if active_num == 1 or active_num == threadnum:
        return active_num,bbvs
    else:
#        new_bbv = [0] * len(bbvs)
#        idx = np.argmax(insnums)
#        print(idx)
#        new_bbv[idx*bbvdim: (idx+1)*bbvdim] = bbvs[idx*bbvdim: (idx+1)*bbvdim]
        return active_num,bbvs
  def extrapolate_feature_of_same_threads( self, feature_stack ):
    if len( feature_stack ) == 0 :
        return 

    detailed_feature = []
    timesum = 0
    insnumsum = 0
    zerofsnum = 0
    sumtimeperins = 0
    numtimeperins = 0 
    
    marker_threshold = threshold_in
    threshold = threshold_in

    clusterid2fs=dict()
    zerofsnum = 0
    fsnum = 0

    for feature_i,feature in enumerate( feature_stack):
        insnum = max(feature["insnums"])
        if feature["feature_level"] == 1:
            feature["solved"] = True
            time = feature["fs"]
            self.time_each_feature_level[0] += time
            self.num_each_feature_level[0]+= insnum
        else:
            time, level = self.extrapolate( feature, feature_stack[:feature_i], marker_threshold, True,True,clusterid2fs )
            if level:
                feature["fs"] = time
                feature["solved"] = True
                self.time_each_feature_level[1] += time
                self.num_each_feature_level[1]+=insnum
            else: 
                time, level = self.extrapolate( feature, feature_stack[:feature_i], threshold, False,True, clusterid2fs )
                if level:
                    feature["fs"] = time
                    feature["solved"] = True
                    self.time_each_feature_level[2] += time
                    self.num_each_feature_level[2]+=insnum
                elif zerofsnum > fsnum * 0.5:
                    feature["solved"] = True
                    feature["fs"] = 0 
                    time = 0
                    self.time_each_feature_level[4] += time
                    self.num_each_feature_level[4]+= insnum
                else:
                    feature_i_begin = max(feature_i - 1,0)
                    detailed_tmp_features_tmp = feature_stack[feature_i_begin:feature_i] #+ feature_stack[feature_i+1:feature_i_end+1]
                    detailed_tmp_features = []
                    for feature_tmp in detailed_tmp_features_tmp:
                        if feature_tmp["solved"]:
                            detailed_tmp_features.append(feature_tmp)
                    time,solved = self.extrapolate(feature,detailed_tmp_features,1,False,True,clusterid2fs,True)
                    if solved:
                        feature["solved"] = True
                        feature["fs"] = time
                        self.time_each_feature_level[3] += time
                        self.num_each_feature_level[3]+= insnum
                    else:
                        feature["solved"] = True
                        if numtimeperins == 0:
                            avgtimeperins = 0
                        else:
                            avgtimeperins = sumtimeperins / numtimeperins
                        feature["fs"] = avgtimeperins * max(feature["insnums"])
                        time_tmp = feature["fs"]
                        self.time_each_feature_level[4] += time_tmp
                        self.num_each_feature_level[4]+= sum(feature["insnums"])


        if feature["fs"] == 0:
            zerofsnum+=1
        fsnum += 1
        sumtimeperins +=  time / insnum
        numtimeperins += 1
        timesum += time

  def print_ref_full_time(self,parallel=False):
     ##preprocessing
    global_ins_num = 0
    detail_ins_num = 0 
    for feature_i,feature in enumerate(self.feature_stack):
        insnum = max( feature["insnums"])
        #if feature["feature_level"] == 1:
        if not parallel:
            if feature["predict_result_type"]  == 3 or feature["predict_result_type"] == 2:
                detail_ins_num += insnum
            global_ins_num += insnum
        else:
            if feature["predict_result_type"]  == 3 or feature["predict_result_type"] == 2:
                global_ins_num += insnum
                detail_ins_num = max(insnum,detail_ins_num)

    return detail_ins_num ,  global_ins_num
   
  def print_full_time(self):

#    if "bbv" not in self.feature_stack[0]:
#        return self.total_time
    total_time = 0
    detailed_feature = []

    time_each_feature_level=[0]*5 
    num_each_feature_level=[0]*5
     ##preprocessing
    bbv_dim = 16
    thread_num = len(self.feature_stack[0]["insnums"])
    clusterid2feature = dict()
    active_nums = [0] * (thread_num+1)
    active_each_feature = [ ]
    for ti  in range( thread_num + 1 ):
        active_each_feature.append([])
    for feature_i,feature in enumerate(self.feature_stack):
     
        activenum,new_bbv =  self.active_bbvthread_num( feature["bbv"],feature["insnums"],thread_num,bbv_dim )
#        feature["bbv"] = new_bbv
#        print(activenum,thread_num)
        active_nums[activenum] += 1
        active_each_feature[activenum].append(feature)
    for ti,features in enumerate( active_each_feature ):
        self.extrapolate_feature_of_same_threads(features)

    total_time = 0
    if "flush" in self.config:
        flush = self.config["flush"]
    else:
        flush = False
    for idx,feature in enumerate( self.feature_stack ):
        total_time += feature["fs"]
        if flush:
            self.record["feature_stack"][idx]["fs"] = feature["fs"]
    if flush:
        with open( "%(output_sniper_validate_dir)s/intra_mtng2.json"%self.config,"w" ) as f:
            record = json.dumps( self.record )
            f.write(record)

    #print( total_time/1e15 )
#    time_each_feature_level[4] = sum(time_each_feature_level[:4]) / sum(num_each_feature_level[:4]) * num_each_feature_level[4]
    print(np.array(self.time_each_feature_level)/1e15)
#    print( sum(np.array(self.time_each_feature_level)/1e15))
    print( np.array( self.num_each_feature_level) /1e9)
    print_info = '\t'.join( map(str,np.array( self.num_each_feature_level) * 1.0 /sum(self.num_each_feature_level)))
    #print( print_info )
    print  "distribution\t", print_info 
    return total_time/1e15


    return self.total_time
  def print_wall_time(self,parallel=False):
    if self.walltime == 0:
        self.walltime=4029.54 ##imag we overwrite this file; use record data
    if parallel:
        allinsnum_details = 0
        maxinsnum_details = 0
        allinsnum = 0
        for feature_i,feature in enumerate(self.feature_stack):
            if feature["predict_result_type"]  == 3 or feature["predict_result_type"] == 2:
                insnum = max(feature["insnums"])
                allinsnum_details += insnum
                maxinsnum_details = max( insnum,maxinsnum_details )

            #######
            insnum = max(feature["insnums"])
            allinsnum += insnum
        return allinsnum
#        return self.walltime * maxinsnum_details/allinsnum_details
    else:
        return self.walltime
  def markerdistribution(self):
    barrierpointnum = 0
    looppointnum=0
    for feature in self.feature_stack:
        recordtype = feature['recordtype']
        if(recordtype == 1 or recordtype == 2 ):
            barrierpointnum +=1
        else:
            looppointnum+=1
    barrierpointnum = 1.0 * barrierpointnum / len(self.feature_stack)
    looppointnum = 1.0 * looppointnum / len(self.feature_stack)
    return barrierpointnum,looppointnum
  def extrapolatedistribution(self):
    feature_levels = [0] * 6
    for feature in self.feature_stack:
        recordtype = feature['feature_level']
        feature_levels[recordtype] += 1
    summary = sum(feature_levels)
    for elem_i,elem in enumerate( feature_levels ) :
        feature_levels[elem_i] = 1.0 * elem / summary
    return feature_levels,[]

  def timedistribution(self):
    clustertime = self.clustertime
    predicttime = self.predicttime
    return clustertime,predicttime

  def predictaccuracy(self):
    feature_levels = [0] * 4
    for feature_i,feature in enumerate(self.feature_stack):
        recordtype = feature['predict_result_type']

        insnum = feature['max_ins_num']
        feature_levels[recordtype] += insnum 
        #feature_levels[recordtype] += 1
    summary = sum(feature_levels)
    for elem_i,elem in enumerate( feature_levels ) :
        feature_levels[elem_i] = 1.0 * elem / summary
    return feature_levels,[]


  def print_overall_runtime(self):
    total_time = 0.0

    time_of_each_region = []
    for i in range(self.region_count):
      # get stats
      stats = self.stats_per_region[i]
      # aggregate total simulated runtime
    #  print( stats.time0 )
      time_of_each_region.append( stats.time0/1e15)
      
#    print "[DUMPSTATS] Overall Simulated Runtime: ", total_time/1e15, ' seconds'
    return time_of_each_region
  def print_walltime(self):
    time_of_each_region = []
    total_time = 0.0
    for i in range(self.region_count):
      # get stats
      stats = self.stats_per_region[i]
      runtime1 = float(max( stats.walltime_per_core))

      total_time += runtime1/1e6 

#    print "[DUMPSTATS] Overall Simulated Runtime: ", total_time/1e15, ' seconds'
    return total_time
def get_args():
    parser = argparse.ArgumentParser(description="Analysing data for detail mode")

    parser.add_argument("--sniper-root", type=str, default="/home/lcx/sniper-public-local/", help="root of sniper")
    parser.add_argument("--dir", type=str, default=None, help="output of sniper")
    parser.add_argument("--segs" , action="store_true", default=False,help="to print time of between each marker")
    parser.add_argument("--flush" , action="store_true", default=False,help="flush out new data")
    args = parser.parse_args()
#    print(args.only_full_time)
    config = dict()
    config["sniper_root"] = args.sniper_root
    
    config["flush"] = args.flush
    config["only-full-time"] = not args.segs
    if args.dir is None:
        print(" Sniper output dir should be assigned"  )
        exit(1)
    config["output_sniper_dir"] = args.dir

    config[ "output_sniper_validate_dir"] = args.dir
    return config
def get_full_version_time( output_sniper_dir, sniper_root_dir ):
    config = dict()

    config[ "output_sniper_validate_dir"] = output_sniper_dir
    config["sniper_root"] = sniper_root_dir
    config["flush"] = True
    hyper_dump_stat = MtngDumpStats( config )

    total_time_mtng_for_each_region = hyper_dump_stat.print_overall_runtime()
    return  total_time_mtng_for_each_region

def get_ref_full_version_time( output_sniper_dir, sniper_root_dir ,parallel=False):

#    total_time_mtng_for_each_region = hyper_dump_stat.print_overall_runtime()
    hyper_dump_stat = get_dump_state(output_sniper_dir,sniper_root_dir,True)
    
    total_time_mtng_for_each_region = hyper_dump_stat.print_ref_full_time(parallel)
    walltime = hyper_dump_stat.print_wall_time()
    if parallel :
        return total_time_mtng_for_each_region[0],walltime 
    return  total_time_mtng_for_each_region[0],total_time_mtng_for_each_region[1]#walltime

#    return  total_time_mtng_for_each_region

def get_dump_state(output_sniper_dir,sniper_root_dir,disable_sniper_file = False):
    config = dict()

    config[ "output_sniper_validate_dir"] = output_sniper_dir
    config[ "output_sniper_dir"] = output_sniper_dir
    config["sniper_root"] = sniper_root_dir
    config["flush"] = True
    config["only-full-time"] = True
    if disable_sniper_file:
        config["sniper_file_enable"] = False
    else:
        config["sniper_file_enable"] = True
    hyper_dump_stat = MtngDumpStats( config )
    return hyper_dump_stat

def get_full_time( output_sniper_dir, sniper_root_dir, parallel=False ):
    hyper_dump_stat = get_dump_state(output_sniper_dir,sniper_root_dir)
    total_time_mtng_for_each_region = hyper_dump_stat.print_full_time()
    walltime = hyper_dump_stat.print_wall_time(parallel)
    
    return  total_time_mtng_for_each_region,walltime
def get_marker_distribution( output_sniper_dir, sniper_root_dir ):

    hyper_dump_stat = get_dump_state(output_sniper_dir,sniper_root_dir)

    barrierpoint,looppoint = hyper_dump_stat.markerdistribution()
    
    return  barrierpoint,looppoint
def extrapolatedistribution( output_sniper_dir, sniper_root_dir ):

    hyper_dump_stat = get_dump_state(output_sniper_dir,sniper_root_dir)
    
    barrierpoint,looppoint = hyper_dump_stat.extrapolatedistribution()
    return  barrierpoint,looppoint
def timedistribution( output_sniper_dir, sniper_root_dir ):

    hyper_dump_stat = get_dump_state(output_sniper_dir,sniper_root_dir)
    
    barrierpoint,looppoint = hyper_dump_stat.timedistribution()
    walltime = hyper_dump_stat.print_wall_time()
    return [ barrierpoint,looppoint,walltime],0


def predictaccuracy( output_sniper_dir, sniper_root_dir ):

    hyper_dump_stat = get_dump_state(output_sniper_dir,sniper_root_dir)
    
    barrierpoint,looppoint = hyper_dump_stat.predictaccuracy()
    return  barrierpoint,looppoint



if __name__ == "__main__":
    config = get_args(  )
#    print(config)
    dump_stat = MtngDumpStats( config )

    total_time_mtng_for_each_region = dump_stat.print_full_time()
    wall_time = dump_stat.print_wall_time()
    print( total_time_mtng_for_each_region)
    print( "Wall Time:", wall_time)
 # def populate_cluster_stats(self):




 # def populate_cluster_stats(self):

