#!/bin/sh
rm -rf out.txt
rm -rf update-write.csv

echo $1 >> out.txt

title="seq_num,   gc_count,   move_page_count,   read_avg,   write_avg"
echo "$title" >> out.txt

./ssd 1 1 $1   
cat ex.out | tail -n 1 >> out.txt

for i in $(seq 2 16)
do 
./ssd 1 1 $1 $i 2  
cat ex.out | tail -n 1 >> out.txt
done

for i in $(seq 2 16)
do 
./ssd 1 1 $1 $i 1  
cat ex.out | tail -n 1 >> out.txt
done

