Statement:
SSDsim is a simulation tool of SSD’s internal hardware and software behavior. It provides specified SSD’s performance, endurance and energy consumption information based on a configurable parameter file and different workloads (trace file).
SSDsim was created by Yang Hu in the end of 2009 and upgraded to version 2.0 after lots of modification and perfection. Its programming language is C and development environment is Microsoft Visual Studio 2008. With the help of Zhiming Zhu, Shuangwu Zhang, Chao Ren, Hao Luo, it is further developed into version 2.x. As the development team, we will continue adding new modules and functions to guarantee its persistent perfection. If you have any questions, suggestions or requirements about it, please feel free to email Yang Hu (yanghu@foxmail.com). We will adopt any reasonable requirements to make SSDsim better.

Application steps:
1)	Compile the code and generate an executablefile.
	A make clean
	B make all
	C, ./ssd
2)	Run the executable file, input the name of trace file
3)	Analyze the output file and statistic file for further experiments.

Note:
1)	Each request of the trace file possesses one line. A request should be ASCII and follow the format: request arriving time(int64), logical device number (int), logical sector number (int), request size (int), access operator (int, 1 for read, 0 for write).
2)	The unit of the request arriving time is nanosecond and the unit of the request size is sector (512 Bytes).
3)	No CRLF at the end of the last request line.


2011-12-03

# proj_4.csv
read request average response time: 15833783343
write request average response time: 8921627996

read request average response time: 15725027419
write request average response time: 8904484477

read 108755924  0.006868599982965
write 17143519  0.001921568463478

#proj_0.csv
read request average response time: 286477986761
write request average response time: 628389536649

read request average response time: 284309713962
write request average response time: 624740822304

read 2168272799  0.00762644641572
write 3648714345   0.005806453055309

#1720_flag.csv
read request average response time: 1060618
write request average response time: 121404624

read request average response time: 1032437
write request average response time: 121340302

read: 28181 0.026570358036541
write: 0.000529815075248


#1815-lun1.csv
read request average response time: 408626848
write request average response time: 372075158

read request average response time: 115555108
write request average response time: 151712481

read：293071740 0.717211170617942
write：220362677 0.592253130213009

#1719_flag.csv
read request average response time: 1306901377
write request average response time: 2317744415

read request average response time: 229795953
write request average response time: 659140751

read: 1077105424 0.824167334242544
write: 1658603664 0.715611114524032

#1613-lun4.csv
read request average response time: 58910971312
write request average response time: 67595758901

read request average response time: 18095022461
write request average response time: 25338743319

read: 40815948851  0.692841213478446
write: 42257015582  0.625143001114747


//prxy_0.csv 中间报错了，所以数据不准
read request average response time: 26003597440
write request average response time: 37593319210

read request average response time: 30931604416
write request average response time: 45630159207
