import pandas as pd
import os
import re

def cleanup_program_name(name):
  ''' strip off junk from program's name '''
  name = re.sub(r'.+/', '', name)
  name = re.sub(r'\.[a-z]+$', '', name)
  return name


def cleanup_aot_opt(opt):
  ''' strip off junk from AOT optimization level flag '''
  opt = re.sub(r'-', '', opt)
  return opt


def cleanup_flags(flag):
  flag = re.sub(r'withserver *-fhalo *; *--halo-strategy=', '', flag)
  flag = re.sub(r'adapt', 'halo', flag)
  flag = re.sub(r'--halo-threads=[0-9]+', '', flag)
  flag = re.sub(r'--halo-metric=', '', flag)
  flag = re.sub(r' *ipc', '-ipc', flag)
  flag = re.sub(r' *calls', '-calls', flag)
  flag = re.sub(r' *-fhalo *', 'halomon', flag)
  flag = re.sub(r'.*none.*', 'default', flag)
  flag = re.sub(r'pgo', 'aot', flag)
  return flag


def read_csv(csv_filename):
  '''
  takes a string with a file path and
  loads the CSV file into a pandas dataframe
  '''
  df = pd.read_csv(csv_filename)
  df['program'] = df['program'].apply(cleanup_program_name)
  df['aot_opt'] = df['aot_opt'].apply(cleanup_aot_opt)
  df['flags'] = df['flags'].apply(cleanup_flags)
  return df