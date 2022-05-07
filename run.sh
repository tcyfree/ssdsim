#!/bin/sh
rm -rf out.txt

title="seq_num,   gc_count,   gc_lpn_count,   read_avg,    write_avg"
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

./ssd 1 1 $1 12 1     
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 16 1     
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 32 1   
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 64 1    
cat ex.out | tail -n 1 >> out.txt