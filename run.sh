#!/bin/sh
rm -rf out.txt
rm -rf update-write.csv

echo $1 >> out.txt

title="seq_num,   gc_count,   move_page_count,   read_avg,   write_avg"
echo "$title" >> out.txt

./ssd 1 1 $1   
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 2 2     
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 4 2     
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 8 2
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 16 2     
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 32 2     
cat ex.out | tail -n 1 >> out.txt


./ssd 1 1 $1 2 1     
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 4 1     
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 8 1
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 16 1     
cat ex.out | tail -n 1 >> out.txt

./ssd 1 1 $1 32 1     
cat ex.out | tail -n 1 >> out.txt


