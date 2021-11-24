#! /bin/bash

sudo apt update
sudo apt install -y \
    python3-pip

git config --global user.name "njima"
git config --global user.email 111111111111111

pip3 install -r requirements.txt