#!/bin/sh
rm -rf out.txt
rm -rf update-write.csv

echo $1 >> out.txt

title="seq_num, gc_count, move_page_count,  read_avg,  write_avg,  trace_ssd, wb_ssd, seq_hdd, rnd_hdd"
echo "$title" >> out.txt

./ssd 1 1 $1   
tail -n 1 ex.out >> out.txt

./ssd 1 1 $1 1     
tail -n 1 ex.out >> out.txt

./ssd 1 1 $1 8 3     
tail -n 1 ex.out >> out.txt

./ssd 1 1 $1 8 2    
tail -n 1 ex.out >> out.txt

./ssd 1 1 $1 8 1     
tail -n 1 ex.out >> out.txt



