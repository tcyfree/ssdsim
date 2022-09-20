#!/bin/bash
rm -rf out-batch.txt
rm -rf update-write.csv

title="seq_num,   gc_count,   move_page_count,   read_avg,   write_avg"

runList="runList.txt" #runList存放准备运行的trace合集

function runTrace() {
  #1.找到运行的excel
  cat $1 | while read line
  do
    echo $line
  done 

  cat $1 | while read line
  do
    saveFile1=${line##*/}
    saveFile2=${saveFile1%%.*}

    echo $saveFile2 >> out-batch.txt

    # 读写率
    # cd ../trace-analysis && ./ssd 1  ../trace/$line && cd ../ssdsim && cat ../trace-analysis/trace-analysis.txt >> out-batch.txt

    echo "$title" >> out-batch.txt

    ./ssd 1 1 ../trace/$line    
    cat ex.out | tail -n 1 >> out-batch.txt

    ./ssd 1 1 ../trace/$line 1
    cat ex.out | tail -n 1 >> out-batch.txt

    ./ssd 1 1 ../trace/$line 2 1    
    cat ex.out | tail -n 1 >> out-batch.txt

    ./ssd 1 1 ../trace/$line 4 1     
    cat ex.out | tail -n 1 >> out-batch.txt

    ./ssd 1 1 ../trace/$line 8 1     
    cat ex.out | tail -n 1 >> out-batch.txt

    ./ssd 1 1 ../trace/$line 16 1     
    cat ex.out | tail -n 1 >> out-batch.txt

    ./ssd 1 1 ../trace/$line 32 1     
    cat ex.out | tail -n 1 >> out-batch.txt

    ./ssd 1 1 ../trace/$line 64 1     
    cat ex.out | tail -n 1 >> out-batch.txt

  done 
}
runTrace $runList
#运行需要修改dir 
