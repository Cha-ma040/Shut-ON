#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import numpy as np
import pyaudio
import random
import csv
from glob import glob
from tqdm import tqdm

cur_dir = os.path.dirname(os.path.abspath(__file__))
natural_dir = os.path.join(cur_dir, 'natural')
trans_dir = os.path.join(cur_dir, 'trans')
ref_csv = os.path.join(cur_dir, 'ref_train.csv')
data = 120
reference = []

def scale(i):
    good_max = data - 1
    good_min = 0
    middle_max = 2*data -1
    middle_min = data - 1
    bad_max = 3*data -1
    bad_min = 2*data -1
    if i>=good_min and i<=good_max:
        decay = random.uniform(0.001, 0.01)
    elif i>=middle_min and i<=middle_max:
        decay = random.uniform(0.01, 0.1)
    elif i>=bad_min and i<=bad_max:
        decay = random.uniform(0.1, 1.0)
    else:
        print("break")
        exit()

    return decay

def labeling(i):
    good_max = data - 1
    good_min = 0
    middle_max = 2*data -1
    middle_min = data - 1
    bad_max = 3*data -1
    bad_min = 2*data -1
    if i>=good_min and i<=good_max:
        label = 0
    elif i>=middle_min and i<=middle_max:
        label = 1
    elif i>=bad_min and i<=bad_max:
        label = 2
    else:
        print("break")
        exit()

    return label


if __name__ == '__main__':

    if os.path.isdir(natural_dir):
        pass
    else:
        os.mkdir(natural_dir)
    
    if os.path.isdir(trans_dir):
        pass
    else:
        os.mkdir(trans_dir)
    
    natural_data = glob(os.path.join(natural_dir, '*'))
    for i in natural_data:
        os.remove(i)
    
    p = pyaudio.PyAudio()
    stream = p.open(format=pyaudio.paFloat32,
                    channels=1,
                    rate=44100,
                    frames_per_buffer=1024,
                    output=True)

    dlen = 1024 * 1 #ノイズデータのデータ長
    mean = 0.0  #ノイズの平均値
    std  = 1.0  #ノイズの分散
    # incident sound
    samples = np.array( [random.gauss(mean, std) for i in range(dlen)] )
    # print(samples.shape)

    for i in tqdm(range(3 * data)):

        # relcetion sound
        decay = scale(i)
        samples_reflect = decay * np.array( [random.gauss(mean, std) for i in range(dlen)] )
        # print(samples_reflect.shape)
    
        #silent
        samples_silent = np.array(np.zeros(3*dlen - len(samples) - len(samples_reflect)))
        # incident - reflection
        samples_in_re = np.hstack([samples, samples_reflect, samples_silent])
    
        data_csv = os.path.join(natural_dir,'test_data_{}.csv'.format(i))
    
        with open(data_csv, 'w') as f:
            writer = csv.writer(f)
            writer.writerow(samples_in_re)
        
        with open(data_csv, 'r') as g:
            data_csv = list(csv.reader(g))
        
        data_chage = data_csv[0]
        trans_data = os.path.join(trans_dir, 'test_data_{}.csv'.format(i))
        with open(trans_data, 'w', newline='') as h:
            writer = csv.writer(h)
            for val in data_chage:
                writer.writerow([val])

        label = labeling(i)
        ref = ['./train/test_data_{}.csv'.format(i), label]
        reference.append(ref)

    head = ['x', 'y']
    with open(ref_csv, 'w') as f:
        writer = csv.writer(f)
        writer.writerow(head)
        for refe in reference:
            writer.writerow(refe)
