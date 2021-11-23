#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
from random import sample
import numpy as np
import pyaudio
import wave
import random
from scipy import pi as mpi
from scipy import arange, cumsum, sin, linspace
import matplotlib.pyplot as plt
import csv
from glob import glob
from tqdm import tqdm

cur_dir = os.path.dirname(os.path.abspath(__file__))
natural_dir = os.path.join(cur_dir, 'natural')
trans_dir = os.path.join(cur_dir, 'trans')
TEST_FILE = os.path.join(cur_dir, 'Fanfare60.wav')
data = 280


# play wav_file sound
def wav_play():
    # available read wav_file
    try:
        wf = wave.open(TEST_FILE, "rb")
    except FileNotFoundError:
        print('[Error 404] No such file or directory:'  + TEST_FILE)
        exit()

    p = pyaudio.PyAudio()
    print(wf.getframerate())
    stream = p.open(format=p.get_format_from_width(wf.getsampwidth()), channels=wf.getnchannels(), rate=wf.getframerate(), output=True)

    chunk = 1024
    data= wf.readframes(chunk)
    print("play")

    while data != '':
        stream.write(data)
        data = wf.readframes(chunk)
    stream.close()
    p.terminate

def make_time_varying_sine(start_freq, end_freq, A, fs, sec = 5.):
    freqs = linspace(start_freq, end_freq, num = int(round(fs * sec)))
    ### 角周波数の変化量
    phazes_diff = 2. * mpi * freqs / fs
    ### 位相
    phazes = cumsum(phazes_diff)
    ### サイン波合成
    ret = A * sin(phazes)

    return ret

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

    dlen = 1024 * 23 #ノイズデータのデータ長
    mean = 0.0  #ノイズの平均値
    std  = 1.0  #ノイズの分散
    # incident sound
    samples = np.array( [random.gauss(mean, std) for i in range(dlen)] )
    # print(samples.shape)

    for i in tqdm(range(data)):

        # relcetion sound
        scale = random.uniform(0.01, 0.1)
        samples_reflect = scale * np.array( [random.gauss(mean, std) for i in range(dlen)] )
        # print(samples_reflect.shape)
    
        #silent
        samples_silent = np.array(np.zeros(3*dlen - len(samples) - len(samples_reflect)))
        # incident - reflection
        samples_in_re = np.hstack([samples, samples_reflect, samples_silent])
    
    
        plt.plot(samples_in_re)
    
        plt.savefig("incident-reflection.png")
        # incident - incident+reflection
        samples_inre = samples + samples_reflect
        print(samples_inre.shape)
        samples_in_inre = np.hstack([samples, samples_inre, samples_silent])
        print(samples_in_inre.shape)
    
        plt.plot(samples_in_inre)
    
        plt.savefig("incident-incident+reflection.png")
    
        exit()
    
        # play noise sound
        # print("play")
        # stream.write(samples_in_re.astype(np.float32).tobytes())
        # stream.close()
        # p.terminate
        
        # data_csv = os.path.join(natural_dir,'test_data_{}.csv'.format(i))
        # # data_csv = os.path.join(natural_dir,'test_data_1.csv')
    
        # with open(data_csv, 'w') as f:
        #     writer = csv.writer(f)
        #     writer.writerow(samples_in_re)
        
        # with open(data_csv, 'r') as g:
        #     data_csv = list(csv.reader(g))
        
        # data_chage = data_csv[0]
        # trans_data = os.path.join(trans_dir, 'test_data_{}.csv'.format(i))
        # # trans_data = os.path.join(trans_dir, 'test_data_1.csv')
        # with open(trans_data, 'w', newline='') as h:
        #     writer = csv.writer(h)
        #     for val in data_chage:
        #         writer.writerow([val])
    
        # # a = np.loadtxt(os.path.join(data_csv), delimiter=',', skiprows=1, ndmin=2)
        # np.savetxt(), a.T, header='Good', comments='')
    