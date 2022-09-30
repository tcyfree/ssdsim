/*****************************************************************************************************************************
This project was supported by the National Basic Research 973 Program of China under Grant No.2011CB302301
Huazhong University of Science and Technology (HUST)   Wuhan National Laboratory for Optoelectronics

FileName： ssd.c
Author: Hu Yang		Version: 2.1	Date:2011/12/02
Description:

History:
<contributor>     <time>        <version>       <desc>                   <e-mail>
Yang Hu	        2009/09/25	      1.0		    Creat SSDsim       yanghu@foxmail.com
                2010/05/01        2.x           Change
Zhiming Zhu     2011/07/01        2.0           Change               812839842@qq.com
Shuangwu Zhang  2011/11/01        2.1           Change               820876427@qq.com
Chao Ren        2011/07/01        2.0           Change               529517386@qq.com
Hao Luo         2011/01/01        2.0           Change               luohao135680@gmail.com
*****************************************************************************************************************************/



#include "ssd.h"

int get_avg_time(int index, int seq)
{
	// if (index > 1024)
	// {
	// 	printf("index:%d",index);
	// 	abort();
	// }
	
	int rand = 2100000;

	int sequential = 300000;

	return seq == 1 ?  sequential : rand;

}

/**
 * @brief call disksim by trace
 * 
 * @param tracename 
 * @return char* 
 */
char* exec_disksim_syssim(char * tracename) 
{
	char average[1024], command[1024];
	FILE * temp;
	//容器里面执行
	// printf("%lf %d %ld %d %d\n", time, devno, logical_block_number,size, isread);
	char cp_trace[32];
	sprintf(cp_trace,"cp %s ../disksim/valid/",tracename);
	system(cp_trace);
	sprintf(command, "cd ../disksim/valid/ && ../src/syssim cheetah4LP.parv %s > temp.txt", tracename);
	// printf("%s\n", command);
	int i = system(command);
	// printf("i: %d\n", i);
	temp = fopen("../disksim/valid/temp.txt","r");
	if(temp == NULL )      /*打开trace文件从中读取请求*/
	{
		printf("the trace temp can't open\n");
	}
	fgets(average, 200, temp);
	// printf("average: %ld\n", atoi(average));
	// printf("average: %s\n", average);
	if (atoi(average) < 0)
	{
		abort();
	}
	
	return atoi(average);
	// if (is_read == 0)
	// {
	// 	if (is_sequential == 1)
	// 	{
	// 		return get_avg_time(times,1);
	// 	} else {
	// 		return get_avg_time(times,0);
	// 	}
		
	// } else {
	// 	return 2100000;
	// }
	// self define cost function for HDD
	// if (is_read == 0)
	// {
	// 	if (is_sequential == 1)
	// 	{
	// 		return (5000000 + 1000000 *times) / times;
	// 	}
	// 	else
	// 	{
	// 		return 6000000;
	// 	}
	// }
	// else
	// {
	// 	return 5000000;
	// }
}
/**
 * @brief Get the aged ratio object
 * 
 * @param ssd 
 * @return double
 */
double get_aged_ratio(struct ssd_info *ssd){
	int64_t free_page_num = 0;
	int cn_id, cp_id, di_id, pl_id;
	printf("Enter get_aged_ratio.\n");
	for(cn_id=0;cn_id<ssd->parameter->channel_number;cn_id++){
		//printf("channel %d\n", cn_id);
		for(cp_id=0;cp_id<ssd->parameter->chip_channel[0];cp_id++){
			//printf("chip %d\n", cp_id);
			for(di_id=0;di_id<ssd->parameter->die_chip;di_id++){
				//printf("die %d\n", di_id);
				for(pl_id=0;pl_id<ssd->parameter->plane_die;pl_id++){
					free_page_num += ssd->channel_head[cn_id].chip_head[cp_id].die_head[di_id].plane_head[pl_id].free_page;
				}
			}
		}
	}
	int page_num = ssd->parameter->page_block * ssd->parameter->block_plane * ssd->parameter->plane_die * ssd->parameter->die_chip * ssd->parameter->chip_num;
	printf("page_num:%d\n", page_num);
	printf("free_page_num:%d\n", free_page_num);
	printf("aged ratio: %.4f\n", (double)(page_num - free_page_num)/page_num);
	return (double)(page_num - free_page_num)/page_num;
}

/********************************************************************************************************************************
1，main函数中initiatio()函数用来初始化ssd,；2，make_aged()函数使SSD成为aged，aged的ssd相当于使用过一段时间的ssd，里面有失效页，
non_aged的ssd是新的ssd，无失效页，失效页的比例可以在初始化参数中设置；3，pre_process_page()函数提前扫一遍读请求，把读请求
的lpn<--->ppn映射关系事先建立好，写请求的lpn<--->ppn映射关系在写的时候再建立，预处理trace防止读请求是读不到数据；4，simulate()是
核心处理函数，trace文件从读进来到处理完成都由这个函数来完成；5，statistic_output()函数将ssd结构中的信息输出到输出文件，输出的是
统计数据和平均数据，输出文件较小，trace_output文件则很大很详细；6，free_all_node()函数释放整个main函数中申请的节点
*********************************************************************************************************************************/
int  main(int argc, char* argv[])
{
	unsigned  int i,j,k;
	double aged_ratio = 0;
	struct ssd_info *ssd;
    char *average;
	#ifdef DEBUG
	printf("enter main\n");
	#endif
	// //顺序读10次
	// average = exec_disksim_syssim("tracename");
	// printf("average-s: %d\n",average);
	// average = exec_disksim_syssim("tracename");
	// printf("average-r: %d\n",average);
	ssd=(struct ssd_info*)malloc(sizeof(struct ssd_info));  //为ssd分配内存
	alloc_assert(ssd,"ssd");
	memset(ssd,0, sizeof(struct ssd_info)); //将ssd指向的那部分结构体内存空间清零，相当于初始化
	//delete tail_latency 
	system("rm tail_latency.csv");
	//*****************************************************
	int sTIMES, speed_up;
	printf("Read parameters to the main function.\n");
	sscanf(argv[1], "%d", &sTIMES);
	sscanf(argv[2], "%d", &speed_up);
	sscanf(argv[3], "%s", &(ssd->tracefilename));
	if (argc == 5)
	{
		sscanf(argv[4], "%d", &(ssd->is_related_work));
		printf("ssd->is_related_work: %d.\n", ssd->is_related_work);
	}
	if (argc == 6)
	{
		sscanf(argv[4], "%d", &(ssd->seq_num));
		printf("ssd->seq_num: %d.\n", ssd->seq_num);
		sscanf(argv[5], "%d", &(ssd->is_sequential));
		printf("is_sequential: %d.\n", ssd->is_sequential);
	}
	printf("Running trace file: %s.\n", ssd->tracefilename);
	//*****************************************************

	ssd=initiation(ssd); //初始化ssd（重点函数模块，需要仔细阅读）
	printf("Chip_channel: %d, %d\n", ssd->parameter->chip_channel[0], ssd->parameter->chip_num); //（各channel上chip数量，整个SSD上chip数量）
	//由于大部分是写少读多，所以老化一部分，更快的GC。而且要在预填数据之前，否则会有错误
	make_aged(ssd);
	get_aged_ratio(ssd);
	//warm_up
	// pre_process_write_read(ssd);
	// get_aged_ratio(ssd);
	//读请求的预处理函数 页操作请求预处理函数
	pre_process_page(ssd); 
	get_aged_ratio(ssd);
	// get_old_zwh(ssd);
	printf("free_lsb: %d, free_csb: %d, free_msb: %d\n", ssd->free_lsb_count, ssd->free_csb_count, ssd->free_msb_count);
	printf("Total request num: %lld.\n", ssd->total_request_num);
	for (i=0;i<ssd->parameter->channel_number;i++)
	{
		for (j=0;j<ssd->parameter->die_chip;j++)
		{
			for (k=0;k<ssd->parameter->plane_die;k++)
			{
				printf("%d,0,%d,%d:  %5d\n",i,j,k,ssd->channel_head[i].chip_head[0].die_head[j].plane_head[k].free_page);
			}
		}
	}
/********************************************************************************************
这段嵌套循环体输出的分别是：

SSDsim将ssd的通道channel，通道上的每个芯片chip，每个芯片上的颗粒die，每个颗粒上面的闪存片plane（也就是块block），
每个闪存片上的闪存页page都用了结构体来进行描述，然后都实现了一个结构体数组来对每个层次的结构体进行一个关系连接；所以
此处的循环体相当于将ssd中按照层次分级将所有空闲状态的pege按照层次的划分输出；显示当前ssd中空闲页的位置；

*********************************************************************************************/
	fprintf(ssd->outputfile,"\t\t\t\t\t\t\t\t\tOUTPUT\n");
	fprintf(ssd->outputfile,"****************** TRACE INFO ******************\n");
	//********************************************

	//sscanf(argv[3], "%s", &
	if(speed_up<=0){
		printf("Parameter ERROR.\n");
		return 0;
		}
	printf("RUN %d TIMES with %dx SPEED.\n", sTIMES,speed_up);
	if(ssd->parameter->fast_gc==1){
		printf("Fast GC is activated, %.2f:%.2f and %.2f:%.2f.\n",ssd->parameter->fast_gc_thr_1,ssd->parameter->fast_gc_filter_1,ssd->parameter->fast_gc_thr_2,ssd->parameter->fast_gc_filter_2);
		}
	printf("dr_switch: %d, dr_cycle: %d\n",ssd->parameter->dr_switch,ssd->parameter->dr_cycle);
	if(ssd->parameter->dr_switch==1){
		printf("Data Reorganization is activated, dr cycle is %d days.\n", ssd->parameter->dr_cycle);
		}
	printf("GC_hard threshold: %.2f.\n", ssd->parameter->gc_hard_threshold);
	ssd->speed_up = speed_up;
	//*********************************************
	//ssd=simulate(ssd);
	srand((unsigned int)time(NULL));

	FILE *fp;
	//获取trace名称
	char *ret = strrchr(ssd->tracefilename, '/') + 1;
	//init file
	char command[32];
    sprintf(command,"%s%s", "rm ", ret);
	system(command);
	//打开文件
	fp = fopen(ret, "a+");
	fprintf(fp, ret);
	fprintf(fp, "\n");
	fprintf(fp, "%d\n", ssd->seq_num);
	fflush(fp);
	fclose(fp);

	ssd=simulate_multiple(ssd, sTIMES); //核心处理函数，对ssd进行一个模拟能耗过程
	statistic_output(ssd); //输出模拟后的结果
/*	free_all_node(ssd);*/

	printf("\n");
	printf("the simulation is completed!\n");

	return 1;
/* 	_CrtDumpMemoryLeaks(); */
}

/******************simulate() *********************************************************************
*simulate()是核心处理函数，主要实现的功能包括
*1,从trace文件中获取一条请求，挂到ssd->request
*2，根据ssd是否有dram分别处理读出来的请求，把这些请求处理成为读写子请求，挂到ssd->channel或者ssd上
*3，按照事件的先后来处理这些读写子请求。
*4，输出每条请求的子请求都处理完后的相关信息到outputfile文件中
**************************************************************************************************/
struct ssd_info *simulate(struct ssd_info *ssd)
{
	int flag=1,flag1=0;
	double output_step=0;
	unsigned int a=0,b=0;
	//errno_t err;

	printf("\n");
	printf("begin simulating.......................\n");
	printf("\n");
	printf("\n");
	printf("   ^o^    OK, please wait a moment, and enjoy music and coffee   ^o^    \n");

	ssd->tracefile = fopen(ssd->tracefilename,"r");
	if(ssd->tracefile == NULL)
	{
		printf("the trace file can't open\n");
		return NULL;
	}

	fprintf(ssd->outputfile,"      arrive           lsn     size ope     begin time    response time    process time\n");
	fflush(ssd->outputfile);

	while(flag!=100)
	{

		flag=get_requests(ssd);

		if(flag == 1)
		{
			//printf("once\n");
			if (ssd->parameter->dram_capacity!=0)
			{
				buffer_management(ssd);

				/*读请求分配子请求函数，这里只处理读请求，
				写请求已经在buffer_management()函数中处理了根据请求队列和buffer命中的检查，
				将每个请求分解成子请求，将子请求队列挂在channel上，不同的channel有自己的子请求队列。*/
				distribute(ssd);
			}
			else
			{
				no_buffer_distribute(ssd);
			}
		}

		process(ssd);
		trace_output(ssd);
		if(flag == 0 && ssd->request_queue == NULL)
			flag = 100;
	}

	fclose(ssd->tracefile);
	return ssd;
}

struct ssd_info *simulate_multiple(struct ssd_info *ssd, int sTIMES)
{
	int flag=1,flag1=0;
	double output_step=0;
	unsigned int a=0,b=0;
	//errno_t err;

	int simulate_times = 0;
	//int sTIMES = 10;
	printf("\n");
	printf("begin simulate_multiple.......................\n");
	printf("\n");
	printf("\n");
	printf("   ^o^    OK, please wait a moment, and enjoy music and coffee   ^o^    \n");
	if(ssd->parameter->turbo_mode == 2 || ssd->parameter->turbo_mode == 0){
		ssd->parameter->lsb_first_allocation=0;
		ssd->parameter->dr_switch=0;
		ssd->parameter->fast_gc=0;
		}
	else{
		ssd->parameter->lsb_first_allocation=1;
		//ssd->parameter->dr_switch=ssd->parameter->dr_switch;
		//ssd->parameter->fast_gc=1;
		}
	/************************IMPORTANT FACTOR************************************/
	//ssd->parameter->turbo_mode_factor = 100;
	fprintf(ssd->statisticfile, "requests            time       gc_count          r_lat          w_lat          w_lsb          w_csb          w_msb           f_gc       mov_page      free_lsb     r_req_count    w_req_count    same_pages       req_lsb       req_csb       req_msb       w_req_count_l    same_pages_l       req_lsb_l       req_csb_l       req_msb_l\n");
	while(simulate_times < sTIMES){
		//*********************************************************
		/*
		if(simulate_times<7){
			ssd->parameter->turbo_mode=0;
			ssd->parameter->dr_switch=0;
			ssd->parameter->fast_gc=0;
			}
		else{
			ssd->parameter->turbo_mode=1;
			ssd->parameter->dr_switch=1;
			ssd->parameter->fast_gc=1;
			}
		*/
		//*********************************************************
		ssd->tracefile = fopen(ssd->tracefilename,"r");
		if(ssd->tracefile == NULL){
			printf("the trace file can't open\n");
			return NULL;
			}
		fseek(ssd->tracefile,0,SEEK_SET);
		printf("simulating %d times.\n", simulate_times);
		//printf("file point: %ld\n", ftell(ssd->tracefile));
		fprintf(ssd->outputfile,"      arrive           lsn     size ope     begin time    response time    process time\n");
		fflush(ssd->outputfile);
		ssd->basic_time = ssd->current_time;
		flag = 1;
		//int trace_count = 0;
		//while(flag!=100 && trace_count < 4500000){
		while(flag!=100){
			flag=get_requests(ssd);//time oprate
			//trace_count++;
			if(flag == 1){
				//printf("once\n");
				if (ssd->parameter->dram_capacity!=0){
					buffer_management(ssd);
					distribute(ssd);
					}
				else{
					no_buffer_distribute(ssd);
					}
				}
			process(ssd);//channel deal.
			trace_output(ssd);
			//if(trace_count%100000 == 0){
			//	printf("trace_num: %d\n",trace_count);
			//	}
			if(flag == 0 && ssd->request_queue == NULL)
				flag = 100;
			if(ssd->completed_request_count > ((int)(ssd->total_request_num/10000))*10000*(simulate_times+1)){
				printf("It should be terminated.\n");
				flag = 100;
				}
			}

		fclose(ssd->tracefile);
		//statistic_output_easy(ssd, simulate_times);
		//ssd->newest_read_avg = 0;
		//ssd->newest_write_avg = 0;
		//ssd->newest_read_request_count = 0;
		//ssd->newest_write_request_count = 0;
		//ssd->newest_write_lsb_count = 0;
		//ssd->newest_write_msb_count = 0;
		simulate_times++;

		unsigned int channel;
		//if((ssd->parameter->dr_switch==1)&&(simulate_times)%(ssd->parameter->dr_cycle)==0){
		/*
		int this_day;
		this_day = (int)(ssd->current_time/1000000000/3600/24);
		if((ssd->parameter->dr_switch==1)&&(this_day>ssd->time_day)&&(this_day%ssd->parameter->dr_cycle==0)){
			ssd->time_day = this_day;
			for(channel=0;channel<ssd->parameter->channel_number;channel++){
				dr_for_channel(ssd, channel);
				}
			}
			*/
		}
	return ssd;
}


/********    get_request    ******************************************************
*	1.get requests that arrived already
*	2.add those request node to ssd->reuqest_queue
*	return	0: reach the end of the trace
*			-1: no request has been added
*			1: add one request to list
*SSD模拟器有三种驱动方式:时钟驱动(精确，太慢) 事件驱动(本程序采用) trace驱动()，
*两种方式推进事件：channel/chip状态改变、trace文件请求达到。
*channel/chip状态改变和trace文件请求到达是散布在时间轴上的点，每次从当前状态到达
*下一个状态都要到达最近的一个状态，每到达一个点执行一次process
********************************************************************************/
int get_requests(struct ssd_info *ssd)
{
//get_request()函数主要负责逐条读取tracefile中的IO请求并且将其挂到ssd->request队列上
	char buffer[200];
	unsigned int lsn=0;
	int device,  size, ope,large_lsn, i = 0,j=0;
	int priority;
	struct request *request1;//I/O请求项
	int flag = 1;
	long filepoint; //文件指针偏移量
	int64_t time_t = 0;//请求到达时间（即时间戳）
	int64_t nearest_event_time;  //寻找所有子请求的最早到达下个状态的时间

	#ifdef DEBUG
	printf("enter get_requests,  current time:%lld\n",ssd->current_time);
	#endif
//feof功能是检测流上的文件结束符，如果文件结束，则返回非0值，否则返回0（即，文件结束：返回非0值；文件未结束：返回0值）。
	if(feof(ssd->tracefile))
		return 0;
//C 库函数 long int ftell(FILE *stream) 返回给定流 stream 的当前文件位置。
	filepoint = ftell(ssd->tracefile);
	fgets(buffer, 200, ssd->tracefile); //从trace文件中读取一行内容至缓冲区
	sscanf(buffer,"%lld %d %d %d %d %d",&time_t,&device,&lsn,&size,&ope,&priority);//按照I/O格式将每行读取的内容中的参数读到time_t，device等中
	//printf("%lld\n", time_t);
    //=======================================
    priority = 1;   //强制设定优先级
    time_t = time_t/(ssd->speed_up);
	time_t = ssd->basic_time + time_t;
	//=======================================
	if ((device<0)&&(lsn<0)&&(size<0)&&(ope<0))//检查这个请求的I/O参数的合理性
	{
		return 100;//不合理返回100，然后赋值给flag退出循环
	}
	//若参数合法，则调整对应的参数（防止不合理）
	if(size==0){
		size = 1;
		}
	if (lsn<ssd->min_lsn)
		ssd->min_lsn=lsn;
	if (lsn>ssd->max_lsn)
		ssd->max_lsn=lsn;
	/******************************************************************************************************
	*上层文件系统发送给SSD的任何读写命令包括两个部分（LSN，size） LSN是逻辑扇区号，对于文件系统而言，它所看到的存
	*储空间是一个线性的连续空间。例如，读请求（260，6）表示的是需要读取从扇区号为260的逻辑扇区开始，总共6个扇区。
	*large_lsn: channel下面有多少个subpage，即多少个sector。overprovide系数：SSD中并不是所有的空间都可以给用户使用，
	*比如32G的SSD可能有10%的空间保留下来留作他用，所以乘以1-provide
	***********************************************************************************************************/
	//（求一个ssd最大的逻辑扇区号，因为chip_num是ssd的prarmeter_value里面的成员（文件initialize.h里面））
	large_lsn=(int)((ssd->parameter->subpage_page*ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->plane_die*ssd->parameter->die_chip*ssd->parameter->chip_num)*(1-ssd->parameter->overprovide));
	//根据当前lsn参数来确定是第几个lsn，也就是具体的逻辑扇区号
	lsn = lsn%large_lsn;

	nearest_event_time=find_nearest_event(ssd);//寻找所有子请求中最早到达下一状态的时间
	if (nearest_event_time==MAX_INT64)//判断这个值是否等于转换到下一个状态的时间极限值
	{
		ssd->current_time=time_t;//如果是，（说明此时获取不到当前IOtrace合理的下一状态预计时间，但是该IO请求是可以进行处理的，只是下一状态预计时间为最大值，I/O会一直被阻塞）则把系统当前时间更新为I/O到达时间（表示I/O已经处理）

		//if (ssd->request_queue_length>ssd->parameter->queue_length)    //如果请求队列的长度超过了配置文件中所设置的长度
		//{
			//printf("error in get request , the queue length is too long\n");
		//}
	}
	else//则nearest_event_time是合理且可用的，表示下一状态是可以到达的
	{
		if(nearest_event_time<time_t)//判断I/O实际到达时间是否大于预计到达时间（若是则表明I/O处于阻塞状态）
		{
			/*******************************************************************************
			*回滚，即如果没有把time_t赋给ssd->current_time，则trace文件已读的一条记录回滚
			*filepoint记录了执行fgets之前的文件指针位置，回滚到文件头+filepoint处
			*int fseek(FILE *stream, long offset, int fromwhere);函数设置文件指针stream的位置。
			*如果执行成功，stream将指向以fromwhere（偏移起始位置：文件头0，当前位置1，文件尾2）为基准，
			*偏移offset（指针偏移量）个字节的位置。如果执行失败(比如offset超过文件自身大小)，则不改变stream指向的位置。
			*文本文件只能采用文件头0的定位方式，本程序中打开文件方式是"r":以只读方式打开文本文件
			**********************************************************************************/
			fseek(ssd->tracefile,filepoint,0);
			if(ssd->current_time<=nearest_event_time)
				ssd->current_time=nearest_event_time;
			return -1;
		}
		else
		{
			if (ssd->request_queue_length>=ssd->parameter->queue_length)//判断ssd中I/O请求对流是否已满
			{
				fseek(ssd->tracefile,filepoint,0);//满了，就回滚重置文件指针
				ssd->current_time=nearest_event_time;//且设置系统时间为下一状态预计到达时间
				return -1;
			}
			else//未满，证明此时当前的IO请求是可以正常插入请求队列
			{
				ssd->current_time=time_t;
			}
		}
	}

	if(time_t < 0)//若IO请求的处理时间非法
	{
		printf("error!\n");
		while(1){}
	}

//程序会继续判断若当前ssd的IO请求队列是否为空；如果为空，则将当前设置好的request1请求插入至IO请求队列中并且更新队列长度；如果非空，则将设置好的request1插入到队列尾部，并且更新队尾指针和队列长度；
	if(feof(ssd->tracefile))//判断是否已经读取完了trace文件中所有的I/Otrace
	{
		//判断结果是已经读取完了，则置空request1，并返回函数
		request1=NULL;
		return 0;
	}
	//若没有读取完，执行到此说明该IO是合理并且可以插入队列
	request1 = (struct request*)malloc(sizeof(struct request));
	alloc_assert(request1,"request");
	memset(request1,0, sizeof(struct request));

	request1->time = time_t;
	request1->lsn = lsn;
	request1->size = size;
	request1->operation = ope;
	request1->priority = priority;//（）设置为不空闲
	request1->begin_time = time_t;
	request1->response_time = 0;
	request1->energy_consumption = 0;
	request1->next_node = NULL;
	request1->distri_flag = 0;              // indicate whether this request has been distributed already
	request1->subs = NULL;
	request1->need_distr_flag = NULL;
	request1->complete_lsn_count=0;         //record the count of lsn served by buffer
	filepoint = ftell(ssd->tracefile);		// set the file point

	if(ssd->request_queue == NULL)          //The queue is empty
	{
		ssd->request_queue = request1;
		ssd->request_tail = request1;
		ssd->request_queue_length++;
	}
	else
	{
		(ssd->request_tail)->next_node = request1;
		ssd->request_tail = request1;
		ssd->request_queue_length++;
	}
	if(ssd->request_queue_length > ssd->max_queue_depth){
		ssd->max_queue_depth = ssd->request_queue_length;
		}
	/*
	if(ssd->request_queue_length > 10){
		if(ssd->parameter->turbo_mode==1 && ssd->performance_mode==0){
			ssd->parameter->turbo_mode_factor = 100;
			ssd->parameter->turbo_mode_factor_2 = 100;
			//printf("Ajusting to high performance, %d, %d.\n", ssd->parameter->turbo_mode_factor, ssd->parameter->turbo_mode_factor_2);
			ssd->performance_mode = 1;
			}
		}
	else if(ssd->request_queue_length < 5){
		if(ssd->parameter->turbo_mode==1 && ssd->performance_mode==1){
			//float turbo_mode_factor, turbo_mode_factor_2;
			//turbo_mode_factor = (((float)ssd->free_lsb_count)/(ssd->free_lsb_count+ssd->free_csb_count+ssd->free_msb_count))*100;
			//turbo_mode_factor_2 = (((float)(ssd->free_lsb_count+ssd->free_csb_count))/(ssd->free_lsb_count+ssd->free_csb_count+ssd->free_msb_count))*100;
			ssd->parameter->turbo_mode_factor = 34;
			ssd->parameter->turbo_mode_factor_2 = 67;
			ssd->performance_mode = 0;
			}
		}
	*/
//程序会继续判断request1为读操作还是写操作：如果为读操作，则需要更新统计读请求平均大小参数，该参数的计算原理是：（当前读请求平均大小 × 读请求数量 + 新的request的大小）/（读请求数量 + 1）；而如果是写操作，则更新统计写操作平均大小参数，该参数的计算原理是：（当前写请求平均大小 × 写请求数量 + 新的request的大小）/（写请求数量 + 1）；
	if (request1->operation==1)             //计算平均请求大小 1为读 0为写
	{
		ssd->ave_read_size=(ssd->ave_read_size*ssd->read_request_count+request1->size)/(ssd->read_request_count+1);
	}
	else
	{
		ssd->ave_write_size=(ssd->ave_write_size*ssd->write_request_count+request1->size)/(ssd->write_request_count+1);
	}


	filepoint = ftell(ssd->tracefile);	//更新文件偏移量
	fgets(buffer, 200, ssd->tracefile);    //寻找下一条请求的到达时间（继续读取下一条I/Otrace）
	sscanf(buffer,"%lld %d %d %d %d",&time_t,&device,&lsn,&size,&ope);
	ssd->next_request_time=time_t;
	fseek(ssd->tracefile,filepoint,0);

//整个get_request()函数便完成了所有的功能。我们可以看到在整个过程中，最为重要的就是在判断tracefile中的IO请求合理性的核心判断过程中，其中的find_nearest_event()函数，它提供了一个寻找当前tracefile的IO请求项在通道channel和芯片chip中对应的最近事件的预计时间。这个时间作为参考可以模拟推进SSD的运行。

	return 1;
}

/**********************************************************************************************************************************************
*首先buffer是个写buffer，就是为写请求服务的，因为读flash的时间tR为20us，写flash的时间tprog为200us，所以为写服务更能节省时间
*  读操作：如果命中了buffer，从buffer读，不占用channel的I/O总线，没有命中buffer，从flash读，占用channel的I/O总线，但是不进buffer了
*  写操作：首先request分成sub_request子请求，如果是动态分配，sub_request挂到ssd->sub_request上，因为不知道要先挂到哪个channel的sub_request上
*          如果是静态分配则sub_request挂到channel的sub_request链上,同时不管动态分配还是静态分配sub_request都要挂到request的sub_request链上
*		   因为每处理完一个request，都要在traceoutput文件中输出关于这个request的信息。处理完一个sub_request,就将其从channel的sub_request链
*		   或ssd的sub_request链上摘除，但是在traceoutput文件输出一条后再清空request的sub_request链。
*		   sub_request命中buffer则在buffer里面写就行了，并且将该sub_page提到buffer链头(LRU)，若没有命中且buffer满，则先将buffer链尾的sub_request
*		   写入flash(这会产生一个sub_request写请求，挂到这个请求request的sub_request链上，同时视动态分配还是静态分配挂到channel或ssd的
*		   sub_request链上),在将要写的sub_page写入buffer链头
***********************************************************************************************************************************************/
struct ssd_info *buffer_management(struct ssd_info *ssd)
{
	unsigned int j,lsn,lpn,last_lpn,first_lpn,index,complete_flag=0, state,full_page;
	unsigned int flag=0,need_distb_flag,lsn_flag,flag1=1,active_region_flag=0;
	struct request *new_request;//指向要处理的请求的指针
	struct buffer_group *buffer_node,key;
	unsigned int mask=0,offset1=0,offset2=0;

	#ifdef DEBUG
	printf("enter buffer_management,  current time:%lld\n",ssd->current_time);
	#endif
	ssd->dram->current_time=ssd->current_time;//设置缓存dram的当前时间为系统的当前时间
	full_page=~(0xffffffff<<ssd->parameter->subpage_page);

	new_request=ssd->request_tail; //从队尾开始处理
	lsn=new_request->lsn;		   //请求的起始逻辑扇区号
	lpn=new_request->lsn/ssd->parameter->subpage_page;
	last_lpn=(new_request->lsn+new_request->size-1)/ssd->parameter->subpage_page;//last_lpn即该IO请求项操作的最大目标页号
	first_lpn=new_request->lsn/ssd->parameter->subpage_page;//first_lpn即该IO请求项操作的起始目标页号

	//need_distr_flag是IO请求项中的子页状态位数组（这里是对其进行初始化设置）（除以32是为了让一个32为的整形数据就能代表32个子页的状态）
	//使用malloc分配后实际上是一个int数组。其中每一个数组元素都存放着对应的子页状态标志信息
	new_request->need_distr_flag=(unsigned int*)malloc(sizeof(unsigned int)*((last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32+1));
	alloc_assert(new_request->need_distr_flag,"new_request->need_distr_flag");
	memset(new_request->need_distr_flag, 0, sizeof(unsigned int)*((last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32+1));

	//读请求
	if(new_request->operation==READ)
	{
		while(lpn<=last_lpn)//当前lpn小于或等于最大目标页号last_lpn时，也就是说明该IO读请求操作尚未完成
		{
			/************************************************************************************************
			 *need_distb_flag表示是否需要执行distribution函数，1表示需要执行，buffer中没有，0表示不需要执行
             *即1表示需要分发，0表示不需要分发，对应点初始全部赋为1
			*************************************************************************************************/
			need_distb_flag=full_page;//先把该lpn下所有子页全部设置1，表明全都要执行distribute函数
			key.group=lpn;//记录当前的lpn
			//avlTreeFind()函数判断该lpn对应的buffer节点是否存在于buffer中
			buffer_node= (struct buffer_group*)avlTreeFind(ssd->dram->buffer, (TREE_NODE *)&key);		// buffer node

			//若读数据缓存命中，且当前lsn未超过最后的lsn，且未超过当前lpn内的lsn（即对当前lpn的操作尚未完成，则进入while循环）
			while((buffer_node!=NULL)&&(lsn<(lpn+1)*ssd->parameter->subpage_page)&&(lsn<=(new_request->lsn+new_request->size-1)))
			{
				lsn_flag=full_page;//表示逻辑子页都要处理
				mask=1 << (lsn%ssd->parameter->subpage_page);//mask记录当前lsn在所在lpn的偏移位置，并标记为1（代表要对lsn位置进行一个读操作）
				if(mask>(2147483648))//mask超过子页规定数量上限32
				{
					printf("the subpage number is larger than 32!add some cases");
					getchar();
				}
				else if((buffer_node->stored & mask)==mask)
				{
					flag=1;//置标志位为1，表示读取子页成功
					lsn_flag=lsn_flag&(~mask);//更新lsn_flag记录，将mask中对应lsn的子页标志在lsn_flag中清零，代表该子页已经读取完成
				}

				//随后程序流会判断flag标志以确定当前读取的子页是否成功完成。
				//若flag为1，说明读操作成功。若flag不为1，则说明读操作未成功。
				if(flag==1)//读取成功
				{	//如果该buffer节点不在buffer的队首，需要将这个节点提到队首，实现了LRU算法，这个是一个双向队列。
					//调整LRU队列
					if(ssd->dram->buffer->buffer_head!=buffer_node)
					{
						if(ssd->dram->buffer->buffer_tail==buffer_node)
						{
							buffer_node->LRU_link_pre->LRU_link_next=NULL;
							ssd->dram->buffer->buffer_tail=buffer_node->LRU_link_pre;
						}
						else
						{
							buffer_node->LRU_link_pre->LRU_link_next=buffer_node->LRU_link_next;
							buffer_node->LRU_link_next->LRU_link_pre=buffer_node->LRU_link_pre;
						}
						buffer_node->LRU_link_next=ssd->dram->buffer->buffer_head;
						ssd->dram->buffer->buffer_head->LRU_link_pre=buffer_node;
						buffer_node->LRU_link_pre=NULL;
						ssd->dram->buffer->buffer_head=buffer_node;
					}
					ssd->dram->buffer->read_hit++;
					new_request->complete_lsn_count++;
				}
				else if(flag==0)//读取未成功,说明mask不合理。因为时lpn命中，但不一定每一个lsn都命中了
					{
						ssd->dram->buffer->read_miss_hit++;//更新缓存读的未命中率
					}

				need_distb_flag=need_distb_flag&lsn_flag;//根据lsn_flag更新need_distb_flag标志

				flag=0;
				lsn++;
			}//至此，lpn也就是当前页中所有目标子页通过循环已全部读取完毕

			//更新子页状态位（操作了状态就要变化）
			index=(lpn-first_lpn)/(32/ssd->parameter->subpage_page);
			//need_distr_flag数组，在初始化时所有子页状态位全为0表示对应的所有子页全部尚未处理。（声明数组但不定义，默认元素全为0）
			new_request->need_distr_flag[index]=new_request->need_distr_flag[index]|(need_distb_flag<<(((lpn-first_lpn)%(32/ssd->parameter->subpage_page))*ssd->parameter->subpage_page));
			lpn++;
            if(!buffer_node)
                lsn=lpn*ssd->parameter->subpage_page;
		}
	}
	//写请求
	else if(new_request->operation==WRITE)
	{
		while(lpn<=last_lpn)
		{
			need_distb_flag=full_page;
			mask=~(0xffffffff<<(ssd->parameter->subpage_page));//初始化为全1，表示全部都要进行处理，赋给mask，代表子页状态屏蔽码（读里面的mask代表lsn偏移位置）
			state=mask;

			if(lpn==first_lpn)//判断当前操作的lpn页号是否是IO请求项的初始页号first_lpn
			{
				//当前IO请求项的初始lsn可能并不是lpn内的第一个lsn，所以需要计算相应的lsn偏移量
				offset1=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-new_request->lsn);
				//此时，state标志便只存放着该IO请求项中在当前lpn的所有目标lsn的状态位信息了
				state=state&(0xffffffff<<offset1);//根据偏移量设置状态位（全设置设为1）
			}
			//如果当前操作的lpn不是初始lpn，就判段是不是最后一个lpn。
			if(lpn==last_lpn)//如果是
			{
				//此时IO请求项的最后一个目标lsn并不是last_lpn的最后一个lsn
				//那么就需要计算出自last_lpn起始lsn到IO请求项的最后一个目标lsn的这一段子页偏移量offset2
				offset2=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-(new_request->lsn+new_request->size));
				state=state&(~(0xffffffff<<offset2));
			}
			/*若当lpn既不是首页也不是最后页时，
			那么只可能说明当前的lpn是处于中间位置的lpn，
			此时处于中间位置的lpn页都是全部子页有效的，
			因此不需要进行偏移量的计算处理。
			*/
			//确定lpn后，就可以针对当前lpn创建写子请求
			ssd=insert2buffer(ssd, lpn, state,NULL,new_request);
			lpn++;
		}
	}
	complete_flag = 1;//操作完成标志
	//随即进入一个for循环针对当前IO请求项的子页状态数组中子页状态情况的一个判断
	for(j=0;j<=(last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32;j++)
	{
		if(new_request->need_distr_flag[j] != 0)//该元素！=0，则对应的逻辑页的子页部分是未命中于buffer中的，也就是存在没有读取成功的部分
		{
			complete_flag = 0;//表示操作尚未完全完成
		}
	}

	/*************************************************************
	*如果请求已经被全部由buffer服务，该请求可以被直接响应，输出结果
	*这里假设dram的服务时间为1000ns
	**************************************************************/
	if((complete_flag == 1)&&(new_request->subs==NULL))
	{
		new_request->begin_time=ssd->current_time;
		new_request->response_time=ssd->current_time+1000;
	}

	return ssd;
}

/*****************************
*lpn向ppn的转换，
******************************/
unsigned int lpn2ppn(struct ssd_info *ssd,unsigned int lsn)
{
	int lpn, ppn;
	struct entry *p_map = ssd->dram->map->map_entry;                //获取映射表
#ifdef DEBUG
	printf("enter lpn2ppn,  current time:%lld\n",ssd->current_time);
#endif
	lpn = lsn/ssd->parameter->subpage_page;			//subpage_page指一个page中包含的子页数量，在参数文件中可以设定
	ppn = (p_map[lpn]).pn;                     //逻辑页lpn和物理页ppn的映射记录在映射表中
	return ppn;
}

/**********************************************************************************
*读请求分配子请求函数，这里只处理读请求，写请求已经在buffer_management()函数中处理了
*根据请求队列和buffer命中的检查，将每个请求分解成子请求，将子请求队列挂在channel上，
*不同的channel有自己的子请求队列
**********************************************************************************/

struct ssd_info *distribute(struct ssd_info *ssd)
{
	unsigned int start, end, first_lsn,last_lsn,lpn,flag=0,flag_attached=0,full_page;
	unsigned int j, k, sub_size;
	int i=0;
	struct request *req;
	struct sub_request *sub;
	int* complt;

	#ifdef DEBUG
	printf("enter distribute,  current time:%lld\n",ssd->current_time);
	#endif
	full_page=~(0xffffffff<<ssd->parameter->subpage_page);

	req = ssd->request_tail;//请求队列的队尾
	if(req->response_time != 0){
		return ssd;
	}
	if (req->operation==WRITE)
	{
		return ssd;
	}

	if(req != NULL)//有队尾，即队列不为空
	{
		if(req->distri_flag == 0)//请求缓存未命中后，尚未分配出去
		{
			//如果还有一些读请求需要处理
			if(req->complete_lsn_count != ssd->request_tail->size)
			{
				first_lsn = req->lsn;	//请求起始地址（lsn）
				last_lsn = first_lsn + req->size;//请求的最后一个lsn
				complt = req->need_distr_flag;//子页状态标志为数组，表示那些子页需要进行操作
				start = first_lsn - first_lsn % ssd->parameter->subpage_page;
				end = (last_lsn/ssd->parameter->subpage_page + 1) * ssd->parameter->subpage_page;
				i = (end - start)/32;

				while(i >= 0)
				{
					/*************************************************************************************
					*一个32位的整型数据的每一位代表一个子页，32/ssd->parameter->subpage_page就表示有多少页，
					*这里的每一页的状态都存放在了 req->need_distr_flag中，也就是complt中，通过比较complt的
					*每一项与full_page，就可以知道，这一页是否处理完成。如果没处理完成则通过creat_sub_request
					函数创建子请求。
					*************************************************************************************/
					for(j=0; j<32/ssd->parameter->subpage_page; j++)//遍历所有相关页
					{
						k = (complt[((end-start)/32-i)] >>(ssd->parameter->subpage_page*j)) & full_page;
						if (k !=0)//表示这个子页需要进行处理
						{
							lpn = start/ssd->parameter->subpage_page+ ((end-start)/32-i)*32/ssd->parameter->subpage_page + j;
							sub_size=transfer_size(ssd,k,lpn,req);
							if (sub_size==0)
							{
								continue;
							}
							else//sub_size！=0
							{
								sub=creat_sub_request(ssd,lpn,sub_size,0,req,req->operation,0);
							}
						}
					}
					i = i-1;
				}

			}
			else
			{
				req->begin_time=ssd->current_time;
				req->response_time=ssd->current_time+1000;
			}

		}
	}
	return ssd;
}


/**********************************************************************
*trace_output()函数是在每一条请求的所有子请求经过process()函数处理完后，
*打印输出相关的运行结果到outputfile文件中，这里的结果主要是运行的时间
**********************************************************************/
void trace_output(struct ssd_info* ssd){
	int flag = 1;
	int64_t start_time, end_time;
	struct request *req, *pre_node;
	struct sub_request *sub, *tmp;

#ifdef DEBUG
	printf("enter trace_output,  current time:%lld\n",ssd->current_time);
#endif
	int debug_0918 = 0;
	pre_node=NULL;
	req = ssd->request_queue;
	start_time = 0;
	end_time = 0;

	if(req == NULL)
		return;

	while(req != NULL)
	{
		sub = req->subs;
		flag = 1;
		start_time = 0;
		end_time = 0;
		if(req->response_time != 0)
		{
			//printf("Rsponse time != 0?\n");
			fprintf(ssd->outputfile,"%16lld %10d %6d %2d %16lld %16lld %10lld\n",req->time,req->lsn, req->size, req->operation, req->begin_time, req->response_time, req->response_time-req->time);
			fflush(ssd->outputfile);
			ssd->completed_request_count++;
			if(ssd->completed_request_count%10000 == 0){
				printf("completed requests: %d\n", ssd->completed_request_count);
				statistic_output_easy(ssd, ssd->completed_request_count);
				ssd->newest_read_avg = 0;
				ssd->newest_write_avg = 0;
				ssd->newest_read_request_count = 0;
				ssd->newest_write_request_count = 0;
				ssd->newest_write_lsb_count = 0;
				ssd->newest_write_csb_count = 0;
				ssd->newest_write_msb_count = 0;
				ssd->newest_write_request_completed_with_same_type_pages = 0;
				//***************************************************************************
				int channel;
				int this_day;
				this_day = (int)(ssd->current_time/1000000000/3600/24);
				/*
				if(this_day>ssd->time_day){
					printf("Another Day begin, %d.\n", this_day);
					}
					*/
				if(this_day>ssd->time_day){
					printf("Day %d begin......\n", this_day);
					ssd->time_day = this_day;
					if((ssd->parameter->dr_switch==1)&&(this_day%ssd->parameter->dr_cycle==0)){
						for(channel=0;channel<ssd->parameter->channel_number;channel++){
							dr_for_channel(ssd, channel);
							}
						}
					/*
					if((ssd->parameter->turbo_mode==2)&&(this_day%7==3)){
						printf("Enter turbo-mode.....\n");
						ssd->parameter->lsb_first_allocation = 1;
						ssd->parameter->fast_gc = 1;
						}
					else if(ssd->parameter->turbo_mode==2){
						//printf("Exist turbo-mode....\n");
						ssd->parameter->lsb_first_allocation = 0;
						ssd->parameter->fast_gc = 0;
						}
						*/
					}
				//***************************************************************************
				}

			if(debug_0918){
				printf("completed requests: %d\n", ssd->completed_request_count);
				}

			if(req->response_time-req->begin_time==0)
			{
				printf("the response time is 0?? \n");
				getchar();
			}

			if (req->operation==READ)
			{
				ssd->read_request_count++;
				ssd->read_avg=ssd->read_avg+(req->response_time-req->time);
				//===========================================
				ssd->newest_read_request_count++;
				ssd->newest_read_avg = ssd->newest_read_avg+(end_time-req->time);
				//===========================================
			}
			else
			{
				ssd->write_request_count++;
				ssd->write_avg=ssd->write_avg+(req->response_time-req->time);
				//===========================================
				ssd->newest_write_request_count++;
				ssd->newest_write_avg = ssd->newest_write_avg+(end_time-req->time);
				ssd->last_write_lat = end_time-req->time;
//				//--------------------------------------------
//				int new_flag = 1;
//				int origin;
//				struct sub_request *next_sub_a;
//				next_sub_a = req->subs;
//				origin = next_sub_a->allocated_page_type;
//				next_sub_a = next_sub_a->next_subs;
//				while(next_sub_a!=NULL){
//					if(next_sub_a->allocated_page_type != origin){
//						new_flag = 0;
//						break;
//						}
//					next_sub_a = next_sub_a->next_subs;
//					}
//				if(new_flag==1){
//					ssd->newest_write_request_completed_with_same_type_pages++;
//					if(origin==1){
//						ssd->newest_msb_request_a++;
//						}
//					else{
//						ssd->newest_lsb_request_a++;
//						}
//					}
				//-------------------------------------------

				//===========================================
			}

			if(pre_node == NULL)
			{
				if(req->next_node == NULL)
				{
					free(req->need_distr_flag);
					req->need_distr_flag=NULL;
					free(req);
					req = NULL;
					ssd->request_queue = NULL;
					ssd->request_tail = NULL;
					ssd->request_queue_length--;
				}
				else
				{
					ssd->request_queue = req->next_node;
					pre_node = req;
					req = req->next_node;
					free(pre_node->need_distr_flag);
					pre_node->need_distr_flag=NULL;
					free((void *)pre_node);
					pre_node = NULL;
					ssd->request_queue_length--;
				}
			}
			else
			{
				if(req->next_node == NULL)
				{
					pre_node->next_node = NULL;
					free(req->need_distr_flag);
					req->need_distr_flag=NULL;
					free(req);
					req = NULL;
					ssd->request_tail = pre_node;
					ssd->request_queue_length--;
				}
				else
				{
					pre_node->next_node = req->next_node;
					free(req->need_distr_flag);
					req->need_distr_flag=NULL;
					free((void *)req);
					req = pre_node->next_node;
					ssd->request_queue_length--;
				}
			}
		}
		else
		{
			//printf("Rsponse time = 0!\n");
			flag=1;
			while(sub != NULL)
			{
				if(start_time == 0)
					start_time = sub->begin_time;
				if(start_time > sub->begin_time)
					start_time = sub->begin_time;
				if(end_time < sub->complete_time)
					end_time = sub->complete_time;
				if((sub->current_state == SR_COMPLETE)||((sub->next_state==SR_COMPLETE)&&(sub->next_state_predict_time<=ssd->current_time)))	// if any sub-request is not completed, the request is not completed
				{
					sub = sub->next_subs;
				}
				else
				{
					flag=0;
					break;
				}
			}

			if (flag == 1)
			{
				//fprintf(ssd->outputfile,"%10I64u %10u %6u %2u %16I64u %16I64u %10I64u\n",req->time,req->lsn, req->size, req->operation, start_time, end_time, end_time-req->time);
				fprintf(ssd->outputfile,"%16lld %10d %6d %2d %16lld %16lld %10lld\n",req->time,req->lsn, req->size, req->operation, start_time, end_time, end_time-req->time);
				fflush(ssd->outputfile);
				FILE *fp;
				fp = fopen("./tail_latency.csv", "a+");
				fprintf(fp, "%lld,", end_time-req->time);
				fflush(fp);
				fclose(fp);
				ssd->completed_request_count++;
				if(ssd->completed_request_count%10000 == 0){
					printf("completed requests: %d, max_queue_depth: %d, ", ssd->completed_request_count, ssd->max_queue_depth);
					printf("free_lsb: %d, free_csb: %d, free_msb: %d\n", ssd->free_lsb_count, ssd->free_csb_count, ssd->free_msb_count);
					ssd->max_queue_depth = 0;
					statistic_output_easy(ssd, ssd->completed_request_count);
					ssd->newest_read_avg = 0;
					ssd->newest_write_avg = 0;
					ssd->newest_write_avg_l = 0;
					ssd->newest_read_request_count = 0;
					ssd->newest_write_request_count = 0;
					ssd->newest_write_lsb_count = 0;
					ssd->newest_write_csb_count = 0;
					ssd->newest_write_msb_count = 0;
					ssd->newest_write_request_completed_with_same_type_pages_l = 0;
					ssd->newest_write_request_num_l= 0;
					ssd->newest_req_with_lsb_l = 0;
					ssd->newest_req_with_csb_l = 0;
					ssd->newest_req_with_msb_l = 0;
					ssd->newest_write_request_completed_with_same_type_pages = 0;
					ssd->newest_req_with_lsb = 0;
					ssd->newest_req_with_csb = 0;
					ssd->newest_req_with_msb = 0;
					//***************************************************************************
					int channel;
					int this_day;
					this_day = (int)(ssd->current_time/1000000000/3600/24);
					if(this_day>ssd->time_day){
						printf("Day %d begin......\n", this_day);
						ssd->time_day = this_day;
						if((ssd->parameter->dr_switch==1)&&(this_day%ssd->parameter->dr_cycle==0)){
							for(channel=0;channel<ssd->parameter->channel_number;channel++){
								dr_for_channel(ssd, channel);
								}
							}
						/*
						if((ssd->parameter->turbo_mode==2)&&(this_day%2==1)){
							printf("Enter turbo-mode.....\n");
							ssd->parameter->lsb_first_allocation = 1;
							ssd->parameter->fast_gc = 1;
							}
						else{
							//printf("Exist turbo-mode....\n");
							ssd->parameter->lsb_first_allocation = 0;
							ssd->parameter->fast_gc = 0;
							}
							*/
						}
					//***************************************************************************
					}

				if(debug_0918){
					printf("completed requests: %d\n", ssd->completed_request_count);
					}
				if(end_time-start_time==0)
				{
					printf("the response time is 0?? position 2\n");
					//getchar();
				}
				if (req->operation==READ)
				{
					ssd->read_request_count++;
					ssd->read_avg=ssd->read_avg+(end_time-req->time);
					//=============================================
					ssd->newest_read_request_count++;
					ssd->newest_read_avg = ssd->newest_read_avg+(end_time-req->time);
					//==============================================
				}
				else
				{
					ssd->write_request_count++;
					ssd->write_avg=ssd->write_avg+(end_time-req->time);
					//=============================================
					ssd->newest_write_request_count++;
					ssd->newest_write_avg = ssd->newest_write_avg+(end_time-req->time);
					ssd->last_write_lat = end_time-req->time;
					ssd->last_ten_write_lat[ssd->write_lat_anchor] = end_time-req->time;
					ssd->write_lat_anchor = (ssd->write_lat_anchor+1)%10;

					//--------------------------------------------
					int new_flag = 1;
					int origin, actual_type;
					int num_of_sub = 1;
					struct sub_request *next_sub_a;
					next_sub_a = req->subs;
					origin = next_sub_a->allocated_page_type;
					actual_type = next_sub_a->allocated_page_type;
					next_sub_a = next_sub_a->next_subs;
					while(next_sub_a!=NULL){
						num_of_sub++;
						if(next_sub_a->allocated_page_type > actual_type){
							actual_type = next_sub_a->allocated_page_type;
							}
						if(next_sub_a->allocated_page_type != origin){
							new_flag = 0;
							}
						next_sub_a = next_sub_a->next_subs;
						}
					if(num_of_sub>1){
						ssd->write_request_count_l++;
						ssd->newest_write_request_num_l++;
						ssd->newest_write_avg_l = ssd->newest_write_avg_l+(end_time-req->time);
						ssd->write_avg_l = ssd->write_avg_l+(end_time-req->time);
						}
					if(new_flag==1){
						ssd->newest_write_request_completed_with_same_type_pages++;
						if(num_of_sub>1){
							ssd->newest_write_request_completed_with_same_type_pages_l++;
							}
						if(origin==1){
							ssd->newest_msb_request_a++;
							}
						else if(origin==0){
							ssd->newest_lsb_request_a++;
							}
						else{
							ssd->newest_csb_request_a++;
							}
						}
					if(actual_type==TARGET_LSB){
						ssd->newest_req_with_lsb++;
						if(num_of_sub>1){
							ssd->newest_req_with_lsb_l++;
							}
						}
					else if(actual_type==TARGET_CSB){
						ssd->newest_req_with_csb++;
						if(num_of_sub>1){
							ssd->newest_req_with_csb_l++;
							}
						}
					else{
						ssd->newest_req_with_msb++;
						if(num_of_sub>1){
							ssd->newest_req_with_msb_l++;
							}
						}
					//-------------------------------------------

					//==============================================
				}

				while(req->subs!=NULL)
				{
					tmp = req->subs;
					req->subs = tmp->next_subs;
					if (tmp->update!=NULL)
					{
						free(tmp->update->location);
						tmp->update->location=NULL;
						free(tmp->update);
						tmp->update=NULL;
					}
					free(tmp->location);
					tmp->location=NULL;
					free(tmp);
					tmp=NULL;
				}

				if(pre_node == NULL)
				{
					if(req->next_node == NULL)
					{
						free(req->need_distr_flag);
						req->need_distr_flag=NULL;
						free(req);
						req = NULL;
						ssd->request_queue = NULL;
						ssd->request_tail = NULL;
						ssd->request_queue_length--;
					}
					else
					{
						ssd->request_queue = req->next_node;
						pre_node = req;
						req = req->next_node;
						free(pre_node->need_distr_flag);
						pre_node->need_distr_flag=NULL;
						free(pre_node);
						pre_node = NULL;
						ssd->request_queue_length--;
					}
				}
				else
				{
					if(req->next_node == NULL)
					{
						pre_node->next_node = NULL;
						free(req->need_distr_flag);
						req->need_distr_flag=NULL;
						free(req);
						req = NULL;
						ssd->request_tail = pre_node;
						ssd->request_queue_length--;
					}
					else
					{
						pre_node->next_node = req->next_node;
						free(req->need_distr_flag);
						req->need_distr_flag=NULL;
						free(req);
						req = pre_node->next_node;
						ssd->request_queue_length--;
					}

				}
			}
			else
			{
				pre_node = req;
				req = req->next_node;
			}
		}
	}
}


/*******************************************************************************
*statistic_output()函数主要是输出处理完一条请求后的相关处理信息。
*1，计算出每个plane的擦除次数即plane_erase和总的擦除次数即erase
*2，打印min_lsn，max_lsn，read_count，program_count等统计信息到文件outputfile中。
*3，打印相同的信息到文件statisticfile中
*******************************************************************************/
void statistic_output(struct ssd_info *ssd)
{
	unsigned int lpn_count=0,i,j,k,m,p,erase=0,plane_erase=0;
	double gc_energy=0.0;
#ifdef DEBUG
	printf("enter statistic_output,  current time:%lld\n",ssd->current_time);
#endif

	for(i=0;i<ssd->parameter->channel_number;i++)
	{
		for(j=0;j<ssd->parameter->chip_channel[0];j++)
		{
			for(k=0;k<ssd->parameter->die_chip;k++)
			{
				for(p=0;p<ssd->parameter->plane_die;p++)
				{
					plane_erase=0;
					for(m=0;m<ssd->parameter->block_plane;m++)
					{
						if(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[p].blk_head[m].erase_count>0)
						{
							erase=erase+ssd->channel_head[i].chip_head[j].die_head[k].plane_head[p].blk_head[m].erase_count;
							plane_erase+=ssd->channel_head[i].chip_head[j].die_head[k].plane_head[p].blk_head[m].erase_count;
						}
						int l;
						for (l = 0; l < ssd->parameter->page_block; l++)
						{
							if (ssd->channel_head[i].chip_head[j].die_head[k].plane_head[p].blk_head[m].page_head[l].lpn != -1 && ssd->dram->map->map_entry[ssd->channel_head[i].chip_head[j].die_head[k].plane_head[p].blk_head[m].page_head[l].lpn].hdd_flag == 2)
							{
								// 被打包块未做GC
								ssd->non_gc_hdd_count++;
							}
							
						}
						
					}
					fprintf(ssd->outputfile,"the %d channel, %d chip, %d die, %d plane has : %13d erase operations\n",i,j,k,p,plane_erase);
					fprintf(ssd->statisticfile,"the %d channel, %d chip, %d die, %d plane has : %13d erase operations\n",i,j,k,p,plane_erase);
				}
			}
		}
	}
	// FILE * fp;
	// fp = fopen("./seq.dat", "w");
	// struct seq_write *seq = ssd->seq_write_head;
	// while (seq)
	// {
	// 	fprintf(fp,"lpn: %d read_num: %d write_num: %d hdd_num: %d hot_num: %d\n", seq->lpn, seq->read_num, seq->write_num, seq->hdd_num, seq->hot_num);
	// 	seq = seq->next;
	// }
	// fflush(fp);
	// fclose(fp);

	// 记录更新写了的lpn
	struct update_write *update_write = ssd->update_write_head;
	FILE * fp;
	fp = fopen("./update-write.csv", "a+");
	fprintf(fp,"%s\n", ssd->tracefilename);
	while (update_write)
	{
		fprintf(fp,"lpn: %11d, num: %d\n", update_write->lpn, update_write->num);
		update_write = update_write->next;
	}
	fflush(fp);
	fclose(fp);

	fprintf(ssd->outputfile,"\n");
	fprintf(ssd->outputfile,"\n");
	fprintf(ssd->outputfile,"---------------------------statistic data---------------------------\n");
	fprintf(ssd->outputfile,"min lsn: %13d\n",ssd->min_lsn);
	fprintf(ssd->outputfile,"max lsn: %13d\n",ssd->max_lsn);
	fprintf(ssd->outputfile,"read count: %13d\n",ssd->read_count);
	fprintf(ssd->outputfile,"program count: %13d",ssd->program_count);
	fprintf(ssd->outputfile,"                        include the flash write count leaded by read requests\n");
	fprintf(ssd->outputfile,"the read operation leaded by un-covered update count: %13d\n",ssd->update_read_count);
	fprintf(ssd->outputfile,"erase count: %13d\n",ssd->erase_count);
	fprintf(ssd->outputfile,"direct erase count: %13d\n",ssd->direct_erase_count);
	fprintf(ssd->outputfile,"gc_count: %13d\n",ssd->gc_count);
	fprintf(ssd->outputfile,"copy back count: %13d\n",ssd->copy_back_count);
	fprintf(ssd->outputfile,"multi-plane program count: %13d\n",ssd->m_plane_prog_count);
	fprintf(ssd->outputfile,"multi-plane read count: %13d\n",ssd->m_plane_read_count);
	fprintf(ssd->outputfile,"interleave write count: %13d\n",ssd->interleave_count);
	fprintf(ssd->outputfile,"interleave read count: %13d\n",ssd->interleave_read_count);
	fprintf(ssd->outputfile,"interleave two plane and one program count: %13d\n",ssd->inter_mplane_prog_count);
	fprintf(ssd->outputfile,"interleave two plane count: %13d\n",ssd->inter_mplane_count);
	fprintf(ssd->outputfile,"gc copy back count: %13d\n",ssd->gc_copy_back);
	fprintf(ssd->outputfile,"write flash count: %13d\n",ssd->write_flash_count);
	//=================================================================
	fprintf(ssd->outputfile,"write LSB count: %13d\n",ssd->write_lsb_count);
	fprintf(ssd->outputfile,"write MSB count: %13d\n",ssd->write_msb_count);
	//=================================================================
	fprintf(ssd->outputfile,"interleave erase count: %13d\n",ssd->interleave_erase_count);
	fprintf(ssd->outputfile,"multiple plane erase count: %13d\n",ssd->mplane_erase_conut);
	fprintf(ssd->outputfile,"interleave multiple plane erase count: %13d\n",ssd->interleave_mplane_erase_count);
	fprintf(ssd->outputfile,"read request count: %13d\n",ssd->read_request_count);
	fprintf(ssd->outputfile,"write request count: %13d\n",ssd->write_request_count);
	fprintf(ssd->outputfile,"read request average size: %13f\n",ssd->ave_read_size);
	fprintf(ssd->outputfile,"write request average size: %13f\n",ssd->ave_write_size);
	fprintf(ssd->outputfile,"read request average response time: %lld\n",ssd->read_avg/ssd->read_request_count);
	fprintf(ssd->outputfile,"write request average response time: %lld\n",ssd->write_avg/ssd->write_request_count);
	fprintf(ssd->outputfile,"buffer read hits: %13d\n",ssd->dram->buffer->read_hit);
	fprintf(ssd->outputfile,"buffer read miss: %13d\n",ssd->dram->buffer->read_miss_hit);
	fprintf(ssd->outputfile,"buffer write hits: %13d\n",ssd->dram->buffer->write_hit);
	fprintf(ssd->outputfile,"buffer write miss: %13d\n",ssd->dram->buffer->write_miss_hit);
	fprintf(ssd->outputfile,"erase: %13d\n",erase);
	fprintf(ssd->outputfile,"sub_request_all: %13d, sub_request_success: %13d\n", ssd->sub_request_all, ssd->sub_request_success);
	// fprintf(ssd->outputfile,"%2d, %8d, %8d, %14lld, %14lld, %10d, %10d, %10d, %8d, %8d, %8d\n", ssd->seq_num, ssd->gc_count, ssd->moved_page_count, ssd->read_avg/ssd->read_request_count, ssd->write_avg/ssd->write_request_count, ssd->update_hdd_count, ssd->non_gc_hdd_count, ssd->trace_write_count, ssd->writeback_count, ssd->ssd_write_hdd_seq_count, ssd->ssd_write_hdd_rand_count);
	fprintf(ssd->outputfile,"%2d, %8d, %8d, %8d, %8d, %14lld, %14lld, %10d, %8d, %8d, %8d\n", ssd->seq_num, ssd->gc_count, ssd->direct_erase_count, ssd->erase_count, ssd->moved_page_count, ssd->read_avg/ssd->read_request_count, ssd->write_avg/ssd->write_request_count, ssd->trace_write_count, ssd->writeback_count, ssd->ssd_write_hdd_seq_count, ssd->ssd_write_hdd_rand_count);
	fflush(ssd->outputfile);

	fclose(ssd->outputfile);


	fprintf(ssd->statisticfile,"\n");
	fprintf(ssd->statisticfile,"\n");
	fprintf(ssd->statisticfile,"---------------------------statistic data---------------------------\n");
	fprintf(ssd->statisticfile,"min lsn: %13d\n",ssd->min_lsn);
	fprintf(ssd->statisticfile,"max lsn: %13d\n",ssd->max_lsn);
	fprintf(ssd->statisticfile,"read count: %13d\n",ssd->read_count);
	fprintf(ssd->statisticfile,"program count: %13d",ssd->program_count);
	fprintf(ssd->statisticfile,"                        include the flash write count leaded by read requests\n");
	fprintf(ssd->statisticfile,"the read operation leaded by un-covered update count: %13d\n",ssd->update_read_count);
	fprintf(ssd->statisticfile,"erase count: %13d\n",ssd->erase_count);
	fprintf(ssd->statisticfile,"direct erase count: %13d\n",ssd->direct_erase_count);
	fprintf(ssd->statisticfile,"copy back count: %13d\n",ssd->copy_back_count);
	fprintf(ssd->statisticfile,"multi-plane program count: %13d\n",ssd->m_plane_prog_count);
	fprintf(ssd->statisticfile,"multi-plane read count: %13d\n",ssd->m_plane_read_count);
	fprintf(ssd->statisticfile,"interleave count: %13d\n",ssd->interleave_count);
	fprintf(ssd->statisticfile,"interleave read count: %13d\n",ssd->interleave_read_count);
	fprintf(ssd->statisticfile,"interleave two plane and one program count: %13d\n",ssd->inter_mplane_prog_count);
	fprintf(ssd->statisticfile,"interleave two plane count: %13d\n",ssd->inter_mplane_count);
	fprintf(ssd->statisticfile,"gc copy back count: %13d\n",ssd->gc_copy_back);
	fprintf(ssd->statisticfile,"write flash count: %13d\n",ssd->write_flash_count);
	//=================================================================
	fprintf(ssd->statisticfile,"write LSB count: %13d\n",ssd->write_lsb_count);
	fprintf(ssd->statisticfile,"write CSB count: %13d\n",ssd->write_csb_count);
	fprintf(ssd->statisticfile,"write MSB count: %13d\n",ssd->write_msb_count);
	//=================================================================
	fprintf(ssd->statisticfile,"waste page count: %13d\n",ssd->waste_page_count);
	fprintf(ssd->statisticfile,"interleave erase count: %13d\n",ssd->interleave_erase_count);
	fprintf(ssd->statisticfile,"multiple plane erase count: %13d\n",ssd->mplane_erase_conut);
	fprintf(ssd->statisticfile,"interleave multiple plane erase count: %13d\n",ssd->interleave_mplane_erase_count);
	fprintf(ssd->statisticfile,"read request count: %13d\n",ssd->read_request_count);
	fprintf(ssd->statisticfile,"write request count: %13d\n",ssd->write_request_count);
	fprintf(ssd->statisticfile,"read request average size: %13f\n",ssd->ave_read_size);
	fprintf(ssd->statisticfile,"write request average size: %13f\n",ssd->ave_write_size);
	fprintf(ssd->statisticfile,"read request average response time: %lld\n",ssd->read_avg/ssd->read_request_count);
	fprintf(ssd->statisticfile,"write request average response time: %lld\n",ssd->write_avg/ssd->write_request_count);
	if(ssd->write_request_count_l==0){
		fprintf(ssd->statisticfile,"large write request average response time: 0\n");
		}
	else{
		fprintf(ssd->statisticfile,"large write request average response time: %lld\n",ssd->write_avg_l/ssd->write_request_count_l);
		}
	fprintf(ssd->statisticfile,"buffer read hits: %13d\n",ssd->dram->buffer->read_hit);
	fprintf(ssd->statisticfile,"buffer read miss: %13d\n",ssd->dram->buffer->read_miss_hit);
	fprintf(ssd->statisticfile,"buffer write hits: %13d\n",ssd->dram->buffer->write_hit);
	fprintf(ssd->statisticfile,"buffer write miss: %13d\n",ssd->dram->buffer->write_miss_hit);
	fprintf(ssd->statisticfile,"erase: %13d\n",erase);
	fprintf(ssd->statisticfile,"sub_request_all: %13d, sub_request_success: %13d\n", ssd->sub_request_all, ssd->sub_request_success);
	fflush(ssd->statisticfile);

	fclose(ssd->statisticfile);
}

void statistic_output_easy(struct ssd_info *ssd, unsigned long completed_requests_num){
	unsigned int lpn_count=0,i,j,k,m,erase=0,plane_erase=0;
	double gc_energy=0.0;
#ifdef DEBUG
	fprintf(ssd->debugfile,"enter statistic_output,  current time:%lld\n",ssd->current_time);
	//printf("enter statistic_output,  current time:%lld\n",ssd->current_time);
#endif
	unsigned long read_avg_lat, write_avg_lat, write_avg_lat_l;
	if(ssd->newest_read_request_count==0){
		read_avg_lat=0;
		}
	else{
		read_avg_lat=ssd->newest_read_avg/ssd->newest_read_request_count;
		}
	if(ssd->newest_write_request_count==0){
		write_avg_lat=0;
		}
	else{
		write_avg_lat=ssd->newest_write_avg/ssd->newest_write_request_count;
		}
	if(ssd->newest_write_request_num_l==0){
		write_avg_lat_l=0;
		}
	else{
		write_avg_lat_l = ssd->newest_write_avg_l/ssd->newest_write_request_num_l;
		}
	fprintf(ssd->statisticfile, "%lld, %16lld, %13d, %13lld, %13lld, %13d, %13d, %13d, ", completed_requests_num, ssd->current_time, ssd->erase_count, read_avg_lat, write_avg_lat,ssd->newest_write_lsb_count,ssd->newest_write_csb_count,ssd->newest_write_msb_count);
	fprintf(ssd->statisticfile, "%13d, %13d, %13d, %13d, %13d, ", ssd->fast_gc_count, ssd->moved_page_count, ssd->free_lsb_count, ssd->newest_read_request_count, ssd->newest_write_request_count);
	fprintf(ssd->statisticfile, "%13d, %13d, %13d, %13d, ", ssd->newest_write_request_completed_with_same_type_pages, ssd->newest_req_with_lsb, ssd->newest_req_with_csb, ssd->newest_req_with_msb);
	fprintf(ssd->statisticfile, "% 13d, %13d, %13d, %13d, %13d, %13d\n", ssd->newest_write_request_num_l, ssd->newest_write_request_completed_with_same_type_pages_l, ssd->newest_req_with_lsb_l, ssd->newest_req_with_csb_l, ssd->newest_req_with_msb_l, write_avg_lat_l);

	//fprintf(ssd->statisticfile, "%13d, %13d, %13d\n", ssd->newest_write_request_completed_with_same_type_pages, ssd->newest_write_lsb_count, ssd->newest_write_msb_count);
	//fprintf(ssd->statisticfile,"\n\n");
	fflush(ssd->statisticfile);
}


/***********************************************************************************
*根据每一页的状态计算出每一需要处理的子页的数目，也就是一个子请求需要处理的子页的页数
************************************************************************************/
unsigned int size(unsigned int stored)
{
	unsigned int i,total=0,mask=0x80000000;

	#ifdef DEBUG
	printf("enter size\n");
	#endif
	for(i=1;i<=32;i++)
	{
		if(stored & mask) total++;
		stored<<=1;
	}
	#ifdef DEBUG
	    printf("leave size\n");
    #endif
    return total;
}


/*********************************************************
*transfer_size()函数的作用就是计算出子请求的需要处理的size
*函数中单独处理了first_lpn，last_lpn这两个特别情况，因为这
*两种情况下很有可能不是处理一整页而是处理一页的一部分，因
*为lsn有可能不是一页的第一个子页。
*********************************************************/
unsigned int transfer_size(struct ssd_info *ssd,int need_distribute,unsigned int lpn,struct request *req)
{
	unsigned int first_lpn,last_lpn,state,trans_size;
	unsigned int mask=0,offset1=0,offset2=0;

	first_lpn=req->lsn/ssd->parameter->subpage_page;
	last_lpn=(req->lsn+req->size-1)/ssd->parameter->subpage_page;

	mask=~(0xffffffff<<(ssd->parameter->subpage_page));
	state=mask;
	if(lpn==first_lpn)
	{
		offset1=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-req->lsn);
		state=state&(0xffffffff<<offset1);
	}
	if(lpn==last_lpn)
	{
		offset2=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-(req->lsn+req->size));
		state=state&(~(0xffffffff<<offset2));
	}

	trans_size=size(state&need_distribute);

	return trans_size;
}


/**********************************************************************************************************
*int64_t find_nearest_event(struct ssd_info *ssd)
*寻找所有子请求的最早到达的下个状态时间,首先看请求的下一个状态时间，如果请求的下个状态时间小于等于当前时间，
*说明请求被阻塞，需要查看channel或者对应die的下一状态时间。Int64是有符号 64 位整数数据类型，值类型表示值介于
*-2^63 ( -9,223,372,036,854,775,808)到2^63-1(+9,223,372,036,854,775,807 )之间的整数。存储空间占 8 字节。
*channel,die是事件向前推进的关键因素，三种情况可以使事件继续向前推进，channel，die分别回到idle状态，die中的
*读数据准备好了
***********************************************************************************************************/
int64_t find_nearest_event(struct ssd_info *ssd)
{
//find_nearest_event()函数的功能了，它的主要任务就是寻找所有独立单元的channel下和每个channel下所有独立单元chip的状态情况，从中确定出当前ssd所有子请求中最早到达的下一状态时间。
	unsigned int i,j;
	//首先函数会先定义三个初始参考变量值time、time1和time2，且全初始化为0x7fffffffffffffff）（无符号_int64类型的最大值）
	int64_t time=MAX_INT64;
	int64_t time1=MAX_INT64;
	int64_t time2=MAX_INT64;
	//随后进入一个for循环针对每一个channel进行一个所有事件的查询过程
	for (i=0;i<ssd->parameter->channel_number;i++)
	{	//若当前也就是第i个channel的下一状态为非空闲状态，则说明此时channel已经在空闲状态，那么就说明下一状态一定是工作状态，但是有两种可能的状态情况
		if (ssd->channel_head[i].next_state==CHANNEL_IDLE)
			if(time1>ssd->channel_head[i].next_state_predict_time)
				if (ssd->channel_head[i].next_state_predict_time>ssd->current_time)
					time1=ssd->channel_head[i].next_state_predict_time;
		for (j=0;j<ssd->parameter->chip_channel[i];j++)
		{
			if ((ssd->channel_head[i].chip_head[j].next_state==CHIP_IDLE)||(ssd->channel_head[i].chip_head[j].next_state==CHIP_DATA_TRANSFER))
				if(time2>ssd->channel_head[i].chip_head[j].next_state_predict_time)
					if (ssd->channel_head[i].chip_head[j].next_state_predict_time>ssd->current_time)
						time2=ssd->channel_head[i].chip_head[j].next_state_predict_time;
		}
	}

	/*****************************************************************************************************
	 *time为所有 A.下一状态为CHANNEL_IDLE且下一状态预计时间大于ssd当前时间的CHANNEL的下一状态预计时间
	 *           B.下一状态为CHIP_IDLE且下一状态预计时间大于ssd当前时间的DIE的下一状态预计时间
	 *		     C.下一状态为CHIP_DATA_TRANSFER且下一状态预计时间大于ssd当前时间的DIE的下一状态预计时间
	 *CHIP_DATA_TRANSFER读准备好状态，数据已从介质传到了register，下一状态是从register传往buffer中的最小值
	 *注意可能都没有满足要求的time，这时time返回0x7fffffffffffffff 。
	*****************************************************************************************************/
	time=(time1>time2)?time2:time1;
	return time;
}

/***********************************************
*free_all_node()函数的作用就是释放所有申请的节点
************************************************/
void free_all_node(struct ssd_info *ssd)
{
	unsigned int i,j,k,l,n;
	struct buffer_group *pt=NULL;
	struct direct_erase * erase_node=NULL;
	for (i=0;i<ssd->parameter->channel_number;i++)
	{
		for (j=0;j<ssd->parameter->chip_channel[0];j++)
		{
			for (k=0;k<ssd->parameter->die_chip;k++)
			{
				for (l=0;l<ssd->parameter->plane_die;l++)
				{
					for (n=0;n<ssd->parameter->block_plane;n++)
					{
						free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[n].page_head);
						ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[n].page_head=NULL;
					}
					free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head);
					ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head=NULL;
					while(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node!=NULL)
					{
						erase_node=ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node;
						ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node=erase_node->next_node;
						free(erase_node);
						erase_node=NULL;
					}
				}

				free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head);
				ssd->channel_head[i].chip_head[j].die_head[k].plane_head=NULL;
			}
			free(ssd->channel_head[i].chip_head[j].die_head);
			ssd->channel_head[i].chip_head[j].die_head=NULL;
		}
		free(ssd->channel_head[i].chip_head);
		ssd->channel_head[i].chip_head=NULL;
	}
	free(ssd->channel_head);
	ssd->channel_head=NULL;

	avlTreeDestroy( ssd->dram->buffer);
	ssd->dram->buffer=NULL;

	free(ssd->dram->map->map_entry);
	ssd->dram->map->map_entry=NULL;
	free(ssd->dram->map);
	ssd->dram->map=NULL;
	free(ssd->dram);
	ssd->dram=NULL;
	free(ssd->parameter);
	ssd->parameter=NULL;

	free(ssd);
	ssd=NULL;
}


/*****************************************************************************
*make_aged()函数的作用就是模拟真实的用过一段时间的ssd，
*那么这个ssd的相应的参数就要改变，所以这个函数实质上就是对ssd中各个参数的赋值。
******************************************************************************/
struct ssd_info *make_aged(struct ssd_info *ssd)
{
	unsigned int i,j,k,l,m,n,ppn;
	int threshould,flag=0;

	if (ssd->parameter->aged==1)//aged=1表示要将这个ssd变成aged
	{
		//threshold表示一个plane中有多少页需要提前置为失效
		threshould=(int)(ssd->parameter->block_plane*ssd->parameter->page_block*ssd->parameter->aged_ratio);//plane里的总页数*aged的占比=aged的页数
		//用多层for循环对整个ssd的有必要的页置为失效
		for (i=0;i<ssd->parameter->channel_number;i++)
			for (j=0;j<ssd->parameter->chip_channel[i];j++)
				for (k=0;k<ssd->parameter->die_chip;k++)
					for (l=0;l<ssd->parameter->plane_die;l++)//到这一层，对每个plane里的需要置为失效的页置为失效
					{
						flag=0;//（宋：记录已经旧化了多少个页）
						for (m=0;m<ssd->parameter->block_plane;m++)//这其实是执行将plane里所有有需要的页置为无效（方式是一个页一个页的执行下去，需要进一步for循环细分）
						{
							if (flag>=threshould)//若该plane里没有要置为失效的页，则退出循环。旧化下一个plane
							{
								break;
							}
							for (n=0;n<(ssd->parameter->page_block*ssd->parameter->aged_ratio+1);n++)//若该plane里没有要置为失效的页，则一个页一个页的执行下去
							{
								ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].page_head[n].valid_state=0;        //表示某一页失效，同时标记valid和free状态都为0
								ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].page_head[n].free_state=0;         //表示某一页失效，同时标记valid和free状态都为0
								ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].page_head[n].lpn=0;  //把valid_state free_state lpn都置为0表示页失效，检测的时候三项都检测，单独lpn=0可以是有效页
								ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].free_page_num--;
								ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].invalid_page_num++;
								ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].last_write_page++;
								ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].free_page--;
								flag++;
								if(n%3==0){
									ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].last_write_lsb=n;
									ssd->free_lsb_count--;
									ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].free_lsb_num--;
									//ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;
									ssd->write_lsb_count++;
									ssd->newest_write_lsb_count++;
									ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].free_lsb_num--;
									}
								else if(n%3==2){
									ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].last_write_msb=n;
									ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].free_msb_num--;
									//ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;
									ssd->write_msb_count++;
									ssd->free_msb_count--;
									ssd->newest_write_msb_count++;
									}
								else{//（n%3==1）
									ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].last_write_csb=n;
									ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].free_csb_num--;
									ssd->write_csb_count++;
									ssd->free_csb_count--;
									ssd->newest_write_csb_count++;
									}
								ppn=find_ppn(ssd,i,j,k,l,m,n);//根据失效页的物理地址返回失效页的物理页号（为什么要这一步骤）

							}
						}
					}
	}
	else
	{
		return ssd;
	}

	return ssd;
}

int get_old_zwh(struct ssd_info *ssd){
	int cn_id, cp_id, di_id, pl_id;
	printf("Enter get_old_zwh.\n");
	for(cn_id=0;cn_id<ssd->parameter->channel_number;cn_id++){
		//printf("channel %d\n", cn_id);
		for(cp_id=0;cp_id<ssd->parameter->chip_channel[0];cp_id++){
			//printf("chip %d\n", cp_id);
			for(di_id=0;di_id<ssd->parameter->die_chip;di_id++){
				//printf("die %d\n", di_id);
				for(pl_id=0;pl_id<ssd->parameter->plane_die;pl_id++){
					//printf("channel %d, chip %d, die %d, plane %d: ", cn_id, cp_id, di_id, pl_id);
					int active_block, ppn, lpn;
					struct local *location;
					lpn = 0;
					while(ssd->channel_head[cn_id].chip_head[cp_id].die_head[di_id].plane_head[pl_id].free_page > (ssd->parameter->page_block*ssd->parameter->block_plane)*0.3){
						//if(cn_id==0&&cp_id==2&&di_id==0&&pl_id==0){
						//	printf("cummulating....\n");
						//	}
						if(find_active_block(ssd,cn_id,cp_id,di_id,pl_id)==FAILURE)
							{
								printf("Wrong in get_old_zwh, find_active_block\n");
								return 0;
							}
						active_block=ssd->channel_head[cn_id].chip_head[cp_id].die_head[di_id].plane_head[pl_id].active_block;
						if(write_page(ssd,cn_id,cp_id,di_id,pl_id,active_block,&ppn)==ERROR)
							{
								return 0;
							}
						location=find_location(ssd,ppn);
						ssd->program_count++;
						ssd->channel_head[cn_id].program_count++;
						ssd->channel_head[cn_id].chip_head[cp_id].program_count++;
						ssd->channel_head[cn_id].chip_head[cp_id].die_head[di_id].plane_head[pl_id].blk_head[active_block].page_head[location->page].lpn=0;
						ssd->channel_head[cn_id].chip_head[cp_id].die_head[di_id].plane_head[pl_id].blk_head[active_block].page_head[location->page].valid_state=0;
						ssd->channel_head[cn_id].chip_head[cp_id].die_head[di_id].plane_head[pl_id].blk_head[active_block].page_head[location->page].free_state=0;
						ssd->channel_head[cn_id].chip_head[cp_id].die_head[di_id].plane_head[pl_id].blk_head[active_block].invalid_page_num++;
						free(location);
						location=NULL;
						}
					//printf("%d\n", ssd->channel_head[cn_id].chip_head[cp_id].die_head[di_id].plane_head[pl_id].free_page);
					}
				}
			}
		}
	printf("Exit get_old_zwh.\n");
}

/*********************************************************************************************
*no_buffer_distribute()函数是处理当ssd没有dram的时候，
*这时读写请求就不必再需要在buffer里面寻找，直接利用creat_sub_request()函数创建子请求，再处理。
*********************************************************************************************/
struct ssd_info *no_buffer_distribute(struct ssd_info *ssd)
{
	// printf("no_buffer_distribute");
	unsigned int lsn,lpn,last_lpn,first_lpn,complete_flag=0, state;
	unsigned int flag=0,flag1=1,active_region_flag=0;           //to indicate the lsn is hitted or not
	struct request *req=NULL;
	struct sub_request *sub=NULL,*sub_r=NULL,*update=NULL;
	struct local *loc=NULL;
	struct channel_info *p_ch=NULL;
	int i;

	unsigned int mask=0;
	unsigned int offset1=0, offset2=0;
	unsigned int sub_size=0;
	unsigned int sub_state=0;

	ssd->dram->current_time=ssd->current_time;
	req=ssd->request_tail;
	lsn=req->lsn;//lsn是起始地址
	lpn=req->lsn/ssd->parameter->subpage_page;
	last_lpn=(req->lsn+req->size-1)/ssd->parameter->subpage_page;
	first_lpn=req->lsn/ssd->parameter->subpage_page;
	// // 统计读HDD的lpn，主要是把sequential lpn当成顺序写。统计到数组里面，后面查找对应时间，加到
	// int temp_lpn = lpn;
	// int seq_count = 0;
	// int len = 512;
    // int arr[len][2];
	// int arr_index = -1;
	// if (req->operation == READ)
	// {
	// 	// 如果只有一个
	// 	if (temp_lpn == last_lpn && ssd->dram->map->map_entry[lpn].hdd_flag == 1)
	// 	{
	// 		FILE *fp;
	// 		char *ret = strrchr(ssd->tracefilename, '/') + 1;
	// 		fp = fopen(ret, "w");
	// 		printf("Read-HDD-rand:%lld %d %d %d %d\n", ssd->current_time, 0, temp_lpn, 1, 1);
	// 		fprintf(fp, "%lld %d %d %d %d\n", ssd->current_time, 0, temp_lpn, 1, 1);
	// 		fflush(fp);
	// 		fclose(fp);
	// 		char *avg = exec_disksim_syssim(ret);
	// 		arr_index++;
	// 		arr[arr_index][0] = temp_lpn;
	// 		arr[arr_index][1] = (int)avg;
	// 	}
	// 	else
	// 	{
	// 		while (temp_lpn <= last_lpn)
	// 		{
	// 			if (ssd->dram->map->map_entry[temp_lpn].hdd_flag == 1)
	// 			{
	// 				seq_count++;
	// 			} else
	// 			{
	// 				if (seq_count != 0)
	// 				{
	// 					FILE *fp;
	// 					char *ret = strrchr(ssd->tracefilename, '/') + 1;
	// 					fp = fopen(ret, "w");
	// 					printf("Read-HDD-seq:%lld %d %d %d %d\n", ssd->current_time, 0, temp_lpn - seq_count, seq_count, 1);
	// 					fprintf(fp, "%lld %d %d %d %d\n", ssd->current_time, 0, temp_lpn - seq_count, seq_count, 1);
	// 					fflush(fp);
	// 					fclose(fp);
	// 					char *avg = exec_disksim_syssim(ret);
	// 					int avg_lpn = (int)avg / seq_count;
	// 					int temp_last_lpn = temp_lpn - seq_count;
	// 					// printf("seq-avg0:%d seq-avg_lpn0:%d  seq_count0:%d\n", (int)avg, avg_lpn, seq_count);
	// 					while (seq_count != 0)
	// 					{
	// 						arr_index++;
	// 						arr[arr_index][0] = temp_last_lpn;
	// 						arr[arr_index][1] = avg_lpn;
	// 						seq_count--;
	// 						temp_last_lpn++;
	// 					}
	// 				}
	// 			}
	// 			temp_lpn++;
	// 		}
	// 		// 全部读HDD
	// 		if (seq_count != 0)
	// 		{
	// 			FILE *fp;
	// 			char *ret = strrchr(ssd->tracefilename, '/') + 1;
	// 			fp = fopen(ret, "w");
	// 			printf("Read-HDD-seq:%lld %d %d %d %d\n", ssd->current_time, 0, temp_lpn - seq_count, seq_count, 1);
	// 			fprintf(fp, "%lld %d %d %d %d\n", ssd->current_time, 0, temp_lpn - seq_count, seq_count, 1);
	// 			fflush(fp);
	// 			fclose(fp);
	// 			char *avg = exec_disksim_syssim(ret);
	// 			int avg_lpn = (int)avg / seq_count;
	// 			// printf("seq-avg:%d seq-avg_lpn:%d  seq_count:%d\n", (int)avg, avg_lpn, seq_count);
	// 			int temp_last_lpn = temp_lpn - seq_count;
	// 			while (seq_count != 0)
	// 			{
	// 				arr_index++;
	// 				arr[arr_index][0] = temp_last_lpn;
	// 				arr[arr_index][1] = avg_lpn;
	// 				seq_count--;
	// 				temp_last_lpn++;
	// 			}
	// 		}
	// 	}
	// }

	if(req->operation==READ)
	{
		while (lpn <= last_lpn)
		{
			sub_state = (ssd->dram->map->map_entry[lpn].state & 0x7fffffff);
			sub_size = size(sub_state);
			// int read_hdd_time = 0;
			//读HDD后，将HDD写回SSD(直接创建对应写请求即可)
			if (ssd->dram->map->map_entry[lpn].hdd_flag == 1)
			{
				// int i;
				// for (i = 0; i <= arr_index; i++)
				// {
				// 	if (arr[i][0] == lpn)
				// 	{
				// 		read_hdd_time = arr[i][1];
				// 	}
				// }
				// if (read_hdd_time == 0)
				// {
				// 	printf("read_hdd_time == 0\n");
				// 	abort();
				// } 
				// 其实这步做不做应该对结果没有影响，后面写子请求覆盖了
				// sub = creat_sub_request_read_hdd(ssd, lpn, sub_size, sub_state, req, req->operation, 0, read_hdd_time);
				// writeback
				int target_page_type;
				int random_num;
				random_num = rand() % 100;
				if (random_num < ssd->parameter->turbo_mode_factor)
				{
					target_page_type = TARGET_LSB;
				}
				else if (random_num < ssd->parameter->turbo_mode_factor_2)
				{
					target_page_type = TARGET_CSB;
				}
				else
				{
					target_page_type = TARGET_MSB;
				}
				// 只有一个lpn
				if (ssd->parameter->subpage_page == 32)
				{
					mask = 0xffffffff;
				}
				else
				{
					mask = ~(0xffffffff << (ssd->parameter->subpage_page));
				}
				state = mask;
				if (lpn == first_lpn)
				{
					offset1 = ssd->parameter->subpage_page - ((lpn + 1) * ssd->parameter->subpage_page - req->lsn);
					state = state & (0xffffffff << offset1);
				}
				if (lpn == last_lpn)
				{
					offset2 = ssd->parameter->subpage_page - ((lpn + 1) * ssd->parameter->subpage_page - (req->lsn + req->size));
					if (offset2 != 32)
					{
						state = state & (~(0xffffffff << offset2));
					}
				}
				sub_size = size(state);
				sub = creat_sub_request_pro(ssd, lpn, sub_size, state, req, WRITE, target_page_type, 1);
			} else
			{
				sub = creat_sub_request(ssd, lpn, sub_size, sub_state, req, req->operation, 0);
			}
			
			lpn++;
		}
	}
	else if(req->operation==WRITE)
	{
		int target_page_type;
		int random_num;
		random_num = rand()%100;
		if(random_num<ssd->parameter->turbo_mode_factor){
			target_page_type = TARGET_LSB;
			}
		else if(random_num<ssd->parameter->turbo_mode_factor_2){
			target_page_type = TARGET_CSB;
			}
		else{
			target_page_type = TARGET_MSB;
			}
		int seq_num = last_lpn - lpn + 1; // number of sequential write to HDD
		int threshold_size = 128; // threshold size to HDD
		//sequential write to HDD 
		if (req->size >= threshold_size && ssd->is_related_work == 1)
		{
			int write_hdd_time = 0;
			FILE *fp;
			char *ret = strrchr(ssd->tracefilename, '/') + 1;
			fp = fopen(ret, "w");
			// 改成一个page-8KB(req->size / 2 / 8)大小
			// printf("ssdup: %lld %d %d %d %d\n", ssd->current_time, 0, lpn, seq_num, 0);
			// disksim: failed: in disk_buffer_sector_done() (disksim_diskctlr.c:4358)
			int exec_num = 1;
			if (seq_num > 64)
			{
				fprintf(fp, "%lld %d %d %d %d\n", ssd->current_time, 0, lpn, 64, 0);
				fprintf(fp, "%lld %d %d %d %d\n", ssd->current_time, 0, lpn, seq_num - 64, 0);
				exec_num = 2;
			}else
			{
				fprintf(fp, "%lld %d %d %d %d\n", ssd->current_time, 0, lpn, seq_num, 0);
			}
			fflush(fp);
			fclose(fp); 
			char *avg = exec_disksim_syssim(ret);
			write_hdd_time = (int)avg * exec_num;
			// if (ssd->HDDTime < ssd->current_time)
			// {
			// 	ssd->HDDTime = ssd->current_time;
			// }
			// write_hdd_time += (ssd->HDDTime - ssd->current_time);
			// ssd->HDDTime += (write_hdd_time - (ssd->HDDTime - ssd->current_time));
			req->response_time = req->time + write_hdd_time;
		}
		while(lpn<=last_lpn)
		{
			if(ssd->parameter->subpage_page == 32){
				mask = 0xffffffff;
				}
			else{
				mask=~(0xffffffff<<(ssd->parameter->subpage_page));
				}
			state=mask;
			//printf("initial state: %x\n", state);
			if(lpn==first_lpn)
			{
				offset1=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-req->lsn);
				//printf("offset1: %d, ", offset1);
				state=state&(0xffffffff<<offset1);
				//printf("state: %x\n", state);
			}
			if(lpn==last_lpn)
			{
				offset2=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-(req->lsn+req->size));
				//printf("offset2: %d, ", offset2);
				if(offset2 != 32){
					state=state&(~(0xffffffff<<offset2));
					}
				//printf("state: %x\n", state);
			}
			//printf("state: %x, ", state);
			sub_size=size(state);
			//printf("sub_size: %d\n", sub_size);
			//  如果写请求大于32KB则认为是顺序写入HDD
			if (req->size >= threshold_size && ssd->is_related_work == 1)
			{
				ssd->dram->map->map_entry[lpn].hdd_flag = 1;
				// ssd->dram->map->map_entry[lpn].state = 0; 
				// ssd->dram->map->map_entry[lpn].pn = 0;
				// sub=creat_sub_request_pro(ssd,lpn,sub_size,state,req,req->operation,target_page_type, 2);
				// printf("Directly write to HDD. %d\n", req->size);
			} else
			{
				sub=creat_sub_request(ssd,lpn,sub_size,state,req,req->operation,target_page_type);
				ssd->trace_write_count++;
			}
			lpn++;
		}
	}

	return ssd;
}


