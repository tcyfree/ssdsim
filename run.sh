#!/bin/sh
rm -rf out.txt
rm -rf update-write.csv

title="seq_num,   gc_count,   gc_lpn_count,   gc_seq_lpn_count,   gc_rand_seq_lpn_count,   read_avg,   write_avg,  update_write_ratio"
echo "$title" >> out.txt

./ssd 1 1 $1   
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 2 1     
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 3 1     
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 4 1     
cat ex.out | tail -n 1 >> out.txt