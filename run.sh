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

./ssd 1 1 $1 6 1
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 8 1
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 10 1
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 16 1
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 26 1
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 32 1
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 48 1
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 64 1
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 128 1
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 256 1
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 512 1
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 1024 1
cat ex.out | tail -n 1 >> out.txt