from os import abort
import numpy as np
import matplotlib.pyplot as plt
import sys

# run command: python3 cdf_print.py tail_latency.csv tail_out.csv
filename = sys.argv[1]
print(sys.argv)
# abort();
lines = []
with open(filename,'r') as f:
    lines = f.read().split('\n')
fo = open(sys.argv[2],"w")

dataSets = []
#test_count = 0
for line in lines:
    #print(line)
    #test_count+=1
    #if test_count == 3:
        #continue
    try:
        dataSets.append(line.split(','))
    except :
        print("Error: Exception Happened... \nPlease Check Your Data Format... ")

temp = []
for set in dataSets:
    temp2 = []
    for item in set:
        if item!='':
            temp2.append(float(item))
    temp2.sort()
    temp.append(temp2)
dataSets = temp

arealength_temp = 0
areawidth_temp = 0
#first_flag=0
area = [0 for j in range(10)]
j=0
flag99=0
flag992=0
flag994=0
flag996=0
flag998=0
flag999=0
for set in dataSets:
    arealength_temp = 0
    areawidth_temp = 0
    continue_flag = 0
    plotDataset = [[],[]]
    count = len(set)
    for i in range(count):

        plotDataset[0].append(float(set[i]))
        plotDataset[1].append((i+1)/count)
        if j==0:
            if (i+1)/count>= 0.99 and flag99==0:
                fo.write(str(int(set[i])/1000000)+','+str((i+1)/count)+'\n')
                flag99=1
            elif (i+1)/count>= 0.992 and flag992==0:
                fo.write(str(int(set[i])/1000000)+','+str((i+1)/count)+'\n')
                flag992=1
            elif (i+1)/count>= 0.994 and flag994==0:
                fo.write(str(int(set[i])/1000000)+','+str((i+1)/count)+'\n')
                flag994=1
            elif (i+1)/count>= 0.996 and flag996==0:
                fo.write(str(int(set[i])/1000000)+','+str((i+1)/count)+'\n')
                flag996=1
            elif (i+1)/count>= 0.998 and flag998==0:
                fo.write(str(int(set[i])/1000000)+','+str((i+1)/count)+'\n')
                flag998=1
            elif (i+1)/count>= 0.999 and flag999==0:
                fo.write(str(int(set[i])/1000000)+','+str((i+1)/count)+'\n')
                flag999=1
            elif (i+1)/count == 1:
                fo.write(str(int(set[i])/1000000)+','+str((i+1)/count)+'\n')
        #if first_flag != 0:
            #print(str(float(set[i])))
            #print(str(arealength_temp))
            #area[j] += (areawidth_temp+(i+1)/count)/(float(set[i])-arealength_temp)
        if float(set[i])>arealength_temp or continue_flag==1:
            area[j] += ((i+1)/count - areawidth_temp)*arealength_temp
            #print(area[j])
            areawidth_temp = (i+1)/count
            if continue_flag == 1:
                continue
        if (i+1)/count>= 0.9999:
            continue_flag = 1
        arealength_temp = float(set[i])
        
        #first_flag = 1
        #print(str((i+1)/count)+'\n')
    #print(plotDataset[0])
    print(area[j])
    j += 1
    print("end")
    if j == 1:
        plt.plot(plotDataset[0], plotDataset[1], '-k', linewidth=2)
    elif j == 2:
        plt.plot(plotDataset[0], plotDataset[1], '-', linewidth=2)
    elif j == 3:
        plt.plot(plotDataset[0], plotDataset[1], '-m', linewidth=2)
    elif j == 4:
        plt.plot(plotDataset[0], plotDataset[1], '-', linewidth=2)
    elif j == 5:
        plt.plot(plotDataset[0], plotDataset[1], '-r', linewidth=2)


# generate a legend box
#plt.legend(['sin(x)', 'cos(x)', '$e^{-x}$'])

#plt.legend(['Baseline', 'RL', 'IPR', 'WS', 'WS+RS'], loc='right')
#plt.legend(['No RR', 'Routine RR'], loc='right')

# plt.legend(['Baseline', 'Cached-GC', 'RL-GC', 'Erase suspension', 'Demand'], loc='right')
# #plt.legend(['BL', 'RL', 'IPR', 'RS'])
# plt.axis([0,500000000,0.9,1])
# plt.show()

