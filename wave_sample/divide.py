#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import random
from tqdm import tqdm
from glob import glob
import shutil

cur_dir = os.path.dirname(os.path.abspath(__file__))
train_dir = os.path.join(cur_dir, 'train')
valid_dir = os.path.join(cur_dir, 'valid')
valid=30

if __name__ == '__main__':
    classes = glob(os.path.join(train_dir, '*'))

    for cls in classes:
        all_data =  glob(os.path.join(cls, '*'))
        to_valid = random.sample(all_data, valid)

        for data in tqdm(to_valid):
            shutil.move(data, os.path.join(valid_dir, os.path.basename(cls)))