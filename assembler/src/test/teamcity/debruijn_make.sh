#!/bin/bash

cd ../../../
#./prepare_cfg
./cpcfg
cd data/
./link_ftp.sh
cd ..
make clean
make -j 5 rd
cd src/test/teamcity/
