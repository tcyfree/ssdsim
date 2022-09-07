/*****************************************************************************************************************************
This project was supported by the National Basic Research 973 Program of China under Grant No.2011CB302301
Huazhong University of Science and Technology (HUST)   Wuhan National Laboratory for Optoelectronics

FileName： pagemap.h
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

#define _CRTDBG_MAP_ALLOC

#include "pagemap.h"
#include "flash.h"
#include "ssd.h"


/************************************************
*断言,当打开文件失败时，输出“open 文件名 error”
*************************************************/
void file_assert(int error,char *s)
{
	if(error == 0) return;
	printf("open %s error\n",s);
	getchar();
	exit(-1);
}

/*****************************************************
*断言,当申请内存空间失败时，输出“malloc 变量名 error”
******************************************************/
void alloc_assert(void *p,char *s)//断言
{
	if(p!=NULL) return;
	printf("malloc %s error\n",s);
	getchar();
	exit(-1);
}

/*********************************************************************************
*断言
*A，读到的time_t，device，lsn，size，ope都<0时，输出“trace error:.....”
*B，读到的time_t，device，lsn，size，ope都=0时，输出“probable read a blank line”
**********************************************************************************/
void trace_assert(int64_t time_t,int device,unsigned int lsn,int size,int ope)//断言
{
	if(time_t <0 || device < 0 || lsn < 0 || size < 0 || ope < 0)
	{
		printf("trace error:%lld %d %d %d %d\n",time_t,device,lsn,size,ope);
		getchar();
		exit(-1);
	}
	if(time_t == 0 && device == 0 && lsn == 0 && size == 0 && ope == 0)
	{
		printf("probable read a blank line\n");
		getchar();
	}
}


/************************************************************************************
*函数的功能是根据物理页号ppn查找该物理页所在的channel，chip，die，plane，block，page
*得到的channel，chip，die，plane，block，page放在结构location中并作为返回值
*************************************************************************************/
struct local *find_location(struct ssd_info *ssd,unsigned int ppn)
{
	struct local *location=NULL;
	unsigned int i=0;
	int pn,ppn_value=ppn;
	int page_plane=0,page_die=0,page_chip=0,page_channel=0;

	pn = ppn;

#ifdef DEBUG
	printf("enter find_location\n");
#endif

	location=(struct local *)malloc(sizeof(struct local));
    alloc_assert(location,"location");
	memset(location,0, sizeof(struct local));

	page_plane=ssd->parameter->page_block*ssd->parameter->block_plane; //number of page per plane
	page_die=page_plane*ssd->parameter->plane_die;                     //number of page per die
	page_chip=page_die*ssd->parameter->die_chip;                       //number of page per chip
	page_channel=page_chip*ssd->parameter->chip_channel[0];            //number of page per channel

	/*******************************************************************************
	*page_channel是一个channel中page的数目， ppn/page_channel就得到了在哪个channel中
	*用同样的办法可以得到chip，die，plane，block，page
	********************************************************************************/
	location->channel = ppn/page_channel;
	location->chip = (ppn%page_channel)/page_chip;
	location->die = ((ppn%page_channel)%page_chip)/page_die;
	location->plane = (((ppn%page_channel)%page_chip)%page_die)/page_plane;
	location->block = ((((ppn%page_channel)%page_chip)%page_die)%page_plane)/ssd->parameter->page_block;
	location->page = (((((ppn%page_channel)%page_chip)%page_die)%page_plane)%ssd->parameter->page_block)%ssd->parameter->page_block;

	return location;
}


/*****************************************************************************
*这个函数的功能是根据参数channel，chip，die，plane，block，page，找到该物理页号
*函数的返回值就是这个物理页号（与上面的函数是相反的操作）
******************************************************************************/
unsigned int find_ppn(struct ssd_info * ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane,unsigned int block,unsigned int page)
{
	unsigned int ppn=0;
	unsigned int i=0;
	int page_plane=0,page_die=0,page_chip=0;
	int page_channel[100];                  /*这个数组存放的是每个channel的page数目*/

#ifdef DEBUG
	printf("enter find_psn,channel:%d, chip:%d, die:%d, plane:%d, block:%d, page:%d\n",channel,chip,die,plane,block,page);
#endif

	/*********************************************
	*计算出plane，die，chip，channel中的page的数目
	**********************************************/
	page_plane=ssd->parameter->page_block*ssd->parameter->block_plane;
	page_die=page_plane*ssd->parameter->plane_die;
	page_chip=page_die*ssd->parameter->die_chip;
	while(i<ssd->parameter->channel_number)
	{
		page_channel[i]=ssd->parameter->chip_channel[i]*page_chip;
		i++;
	}

    /****************************************************************************
	*计算物理页号ppn，ppn是channel，chip，die，plane，block，page中page个数的总和
	*****************************************************************************/
	i=0;
	while(i<channel)
	{
		ppn=ppn+page_channel[i];
		i++;
	}
	ppn=ppn+page_chip*chip+page_die*die+page_plane*plane+block*ssd->parameter->page_block+page;

	return ppn;
}

/********************************
*函数功能是获得一个读子请求的状态
*********************************/
int set_entry_state(struct ssd_info *ssd,unsigned int lsn,unsigned int size)
{
	int temp,state,move;
	if(size == 32){
		temp = 0xffffffff;
		}
	else{
		temp=~(0xffffffff<<size);
		}
	move=lsn%ssd->parameter->subpage_page;
	state=temp<<move;

	return state;
}

/**************************************************
*读请求预处理函数，当读请求所读得页里面没有数据时，
*需要预处理往该页里面写数据，以保证能读到数据
***************************************************/
struct ssd_info *pre_process_page(struct ssd_info *ssd)
{
	int fl=0;
	unsigned int device,lsn,size,ope,lpn,full_page;//IO操作的相关参数，分别是目标设备号，逻辑扇区号，操作长度，操作类型，逻辑页号
	unsigned int largest_lsn,sub_size,ppn,add_size=0;//最大的逻辑扇区号，子页操作长度，物理页号，该IO已处理完毕的长度
	unsigned int i=0,j,k;
	int map_entry_new,map_entry_old,modify;
	int flag=0;
	char buffer_request[200];//请求队列缓冲区
	struct local *location;
	int64_t time;

	printf("\n");
	printf("begin pre_process_page.................\n");

	ssd->tracefile=fopen(ssd->tracefilename,"r");//以只读方式打开trace文件，从中获取I/O请求
	if(ssd->tracefile == NULL )      /*打开trace文件从中读取请求*/
	{
		printf("the trace file can't open\n");
		return NULL;
	}
	if(ssd->parameter->subpage_page == 32){
		full_page = 0xffffffff;
		}
	else{
		full_page=~(0xffffffff<<(ssd->parameter->subpage_page));
		}
	//full_page=~(0xffffffff<<(ssd->parameter->subpage_page));
	/*计算出这个ssd的最大逻辑扇区号*/
	largest_lsn=(unsigned int )((ssd->parameter->chip_num*ssd->parameter->die_chip*ssd->parameter->plane_die*ssd->parameter->block_plane*ssd->parameter->page_block*ssd->parameter->subpage_page)*(1-ssd->parameter->overprovide));

	while(fgets(buffer_request,200,ssd->tracefile))
	//逐行从tracefile文件中每次读取199字符，直到读完整个trace为止
	{
		sscanf(buffer_request,"%lld %d %d %d %d",&time,&device,&lsn,&size,&ope);
		fl++;   //已读的I/O数量计数
		trace_assert(time,device,lsn,size,ope);       /*断言，当读到的time，device，lsn，size，ope不合法时就会处理*/
		ssd->total_request_num++;
		add_size=0;        /*add_size是这个请求已经预处理的大小*/

//若ope为0则说明是写请求，但是由于这个pre_process_page只是服务于读请求，所以程序流会忽略写请求直接继续调用fget()读取下一个IOtrace进行读请求的操作。
//而当ope为1时说明IO为读请求，程序会判断已完成的IO长度add_size是否小于IO长度size，若大于或等于size说明读IO已经处理完毕继续下一IO的读取；否则说明该IO读尚未处理完毕，要继续下面的步骤：首先程序会调整lsn（防止lsn比最大的lsn还大），然后计算sub_size即lsn所在的page中实际IO需要操作的长度:这部分长度 = page子页数量 - lsn在page中的起始子页位置；随后判断已经处理的IO长度add_size和当前page中的实际操作长度sub_size之和是否超过该IOtrace的长度size：若已经超过说明sub_size实际上的长度需要调整(实际上不可能超过IOtrace所规定的size，否则就是错误操作，而这里的sub_size主要是针对首次的lsn和最后一次的lsn，因为其余中间部分的lsn基本上都会是从page页的起始位置开始，操作的长度都会是整个page大小)
		if(ope==1)     /*这里只是读请求的预处理，需要提前将相应位置的信息进行相应修改*/
		{
			while(add_size<size)//若已经预处理的操作长度<操作的长度，则执行下面操作
			{
				lsn=lsn%largest_lsn;                                    /*防止获得的lsn比最大的lsn还大*/
				sub_size=ssd->parameter->subpage_page-(lsn%ssd->parameter->subpage_page);
				//这里的sub_size主要是为了定位好子请求操作位置的，这个位置是相对于某一个特定的page而言的，从这个page的第一个sub_page开始计算到这个特定的操作位置
				if(add_size+sub_size>=size) /*只有当一个请求的大小小于一个page的大小时或者是处理一个请求的最后一个page时会出现这种情况*/
				{
					sub_size=size-add_size;
					add_size+=sub_size;
				}

//如果非法的话则打印sub_size的信息；同时程序会继续统计当前lsn所在的lpn，并判断该lpn在内存dram中的映射表项map_entry[lpn]的相关子页映射是否有效：若map_entry[lpn].state不为0则说明此时该lpn的子页映射可能有效，可能存在有直接可用的子页。
//程序会接着判断state是否>0.若不成立只能说明此时state<0也就是子页映射全部有效(这里是因为map_entry[lpn].state是一个int类型，默认下应该是个有符号的整型取值范围，因此若state<0那么换算成二进制数值便是全1的情况，此时所有子页都是有效置位)
				if((sub_size>ssd->parameter->subpage_page)||(add_size>size))/*当预处理一个子大小时，这个大小大于一个page或是已经处理的大小大于size就报错*/
				{
					printf("pre_process sub_size:%d\n",sub_size);
				}

                /*******************************************************************************************************
				*利用逻辑扇区号lsn计算出逻辑页号lpn
				*判断这个dram中映射表map中在lpn位置的状态
				*A，这个状态==0，表示以前没有写过，现在需要直接将sub_size大小的子页写进去写进去（应该就仅仅打个标记）
				*B，这个状态>0，表示，以前有写过，这需要进一步比较状态，因为新写的状态可以与以前的状态有重叠的扇区的地方
				********************************************************************************************************/
				lpn=lsn/ssd->parameter->subpage_page;  //所在逻辑页号=逻辑扇区号（逻辑子页号）/一个页里子页数
//若map_entry[lpn].state为0则说明此时该lpn对应的子页映射无效，需要程序重新分配出有效状态的物理子页给读操作请求，因此需要从SSD中先将数据写入该子页中然后以供读请求操作。函数会调用get_ppn_for_pre_process()函数取得当前IOtrace的lsn对应的物理页号ppn，随后根据ppn调用find_location()函数取得对应的物理地址信息.接着统计更新ssd、当前所在的channel和channel下所在的chip中的program_count计数，表示已经成功从SSD中写入数据并且读到了dram中，此时需要更新lpn对应的map_entry[lpn].pn为新获取到的ppn。跟着程序会调用set_entry_state()函数重新设置更新子页的状态位并且将其赋值给map_entry[lpn].state。以及根据location物理地址更新lsn对应的物理page的相关参数，主要是lpn、valid_state有效状态位和free_state空闲状态位标志，最后释放并置空location。
				if(ssd->dram->map->map_entry[lpn].state==0)                 /*状态为0的情况*/
				{
					/**************************************************************
					*获得利用get_ppn_for_pre_process函数获得ppn，再得到location
					*修改ssd的相关参数，dram的映射表map，以及location下的page的状态
					***************************************************************/
					ppn=get_ppn_for_pre_process(ssd,lsn);
					location=find_location(ssd,ppn);
					ssd->program_count++;
					ssd->channel_head[location->channel].program_count++;
					ssd->channel_head[location->channel].chip_head[location->chip].program_count++;	//至此，表示已经成功的写入数据（读请求预处理）
					ssd->dram->map->map_entry[lpn].pn=ppn;
					ssd->dram->map->map_entry[lpn].state=set_entry_state(ssd,lsn,sub_size);   //0001（获取的子请求状态赋值给逻辑页）
					ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn=lpn;
					ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state=ssd->dram->map->map_entry[lpn].state;
					ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state=((~ssd->dram->map->map_entry[lpn].state)&full_page);

					free(location);
					location=NULL;
				}//if(ssd->dram->map->map_entry[lpn].state==0)
//如果state>0则说明可能其中只是有部分子页有效，而有可能需要读取数据的子页并未存在dram和buffer中，所以需要将从SSD中写入到dram中的子页进行相应的操作，但是必须要确认到哪些子页需要进行写入。因此程序进行进一步的确认，首先函数会调用set_entry_state()进行子页映射状态位的设置，设置后的结果保存在map_entry_new中set_entry_state()函数会根据lsn和sub_size的参数对从lsn位置开始的子页长度为sub_size的部分标记状态位置1，代表了需要重新在map_entry[lpn].state中进行设置的子页有效映射。接着函数会用map_entry_old保存当前dram中map_entry[lpn].state的位映射状态信息，将map_entry_new和map_entry_old进行一个按位或计算操作从而更新映射状态位，并将结果保存在modify中。随后将lpn的映射物理页pn保存至ppn中；完成映射更新后就代表了程序已经完成了该lsn在当前lpn下的读操作。随后程序会调用find_location()函数进行物理地址的查找，紧接着会统计相关的ssd信息：更新整个ssd、当前channel和该channel下当前所在的chip中的program_count计数，表示完成一次写page操作计数；然后将modify中保存的映射更新信息赋值给当前lsn所在的lpn对应的map_entry[lpn].state，表示已经完成了将数据写入到SSD相对应的page子页中且已经读取到了dram中；同时根据location物理地址结构体的信息设置page中的子页有效映射位valid_state也为modify以保持与dram中的同步；且同时设置更新该page中剩余free状态的子页，接着释放并置空location。
				else if(ssd->dram->map->map_entry[lpn].state>0)           /*状态不为0的情况*/
				{
					map_entry_new=set_entry_state(ssd,lsn,sub_size);      /*得到新的状态，并与原来的状态相或的到一个状态*/
					map_entry_old=ssd->dram->map->map_entry[lpn].state;
                    modify=map_entry_new|map_entry_old;
					ppn=ssd->dram->map->map_entry[lpn].pn;
					location=find_location(ssd,ppn);

					ssd->program_count++;
					ssd->channel_head[location->channel].program_count++;
					ssd->channel_head[location->channel].chip_head[location->chip].program_count++;
					ssd->dram->map->map_entry[lsn/ssd->parameter->subpage_page].state=modify;
					ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state=modify;
					ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state=((~modify)&full_page);

					free(location);
					location=NULL;
				}//else if(ssd->dram->map->map_entry[lpn].state>0)
//当函数完成上述过程后，证明已经成功完成了当前IOtrace的lsn所对应的lpn读操作，这时候只是读完成了至多一个page的大小，因此程序此时需要更新lsn的位置即令lsn加上已经读取完毕的sub_size大小，且同时更新已经处理的IO长度add_size。接着程序流会继续判断当前是否已经完成了该IOtrace的处理，如果未完成则重复以上的过程。当完成了该IOtrace的读请求处理后，程序会回到fgets()函数中继续读取tracefile中的下一个IO，周始反复一直到读取完所有的IOtrace。
				lsn=lsn+sub_size;                                         /*下个子请求的起始位置*/
				add_size+=sub_size;                                       /*已经处理了的add_size大小变化*/
			}//while(add_size<size)
		}//if(ope==1)
	}

//当程序处理完tracefile中所有的读请求后，便开始打印处理完成信息，并且调用fclose()关闭tracefile文件流。

	printf("\n");
	printf("pre_process is complete!\n");

	fclose(ssd->tracefile);
//随即用一个多重嵌套for循环将SSD当前状态下每一个plane的free空闲状态页的数量都按照固定格式写入到ssd->outputfile中。每一次针对每个plane都会用fflush()函数立即将缓冲区中的数据流更新写入到outputfile文件中。当完成上述所有操作后，pre_process_page便完成了其任务，返回ssd结构体。
	for(i=0;i<ssd->parameter->channel_number;i++)
    for(j=0;j<ssd->parameter->die_chip;j++)
	for(k=0;k<ssd->parameter->plane_die;k++)
	{
		fprintf(ssd->outputfile,"chip:%d,die:%d,plane:%d have free page: %d\n",i,j,k,ssd->channel_head[i].chip_head[0].die_head[j].plane_head[k].free_page);
		fflush(ssd->outputfile);
	}

	return ssd;
}
/**************************************************
*预处理数据到SSD中：1 预读 2 更新写
***************************************************/
struct ssd_info *pre_process_write_read(struct ssd_info *ssd)
{
	int fl=0;
	unsigned int device,lsn,size,ope,lpn,full_page;//IO操作的相关参数，分别是目标设备号，逻辑扇区号，操作长度，操作类型，逻辑页号
	unsigned int largest_lsn,sub_size,ppn,add_size=0;//最大的逻辑扇区号，子页操作长度，物理页号，该IO已处理完毕的长度`
	unsigned int i=0,j,k;
	int map_entry_new,map_entry_old,modify;
	int flag=0;
	char buffer_request[200];//请求队列缓冲区
	struct local *location;
	int64_t time;

	printf("\n");
	printf("begin pre_process_write_read.................\n");
	ssd->tracefile2=fopen("../trace/1618_1.csv","r");//以只读方式打开trace文件，从中获取I/O请求
	if(ssd->tracefile2 == NULL )      /*打开trace文件从中读取请求*/
	{
		printf("the trace file2 can't open\n");
		return NULL;
	}
	if(ssd->parameter->subpage_page == 32){
		full_page = 0xffffffff;
		}
	else{
		full_page=~(0xffffffff<<(ssd->parameter->subpage_page));
		}
	//full_page=~(0xffffffff<<(ssd->parameter->subpage_page));
	/*计算出这个ssd的最大逻辑扇区号*/
	largest_lsn=(unsigned int )((ssd->parameter->chip_num*ssd->parameter->die_chip*ssd->parameter->plane_die*ssd->parameter->block_plane*ssd->parameter->page_block*ssd->parameter->subpage_page)*(1-ssd->parameter->overprovide));

	while(fgets(buffer_request,200,ssd->tracefile2))
	//逐行从tracefile文件中每次读取199字符，直到读完整个trace为止
	{
		sscanf(buffer_request,"%lld %d %d %d %d",&time,&device,&lsn,&size,&ope);
		if(buffer_request[0]=='\n')
			break;
		fl++;   //已读的I/O数量计数
		trace_assert(time,device,lsn,size,ope);       /*断言，当读到的time，device，lsn，size，ope不合法时就会处理*/
		// ssd->total_request_num++;
		add_size=0;        /*add_size是这个请求已经预处理的大小*/

//若ope为0则说明是写请求，但是由于这个pre_process_page只是服务于读请求，所以程序流会忽略写请求直接继续调用fget()读取下一个IOtrace进行读请求的操作。
//而当ope为1时说明IO为读请求，程序会判断已完成的IO长度add_size是否小于IO长度size，若大于或等于size说明读IO已经处理完毕继续下一IO的读取；否则说明该IO读尚未处理完毕，要继续下面的步骤：首先程序会调整lsn（防止lsn比最大的lsn还大），然后计算sub_size即lsn所在的page中实际IO需要操作的长度:这部分长度 = page子页数量 - lsn在page中的起始子页位置；随后判断已经处理的IO长度add_size和当前page中的实际操作长度sub_size之和是否超过该IOtrace的长度size：若已经超过说明sub_size实际上的长度需要调整(实际上不可能超过IOtrace所规定的size，否则就是错误操作，而这里的sub_size主要是针对首次的lsn和最后一次的lsn，因为其余中间部分的lsn基本上都会是从page页的起始位置开始，操作的长度都会是整个page大小)
// if(ope==1)     /*这里只是读请求的预处理，需要提前将相应位置的信息进行相应修改*/
// 		{
		while(add_size<size)//若已经预处理的操作长度<操作的长度，则执行下面操作
			{
				lsn=lsn%largest_lsn;                                    /*防止获得的lsn比最大的lsn还大*/
				sub_size=ssd->parameter->subpage_page-(lsn%ssd->parameter->subpage_page);
				//这里的sub_size主要是为了定位好子请求操作位置的，这个位置是相对于某一个特定的page而言的，从这个page的第一个sub_page开始计算到这个特定的操作位置
				if(add_size+sub_size>=size) /*只有当一个请求的大小小于一个page的大小时或者是处理一个请求的最后一个page时会出现这种情况*/
				{
					sub_size=size-add_size;
					add_size+=sub_size;
				}

//如果非法的话则打印sub_size的信息；同时程序会继续统计当前lsn所在的lpn，并判断该lpn在内存dram中的映射表项map_entry[lpn]的相关子页映射是否有效：若map_entry[lpn].state不为0则说明此时该lpn的子页映射可能有效，可能存在有直接可用的子页。
//程序会接着判断state是否>0.若不成立只能说明此时state<0也就是子页映射全部有效(这里是因为map_entry[lpn].state是一个int类型，默认下应该是个有符号的整型取值范围，因此若state<0那么换算成二进制数值便是全1的情况，此时所有子页都是有效置位)
				if((sub_size>ssd->parameter->subpage_page)||(add_size>size))/*当预处理一个子大小时，这个大小大于一个page或是已经处理的大小大于size就报错*/
				{
					printf("pre_process sub_size:%d\n",sub_size);
				}

                /*******************************************************************************************************
				*利用逻辑扇区号lsn计算出逻辑页号lpn
				*判断这个dram中映射表map中在lpn位置的状态
				*A，这个状态==0，表示以前没有写过，现在需要直接将sub_size大小的子页写进去写进去（应该就仅仅打个标记）
				*B，这个状态>0，表示，以前有写过，这需要进一步比较状态，因为新写的状态可以与以前的状态有重叠的扇区的地方
				********************************************************************************************************/
				lpn=lsn/ssd->parameter->subpage_page;  //所在逻辑页号=逻辑扇区号（逻辑子页号）/一个页里子页数
//若map_entry[lpn].state为0则说明此时该lpn对应的子页映射无效，需要程序重新分配出有效状态的物理子页给读操作请求，因此需要从SSD中先将数据写入该子页中然后以供读请求操作。函数会调用get_ppn_for_pre_process()函数取得当前IOtrace的lsn对应的物理页号ppn，随后根据ppn调用find_location()函数取得对应的物理地址信息.接着统计更新ssd、当前所在的channel和channel下所在的chip中的program_count计数，表示已经成功从SSD中写入数据并且读到了dram中，此时需要更新lpn对应的map_entry[lpn].pn为新获取到的ppn。跟着程序会调用set_entry_state()函数重新设置更新子页的状态位并且将其赋值给map_entry[lpn].state。以及根据location物理地址更新lsn对应的物理page的相关参数，主要是lpn、valid_state有效状态位和free_state空闲状态位标志，最后释放并置空location。
				if(ssd->dram->map->map_entry[lpn].state==0)                 /*状态为0的情况*/
				{
					/**************************************************************
					*获得利用get_ppn_for_pre_process函数获得ppn，再得到location
					*修改ssd的相关参数，dram的映射表map，以及location下的page的状态
					***************************************************************/
					ppn=get_ppn_for_pre_process(ssd,lsn);
					location=find_location(ssd,ppn);
					ssd->program_count++;
					ssd->channel_head[location->channel].program_count++;
					ssd->channel_head[location->channel].chip_head[location->chip].program_count++;	//至此，表示已经成功的写入数据（读请求预处理）
					ssd->dram->map->map_entry[lpn].pn=ppn;
					ssd->dram->map->map_entry[lpn].state=set_entry_state(ssd,lsn,sub_size);   //0001（获取的子请求状态赋值给逻辑页）
					ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn=lpn;
					ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state=ssd->dram->map->map_entry[lpn].state;
					ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state=((~ssd->dram->map->map_entry[lpn].state)&full_page);

					free(location);
					location=NULL;
				}//if(ssd->dram->map->map_entry[lpn].state==0)
//如果state>0则说明可能其中只是有部分子页有效，而有可能需要读取数据的子页并未存在dram和buffer中，所以需要将从SSD中写入到dram中的子页进行相应的操作，但是必须要确认到哪些子页需要进行写入。因此程序进行进一步的确认，首先函数会调用set_entry_state()进行子页映射状态位的设置，设置后的结果保存在map_entry_new中set_entry_state()函数会根据lsn和sub_size的参数对从lsn位置开始的子页长度为sub_size的部分标记状态位置1，代表了需要重新在map_entry[lpn].state中进行设置的子页有效映射。接着函数会用map_entry_old保存当前dram中map_entry[lpn].state的位映射状态信息，将map_entry_new和map_entry_old进行一个按位或计算操作从而更新映射状态位，并将结果保存在modify中。随后将lpn的映射物理页pn保存至ppn中；完成映射更新后就代表了程序已经完成了该lsn在当前lpn下的读操作。随后程序会调用find_location()函数进行物理地址的查找，紧接着会统计相关的ssd信息：更新整个ssd、当前channel和该channel下当前所在的chip中的program_count计数，表示完成一次写page操作计数；然后将modify中保存的映射更新信息赋值给当前lsn所在的lpn对应的map_entry[lpn].state，表示已经完成了将数据写入到SSD相对应的page子页中且已经读取到了dram中；同时根据location物理地址结构体的信息设置page中的子页有效映射位valid_state也为modify以保持与dram中的同步；且同时设置更新该page中剩余free状态的子页，接着释放并置空location。
				else if(ssd->dram->map->map_entry[lpn].state>0)           /*状态不为0的情况*/
				{
					map_entry_new=set_entry_state(ssd,lsn,sub_size);      /*得到新的状态，并与原来的状态相或的到一个状态*/
					map_entry_old=ssd->dram->map->map_entry[lpn].state;
                    modify=map_entry_new|map_entry_old;
					ppn=ssd->dram->map->map_entry[lpn].pn;
					location=find_location(ssd,ppn);

					ssd->program_count++;
					ssd->channel_head[location->channel].program_count++;
					ssd->channel_head[location->channel].chip_head[location->chip].program_count++;
					ssd->dram->map->map_entry[lsn/ssd->parameter->subpage_page].state=modify;
					ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state=modify;
					ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state=((~modify)&full_page);

					free(location);
					location=NULL;
				}//else if(ssd->dram->map->map_entry[lpn].state>0)
//当函数完成上述过程后，证明已经成功完成了当前IOtrace的lsn所对应的lpn读操作，这时候只是读完成了至多一个page的大小，因此程序此时需要更新lsn的位置即令lsn加上已经读取完毕的sub_size大小，且同时更新已经处理的IO长度add_size。接着程序流会继续判断当前是否已经完成了该IOtrace的处理，如果未完成则重复以上的过程。当完成了该IOtrace的读请求处理后，程序会回到fgets()函数中继续读取tracefile中的下一个IO，周始反复一直到读取完所有的IOtrace。
				lsn=lsn+sub_size;                                         /*下个子请求的起始位置*/
				add_size+=sub_size;                                       /*已经处理了的add_size大小变化*/
			}//while(add_size<size)
		// }
	}

//当程序处理完tracefile中所有的读请求后，便开始打印处理完成信息，并且调用fclose()关闭tracefile文件流。

	printf("\n");
	printf("pre_process_write_read is complete!\n");

	fclose(ssd->tracefile2);
//随即用一个多重嵌套for循环将SSD当前状态下每一个plane的free空闲状态页的数量都按照固定格式写入到ssd->outputfile中。每一次针对每个plane都会用fflush()函数立即将缓冲区中的数据流更新写入到outputfile文件中。当完成上述所有操作后，pre_process_page便完成了其任务，返回ssd结构体。
	for(i=0;i<ssd->parameter->channel_number;i++)
    for(j=0;j<ssd->parameter->die_chip;j++)
	for(k=0;k<ssd->parameter->plane_die;k++)
	{
		fprintf(ssd->outputfile,"chip:%d,die:%d,plane:%d have free page2: %d\n",i,j,k,ssd->channel_head[i].chip_head[0].die_head[j].plane_head[k].free_page);
		fflush(ssd->outputfile);
	}

	// unsigned int x=0,y=0,i=0,j=0,k=0,l=0,m=0,n=0; //这里定义的八个uint变量应该就是指为了循环遍历初始化上述这八个层次中的所有结构参数而设定的
    // unsigned int lpn = 0;
    // unsigned int lsn = 100000;
    // unsigned int ppn, full_page;
	// printf("enter\n");
    // for (i = 0; i < ssd->parameter->channel_number; i++)
    // {
    //     for (j = 0; j < ssd->parameter->chip_num / ssd->parameter->channel_number; j++)
    //     {
    //         for (k = 0; k < ssd->parameter->die_chip; k++)
    //         {
    //             for (l = 0; l < ssd->parameter->plane_die; l++)
    //             {
    //                 for (m = 0; m < ssd->parameter->block_plane; m++)
    //                 {
    //                     for (n = 0; n < 0.02 * ssd->parameter->page_block; n++)
    //                     {
    //                         ppn = find_ppn(ssd, i, j, k, l, m, n);
    //                         //modify state
    //                         ssd->dram->map->map_entry[lpn].pn = ppn;
    //                         ssd->dram->map->map_entry[lpn].state = set_entry_state(ssd, 0, 16);   //0001
    //                         ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].page_head[n].lpn = lpn;
    //                         ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].page_head[n].valid_state = ssd->dram->map->map_entry[lpn].state;
    //                         ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].page_head[n].free_state = ((~ssd->dram->map->map_entry[lpn].state) & full_page);
    //                         //--
    //                         ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].last_write_page++;
    //                         ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].free_page_num--;
    //                         ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].free_page--;
    //                         lpn++;
    //                     }
    //                 }

    //             }
    //         }
    //     }
    // }
	// abort();

	return ssd;
}

/**************************************
*函数功能是为预处理函数获取物理页号ppn
*获取页号分为动态获取和静态获取
**************************************/
unsigned int get_ppn_for_pre_process(struct ssd_info *ssd,unsigned int lsn)
{
	unsigned int channel=0,chip=0,die=0,plane=0;
	unsigned int ppn,lpn;
	unsigned int active_block;
	unsigned int channel_num=0,chip_num=0,die_num=0,plane_num=0;

#ifdef DEBUG
	printf("enter get_psn_for_pre_process\n");
#endif

	channel_num=ssd->parameter->channel_number;
	chip_num=ssd->parameter->chip_channel[0];
	die_num=ssd->parameter->die_chip;
	plane_num=ssd->parameter->plane_die;
	lpn=lsn/ssd->parameter->subpage_page;//根据逻辑扇区号找到所在的逻辑页

	//看根据lsn申请物理地址时channel,chip等是怎么分配的，然后反过来就知道求对应的channel,chip，die,plane
	if (ssd->parameter->allocation_scheme==0)                           /*动态方式下获取ppn*/
	{
		if (ssd->parameter->dynamic_allocation==0)                      /*表示全动态方式下，也就是channel，chip，die，plane，block等都是动态分配*/
		{
			channel=ssd->token;
			ssd->token=(ssd->token+1)%ssd->parameter->channel_number;
			chip=ssd->channel_head[channel].token;
			ssd->channel_head[channel].token=(chip+1)%ssd->parameter->chip_channel[0];
			die=ssd->channel_head[channel].chip_head[chip].token;
			ssd->channel_head[channel].chip_head[chip].token=(die+1)%ssd->parameter->die_chip;
			plane=ssd->channel_head[channel].chip_head[chip].die_head[die].token;
			ssd->channel_head[channel].chip_head[chip].die_head[die].token=(plane+1)%ssd->parameter->plane_die;
		}
		else if (ssd->parameter->dynamic_allocation==1)                 /*表示半动态方式，channel静态给出，package，die，plane动态分配*/
		{
			channel=lpn%ssd->parameter->channel_number;
			chip=ssd->channel_head[channel].token;
			ssd->channel_head[channel].token=(chip+1)%ssd->parameter->chip_channel[0];
			die=ssd->channel_head[channel].chip_head[chip].token;
			ssd->channel_head[channel].chip_head[chip].token=(die+1)%ssd->parameter->die_chip;
			plane=ssd->channel_head[channel].chip_head[chip].die_head[die].token;
			ssd->channel_head[channel].chip_head[chip].die_head[die].token=(plane+1)%ssd->parameter->plane_die;
		}
	}
	else if (ssd->parameter->allocation_scheme==1)                       /*表示静态分配，同时也有0,1,2,3,4,5这6中不同静态分配方式*/
	{
		switch (ssd->parameter->static_allocation)
		{

		case 0:
			{
				channel=(lpn/(plane_num*die_num*chip_num))%channel_num;
				chip=lpn%chip_num;
				die=(lpn/chip_num)%die_num;
				plane=(lpn/(die_num*chip_num))%plane_num;
				break;
			}
		case 1:
			{
				channel=lpn%channel_num;
				chip=(lpn/channel_num)%chip_num;
				die=(lpn/(chip_num*channel_num))%die_num;
				plane=(lpn/(die_num*chip_num*channel_num))%plane_num;
				break;
			}
		case 2:
			{
				channel=lpn%channel_num;
				chip=(lpn/(plane_num*channel_num))%chip_num;
				die=(lpn/(plane_num*chip_num*channel_num))%die_num;
				plane=(lpn/channel_num)%plane_num;
				break;
			}
		case 3:
			{
				channel=lpn%channel_num;
				chip=(lpn/(die_num*channel_num))%chip_num;
				die=(lpn/channel_num)%die_num;
				plane=(lpn/(die_num*chip_num*channel_num))%plane_num;
				break;
			}
		case 4:
			{
				channel=lpn%channel_num;
				chip=(lpn/(plane_num*die_num*channel_num))%chip_num;
				die=(lpn/(plane_num*channel_num))%die_num;
				plane=(lpn/channel_num)%plane_num;

				break;
			}
		case 5:
			{
				channel=lpn%channel_num;
				chip=(lpn/(plane_num*die_num*channel_num))%chip_num;
				die=(lpn/channel_num)%die_num;
				plane=(lpn/(die_num*channel_num))%plane_num;

				break;
			}
		default : return 0;
		}
	}

	/******************************************************************************
	*根据上述分配方法找到channel，chip，die，plane后，再在这个里面找到active_block（每个plane里只有一个活跃块，只有再活跃块中才能进行操作）
	*接着获得ppn
	******************************************************************************/
	if(find_active_block(ssd,channel,chip,die,plane)==FAILURE)
	{
		printf("the read operation is expand the capacity of SSD\n");
		return 0;
	}
	active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;
	if(write_page(ssd,channel,chip,die,plane,active_block,&ppn)==ERROR)
	{
		return 0;
	}

	return ppn;
}

int check_plane(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane){
	unsigned int free_page_num;
	unsigned int cumulate_free_page_num=0;
	unsigned int cumulate_free_lsb_num=0;
	unsigned int i;
	struct plane_info candidate_plane;
	candidate_plane = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane];
	for(i=0;i<ssd->parameter->block_plane;i++){
		if(candidate_plane.blk_head[i].free_lsb_num+candidate_plane.blk_head[i].free_csb_num+candidate_plane.blk_head[i].free_msb_num!=candidate_plane.blk_head[i].free_page_num){
			printf("Meta data Wrong, BLOCK LEVEL!!\n");
			return FALSE;
			}
		cumulate_free_page_num+=candidate_plane.blk_head[i].free_page_num;
		cumulate_free_lsb_num+=candidate_plane.blk_head[i].free_lsb_num;
		}
	if(cumulate_free_page_num!=candidate_plane.free_page){
		printf("Meta data Wrong, PLANE LEVEL!!\n");
		if(cumulate_free_page_num>candidate_plane.free_page){
			printf("The data in plane_info is SMALLER than the blocks.\n");
			}
		else{
			printf("The data in plane_info is GREATER than the blocks.\n");
			}
		return FALSE;
		}
	return TRUE;
}

/**************************************************************************************************
*该函数实现优先寻找LSB page写入的策略
**************************************************************************************************/
struct ssd_info *get_ppn_lf(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane,struct sub_request *sub)
{
	//printf("Turbo mode page allocation.\n");
	int old_ppn=-1;
	unsigned int ppn,lpn,full_page;
	unsigned int active_block;
	unsigned int block;
	unsigned int page,flag=0,flag1=0;
	unsigned int old_state=0,state=0,copy_subpage=0;
	struct local *location;
	struct direct_erase *direct_erase_node,*new_direct_erase,*direct_fast_erase_node,*new_direct_fast_erase;
	struct gc_operation *gc_node;

	unsigned int available_page;

	unsigned int i=0,j=0,k=0,l=0,m=0,n=0;

#ifdef DEBUG
	printf("enter get_ppn,channel:%d, chip:%d, die:%d, plane:%d\n",channel,chip,die,plane);
#endif
	if(ssd->parameter->subpage_page == 32){
		full_page = 0xffffffff;
		}
	else{
		full_page=~(0xffffffff<<(ssd->parameter->subpage_page));
		}
	lpn=sub->lpn;

	//尝试在目标plane中寻找有空闲页的block，其中优先寻找有空闲lsb page的block，否则寻找有空闲msb page的block
	if(sub->target_page_type==TARGET_LSB){
		available_page = find_active_block_lf(ssd,channel,chip,die,plane);
		}
	else if(sub->target_page_type==TARGET_CSB){
		available_page = find_active_block_csb(ssd, channel, chip, die, plane);
		if(available_page==FAILURE){
			available_page = find_active_block_lf(ssd,channel,chip,die,plane);
			}
		}
	else if(sub->target_page_type==TARGET_MSB){
		available_page = find_active_block_msb(ssd, channel, chip, die, plane);
		if(available_page==FAILURE){
			available_page = find_active_block_lf(ssd,channel,chip,die,plane);
			}
		}
	else{
		printf("WRONG Target page type!!!!\n");
		return ssd;
		}
	if(available_page == SUCCESS_LSB){
		active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_lsb_block;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_lsb += 3;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_lsb_num--;
		ssd->free_lsb_count--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_lsb;
		sub->allocated_page_type = TARGET_LSB;
		}
	else if(available_page == SUCCESS_CSB){
		active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_csb_block;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_csb += 3;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_csb_num--;
		ssd->free_csb_count--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_csb;
		sub->allocated_page_type = TARGET_CSB;
		}
	else if(available_page == SUCCESS_MSB){
		active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_msb_block;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_msb += 3;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_msb_num--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;
		ssd->free_msb_count--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_msb;
		sub->allocated_page_type = TARGET_MSB;
		}
	else{
		printf("ERROR :there is no free page in channel:%d, chip:%d, die:%d, plane:%d\n",channel,chip,die,plane);
		printf("free page number: %d, free lsb page number: %d\n",ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page,ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_lsb_num);
		return ssd;
	}
	ssd->sub_request_all++;
	if(sub->allocated_page_type == sub->target_page_type){
		ssd->sub_request_success++;
		}
	//检查页编号是否超出范围
	if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page>=ssd->parameter->page_block){
		printf("ERROR! the last write page larger than the number of pages per block!!\n");
		printf("The active block is %d, ",active_block);
		if(available_page == SUCCESS_LSB){
			printf("targeted LSB page.\n");
			}
		else if(available_page == SUCCESS_CSB){
			printf("targeted CSB page.\n");
			}
		else{
			printf("targeted MSB page.\n");
			}
		for(i=0;i<ssd->parameter->block_plane;i++){
			printf("%d, ", ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].last_write_lsb);
			}
		printf("\n");
		for(i=0;i<ssd->parameter->block_plane;i++){
			printf("%d, ", ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].last_write_csb);
			}
		printf("\n");
		for(i=0;i<ssd->parameter->block_plane;i++){
			printf("%d, ", ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].last_write_msb);
			}
		printf("\n");
		//printf("The page number: %d.\n", ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page);
		while(1){}
		}

	//成功寻找到活跃块以及可以写入的空闲page
	block=active_block;
	page=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page;
	/*
	if(lpn == 40470){
		printf("+++++++++++++lpn:40470, location:%d, %d, %d, %d, %d, %d.\n",channel, chip, die, plane, active_block, page);
		}
	*/
	//判断该逻辑页是否有对应的旧物理页，如果有就找出来并且置为无效页
	if(ssd->dram->map->map_entry[lpn].state == 0){
		if(ssd->dram->map->map_entry[lpn].pn != 0){
			printf("Error in get_ppn_lf()_position_1, pn: %d.\n", ssd->dram->map->map_entry[lpn].pn);
			return NULL;
			}
		ssd->dram->map->map_entry[lpn].pn = find_ppn(ssd,channel,chip,die,plane,block,page);
		ssd->dram->map->map_entry[lpn].state = sub->state;
		}
	else{
		ppn = ssd->dram->map->map_entry[lpn].pn;
		location = find_location(ssd,ppn);
		if(ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn != lpn)
			{
			printf("Error in get_ppn()_position_2\n");
			printf("lpn:%d, ppn:%d, location:%d, %d, %d, %d, %d, %d, ",lpn,ppn,location->channel,location->chip,location->die,location->plane,location->block,location->page);
			printf("the wrong lpn:%d.\n", ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn);
			return NULL;
			}
		ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state=0;			  /*表示某一页失效，同时标记valid和free状态都为0*/
		ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state=0;			  /*表示某一页失效，同时标记valid和free状态都为0*/
		ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn=0;
		ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num++;
		if((location->page)%3==0){
			ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_lsb_num++;
			}
		/*******************************************************************************************
		*该block中全是invalid的页，可以直接删除，就在创建一个可擦除的节点，挂在location下的plane下面
		********************************************************************************************/
		struct blk_info target_block = ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block];
		if(ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num == ssd->parameter->page_block)
			{
			new_direct_erase = (struct direct_erase *)malloc(sizeof(struct direct_erase));
			alloc_assert(new_direct_erase,"new_direct_erase");
			memset(new_direct_erase,0, sizeof(struct direct_erase));

			new_direct_erase->block = location->block;
			new_direct_erase->next_node = NULL;
			direct_erase_node = ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node;
			if(direct_erase_node == NULL){
				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node=new_direct_erase;
				}
			else{
				new_direct_erase->next_node = ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node;
				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node = new_direct_erase;
				}
			}
		/*
		else if((target_block.free_msb_num == ssd->parameter->page_block/2)&&(target_block.invalid_lsb_num == ssd->parameter->page_block/2)){
			//这个块只有lsb被写，而且所有lsb页都是无效页
			printf("==>A FAST GC CAN BE TRIGGER.\n");
			new_direct_fast_erase = (struct direct_erase *)malloc(sizeof(struct direct_erase));
			alloc_assert(new_direct_fast_erase,"new_direct_fast_erase");
			memset(new_direct_fast_erase,0, sizeof(struct direct_erase));

			new_direct_fast_erase->block = location->block;
			new_direct_fast_erase->next_node = NULL;
			direct_fast_erase_node = ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].fast_erase_node;
			if(direct_fast_erase_node == NULL){
				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].fast_erase_node=new_direct_fast_erase;
				}
			else{
				new_direct_fast_erase->next_node = ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].fast_erase_node;
				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].fast_erase_node = new_direct_fast_erase;
				}
			//ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].fast_erase = TRUE;
			printf("====>FAST GC OPT IS INITIALED.\n");
			}
			*/
		free(location);
		location = NULL;
		ssd->dram->map->map_entry[lpn].pn = find_ppn(ssd,channel,chip,die,plane,block,page);
		ssd->dram->map->map_entry[lpn].state = (ssd->dram->map->map_entry[lpn].state|sub->state);
		}

	sub->ppn=ssd->dram->map->map_entry[lpn].pn; 									 /*修改sub子请求的ppn，location等变量*/
	sub->location->channel=channel;
	sub->location->chip=chip;
	sub->location->die=die;
	sub->location->plane=plane;
	sub->location->block=active_block;
	sub->location->page=page;

	ssd->program_count++;															/*修改ssd的program_count,free_page等变量*/
	ssd->channel_head[channel].program_count++;
	ssd->channel_head[channel].chip_head[chip].program_count++;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page--;
	//if(page%2==0){
	//	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_lsb_num--;
	//	}
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].lpn=lpn;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].valid_state=sub->state;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].free_state=((~(sub->state))&full_page);
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].written_count++;
	/*
	if(lpn == 40470){
		printf("+++++++++++++lpn:40470, stored lpn:%d\n",ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].lpn);
		}
	*/
	ssd->write_flash_count++;
	if(page%3 == 0){
		ssd->write_lsb_count++;
		ssd->newest_write_lsb_count++;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_lsb_num--;
		//ssd->free_lsb_count--;
		}
	else if(page%3 == 1){
		ssd->write_csb_count++;
		ssd->newest_write_csb_count++;
		}
	else{
		ssd->write_msb_count++;
		ssd->newest_write_msb_count++;
		}

	if (ssd->parameter->active_write == 0)											/*如果没有主动策略，只采用gc_hard_threshold，并且无法中断GC过程*/
		{																				/*如果plane中的free_page的数目少于gc_hard_threshold所设定的阈值就产生gc操作*/
		if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page < (ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->gc_hard_threshold))
			{
			printf("+++++Garbage Collection is NEEDED.\n");
			gc_node=(struct gc_operation *)malloc(sizeof(struct gc_operation));
			alloc_assert(gc_node,"gc_node");
			memset(gc_node,0, sizeof(struct gc_operation));

			gc_node->next_node=NULL;
			gc_node->chip=chip;
			gc_node->die=die;
			gc_node->plane=plane;
			gc_node->block=0xffffffff;
			gc_node->page=0;
			gc_node->state=GC_WAIT;
			gc_node->priority=GC_UNINTERRUPT;
			gc_node->next_node=ssd->channel_head[channel].gc_command;
			ssd->channel_head[channel].gc_command=gc_node;
			ssd->gc_request++;
			}
		/*IMPORTANT!!!!!!*/
		/*Fast Collection Function*/
		else if(ssd->parameter->fast_gc == 1){
			if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_lsb_num < (ssd->parameter->page_block*ssd->parameter->block_plane/2)*ssd->parameter->fast_gc_thr_2){
				//printf("==>A FAST GC CAN BE TRIGGERED, %d,%d,%d,%d,,,%d.\n",channel,chip,die,plane,ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_lsb_num);
				struct gc_operation *temp_gc_opt;
				int temp_flag=0;
				temp_gc_opt = ssd->channel_head[channel].gc_command;
				while(temp_gc_opt != NULL){
					if(temp_gc_opt->chip==chip&&temp_gc_opt->die==die&&temp_gc_opt->plane==plane){
						temp_flag=1;
						break;
						}
					temp_gc_opt = temp_gc_opt->next_node;
					}
				if(temp_flag!=1){
					gc_node=(struct gc_operation *)malloc(sizeof(struct gc_operation));
					alloc_assert(gc_node,"fast_gc_node");
					memset(gc_node,0, sizeof(struct gc_operation));

					gc_node->next_node=NULL;
					gc_node->chip=chip;
					gc_node->die=die;
					gc_node->plane=plane;
					gc_node->block=0xffffffff;
					gc_node->page=0;
					gc_node->state=GC_WAIT;
					gc_node->priority=GC_FAST_UNINTERRUPT_EMERGENCY;
					gc_node->next_node=ssd->channel_head[channel].gc_command;
					ssd->channel_head[channel].gc_command=gc_node;
					ssd->gc_request++;
					}
				}
			else if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_lsb_num < (ssd->parameter->page_block*ssd->parameter->block_plane/2)*ssd->parameter->fast_gc_thr_1){
				//printf("==>A FAST GC CAN BE TRIGGERED, %d,%d,%d,%d,,,%d.\n",channel,chip,die,plane,ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_lsb_num);
				struct gc_operation *temp_gc_opt;
				int temp_flag=0;
				temp_gc_opt = ssd->channel_head[channel].gc_command;
				while(temp_gc_opt != NULL){
					if(temp_gc_opt->chip==chip&&temp_gc_opt->die==die&&temp_gc_opt->plane==plane){
						temp_flag=1;
						break;
						}
					temp_gc_opt = temp_gc_opt->next_node;
					}
				if(temp_flag!=1){
					gc_node=(struct gc_operation *)malloc(sizeof(struct gc_operation));
					alloc_assert(gc_node,"fast_gc_node");
					memset(gc_node,0, sizeof(struct gc_operation));

					gc_node->next_node=NULL;
					gc_node->chip=chip;
					gc_node->die=die;
					gc_node->plane=plane;
					gc_node->block=0xffffffff;
					gc_node->page=0;
					gc_node->state=GC_WAIT;
					gc_node->priority=GC_FAST_UNINTERRUPT;
					gc_node->next_node=ssd->channel_head[channel].gc_command;
					ssd->channel_head[channel].gc_command=gc_node;
					ssd->gc_request++;
					}
				}
			else if(ssd->request_queue->priority==0){
				//printf("It is IDLE time for garbage collection.\n")
				struct gc_operation *temp_gc_opt;
				int temp_flag=0;
				temp_gc_opt = ssd->channel_head[channel].gc_command;
				/*
				while(temp_gc_opt != NULL){
					//修改为每个chip最多有一个回收，避免过多回收影响性能
					//if(temp_gc_opt->chip==chip&&temp_gc_opt->die==die&&temp_gc_opt->plane==plane){
					if(temp_gc_opt->chip==chip){
						temp_flag=1;
						break;
						}
					temp_gc_opt = temp_gc_opt->next_node;
					}
					*/
				//要求每次进行gc的channel数量不超过总channel数的25%
				unsigned int channel_count,gc_channel_num = 0;
				unsigned int newest_avg_write_lat;
				if(ssd->newest_write_request_count==0){
					newest_avg_write_lat = 0;
					}
				else{
					newest_avg_write_lat = ssd->newest_write_avg/ssd->newest_write_request_count;
					}
				if(newest_avg_write_lat >= 8000000){
				/*
				unsigned int last_ten_write_avg_lat=0;
				unsigned int fgc_i;
				for(fgc_i=0;fgc_i<10;fgc_i++){
					last_ten_write_avg_lat += ssd->last_ten_write_lat[fgc_i];
					}
				last_ten_write_avg_lat = last_ten_write_avg_lat/10;
				if(last_ten_write_avg_lat >= 8000000){
				*/
					temp_flag=1;
					}
				else if(temp_gc_opt == NULL){
					for(channel_count=0;channel_count<ssd->parameter->channel_number;channel_count++){
						if(ssd->channel_head[channel_count].gc_command != NULL){
							gc_channel_num++;
							}
						}
					if(gc_channel_num >= 4){
						temp_flag=1;
						}
					}
				else{
					temp_flag=1;
					}
				if(temp_flag!=1){
					gc_node=(struct gc_operation *)malloc(sizeof(struct gc_operation));
					alloc_assert(gc_node,"fast_gc_node");
					memset(gc_node,0, sizeof(struct gc_operation));

					gc_node->next_node=NULL;
					gc_node->chip=chip;
					gc_node->die=die;
					gc_node->plane=plane;
					gc_node->block=0xffffffff;
					gc_node->page=0;
					gc_node->state=GC_WAIT;
					gc_node->priority=GC_FAST_UNINTERRUPT_IDLE;
					gc_node->next_node=ssd->channel_head[channel].gc_command;
					ssd->channel_head[channel].gc_command=gc_node;
					ssd->gc_request++;
					}
				}
			}
		}
	if(check_plane(ssd, channel, chip, die, plane) == FALSE){
		printf("Something Wrong Happened.\n");
		return FAILURE;
		}
	return ssd;
}

/***************************************************************************************************
*函数功能是在所给的channel，chip，die，plane里面找到一个active_block然后再在这个block里面找到一个页，
*再利用find_ppn找到ppn。
****************************************************************************************************/
struct ssd_info *get_ppn(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane,struct sub_request *sub)
{
//	if(ssd->parameter->lsb_first_allocation== 1){
//		return get_ppn_lf(ssd,channel,chip,die,plane,sub);
//
//		//if((rand()%100)<ssd->parameter->turbo_mode_factor){
//			//printf("turbo write\n...");
//			//return get_ppn_lf(ssd,channel,chip,die,plane,sub);
//
//			//}
//		}
	int old_ppn=-1;
	unsigned int ppn,lpn,full_page;
	unsigned int active_block;
	unsigned int block;
	unsigned int page,flag=0,flag1=0;
	unsigned int old_state=0,state=0,copy_subpage=0;
	struct local *location;
	struct direct_erase *direct_erase_node,*new_direct_erase;
	struct gc_operation *gc_node;

	unsigned int i=0,j=0,k=0,l=0,m=0,n=0;

#ifdef DEBUG
	printf("enter get_ppn,channel:%d, chip:%d, die:%d, plane:%d\n",channel,chip,die,plane);
#endif
	if(ssd->parameter->subpage_page == 32){
		full_page = 0xffffffff;
		}
	else{
		full_page=~(0xffffffff<<(ssd->parameter->subpage_page));
		}
	lpn=sub->lpn;

	/*************************************************************************************
	*利用函数find_active_block在channel，chip，die，plane找到活跃block
	*并且修改这个channel，chip，die，plane，active_block下的last_write_page和free_page_num
	**************************************************************************************/
	if(find_active_block(ssd,channel,chip,die,plane)==FAILURE)
	{
		printf("ERROR :there is no free page in channel:%d, chip:%d, die:%d, plane:%d\n",channel,chip,die,plane);
		return ssd;
	}

	active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page++;
	//查找是否有被提前释放掉的page
	// for (i = 0; i < ssd->parameter->page_block; i++)
	// {
	// 	if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[i].valid_state == -1)
	// 	{
	// 		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[i].lpn;
	// 	}
	// 	else
	// 	{
	// 		if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[i].valid_state > 0)
	// 		{
	// 			ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page++;
	// 		}
	// 	}
	// }

	//******************************************Modification for turbo_mode*********************************
	/*
	if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page%3 == 0){
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_lsb += 3;
		}
	else if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page%3 == 1){
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_csb += 3;
		}
	else if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page%3 == 2){
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_msb += 3;
		}
	else{
		printf("Not gonna happen.\n");
		}
		*/
	/*
	if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_msb<ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_csb){
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_msb+3;
		}
	else if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_csb<ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_lsb){
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_csb+3;
		}
	else{
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_lsb+3;
		}
		*/
	//******************************************
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;

	if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page>=ssd->parameter->page_block)
	{
		printf("get_ppn：error! the last write page  %d  larger than the number %d of pages per block!!\n", ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page, ssd->parameter->page_block);
		while(1){}
	}

	block=active_block;
	page=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page;
	if(page%3==0){
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_lsb=page;
		ssd->free_lsb_count--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_lsb_num--;
		//ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;
		ssd->write_lsb_count++;
		ssd->newest_write_lsb_count++;
		//**********************
		sub->allocated_page_type = TARGET_LSB;
		//**********************
		}
	else if(page%3==2){
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_msb=page;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_msb_num--;
		//ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;
		ssd->write_msb_count++;
		ssd->free_msb_count--;
		ssd->newest_write_msb_count++;
		//**********************
		sub->allocated_page_type = TARGET_MSB;
		//**********************
		}
	else{
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_csb=page;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_csb_num--;
		ssd->write_csb_count++;
		ssd->free_csb_count--;
		ssd->newest_write_csb_count++;
		sub->allocated_page_type = TARGET_CSB;
		}
	/*至此，物理地址已找到，现在要做的就是建立映射
	首先，看这个物理地址是不是已经有数据了，
	若没有，直接把物理地址转换成ppn，然后建立lpn与ppn之间的映射关系即可
	若已经有数据了，就需原来的数据置为失效，现有的请求写到其他位置
	*/
	if(ssd->dram->map->map_entry[lpn].state==0)         //没有映射关系，表示该子请求不是更新请求，可以直接写入         /*this is the first logical page*/
	{
		if(ssd->dram->map->map_entry[lpn].pn!=0)
		{
			printf("Error in get_ppn()\n");
		}
		ssd->dram->map->map_entry[lpn].pn=find_ppn(ssd,channel,chip,die,plane,block,page);
		ssd->dram->map->map_entry[lpn].state=sub->state;
	}
	else   /*这个逻辑页进行了更新，需要将原来的页置为失效*/
	{
		ppn=ssd->dram->map->map_entry[lpn].pn;
		location=find_location(ssd,ppn);
		//如果是hdd_flag=1,则location.lpn=0(move_page)
		if(	ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn!=lpn && ssd->dram->map->map_entry[lpn].hdd_flag == 0)
		{
			printf("lpn:%d locaton-lpn:%d\n", lpn, ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn);
			printf("\nError %din get_ppn()2\n", ssd->dram->map->map_entry[lpn].hdd_flag);
		}

		ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state=0;             /*表示某一页失效，同时标记valid和free状态都为0*/
		ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state=0;              /*表示某一页失效，同时标记valid和free状态都为0*/
		ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn=0;   //删除该页的映射
		ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num++;
		if((location->page)%3==0){
			ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_lsb_num++;
			}
		/*******************************************************************************************
		*该block中全是invalid的页，可以直接删除，就在创建一个可擦除的节点，挂在location下的plane下面
		********************************************************************************************/
		if (ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num==ssd->parameter->page_block)
		{
			new_direct_erase=(struct direct_erase *)malloc(sizeof(struct direct_erase));
            alloc_assert(new_direct_erase,"new_direct_erase");
			memset(new_direct_erase,0, sizeof(struct direct_erase));

			new_direct_erase->block=location->block;
			new_direct_erase->next_node=NULL;
			direct_erase_node=ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node;
			if (direct_erase_node==NULL)
			{
				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node=new_direct_erase;
			}
			else
			{
				new_direct_erase->next_node=ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node;
				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node=new_direct_erase;
			}
		}

		free(location);
		location=NULL;
		ssd->dram->map->map_entry[lpn].pn=find_ppn(ssd,channel,chip,die,plane,block,page);
		ssd->dram->map->map_entry[lpn].state=(ssd->dram->map->map_entry[lpn].state|sub->state);
		//逻辑页更新之后，将hdd_flag置为0
		if (ssd->dram->map->map_entry[lpn].hdd_flag != 0)
		{
			// printf("update data hdd_flag:%d lpn:%d\n", ssd->dram->map->map_entry[lpn].hdd_flag, lpn);
			// if (ssd->dram->map->map_entry[lpn].hdd_flag == 2)
			// {
			// 	// record_update_write(ssd, lpn);
			// }
			// 被打包page更新
			if (ssd->dram->map->map_entry[lpn].hdd_flag == 2)
			{
				ssd->update_hdd_count++;
			}
			ssd->dram->map->map_entry[lpn].hdd_flag=0;
		}
	}


	sub->ppn=ssd->dram->map->map_entry[lpn].pn;                                      /*修改sub子请求的ppn，location等变量*/
	sub->location->channel=channel;
	sub->location->chip=chip;
	sub->location->die=die;
	sub->location->plane=plane;
	sub->location->block=active_block;
	sub->location->page=page;

	ssd->program_count++;                                                           /*修改ssd的program_count,free_page等变量*/
	ssd->channel_head[channel].program_count++;
	ssd->channel_head[channel].chip_head[chip].program_count++;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page--;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].lpn=lpn;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].valid_state=sub->state;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].free_state=((~(sub->state))&full_page);
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].written_count++;
	ssd->write_flash_count++;

	if (ssd->parameter->active_write==0)                                            /*如果没有主动策略，只采用gc_hard_threshold，并且无法中断GC过程*/
	{                                                                               /*如果plane中的free_page的数目少于gc_hard_threshold所设定的阈值就产生gc操作*/
		if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page<(ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->gc_hard_threshold))
		{
			gc_node=(struct gc_operation *)malloc(sizeof(struct gc_operation));
			alloc_assert(gc_node,"gc_node");
			memset(gc_node,0, sizeof(struct gc_operation));

			gc_node->next_node=NULL;
			gc_node->chip=chip;
			gc_node->die=die;
			gc_node->plane=plane;
			gc_node->block=0xffffffff;
			gc_node->page=0;
			gc_node->state=GC_WAIT;
			gc_node->priority=GC_UNINTERRUPT;
			gc_node->next_node=ssd->channel_head[channel].gc_command;
			ssd->channel_head[channel].gc_command=gc_node;
			ssd->gc_request++;
		}
	}
//	if(check_plane(ssd, channel, chip, die, plane) == FALSE){
//		printf("Something Wrong Happened.\n");
//		return FAILURE;
//		}
	return ssd;
}
/*****************************************************************************************
*这个函数功能是为gc操作寻找新的ppn，因为在gc操作中需要找到新的物理块存放原来物理块上的数据
*在gc中寻找新物理块的函数，不会引起循环的gc操作
******************************************************************************************/
unsigned int get_ppn_for_gc_lf(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane)
{
	unsigned int ppn;
	unsigned int active_block,block,page;
	int i;

	unsigned int available_page;
#ifdef DEBUG
	printf("enter get_ppn_for_gc,channel:%d, chip:%d, die:%d, plane:%d\n",channel,chip,die,plane);
#endif
	/*
	if(find_active_block(ssd,channel,chip,die,plane)!=SUCCESS)
	{
		printf("\n\n Error int get_ppn_for_gc().\n");
		return 0xffffffff;
	}
	*/
	//available_page = find_active_block_lf(ssd,channel,chip,die,plane);

	available_page = find_active_block_msb(ssd, channel, chip, die, plane);
	if(available_page==FAILURE){
		available_page = find_active_block_lf(ssd,channel,chip,die,plane);
		}

	if(available_page == SUCCESS_LSB){
		active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_lsb_block;
		//printf("the last write lsb page: %d.\n",ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_lsb);
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_lsb += 3;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_lsb_num--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_lsb_num--;
		ssd->free_lsb_count--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_lsb;
		ssd->write_lsb_count++;
		ssd->newest_write_lsb_count++;
		}
	else if(available_page == SUCCESS_CSB){
		active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_csb_block;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_csb += 3;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_csb_num--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_csb;
		ssd->write_csb_count++;
		ssd->free_csb_count--;
		ssd->newest_write_csb_count++;
		}
	else if(available_page == SUCCESS_MSB){
		active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_msb_block;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_msb += 3;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_msb_num--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_msb;
		ssd->write_msb_count++;
		ssd->free_msb_count--;
		ssd->newest_write_msb_count++;
		}
	else{
		printf("ERROR :there is no free page in channel:%d, chip:%d, die:%d, plane:%d\n",channel,chip,die,plane);
		while(1){
			}
	}

	if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page>=ssd->parameter->page_block){
		printf("ERROR! the last write page larger than the number of pages per block!! pos 2\n");
		for(i=0;i<ssd->parameter->block_plane;i++){
			printf("%d, ", ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].last_write_lsb);
			}
		printf("\n");
		for(i=0;i<ssd->parameter->block_plane;i++){
			printf("%d, ", ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].last_write_page);
			}
		printf("\n");
		//printf("The page number: %d.\n", ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page);
		while(1){}
		}


	//active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;

	//ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page++;
	//ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;
	/*
	if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page>ssd->parameter->page_block)
	{
		printf("DDD error! the last write page larger than 64!!\n");
		while(1){}
	}
	*/
	block=active_block;
	page=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page;

	ppn=find_ppn(ssd,channel,chip,die,plane,block,page);

	ssd->program_count++;
	ssd->channel_head[channel].program_count++;
	ssd->channel_head[channel].chip_head[chip].program_count++;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page--;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].written_count++;
	ssd->write_flash_count++;

	return ppn;
}


 unsigned int get_ppn_for_gc(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane)
{
//	if(ssd->parameter->turbo_mode != 0){
//		return get_ppn_for_gc_lf(ssd, channel, chip, die, plane);
//		}
	unsigned int ppn;
	unsigned int active_block,block,page;

#ifdef DEBUG
	printf("enter get_ppn_for_gc,channel:%d, chip:%d, die:%d, plane:%d\n",channel,chip,die,plane);
#endif

	if(find_active_block(ssd,channel,chip,die,plane)!=SUCCESS)
	{
		printf("\n\n Error int get_ppn_for_gc().\n");
		return 0xffffffff;
	}

	active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;

	//*********************************************
	/*
	if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_msb<ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_csb){
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_msb+3;
		}
	else if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_csb<ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_lsb){
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_csb+3;
		}
	else{
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_lsb+3;
		}
		*/
	//*********************************************

	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page++;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;

	if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page>ssd->parameter->page_block)
	{
		printf("DDD error! the last write page larger than 64!!\n");
		while(1){}
	}

	block=active_block;
	page=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page;

	if(page%3==0){
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_lsb=page;
		ssd->free_lsb_count--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_lsb_num--;
		//ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;
		ssd->write_lsb_count++;
		ssd->newest_write_lsb_count++;
		}
	else if(page%3==2){
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_msb=page;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_msb_num--;
		//ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;
		ssd->write_msb_count++;
		ssd->free_msb_count--;
		ssd->newest_write_msb_count++;
		}
	else{
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_csb=page;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_csb_num--;
		ssd->write_csb_count++;
		ssd->free_csb_count--;
		ssd->newest_write_csb_count++;
		}

	ppn=find_ppn(ssd,channel,chip,die,plane,block,page);

	ssd->program_count++;
	ssd->channel_head[channel].program_count++;
	ssd->channel_head[channel].chip_head[chip].program_count++;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page--;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].written_count++;
	ssd->write_flash_count++;

	return ppn;

}

/*********************************************************************************************************************
* 朱志明 于2011年7月28日修改
*函数的功能就是erase_operation擦除操作，把channel，chip，die，plane下的block擦除掉
*也就是初始化这个block的相关参数，eg：free_page_num=page_block，invalid_page_num=0，last_write_page=-1，erase_count++
*还有这个block下面的每个page的相关参数也要修改。
*********************************************************************************************************************/

Status erase_operation(struct ssd_info * ssd,unsigned int channel ,unsigned int chip ,unsigned int die ,unsigned int plane ,unsigned int block, int from )
{
	unsigned int flag, i;
	flag = 0;
	for(i=0;i<ssd->parameter->page_block;i++)
	{
		// hdd_flag !=0 表明可以直接擦除
		if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].valid_state != 0 && ssd->dram->map->map_entry[ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].lpn].hdd_flag == 0)
		{
			printf("from: %d\n", from);
			printf("valid_state: %d\n", ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].valid_state);
			printf("hdd_flag: %d lpn: %d\n", ssd->dram->map->map_entry[ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].lpn].hdd_flag, ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].lpn);
			flag = 1;
			break;
		}
	}
	if (flag == 1)
	{
		printf("Erasing a block with valid data: %d, %d, %d, %d, %d.\n", channel, chip, die, plane, block);
		// abort();
		//直接返回，并不会擦除有效数据
		return FAILURE;
	}
	unsigned int origin_free_page_num, origin_free_lsb_num, origin_free_csb_num, origin_free_msb_num;
	origin_free_page_num = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_page_num;
	origin_free_lsb_num = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_lsb_num;
	origin_free_csb_num = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_csb_num;
	origin_free_msb_num = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_msb_num;
//	if(origin_free_lsb_num+origin_free_csb_num+origin_free_msb_num!=origin_free_page_num){
//		printf("WRONG METADATA in BLOCK LEVEL.(erase_operation)\n");
//		}
	//printf("ERASE_OPERATION: %d, %d, %d, %d, %d.\n",channel,chip,die,plane,block);
	//unsigned int i=0;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_page_num=ssd->parameter->page_block;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].invalid_page_num=0;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_page=-1;
	//*****************************************************
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_lsb=-3;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_csb=-2;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_msb=-1;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_lsb_num=(int)(ssd->parameter->page_block/3);
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_csb_num=(int)(ssd->parameter->page_block/3);
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_msb_num=(int)(ssd->parameter->page_block/3);
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].invalid_lsb_num=0;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].fast_erase=FALSE;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].dr_state=DR_STATE_NULL;
	//*****************************************************
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].erase_count++;
	for (i=0;i<ssd->parameter->page_block;i++)
	{
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].free_state=PG_SUB;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].valid_state=0;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].lpn=-1;
	}
	ssd->erase_count++;
	ssd->channel_head[channel].erase_count++;
	ssd->channel_head[channel].chip_head[chip].erase_count++;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page+=ssd->parameter->page_block;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page-=origin_free_page_num;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_lsb_num+=(ssd->parameter->page_block/3);
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_lsb_num-=origin_free_lsb_num;
	ssd->free_lsb_count+=(ssd->parameter->page_block/3);
	ssd->free_csb_count+=(ssd->parameter->page_block/3);
	ssd->free_msb_count+=(ssd->parameter->page_block/3);
	ssd->free_lsb_count-=origin_free_lsb_num;
	ssd->free_csb_count-=origin_free_csb_num;
	ssd->free_msb_count-=origin_free_msb_num;

	return SUCCESS;

}


/**************************************************************************************
*这个函数的功能是处理INTERLEAVE_TWO_PLANE，INTERLEAVE，TWO_PLANE，NORMAL下的擦除的操作。
***************************************************************************************/
Status erase_planes(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die1, unsigned int plane1,unsigned int command)
{
	unsigned int die=0;
	unsigned int plane=0;
	unsigned int block=0;
	struct direct_erase *direct_erase_node=NULL;
	unsigned int block0=0xffffffff;
	unsigned int block1=0;

	if((ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane1].erase_node==NULL)||
		((command!=INTERLEAVE_TWO_PLANE)&&(command!=INTERLEAVE)&&(command!=TWO_PLANE)&&(command!=NORMAL)))     /*如果没有擦除操作，或者command不对，返回错误*/
	{
		return ERROR;
	}

	/************************************************************************************************************
	*处理擦除操作时，首先要传送擦除命令，这时channel，chip处于传送命令的状态，即CHANNEL_TRANSFER，CHIP_ERASE_BUSY
	*下一状态是CHANNEL_IDLE，CHIP_IDLE。
	*************************************************************************************************************/
	block1=ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane1].erase_node->block;

	ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;
	ssd->channel_head[channel].current_time=ssd->current_time;
	ssd->channel_head[channel].next_state=CHANNEL_IDLE;

	ssd->channel_head[channel].chip_head[chip].current_state=CHIP_ERASE_BUSY;
	ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
	ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;

	if(command==INTERLEAVE_TWO_PLANE)                                       /*高级命令INTERLEAVE_TWO_PLANE的处理*/
	{
		for(die=0;die<ssd->parameter->die_chip;die++)
		{
			block0=0xffffffff;
			if(die==die1)
			{
				block0=block1;
			}
			for (plane=0;plane<ssd->parameter->plane_die;plane++)
			{
				direct_erase_node=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node;
				if(direct_erase_node!=NULL)
				{

					block=direct_erase_node->block;

					if(block0==0xffffffff)
					{
						block0=block;
					}
					else
					{
						if(block!=block0)
						{
							continue;
						}

					}
					ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node=direct_erase_node->next_node;
					erase_operation(ssd,channel,chip,die,plane,block,0);     /*真实的擦除操作的处理*/
					free(direct_erase_node);
					direct_erase_node=NULL;
					ssd->direct_erase_count++;
				}

			}
		}

		ssd->interleave_mplane_erase_count++;                             /*发送了一个interleave two plane erase命令,并计算这个处理的时间，以及下一个状态的时间*/
		ssd->channel_head[channel].next_state_predict_time=ssd->current_time+18*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tWB;
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time-9*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tBERS;

	}
	else if(command==INTERLEAVE)                                          /*高级命令INTERLEAVE的处理*/
	{
		for(die=0;die<ssd->parameter->die_chip;die++)
		{
			for (plane=0;plane<ssd->parameter->plane_die;plane++)
			{
				if(die==die1)
				{
					plane=plane1;
				}
				direct_erase_node=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node;
				if(direct_erase_node!=NULL)
				{
					block=direct_erase_node->block;
					ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node=direct_erase_node->next_node;
					erase_operation(ssd,channel,chip,die,plane,block,0);
					free(direct_erase_node);
					direct_erase_node=NULL;
					ssd->direct_erase_count++;
					break;
				}
			}
		}
		ssd->interleave_erase_count++;
		ssd->channel_head[channel].next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC;
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tBERS;
	}
	else if(command==TWO_PLANE)                                          /*高级命令TWO_PLANE的处理*/
	{

		for(plane=0;plane<ssd->parameter->plane_die;plane++)
		{
			direct_erase_node=ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane].erase_node;
			if((direct_erase_node!=NULL))
			{
				block=direct_erase_node->block;
				if(block==block1)
				{
					ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane].erase_node=direct_erase_node->next_node;
					erase_operation(ssd,channel,chip,die1,plane,block,0);
					free(direct_erase_node);
					direct_erase_node=NULL;
					ssd->direct_erase_count++;
				}
			}
		}

		ssd->mplane_erase_conut++;
		ssd->channel_head[channel].next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC;
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tBERS;
	}
	else if(command==NORMAL)                                             /*普通命令NORMAL的处理*/
	{
		direct_erase_node=ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane1].erase_node;
		block=direct_erase_node->block;
		ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane1].erase_node=direct_erase_node->next_node;
		free(direct_erase_node);
		direct_erase_node=NULL;
		// unsigned int flag, i;
		// flag = 0;
		// for (i = 0; i < ssd->parameter->page_block; i++)
		// {
		// 	// hdd_flag !=0 表明可以直接擦除
		// 	if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].valid_state != 0 && ssd->dram->map->map_entry[ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].lpn].hdd_flag == 0)
		// 	{
		// 		printf("erase_planes-gc-valid_state: %d\n", ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].valid_state);
		// 		printf("erase_planes-hdd_flag: %d lpn: %d\n", ssd->dram->map->map_entry[ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].lpn].hdd_flag, ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].lpn);
		// 		flag = 1;
		// 	}
		// }     
		// if (flag == 1)
		// {
		// 	printf("NORMAL\n");
		// 	printf("erase_planes-Erasing a block with valid data: %d, %d, %d, %d, %d.\n", channel, chip, die, plane, block);
		// }
		erase_operation(ssd,channel,chip,die1,plane1,block,2);

		ssd->direct_erase_count++;
		ssd->channel_head[channel].next_state_predict_time=ssd->current_time+5*ssd->parameter->time_characteristics.tWC;
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tWB+ssd->parameter->time_characteristics.tBERS;
	}
	else
	{
		return ERROR;
	}

	direct_erase_node=ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane1].erase_node;

	if(((direct_erase_node)!=NULL)&&(direct_erase_node->block==block1))
	{
		return FAILURE;
	}
	else
	{
		return SUCCESS;
	}
}

Status fast_erase_planes(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die1, unsigned int plane1,unsigned int command)
{
	unsigned int die=0;
	unsigned int plane=0;
	unsigned int block=0;
	struct direct_erase *direct_erase_node=NULL;
	unsigned int block0=0xffffffff;
	unsigned int block1=-1;
	/*
	if((ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane1].fast_erase_node==NULL)|| (command!=NORMAL))
	{
		return ERROR;
	}
	*/
	/************************************************************************************************************
	*处理擦除操作时，首先要传送擦除命令，这是channel，chip处于传送命令的状态，即CHANNEL_TRANSFER，CHIP_ERASE_BUSY
	*下一状态是CHANNEL_IDLE，CHIP_IDLE。
	*************************************************************************************************************/
	struct plane_info candidate_plane;
	candidate_plane = ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane1];
	while(block < ssd->parameter->block_plane)
		{
		if((candidate_plane.blk_head[block].free_msb_num==ssd->parameter->page_block/2)&&(candidate_plane.blk_head[block].invalid_lsb_num==ssd->parameter->page_block/2)&&(candidate_plane.blk_head[block].fast_erase!=TRUE))
			{
			block1 = block;
			break;
			}
		block++;
		}
	//block1=ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane1].fast_erase_node->block;
	if(block1==-1){
		return FAILURE;
		}
	ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;
	ssd->channel_head[channel].current_time=ssd->current_time;
	ssd->channel_head[channel].next_state=CHANNEL_IDLE;

	ssd->channel_head[channel].chip_head[chip].current_state=CHIP_ERASE_BUSY;
	ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
	ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;


	if(command==NORMAL)                                             /*普通命令NORMAL的处理*/
	{
		//direct_erase_node=ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane1].fast_erase_node;
		//block=direct_erase_node->block;
		//ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane1].fast_erase_node=direct_erase_node->next_node;
		//free(direct_erase_node);
		//direct_erase_node=NULL;
		candidate_plane.blk_head[block1].fast_erase = TRUE;
		erase_operation(ssd,channel,chip,die1,plane1,block1,0);

		ssd->fast_gc_count++;
		ssd->channel_head[channel].next_state_predict_time=ssd->current_time+5*ssd->parameter->time_characteristics.tWC;
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tWB+ssd->parameter->time_characteristics.tBERS;
		return SUCCESS;
	}
	else
	{
		printf("Wrong Commend for fast gc.\n");
		return ERROR;
	}
	/*
	direct_erase_node=ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane1].fast_erase_node;

	if(((direct_erase_node)!=NULL)&&(direct_erase_node->block==block1))
	{
		return FAILURE;
	}
	else
	{
		return SUCCESS;
	}
	*/
}


/*******************************************************************************************************************
*GC操作由某个plane的free块少于阈值进行触发，当某个plane被触发时，GC操作占据这个plane所在的die，因为die是一个独立单元。
*对一个die的GC操作，尽量做到四个plane同时erase，利用interleave erase操作。GC操作应该做到可以随时停止（移动数据和擦除
*时不行，但是间隙时间可以停止GC操作），以服务新到达的请求，当请求服务完后，利用请求间隙时间，继续GC操作。可以设置两个
*GC阈值，一个软阈值，一个硬阈值。软阈值表示到达该阈值后，可以开始主动的GC操作，利用间歇时间，GC可以被新到的请求中断；
*当到达硬阈值后，强制性执行GC操作，且此GC操作不能被中断，直到回到硬阈值以上。
*在这个函数里面，找出这个die所有的plane中，有没有可以直接删除的block，要是有的话，利用interleave two plane命令，删除
*这些block，否则有多少plane有这种直接删除的block就同时删除，不行的话，最差就是单独这个plane进行删除，连这也不满足的话，
*直接跳出，到gc_parallelism函数进行进一步GC操作。该函数寻找全部为invalid的块，直接删除，找到可直接删除的返回1，没有找
*到返回-1。
*********************************************************************************************************************/
int gc_direct_erase(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane)
{
	unsigned int lv_die=0,lv_plane=0;                                                           /*为避免重名而使用的局部变量 Local variables*/
	unsigned int interleaver_flag=FALSE,muilt_plane_flag=FALSE;
	unsigned int normal_erase_flag=TRUE;

	struct direct_erase * direct_erase_node1=NULL;
	struct direct_erase * direct_erase_node2=NULL;

	direct_erase_node1=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node;//单位：block
	if (direct_erase_node1==NULL)
	{
		return FAILURE;
	}

	/********************************************************************************************************
	*当能处理TWOPLANE高级命令时，就在相应的channel，chip，die中两个不同的plane找到可以执行TWOPLANE操作的block
	*并置muilt_plane_flag为TRUE
	*********************************************************************************************************/
	if((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE)
	{
		for(lv_plane=0;lv_plane<ssd->parameter->plane_die;lv_plane++)
		{
			direct_erase_node2=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node;
			if((lv_plane!=plane)&&(direct_erase_node2!=NULL))
			{
				if((direct_erase_node1->block)==(direct_erase_node2->block))
				{
					muilt_plane_flag=TRUE;
					break;
				}
			}
		}
	}

	/***************************************************************************************
	*当能处理INTERLEAVE高级命令时，就在相应的channel，chip找到可以执行INTERLEAVE的两个block
	*并置interleaver_flag为TRUE
	****************************************************************************************/
	if((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE)
	{
		for(lv_die=0;lv_die<ssd->parameter->die_chip;lv_die++)
		{
			if(lv_die!=die)
			{
				for(lv_plane=0;lv_plane<ssd->parameter->plane_die;lv_plane++)
				{
					if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node!=NULL)
					{
						interleaver_flag=TRUE;
						break;
					}
				}
			}
			if(interleaver_flag==TRUE)
			{
				break;
			}
		}
	}

	/************************************************************************************************************************
	*A，如果既可以执行twoplane的两个block又有可以执行interleaver的两个block，那么就执行INTERLEAVE_TWO_PLANE的高级命令擦除操作
	*B，如果只有能执行interleaver的两个block，那么就执行INTERLEAVE高级命令的擦除操作
	*C，如果只有能执行TWO_PLANE的两个block，那么就执行TWO_PLANE高级命令的擦除操作
	*D，没有上述这些情况，那么就只能够执行普通的擦除操作了
	*************************************************************************************************************************/
	if ((muilt_plane_flag==TRUE)&&(interleaver_flag==TRUE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE)&&((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE))
	{
		if(erase_planes(ssd,channel,chip,die,plane,INTERLEAVE_TWO_PLANE)==SUCCESS)
		{
			return SUCCESS;
		}
	}
	else if ((interleaver_flag==TRUE)&&((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE))
	{
		if(erase_planes(ssd,channel,chip,die,plane,INTERLEAVE)==SUCCESS)
		{
			return SUCCESS;
		}
	}
	else if ((muilt_plane_flag==TRUE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE))
	{
		if(erase_planes(ssd,channel,chip,die,plane,TWO_PLANE)==SUCCESS)
		{
			return SUCCESS;
		}
	}

	if ((normal_erase_flag==TRUE))                              /*不是每个plane都有可以直接删除的block，只对当前plane进行普通的erase操作，或者只能执行普通命令*/
	{
		if (erase_planes(ssd,channel,chip,die,plane,NORMAL)==SUCCESS)
		{
			return SUCCESS;
		}
		else
		{
			return FAILURE;                                     /*目标的plane没有可以直接删除的block，需要寻找目标擦除块后在实施擦除操作*/
		}
	}
	return SUCCESS;
}

int gc_direct_fast_erase(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane)
{
	unsigned int lv_die=0,lv_plane=0;                                                           /*为避免重名而使用的局部变量 Local variables*/
	unsigned int interleaver_flag=FALSE,muilt_plane_flag=FALSE;
	unsigned int normal_erase_flag=TRUE;

	struct direct_erase * direct_erase_node1=NULL;
	struct direct_erase * direct_erase_node2=NULL;

	/*
	direct_erase_node1=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].fast_erase_node;
	if (direct_erase_node1==NULL)
	{
		return FAILURE;
	}
    */
	/********************************************************************************************************
	*当能处理TWOPLANE高级命令时，就在相应的channel，chip，die中两个不同的plane找到可以执行TWOPLANE操作的block
	*并置muilt_plane_flag为TRUE
	*********************************************************************************************************/
	if((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE)
	{
		printf("Advanced commands are NOT supported.\n");
		return FAILURE;
	}

	/***************************************************************************************
	*当能处理INTERLEAVE高级命令时，就在相应的channel，chip找到可以执行INTERLEAVE的两个block
	*并置interleaver_flag为TRUE
	****************************************************************************************/
	if((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE)
	{
		printf("Advanced commands are NOT supported.\n");
		return FAILURE;
	}

	/************************************************************************************************************************
	*A，如果既可以执行twoplane的两个block又有可以执行interleaver的两个block，那么就执行INTERLEAVE_TWO_PLANE的高级命令擦除操作
	*B，如果只有能执行interleaver的两个block，那么就执行INTERLEAVE高级命令的擦除操作
	*C，如果只有能执行TWO_PLANE的两个block，那么就执行TWO_PLANE高级命令的擦除操作
	*D，没有上述这些情况，那么就只能够执行普通的擦除操作了
	*************************************************************************************************************************/
	/*
	if ((muilt_plane_flag==TRUE)&&(interleaver_flag==TRUE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE)&&((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE))
	{
		if(erase_planes(ssd,channel,chip,die,plane,INTERLEAVE_TWO_PLANE)==SUCCESS)
		{
			return SUCCESS;
		}
	}
	else if ((interleaver_flag==TRUE)&&((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE))
	{
		if(erase_planes(ssd,channel,chip,die,plane,INTERLEAVE)==SUCCESS)
		{
			return SUCCESS;
		}
	}
	else if ((muilt_plane_flag==TRUE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE))
	{
		if(erase_planes(ssd,channel,chip,die,plane,TWO_PLANE)==SUCCESS)
		{
			return SUCCESS;
		}
	}
	*/

	if ((normal_erase_flag==TRUE))                              /*不是每个plane都有可以直接删除的block，只对当前plane进行普通的erase操作，或者只能执行普通命令*/
	{
		if (fast_erase_planes(ssd,channel,chip,die,plane,NORMAL)==SUCCESS)
		{
			return SUCCESS;
		}
		else
		{
			return FAILURE;                                     /*目标的plane没有可以直接删除的block，需要寻找目标擦除块后在实施擦除操作*/
		}
	}
	else{
		printf("Wrong command for fast gc.\n");
		return FAILURE;
		}
	//return SUCCESS;
}

Status move_page(struct ssd_info * ssd, struct local *location, unsigned int * transfer_size)
{
	struct local *new_location=NULL;
	unsigned int free_state=0,valid_state=0;
	unsigned int lpn=0,old_ppn=0,ppn=0;
	//printf("state of this block: %d\n",ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].fast_erase);

	lpn=ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn;
	valid_state=ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state;
	free_state=ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state;
	old_ppn=find_ppn(ssd,location->channel,location->chip,location->die,location->plane,location->block,location->page);      /*记录这个有效移动页的ppn，对比map或者额外映射关系中的ppn，进行删除和添加操作*/
	ppn=get_ppn_for_gc(ssd,location->channel,location->chip,location->die,location->plane);                /*找出来的ppn一定是在发生gc操作的plane中,才能使用copyback操作，为gc操作获取ppn*/

	new_location=find_location(ssd,ppn);                                                                   /*根据新获得的ppn获取new_location*/
	// printf("MOVE PAGE, lpn:%d, old_ppn:%d, new_ppn:%d, FROM %d,%d,%d,%d,%d,%d TO %d,%d,%d,%d,%d,%d.\n",lpn,old_ppn,ppn,location->channel,location->chip,location->die,location->plane,location->block,location->page,new_location->channel,new_location->chip,new_location->die,new_location->plane,new_location->block,new_location->page);
	
	if(new_location->channel==location->channel && new_location->chip==location->chip && new_location->die==location->die && new_location->plane==location->plane && new_location->block==location->block){
		printf("MOVE PAGE WRONG!!! Page is moved to the same block.\n");
		return FAILURE;
		}
	
	if(new_location->block == location->block){
		printf("Data is moved to the same block!!!!!!\n");
		return FAILURE;
		}
	if ((ssd->parameter->advanced_commands&AD_COPYBACK)==AD_COPYBACK)
	{
		printf("Wrong COMMANDS.\n");
		return FAILURE;
		if (ssd->parameter->greed_CB_ad==1)                                                                /*贪婪地使用高级命令*/
		{
			ssd->copy_back_count++;
			ssd->gc_copy_back++;
			while (old_ppn%2!=ppn%2)
			{
				ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].free_state=0;
				ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].lpn=0;
				ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].valid_state=0;
				ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].invalid_page_num++;

				free(new_location);
				new_location=NULL;

				ppn=get_ppn_for_gc(ssd,location->channel,location->chip,location->die,location->plane);    /*找出来的ppn一定是在发生gc操作的plane中，并且满足奇偶地址限制,才能使用copyback操作*/
				ssd->program_count--;
				ssd->write_flash_count--;
				ssd->waste_page_count++;
			}
			if(new_location==NULL)
			{
				new_location=find_location(ssd,ppn);
			}

			ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].free_state=free_state;
			ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].lpn=lpn;
			ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].valid_state=valid_state;
		}
		else
		{
			if (old_ppn%2!=ppn%2)
			{
				(* transfer_size)+=size(valid_state);
			}
			else
			{

				ssd->copy_back_count++;
				ssd->gc_copy_back++;
			}
		}
	}
	else
	{
		(* transfer_size)+=size(valid_state);
	}
	ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].free_state=free_state;
	ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].lpn=lpn;
	ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].valid_state=valid_state;


	ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state=0;
	ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn=0;
	ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state=0;
	ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num++;
	if((location->page)%3==0){
		ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_lsb_num++;
		}

	if (old_ppn==ssd->dram->map->map_entry[lpn].pn)                                                     /*修改映射表*/
	{
		ssd->dram->map->map_entry[lpn].pn=ppn;
	}
	//如果不被更新写，下一次不做GC
	// printf("move-page-lpn:%d\n", lpn);
	ssd->dram->map->map_entry[lpn].hdd_flag = 2;
	ssd->moved_page_count++;

	free(new_location);
	new_location=NULL;

	return SUCCESS;
}

Status adjust_page_hdd(struct ssd_info * ssd, struct local *location, unsigned int * transfer_size)
{
	unsigned int lpn=0, page_num=0,valid_state=0;
	lpn=ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn;
	valid_state=ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state;

	ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state=0;
	ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn=0;
	ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state=0;
	ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num++;

    // (* transfer_size)+=size(valid_state);

	//修改map信息，设hdd_flag置为hdd标识
	ssd->dram->map->map_entry[lpn].hdd_flag=1;

	ssd->dram->map->map_entry[lpn].pn=0;

	ssd->dram->map->map_entry[lpn].state=0;
	// printf("adjust-page-lpn:%d %d\n", location->page, lpn);

	return SUCCESS;
}

/**
 * @brief 查找的连续page对应block置为无效
 * 
 * @param ssd 
 * @param location 
 * @param transfer_size 
 * @return Status 
 */
Status sequential_page_invalid(struct ssd_info * ssd, struct local *location, unsigned int * transfer_size)
{
	unsigned int lpn=0, page_num=0,valid_state=0;
	lpn=ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn;

	// ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state=0;
	// ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn=0;
	// 这里应该注释掉吧，其实是有效的页，可以正常访问
	// ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state=0;
	// ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num++;
	if (lpn != -1)
	{
		ssd->dram->map->map_entry[lpn].hdd_flag=2;
	}
	// ssd->dram->map->map_entry[lpn].pn=0;
	// ssd->dram->map->map_entry[lpn].state=0;

	return SUCCESS;
}

/**
 * @brief 排序
 * 
 * @param a 
 * @param m 
 */
void sort(int a[], int m)
{
    int i = 0;
    int j = 0;
    int tmp = 0;
    for (i = 0; i<m - 1; i++)
    {
        for (j = 0; j < m - i - 1; j++)
        {
            if (a[j]>a[j + 1])
            {
                tmp = a[j + 1];
                a[j + 1] = a[j];
                a[j] = tmp;
            }
        }
    }
}

/**
 * @brief Get the page_i by lpn 
 * 
 * @param ssd 
 * @param channel 
 * @param chip 
 * @param die 
 * @param plane 
 * @param block 
 * @param search_lpn 
 * @return int 
 */
int get_page_i_by_lpn(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane, int block, int search_lpn)
{
	int lpn_flag = 0, r_i;
	for (r_i = 0; r_i < ssd->parameter->page_block; r_i++)
	{
		if (search_lpn == ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[r_i].lpn)
		{
			lpn_flag = 1;
			break;
		}
	}
	if (lpn_flag == 0)
	{
		printf("r_i:%d lpn:%d\n", -1, search_lpn);
		abort();
		r_i = -1;
	}
	return r_i;
}

/**
 * @brief make channel parallel to serial for HDD
 * 
 * @param ssd 
 * @param channel 
 * @return double 
 */
double parallel_to_serial(struct ssd_info *ssd,unsigned int channel)
{
	int i;
	unsigned long serial_diff = 0, temp = 0;
	printf("ssd->current_time:%lld channel_current_time:%lld channel_next_state_predict_time%lld channel:%d\n", ssd->current_time, ssd->channel_head[channel].current_time, ssd->channel_head[channel].next_state_predict_time, channel);
	for (i = 0; i < ssd->parameter->channel_number; i++)
	{
		if (i != channel)
		{
			printf("ssd->current_time:%lld channel_current_time:%lld channel_next_state_predict_time%lld i:%d channel:%d\n", ssd->current_time, ssd->channel_head[i].current_time, ssd->channel_head[i].next_state_predict_time, i, channel);
			if (ssd->channel_head[i].current_time <= ssd->channel_head[channel].current_time && ssd->channel_head[channel].current_time < ssd->channel_head[i].next_state_predict_time && ssd->channel_head[channel].next_state_predict_time >= ssd->channel_head[i].next_state_predict_time)
			{
				temp = ssd->channel_head[i].next_state_predict_time - ssd->channel_head[channel].current_time;
				if (serial_diff < temp)
				{
					serial_diff = temp;
				}
				printf("serial_diff:%lld\n", serial_diff);
				abort();
			}
		}
	}
	printf("serial_diff2:%lld\n", serial_diff);
	if (serial_diff != 0)
	{
		abort();
	}
	return serial_diff;
}

/**
 * @brief Get the block
 * 
 * @param ssd 
 * @param channel 
 * @param chip 
 * @param die 
 * @param plane 
 * @return int 
 */
int get_block(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane)
{
	unsigned int i = 0, j = 0, invalid_page = 0, lpn;
	unsigned int block, active_block; /*记录失效页最多的块号*/

	if (find_active_block(ssd, channel, chip, die, plane) != SUCCESS) /*获取活跃块*/
	{
		printf("\n\n Error in uninterrupt_gc().\n");
		return ERROR;
	}
	active_block = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;
	// printf("active-block %d\n", active_block);
	invalid_page = 0;
	block = -1;
	if (ssd->is_sequential != 1)
	{
		for (i = 0; i < ssd->parameter->block_plane; i++) /*查找最多invalid_page的块号，以及最大的invalid_page_num*/
		{
			if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].free_page_num > 0)
			{
				continue;
			}
			if ((active_block != i) && (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].invalid_page_num > invalid_page))
			{
				invalid_page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].invalid_page_num;
				block = i;
			}
		}
		return block;
	}

	//按照不同Page的属性计算每个Block的实际无效页比例系数t，当需要GC时，选择值最大的Block执行GC。
	int page1, page2, page3, page4;
	// 第一类是无效的Page，GC时不产生Page move，直接擦除增加SSD的有效空间；
	// 第二类是之前已经打包写入HDD中的Page，并且该类Page还并未更新（包括Page_gc,Page_nongc），此时可以将其直接擦除，释放有效空间；
	// 第三类是普通Page，即在GC时直接写入到HDD中，将SSD中的Page擦除，增加SSD的有效空间；
	// 最后一类是冷写热读的Page，GC时通过Page move保留在SSD中，GC完成后也不增加SSD的有效空间。
	float t = 0;
	for (i = 0; i < ssd->parameter->block_plane; i++)
	{
		float t_i = 0;
		if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].free_page_num > 0)
		{
			continue;
		}
		if ((active_block != i))
		{
			page1 = page2 = page3 = page4 = 0;
			for (j = 0; j < ssd->parameter->page_block; j++)
			{
				lpn = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].page_head[j].lpn;

				if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].page_head[j].valid_state <= 0)
				{
					page1++;
				}
				else if (ssd->dram->map->map_entry[lpn].hdd_flag != 0)
				{
					// printf("lpn:%d hdd_flag %d\n", lpn, ssd->dram->map->map_entry[lpn].hdd_flag);
					page2++;
				}
				else
				{
					struct read_hot *r_hot_q = ssd->read_hot_head;
					struct write_hot *w_hot_q = ssd->write_hot_head;
					int hot_r = 0;
					int hot_w = 0;
					//查找热读
					while (r_hot_q)
					{
						if (r_hot_q->lpn == lpn)
						{
							// printf("hot_read lpn %d\n", lpn);
							hot_r = 1;
							break;
						}
						r_hot_q = r_hot_q->next;
					}
					//查找热写
					while (w_hot_q)
					{
						if (w_hot_q->lpn == lpn)
						{
							// printf("hot_write lpn %d\n", lpn);
							hot_w = 1;
							break;
						}
						w_hot_q = w_hot_q->next;
					}
					if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].page_head[j].valid_state > 0 && ssd->dram->map->map_entry[lpn].hdd_flag == 0) /*该页是有效页，需要copyback操作*/
					{
						if (hot_r == 0)
						{
							page3++;
						}
					}
					if (hot_r == 1 && hot_w == 0)
					{
						page4++;
					}
				}
			}
			// printf("%d %d %d %d\n", page1, page2, page3, page4);
			t_i = ((float)page1 + (float)page2 * 0.5 + (float)page3 * 0.25) / (float)(page1 + page2 + page3 + page4);
			if (t < t_i)
			{
				t = t_i;
				block = i;
			}
		}
	}
	if (t < 1)
	{
		// printf("block %d t %f\n", block, t);
	}
	else
	{
		int t_b = block;
		// t = 1
		for (i = 0; i < ssd->parameter->block_plane; i++) /*查找最多invalid_page的块号，以及最大的invalid_page_num*/
		{
			if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].free_page_num > 0)
			{
				continue;
			}
			if ((active_block != i) && (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].invalid_page_num > invalid_page))
			{
				invalid_page = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].invalid_page_num;
				block = i;
			}
		}
		if (t_b != block)
		{
			printf("t %f t-b %d b %d\n", t, t_b, block);
			// abort();
		}
	}

	return block;
}

/*******************************************************************************************************************************************
*目标的plane没有可以直接删除的block，需要寻找目标擦除块后在实施擦除操作，用在不能中断的gc操作中，成功删除一个块，返回1，没有删除一个块返回-1
*在这个函数中，不用考虑目标channel,die是否是空闲的,擦除invalid_page_num最多的block
********************************************************************************************************************************************/
int uninterrupt_gc(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane)
{
	unsigned int i=0, j=0, invalid_page=0, lpn;
	unsigned int block,active_block,transfer_size,free_page,page_move_count=0;                           /*记录失效页最多的块号*/
	struct local *  location=NULL;
	unsigned int total_invalid_page_num=0;

	transfer_size=0;
	block=-1;
	block = get_block(ssd, channel, chip, die, plane);
	if (block==-1)
	{
		return 1;
	}
	
	ssd->gc_count++;

	//if(invalid_page<5)
	//{
		//printf("\ntoo less invalid page. \t %d\t %d\t%d\t%d\t%d\t%d\t\n",invalid_page,channel,chip,die,plane,block);
	//}
	//***********************************************
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].fast_erase = TRUE;
	//***********************************************
	free_page=0;
	unsigned int times = 0, write_hdd_time = 0, large_lsn = 0, max_lpn = 0;
	large_lsn=(int)((ssd->parameter->subpage_page*ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->plane_die*ssd->parameter->die_chip*ssd->parameter->chip_num)*(1-ssd->parameter->overprovide));
	max_lpn = large_lsn / ssd->parameter->subpage_page;
	// printf("gc-block: %d %d %d %d %d\n", channel, chip, die, plane, block);
	struct local *location_check = NULL;
	if (ssd->is_sequential == 1)
	{
		int arr[1024], l = 0;
		for (i = 0; i < ssd->parameter->page_block; i++) /*逐个检查每个block 中的page，如果有有效数据的page需要移动到其他地方存储*/
		{
			if ((ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].free_state & PG_SUB) == 0x0000000f)
			{
				free_page++;
			}
			if (free_page != 0)
			{
				// printf("\ntoo much free page. \t %d\t .%d\t%d\t%d\t%d\t\n",free_page,channel,chip,die,plane);
			}
			int lpn = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].lpn;
			if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].valid_state > 0 && ssd->dram->map->map_entry[lpn].hdd_flag == 0) /*该页是有效页，需要copyback操作*/
			{
				// printf("i: %d lpn:%d hdd_flag:%d\n", i, lpn, ssd->dram->map->map_entry[lpn].hdd_flag);
				arr[l] = lpn;
				l++;
			}
		}
		//先将block中有效lpn取出、排序
		sort(arr, l);
		// int i = 0;
		// for (i = 0; i < l; i++)
		// {
		// 	printf("arr: %d %d\n", arr[i], l);
		// }
		// 最大lpn
		max_lpn = ssd->parameter->page_block * ssd->parameter->block_plane * ssd->parameter->plane_die * ssd->parameter->die_chip * ssd->parameter->chip_num;
		int seq_num = 0;
		int is_sequential = 0;
		int random_num = 0;
		int page_i = 0;
		//给每个move_page查找顺序块
		for (i = 0; i < l; i++)
		{
			//防止1无效，但2有效的情况 或 已经被打包走了
			if (ssd->dram->map->map_entry[arr[i]].state == 0 || ssd->dram->map->map_entry[arr[i]].hdd_flag != 0)
			{
				// printf("lpn:%d hdd_flag:%d state:%d\n", arr[i], ssd->dram->map->map_entry[arr[i]].hdd_flag, ssd->dram->map->map_entry[arr[i]].state);
				// abort();
				continue;
			}
			int temp = arr[i];
			seq_num = 0;
			int j = 0, is_seq = 0;
			// 热读 热写
			struct read_hot *r_hot_q;
			struct write_hot *w_hot_q;
			int hot_r = 0;
			int hot_w = 0;
			for (j = arr[i] + 1; j <= max_lpn && l > 1; j++)
			{
				//有可能会很多连续的，所以加个判断限制
				if (seq_num >= (ssd->seq_num - 1))
				{
					break;
				}
				//有效且非hdd
				if (ssd->dram->map->map_entry[j].state != 0 && ssd->dram->map->map_entry[j].hdd_flag == 0 && j - temp == 1)
				{
					if (random_num != 0)
					{
						char *avg = exec_disksim_syssim(random_num, 0, 0);
						// char *avg = (char)5000000; // 由于每次都是重头开始算，这样和random比较不划算。到时候统计一下该块有多少连续lpn个数，加到总时间上。
						write_hdd_time += (int)avg * random_num;
						random_num = 0;
					}
					//查找的page在同一通道
					location_check = find_location(ssd, ssd->dram->map->map_entry[j].pn);
					if (location_check->chip != chip && location_check->channel != channel)
					{
						break;
					}
					// printf("j:%d\n", j);
					is_seq = 1;
					r_hot_q = ssd->read_hot_head;
					w_hot_q = ssd->write_hot_head;
					//查找热读
					while (r_hot_q)
					{
						if (r_hot_q->lpn == j)
						{
							hot_r = 1;
							break;
						}
						r_hot_q = r_hot_q->next;
					}
					//查找热写
					while (w_hot_q)
					{
						if (w_hot_q->lpn == j)
						{
							hot_w = 1;
							break;
						}
						w_hot_q = w_hot_q->next;
					}
					//查找的page是否是当前块
					//机制1：针对第一类Page，执行GC时只将存在于热读列表中同时不在热写列表中的数据保留在SSD中，而将其他数据写入到HDD中。
					if (location_check->channel == channel && location_check->chip == chip && location_check->die == die && location_check->plane == plane && location_check->block == block)
					{
						location = (struct local *)malloc(sizeof(struct local));
						alloc_assert(location, "location");
						memset(location, 0, sizeof(struct local));
						location->channel = channel;
						location->chip = chip;
						location->die = die;
						location->plane = plane;
						location->block = block;
						location->page = location_check->page;
						if (hot_r == 1 && hot_w == 0)
						{
							printf("rule1 %d\n", j);
							move_page(ssd, location, &transfer_size); /*真实的move_page操作*/
							page_move_count++;
						}
						else
						{
							adjust_page_hdd(ssd, location, &transfer_size);
						}
						free(location);
						location = NULL;
						free(location_check);
						location_check = NULL;
					}
					else
					{
						// 机制2：针对第二类Page，执行GC时只将能够构成顺序写的数据同时不在热写列表中的数据写入到HDD中，而将其他数据保留在SSD中。
						if (hot_w == 0)
						{
							// printf("rule2 %d\n", j);
							location = find_location(ssd, ssd->dram->map->map_entry[j].pn);
							// hdd_flag=2表示连续块，下次读从SSD读取，表示热数据
							//  ssd->dram->map->map_entry[j].hdd_flag = 2;
							sequential_page_invalid(ssd, location, &transfer_size);
						}
					}
					temp = j;
					//热数据也要算到打包数量块里面
					times++;
					seq_num++;
				}
				else
				{
					break;
				}
			}
			// 被查找的lpn:arr[i]
			r_hot_q = ssd->read_hot_head;
			w_hot_q = ssd->write_hot_head;
			//查找热读
			while (r_hot_q)
			{
				if (r_hot_q->lpn == arr[i])
				{
					hot_r = 1;
					break;
				}
				r_hot_q = r_hot_q->next;
			}
			//查找热写
			while (w_hot_q)
			{
				if (w_hot_q->lpn == arr[i])
				{
					hot_w = 1;
					break;
				}
				w_hot_q = w_hot_q->next;
			}
			page_i = get_page_i_by_lpn(ssd, channel, chip, die, plane, block, arr[i]);
			location = (struct local *)malloc(sizeof(struct local));
			alloc_assert(location, "location");
			memset(location, 0, sizeof(struct local));
			location->channel = channel;
			location->chip = chip;
			location->die = die;
			location->plane = plane;
			location->block = block;
			location->page = page_i;
			if (hot_r == 1 && hot_w == 0)
			{
				printf("rule1-0 %d\n", j);
				move_page(ssd, location, &transfer_size); /*真实的move_page操作*/
				page_move_count++;
				
			}
			else
			{
				adjust_page_hdd(ssd, location, &transfer_size);
			}
			free(location);
			location = NULL;
			free(location_check);
			location_check = NULL;
			if (is_seq)
			{
				times++; //被查找的page
			}
			else
			{
				random_num++;
			}
			if (times != 0)
			{
				if (times == 1)
				{
					printf("times:%d\n", times);
					abort();
				}
				char *avg = exec_disksim_syssim(times, 0, 1); //顺序写times次
				write_hdd_time += (int)avg * times;
				if (write_hdd_time < 0)
				{
					printf("write_hdd_time:%d\n", write_hdd_time);
					abort();
				}
			}
			times = 0;
		}
	}
	//related work
	else if (ssd->is_sequential == 2)
	{
		int arr[1024], l = 0;
		for (i = 0; i < ssd->parameter->page_block; i++) /*逐个检查每个block 中的page，如果有有效数据的page需要移动到其他地方存储*/
		{
			if ((ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].free_state & PG_SUB) == 0x0000000f)
			{
				free_page++;
			}
			if (free_page != 0)
			{
				// printf("\ntoo much free page. \t %d\t .%d\t%d\t%d\t%d\t\n",free_page,channel,chip,die,plane);
			}
			int lpn = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].lpn;
			if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].valid_state > 0 && ssd->dram->map->map_entry[lpn].hdd_flag == 0) /*该页是有效页，需要copyback操作*/
			{
				// printf("i: %d lpn:%d hdd_flag:%d\n", i, lpn, ssd->dram->map->map_entry[lpn].hdd_flag);
				arr[l] = lpn;
				l++;
			}
		}
		//先将block中有效lpn取出、排序
		sort(arr, l);
		// int i = 0;
		// for (i = 0; i < l; i++)
		// {
		// 	printf("arr: %d %d\n", arr[i], l);
		// }
		// int max_lpn = ssd->parameter->page_block * ssd->parameter->block_plane * ssd->parameter->plane_die * ssd->parameter->die_chip * ssd->parameter->chip_num;
		int seq_num = 0;
		int is_sequential = 0;
		int random_num = 0;
		//给每个move_page查找顺序块
		for (i = 0; i < l; i++)
		{
			//防止1无效，但2有效的情况 或 已经被打包走了
			if (ssd->dram->map->map_entry[arr[i]].state == 0 || ssd->dram->map->map_entry[arr[i]].hdd_flag != 0)
			{
				// printf("lpn:%d hdd_flag:%d state:%d\n", arr[i], ssd->dram->map->map_entry[arr[i]].hdd_flag, ssd->dram->map->map_entry[arr[i]].state);
				// abort();
				continue;
			}
			int temp = arr[i];
			seq_num = 0;
			int j = 0, is_seq = 0;
			for (j = arr[i] + 1; j <= max_lpn && l > 1; j++)
			{
				//有可能会很多连续的，所以加个判断限制
				if (seq_num >= (ssd->seq_num - 1))
				{
					break;
				}
				//有效且非hdd
				if (ssd->dram->map->map_entry[j].state != 0 && ssd->dram->map->map_entry[j].hdd_flag == 0 && j - temp == 1)
				{
					if (random_num != 0)
					{
						char *avg = exec_disksim_syssim(random_num, 0, 0);
						// char *avg = (char)5000000; // 由于每次都是重头开始算，这样和random比较不划算。到时候统计一下该块有多少连续lpn个数，加到总时间上。
						write_hdd_time += (int)avg * random_num;
						random_num = 0;
					}
					//查找的page在同一通道
					location_check = find_location(ssd, ssd->dram->map->map_entry[j].pn);
					if (location_check->chip != chip && location_check->channel != channel)
					{
						break;
					}
					is_seq = 1;
					if (location_check->channel == channel && location_check->chip == chip && location_check->die == die && location_check->plane == plane && location_check->block == block)
					{
						location = (struct local *)malloc(sizeof(struct local));
						alloc_assert(location, "location");
						memset(location, 0, sizeof(struct local));
						location->channel = channel;
						location->chip = chip;
						location->die = die;
						location->plane = plane;
						location->block = block;
						location->page = location_check->page;
						adjust_page_hdd(ssd, location, &transfer_size);
						free(location);
						location = NULL;
						free(location_check);
						location_check = NULL;
					}
					else
					{
						location = find_location(ssd, ssd->dram->map->map_entry[j].pn);
						sequential_page_invalid(ssd, location, &transfer_size);
					}
					temp = j;
					//热数据也要算到打包数量块里面
					times++;
					seq_num++;
				}
				else
				{
					break;
				}
			}
			// 被查找的lpn:arr[i]
			struct read_hot *hot = ssd->read_hot_head;
			int hot_flag = 0;
			location = (struct local *)malloc(sizeof(struct local));
			alloc_assert(location, "location");
			memset(location, 0, sizeof(struct local));
			location->channel = channel;
			location->chip = chip;
			location->die = die;
			location->plane = plane;
			location->block = block;
			int page_i;
			page_i = get_page_i_by_lpn(ssd, channel, chip, die, plane, block, arr[i]);
			location->page = page_i;
			adjust_page_hdd(ssd, location, &transfer_size);
			free(location);
			location = NULL;
			if (is_seq)
			{
				times++; //被查找的page
			}
			else
			{
				random_num++;
			}
			if (times != 0)
			{
				if (times == 1)
				{
					printf("times:%d\n", times);
					abort();
				}
				char *avg = exec_disksim_syssim(times, 0, 1); //顺序写times次
				write_hdd_time += (int)avg * times;
				if (write_hdd_time < 0)
				{
					printf("write_hdd_time:%d\n", write_hdd_time);
					abort();
				}
			}
			times = 0;
		}
	}
	else
	{	
		int arr_r[512], l_r = 0;
		for (i = 0; i < ssd->parameter->page_block; i++) /*逐个检查每个block 中的page，如果有有效数据的page需要移动到其他地方存储*/
		{
			int lpn = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].lpn;
			if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].valid_state > 0 && ssd->dram->map->map_entry[lpn].hdd_flag == 0) /*该页是有效页*/
			{
				arr_r[l_r] = lpn;
				l_r++;
			}
		}
		//将lpn排序
		sort(arr_r, l_r);
		// int j_i;
		// for (j_i = 0; j_i < l_r; j_i++)
		// {
		// 	printf("%d %d\n", arr_r[j_i], l_r);
		// }
		//只有一个有效页
		if (l_r == 1)
		{
			int page_i;
			page_i = get_page_i_by_lpn(ssd, channel, chip, die, plane, block, arr_r[0]);
			
			location = (struct local *)malloc(sizeof(struct local));
			alloc_assert(location, "location");
			memset(location, 0, sizeof(struct local));
			location->channel = channel;
			location->chip = chip;
			location->die = die;
			location->plane = plane;
			location->block = block;
			location->page = page_i;
			adjust_page_hdd(ssd, location, &transfer_size);
			char *avg = exec_disksim_syssim(1, 0, 0);
			write_hdd_time += (int)avg * 1;
		}
		//块中有多个有效页
		int random_seq_num = 0;
		int random_num = 0; //非连续个数，用于一次性写入hdd
		for (i = 0; i < l_r && l_r > 1; i++)
		{
			if (i == 0)
			{
				if (arr_r[i + 1] - arr_r[i] == 1)
				{
					if (random_num != 0)
					{
						char *avg = exec_disksim_syssim(random_num, 0, 0);
						write_hdd_time += (int)avg * random_num;
						random_num = 0;
					}
					int page_i;
					page_i = get_page_i_by_lpn(ssd, channel, chip, die, plane, block, arr_r[i]);
					location = (struct local *)malloc(sizeof(struct local));
					alloc_assert(location, "location");
					memset(location, 0, sizeof(struct local));
					location->channel = channel;
					location->chip = chip;
					location->die = die;
					location->plane = plane;
					location->block = block;
					location->page = page_i;
					adjust_page_hdd(ssd, location, &transfer_size);
					random_seq_num++;
				}
				else
				{
					int page_i;
					page_i = get_page_i_by_lpn(ssd, channel, chip, die, plane, block, arr_r[i]);
					location = (struct local *)malloc(sizeof(struct local));
					alloc_assert(location, "location");
					memset(location, 0, sizeof(struct local));
					location->channel = channel;
					location->chip = chip;
					location->die = die;
					location->plane = plane;
					location->block = block;
					location->page = page_i;
					adjust_page_hdd(ssd, location, &transfer_size); /*将该page标识为HDD*/
					random_num++;
				}
			}
			else
			{
				if (arr_r[i] - arr_r[i - 1] == 1)
				{
					if (random_num != 0)
					{
						char *avg = exec_disksim_syssim(random_num, 0, 0);
						write_hdd_time += (int)avg * random_num;
						random_num = 0;
					}
					int page_i;
					page_i = get_page_i_by_lpn(ssd, channel, chip, die, plane, block, arr_r[i]);
					location = (struct local *)malloc(sizeof(struct local));
					alloc_assert(location, "location");
					memset(location, 0, sizeof(struct local));
					location->channel = channel;
					location->chip = chip;
					location->die = die;
					location->plane = plane;
					location->block = block;
					location->page = page_i;
					adjust_page_hdd(ssd, location, &transfer_size); /*将该page标识为HDD*/
					random_seq_num++;
				}
				else if (i < l_r - 1 && arr_r[i + 1] - arr_r[i] == 1)
				{
					if (random_num != 0)
					{
						char *avg = exec_disksim_syssim(random_num, 0, 0);
						write_hdd_time += (int)avg * random_num;
						random_num = 0;
					}
					//1 3,4,5 7,8
					if (random_seq_num != 0)
					{
						char *avg = exec_disksim_syssim(random_seq_num, 0, 1);
						write_hdd_time += (int)avg * random_seq_num;
						random_seq_num = 0;
					}
					int page_i;
					page_i = get_page_i_by_lpn(ssd, channel, chip, die, plane, block, arr_r[i]);
					location = (struct local *)malloc(sizeof(struct local));
					alloc_assert(location, "location");
					memset(location, 0, sizeof(struct local));
					location->channel = channel;
					location->chip = chip;
					location->die = die;
					location->plane = plane;
					location->block = block;
					location->page = page_i;
					adjust_page_hdd(ssd, location, &transfer_size); /*将该page标识为HDD*/
					random_seq_num++;
				}
				else
				{
					if (random_seq_num != 0)
					{
						char *avg = exec_disksim_syssim(random_seq_num, 0, 1);
						write_hdd_time += (int)avg * random_seq_num;
						random_seq_num = 0;
					}
					int page_i;
					page_i = get_page_i_by_lpn(ssd, channel, chip, die, plane, block, arr_r[i]);
					location = (struct local *)malloc(sizeof(struct local));
					alloc_assert(location, "location");
					memset(location, 0, sizeof(struct local));
					location->channel = channel;
					location->chip = chip;
					location->die = die;
					location->plane = plane;
					location->block = block;
					location->page = page_i;
					adjust_page_hdd(ssd, location, &transfer_size); /*将该page标识为HDD*/
					random_num++;
					// char *avg = exec_disksim_syssim(1, 0, 0);
					// write_hdd_time += (int)avg * 1;
				}
			}
		}
		if (random_num != 0)
		{
			char *avg = exec_disksim_syssim(random_num, 0, 0);
			write_hdd_time += (int)avg * random_num;
			random_num = 0;
		}
	}
	erase_operation(ssd,channel ,chip , die,plane ,block,1);	                                              /*执行完move_page操作后，就立即执行block的擦除操作*/

	// printf("erase, page_move:%d\n", page_move_count);
    ssd->channel_head[channel].current_state=CHANNEL_GC;
	ssd->channel_head[channel].current_time=ssd->current_time;
	ssd->channel_head[channel].next_state=CHANNEL_IDLE;
	ssd->channel_head[channel].chip_head[chip].current_state=CHIP_ERASE_BUSY;
	ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
	ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;

	/***************************************************************
	*在可执行COPYBACK高级命令与不可执行COPYBACK高级命令这两种情况下，
	*channel下个状态时间的计算，以及chip下个状态时间的计算
	***************************************************************/
	if ((ssd->parameter->advanced_commands&AD_COPYBACK)==AD_COPYBACK)
	{
		if (ssd->parameter->greed_CB_ad==1)
		{

			ssd->channel_head[channel].next_state_predict_time=ssd->current_time+page_move_count*(7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tPROG);
			ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tBERS;
		}
	}
	else
	{
		// int write_hdd_time = ssd->parameter->time_characteristics.tWH;  // 数据写入HDD的时间，如果是随机写入则乘以page_move_count，否则按照顺序写入只记一次时间
        //上面这一行+的时间==hdd的写入时间
		// ssd->channel_head[channel].next_state_predict_time=ssd->scurrent_time+page_move_count*(7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tPROG)+transfer_size*SECTOR*(ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tRC);
		// printf("write_hdd_time: %d\n", write_hdd_time);

		//将ssd的并行时间修改为hdd的串行
		// int i;
		// unsigned long serial_diff=0, temp=0;
		// printf("ssd->current_time:%lld channel_current_time:%lld channel_next_state_predict_time%lld channel:%d\n", ssd->current_time, ssd->channel_head[channel].current_time, ssd->channel_head[channel].next_state_predict_time, channel);
		// for (i = 0; i < ssd->parameter->channel_number; i++)
		// {
		// 	if (i != channel)
		// 	{
		// 		printf("ssd->current_time:%lld channel_current_time:%lld channel_next_state_predict_time%lld i:%d channel:%d\n", ssd->current_time, ssd->channel_head[i].current_time, ssd->channel_head[i].next_state_predict_time, i, channel);
		// 		if (ssd->channel_head[i].current_time <= ssd->channel_head[channel].current_time && ssd->channel_head[channel].current_time < ssd->channel_head[i].next_state_predict_time && ssd->channel_head[channel].next_state_predict_time >= ssd->channel_head[i].next_state_predict_time)
		// 		{
		// 			temp = ssd->channel_head[i].next_state_predict_time - ssd->channel_head[channel].current_time;
		// 			if (serial_diff < temp)
		// 			{
		// 				serial_diff = temp;
		// 			}
		// 			printf("serial_diff:%lld\n", serial_diff);
		// 			abort();
		// 		}
		// 	}
		// }
		// printf("serial_diff2:%lld\n",serial_diff);
		// if (serial_diff != 0)
		// {
		// 	abort();
		// }
		

		// ssd->channel_head[channel].next_state_predict_time=ssd->current_time+page_move_count*(ssd->parameter->time_characteristics.tR)+transfer_size*SECTOR*(ssd->parameter->time_characteristics.tRC) + write_hdd_time;

		
		if(ssd->HDDTime < ssd->current_time){
			ssd->HDDTime = ssd->current_time;
		}
		write_hdd_time += (ssd->HDDTime - ssd->current_time);
		ssd->HDDTime += (write_hdd_time - (ssd->HDDTime - ssd->current_time));

		ssd->channel_head[channel].next_state_predict_time=ssd->current_time+page_move_count*(7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tPROG)+transfer_size*SECTOR*(ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tRC) + write_hdd_time;

		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tBERS;

	}

	return 1;
}

int uninterrupt_fast_gc(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane,unsigned int priority)
{
	unsigned int i=0,invalid_page=0;
	unsigned int block,block1,active_block,transfer_size,free_page,page_move_count=0;                           /*记录失效页最多的块号*/
	struct local *  location=NULL;
	unsigned int total_invalid_page_num=0;

	block1 = -1;
	block = 0;
	struct plane_info candidate_plane;
	candidate_plane = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane];
	unsigned int invalid_lsb_num = 0;
	while(block < ssd->parameter->block_plane-1){
		//printf("Enter while, %d\n", block);
		if((candidate_plane.blk_head[block].free_msb_num==ssd->parameter->page_block/2)&&(candidate_plane.blk_head[block].fast_erase!=TRUE)){
			if(candidate_plane.blk_head[block].free_lsb_num==0 && candidate_plane.blk_head[block].invalid_lsb_num>invalid_lsb_num){
				block1 = block;
				invalid_lsb_num = candidate_plane.blk_head[block].invalid_lsb_num;
				}
			}
		block++;
		}
	if (block1==-1)
	{
		//printf("====>No block has invalid LSB page??\n");
		return SUCCESS;
	}
	if(priority==GC_FAST_UNINTERRUPT_EMERGENCY){
		if(candidate_plane.blk_head[block1].invalid_lsb_num < ssd->parameter->page_block/2*ssd->parameter->fast_gc_filter_2){
			return SUCCESS;
			}
		}
	else if(priority==GC_FAST_UNINTERRUPT){
		if(candidate_plane.blk_head[block1].invalid_lsb_num < ssd->parameter->page_block/2*ssd->parameter->fast_gc_filter_1){
			return SUCCESS;
			}
		}
	else if(priority==GC_FAST_UNINTERRUPT_IDLE){
		if(candidate_plane.blk_head[block1].invalid_lsb_num < ssd->parameter->page_block/2*ssd->parameter->fast_gc_filter_idle){
			return SUCCESS;
			}
		//printf("FAST_GC_IDLE on %d, %d, %d, %d, %d.\n", channel, chip, die, plane, block1);
		ssd->idle_fast_gc_count++;
		}
	else{
		printf("GC_FAST ERROR. PARAMETERS WRONG.\n");
		return SUCCESS;
		}

	free_page=0;
	struct blk_info candidate_block;
	candidate_block = candidate_plane.blk_head[block1];
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block1].fast_erase = TRUE;
	//printf("%d %d %d %d %d is selected, free_msbs:%d, invalid_lsbs:%d.\n",channel,chip,die,plane,block1, candidate_block.free_msb_num, candidate_block.invalid_lsb_num);
	//printf("begin to move pages.\n");
	//printf("<<<Fast GC Candidate, %d, %d, %d, %d, %d.\n",channel,chip,die,plane,block1);
	//printf("Begin to MOVE PAGES.\n");
	for(i=0;i<ssd->parameter->page_block;i++)		                                                     /*逐个检查每个page，如果有有效数据的page需要移动到其他地方存储*/
	{
		if(candidate_block.page_head[i].valid_state>0) /*该页是有效页，需要copyback操作*/
		{
			//printf("move page %d.\n",i);
			location=(struct local * )malloc(sizeof(struct local ));
			alloc_assert(location,"location");
			memset(location,0, sizeof(struct local));

			location->channel=channel;
			location->chip=chip;
			location->die=die;
			location->plane=plane;
			location->block=block1;
			location->page=i;
			//printf("state of this block: %d.\n",candidate_block.fast_erase);
			move_page(ssd, location, &transfer_size);                                                   /*真实的move_page操作*/
			page_move_count++;

			free(location);
			location=NULL;
		}
	}
	//printf("block: %d, %d, %d, %d, %d going to be erased.\n",channel, chip, die, plane, block1);
	erase_operation(ssd,channel ,chip , die, plane ,block1,0);	                                              /*执行完move_page操作后，就立即执行block的擦除操作*/
	//printf("ERASE OPT: %d, %d, %d, %d, %d\n",channel,chip,die,plane,block1);
	ssd->channel_head[channel].current_state=CHANNEL_GC;
	ssd->channel_head[channel].current_time=ssd->current_time;
	ssd->channel_head[channel].next_state=CHANNEL_IDLE;
	ssd->channel_head[channel].chip_head[chip].current_state=CHIP_ERASE_BUSY;
	ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
	ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;

	ssd->fast_gc_count++;

	/***************************************************************
	*在可执行COPYBACK高级命令与不可执行COPYBACK高级命令这两种情况下，
	*channel下个状态时间的计算，以及chip下个状态时间的计算
	***************************************************************/
	if ((ssd->parameter->advanced_commands&AD_COPYBACK)==AD_COPYBACK)
	{
		if (ssd->parameter->greed_CB_ad==1)
		{

			ssd->channel_head[channel].next_state_predict_time=ssd->current_time+page_move_count*(7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tPROG);
			ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tBERS;
		}
	}
	else
	{

		ssd->channel_head[channel].next_state_predict_time=ssd->current_time+page_move_count*(7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tPROG)+transfer_size*SECTOR*(ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tRC);
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tBERS;

	}
	//printf("===>Exit uninterrupt_fast_gc Successfully.\n");
	return 1;
}

int uninterrupt_dr(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane)
{
	unsigned int i=0,invalid_page=0;
	unsigned int block,active_block,transfer_size,free_page,page_move_count=0;                           /*记录失效页最多的块号*/
	struct local *  location=NULL;
	unsigned int total_invalid_page_num=0;
	/*
	if(find_active_block_dr(ssd,channel,chip,die,plane)!=SUCCESS)
	{
		printf("\n\n Error in uninterrupt_dr(). No active block\n");
		return ERROR;
	}
	active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;
	*/
	invalid_page=0;
	transfer_size=0;
	block=-1;
	for(i=0;i<ssd->parameter->block_plane;i++)                                                           /*查找最多invalid_page的块号，以及最大的invalid_page_num*/
	{
		total_invalid_page_num+=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].invalid_page_num;
		if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].invalid_page_num>invalid_page)
		{
			invalid_page=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].invalid_page_num;
			block=i;
		}
	}

	if (block==-1)
	{
		printf("No block is selected.\n");
		return 1;
	}
	//把目标block的dr_state设为输出
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].dr_state=DR_STATE_OUTPUT;


	//printf("Block %d with %d invalid pages is selected.\n", block, ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].invalid_page_num);
	free_page=0;
	for(i=0;i<ssd->parameter->page_block;i++)		                                                     /*逐个检查每个page，如果有有效数据的page需要移动到其他地方存储*/
	{
		if ((ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].free_state&PG_SUB)==0x0000000f)
		{
			free_page++;
		}
		/*
		if(free_page!=0)
		{
			//printf("\ntoo much free page. \t %d\t .%d\t%d\t%d\t%d\t\n",free_page,channel,chip,die,plane);
		}
		*/
		if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].valid_state>0) /*该页是有效页，需要copyback操作*/
		{
			location=(struct local * )malloc(sizeof(struct local ));
			alloc_assert(location,"location");
			memset(location,0, sizeof(struct local));

			location->channel=channel;
			location->chip=chip;
			location->die=die;
			location->plane=plane;
			location->block=block;
			location->page=i;
			if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].dr_state != DR_STATE_OUTPUT){
				printf("The DR_STATE is not settle.\n");
				}
			move_page(ssd, location, &transfer_size);                                                   /*真实的move_page操作*/
			page_move_count++;

			free(location);
			location=NULL;
		}
	}
	erase_operation(ssd,channel ,chip , die,plane ,block,0);	                                              /*执行完move_page操作后，就立即执行block的擦除操作*/
	/*
	ssd->channel_head[channel].current_state=CHANNEL_GC;
	ssd->channel_head[channel].current_time=ssd->current_time;
	ssd->channel_head[channel].next_state=CHANNEL_IDLE;
	ssd->channel_head[channel].chip_head[chip].current_state=CHIP_ERASE_BUSY;
	ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
	ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;
	*/
	/***************************************************************
	*在可执行COPYBACK高级命令与不可执行COPYBACK高级命令这两种情况下，
	*channel下个状态时间的计算，以及chip下个状态时间的计算
	***************************************************************/
	/*
	if ((ssd->parameter->advanced_commands&AD_COPYBACK)==AD_COPYBACK)
	{
		if (ssd->parameter->greed_CB_ad==1)
		{

			ssd->channel_head[channel].next_state_predict_time=ssd->current_time+page_move_count*(7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tPROG);
			ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tBERS;
		}
	}
	else
	{

		ssd->channel_head[channel].next_state_predict_time=ssd->current_time+page_move_count*(7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tPROG)+transfer_size*SECTOR*(ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tRC);
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tBERS;

	}
	*/
	return 1;
}


/*******************************************************************************************************************************************
*目标的plane没有可以直接删除的block，需要寻找目标擦除块后在实施擦除操作，用在可以中断的gc操作中，成功删除一个块，返回1，没有删除一个块返回-1
*在这个函数中，不用考虑目标channel,die是否是空闲的
********************************************************************************************************************************************/
int interrupt_gc(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane,struct gc_operation *gc_node)
{
	unsigned int i,block,active_block,transfer_size,invalid_page=0;
	struct local *location;

	active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;
	transfer_size=0;
	if (gc_node->block>=ssd->parameter->block_plane)
	{
		for(i=0;i<ssd->parameter->block_plane;i++)
		{
			if((active_block!=i)&&(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].invalid_page_num>invalid_page))
			{
				invalid_page=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].invalid_page_num;
				block=i;
			}
		}
		gc_node->block=block;
	}

	if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[gc_node->block].invalid_page_num!=ssd->parameter->page_block)     /*还需要执行copyback操作*/
	{
		for (i=gc_node->page;i<ssd->parameter->page_block;i++)
		{
			if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[gc_node->block].page_head[i].valid_state>0)
			{
				location=(struct local * )malloc(sizeof(struct local ));
				alloc_assert(location,"location");
				memset(location,0, sizeof(struct local));

				location->channel=channel;
				location->chip=chip;
				location->die=die;
				location->plane=plane;
				location->block=block;
				location->page=i;
				transfer_size=0;

				move_page( ssd, location, &transfer_size);

				free(location);
				location=NULL;

				gc_node->page=i+1;
				ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[gc_node->block].invalid_page_num++;
				ssd->channel_head[channel].current_state=CHANNEL_C_A_TRANSFER;
				ssd->channel_head[channel].current_time=ssd->current_time;
				ssd->channel_head[channel].next_state=CHANNEL_IDLE;
				ssd->channel_head[channel].chip_head[chip].current_state=CHIP_COPYBACK_BUSY;
				ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
				ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;

				if ((ssd->parameter->advanced_commands&AD_COPYBACK)==AD_COPYBACK)
				{
					ssd->channel_head[channel].next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+7*ssd->parameter->time_characteristics.tWC;
					ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;
				}
				else
				{
					ssd->channel_head[channel].next_state_predict_time=ssd->current_time+(7+transfer_size*SECTOR)*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(7+transfer_size*SECTOR)*ssd->parameter->time_characteristics.tWC;
					ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;
				}
				return 0;
			}
		}
	}
	else
	{
		erase_operation(ssd,channel ,chip, die,plane,gc_node->block,0);

		ssd->channel_head[channel].current_state=CHANNEL_C_A_TRANSFER;
		ssd->channel_head[channel].current_time=ssd->current_time;
		ssd->channel_head[channel].next_state=CHANNEL_IDLE;
		ssd->channel_head[channel].next_state_predict_time=ssd->current_time+5*ssd->parameter->time_characteristics.tWC;

		ssd->channel_head[channel].chip_head[chip].current_state=CHIP_ERASE_BUSY;
		ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
		ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tBERS;

		return 1;                                                                      /*该gc操作完成，返回1，可以将channel上的gc请求节点删除*/
	}

	printf("there is a problem in interrupt_gc\n");
	return 1;
}

/*************************************************************
*函数的功能是当处理完一个gc操作时，需要把gc链上的gc_node删除掉
**************************************************************/
int delete_gc_node(struct ssd_info *ssd, unsigned int channel,struct gc_operation *gc_node)
{
	struct gc_operation *gc_pre=NULL;
	if(gc_node==NULL)
	{
		return ERROR;
	}

	if (gc_node==ssd->channel_head[channel].gc_command)
	{
		ssd->channel_head[channel].gc_command=gc_node->next_node;
	}
	else
	{
		gc_pre=ssd->channel_head[channel].gc_command;
		while (gc_pre->next_node!=NULL)
		{
			if (gc_pre->next_node==gc_node)
			{
				gc_pre->next_node=gc_node->next_node;
				break;
			}
			gc_pre=gc_pre->next_node;
		}
	}
	free(gc_node);
	gc_node=NULL;
	ssd->gc_request--;
	return SUCCESS;
}

/***************************************
*这个函数的功能是处理channel的每个gc操作(这里都是不可中断的gc)
****************************************/
Status gc_for_channel(struct ssd_info *ssd, unsigned int channel)
{
	int flag_direct_erase=1,flag_gc=1,flag_invoke_gc=1;
	unsigned int chip,die,plane,flag_priority=0;
	unsigned int current_state=0, next_state=0;
	long long next_state_predict_time=0;
	struct gc_operation *gc_node=NULL,*gc_p=NULL;

	/*******************************************************************************************
	*查找每一个gc_node，获取gc_node所在的chip的当前状态，下个状态，下个状态的预计时间
	*如果当前状态是空闲，或是下个状态是空闲而下个状态的预计时间小于当前时间，并且是不可中断的gc
	*那么就flag_priority令为1，否则为0
	********************************************************************************************/
	gc_node=ssd->channel_head[channel].gc_command;
	while (gc_node!=NULL)
	{
		current_state=ssd->channel_head[channel].chip_head[gc_node->chip].current_state;
		next_state=ssd->channel_head[channel].chip_head[gc_node->chip].next_state;
		next_state_predict_time=ssd->channel_head[channel].chip_head[gc_node->chip].next_state_predict_time;
		if((current_state==CHIP_IDLE)||((next_state==CHIP_IDLE)&&(next_state_predict_time<=ssd->current_time)))
		{
			if (gc_node->priority==GC_UNINTERRUPT)                                     /*这个gc请求是不可中断的，优先服务这个gc操作*/
			{
				flag_priority=1;
				break;
			}
		}
		gc_node=gc_node->next_node;
	}
	if (flag_priority!=1)                                                              /*没有找到不可中断的gc请求，首先执行队首的gc请求*/
	{
		gc_node=ssd->channel_head[channel].gc_command;
		while (gc_node!=NULL)
		{
			current_state=ssd->channel_head[channel].chip_head[gc_node->chip].current_state;
			next_state=ssd->channel_head[channel].chip_head[gc_node->chip].next_state;
			next_state_predict_time=ssd->channel_head[channel].chip_head[gc_node->chip].next_state_predict_time;
			 /**********************************************
			 *需要gc操作的目标chip是空闲的，才可以进行gc操作
			 ***********************************************/
			if((current_state==CHIP_IDLE)||((next_state==CHIP_IDLE)&&(next_state_predict_time<=ssd->current_time)))
			{
				break;
			}
			gc_node=gc_node->next_node;
		}

	}
	if(gc_node==NULL)
	{
		return FAILURE;
	}

	chip=gc_node->chip;
	die=gc_node->die;
	plane=gc_node->plane;

	if (gc_node->priority==GC_UNINTERRUPT)
	{
		flag_direct_erase=gc_direct_erase(ssd,channel,chip,die,plane);
		if (flag_direct_erase!=SUCCESS)
		{
			flag_gc=uninterrupt_gc(ssd,channel,chip,die,plane);                         /*当一个完整的gc操作完成时（已经擦除一个块，回收了一定数量的flash空间），返回1，将channel上相应的gc操作请求节点删除*/
			if (flag_gc==1)
			{
				delete_gc_node(ssd,channel,gc_node);
			}
		}
		else
		{
			delete_gc_node(ssd,channel,gc_node);
		}
		return SUCCESS;
	}
	/*******************************************************************************
	*可中断的gc请求，需要首先确认该channel上没有子请求在这个时刻需要使用这个channel，
	*没有的话，在执行gc操作，有的话，不执行gc操作
	********************************************************************************/
	else if(gc_node->priority==GC_FAST_UNINTERRUPT || gc_node->priority==GC_FAST_UNINTERRUPT_EMERGENCY || gc_node->priority==GC_FAST_UNINTERRUPT_IDLE){
		//printf("===>GC_FAST on %d,%d,%d,%d begin.\n",channel,chip,die,plane);
		flag_direct_erase=gc_direct_fast_erase(ssd,channel,chip,die,plane);
		if (flag_direct_erase!=SUCCESS)
		{
			/*
			printf("Something Weird happened.\n");
			return FAILURE;
			*/
			//printf("NO BLOCK CAN BE ERASED DIRECTLY.\n");
			flag_gc=uninterrupt_fast_gc(ssd,channel,chip,die,plane,gc_node->priority);
			if (flag_gc==1)
			{
				delete_gc_node(ssd,channel,gc_node);
			}
		}
		else
		{
			//printf("THERE IS BLOCK CAN BE ERASED DIRECTLY.\n");
			delete_gc_node(ssd,channel,gc_node);
		}
		//printf("===>GC_FAST on %d,%d,%d,%d successed.\n",channel,chip,die,plane);
		return SUCCESS;
		}
	else
	{
		flag_invoke_gc=decide_gc_invoke(ssd,channel);                                  /*判断是否有子请求需要channel，如果有子请求需要这个channel，那么这个gc操作就被中断了*/

		if (flag_invoke_gc==1)
		{
			flag_direct_erase=gc_direct_erase(ssd,channel,chip,die,plane);
			if (flag_direct_erase==-1)
			{
				flag_gc=interrupt_gc(ssd,channel,chip,die,plane,gc_node);             /*当一个完整的gc操作完成时（已经擦除一个块，回收了一定数量的flash空间），返回1，将channel上相应的gc操作请求节点删除*/
				if (flag_gc==1)
				{
					delete_gc_node(ssd,channel,gc_node);
				}
			}
			else if (flag_direct_erase==1)
			{
				delete_gc_node(ssd,channel,gc_node);
			}
			return SUCCESS;
		}
		else
		{
			return FAILURE;
		}
	}
}

Status find_invalid_block(struct ssd_info *ssd, unsigned int channel, unsigned int chip, unsigned int die, unsigned int plane){
	unsigned int block, flag;
	for(block=0;block<ssd->parameter->block_plane;block++){
		if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].invalid_page_num>(ssd->parameter->page_block*ssd->parameter->dr_filter)){
			return TRUE;
			}
		}
	return FALSE;
}

Status dr_for_channel(struct ssd_info *ssd, unsigned int channel)
{
	int flag_direct_erase=1,flag_gc=1,flag_invoke_gc=1;
	unsigned int chip,die,plane,flag_priority=0;
	unsigned int current_state=0, next_state=0;
	long long next_state_predict_time=0;
	struct gc_operation *gc_node=NULL,*gc_p=NULL;

	/*******************************************************************************************
	*查找每一个gc_node，获取gc_node所在的chip的当前状态，下个状态，下个状态的预计时间
	*如果当前状态是空闲，或是下个状态是空闲而下个状态的预计时间小于当前时间，并且是不可中断的gc
	*那么就flag_priority令为1，否则为0
	********************************************************************************************/
	//======================================================================
	//所有垃圾回收操作都清除掉
	ssd->channel_head[channel].gc_command=NULL;
	gc_node=ssd->channel_head[channel].gc_command;
	//轮询所有plane，把有无效页的block都回收掉
	unsigned int plane_flag;
	unsigned int counter;
	for(chip=0;chip<ssd->parameter->chip_channel[0];chip++){
		for(die=0;die<ssd->parameter->die_chip;die++){
			for(plane=0;plane<ssd->parameter->plane_die;plane++){
				plane_flag = find_invalid_block(ssd, channel, chip, die, plane);
				counter=0;
				while(plane_flag==TRUE){
					counter++;
					//printf("There is data should be reorganize in %d, %d, %d, %d.\n",channel, chip, die, plane);
					uninterrupt_dr(ssd, channel, chip, die, plane);
					plane_flag = find_invalid_block(ssd, channel, chip, die, plane);
					}
				printf("SUCCESS. Data in %d, %d, %d, %d is reorganized.\n",channel, chip, die, plane);
				}
			}
    	}
	return SUCCESS;
}


/************************************************************************************************************
*flag用来标记gc函数是在ssd整个都是idle的情况下被调用的（1），还是确定了channel，chip，die，plane被调用（0）
*进入gc函数，需要判断是否是不可中断的gc操作，如果是，需要将一整块目标block完全擦除后才算完成；如果是可中断的，
*在进行GC操作前，需要判断该channel，die是否有子请求在等待操作，如果没有则开始一步一步的操作，找到目标
*块后，一次执行一个copyback操作，跳出gc函数，待时间向前推进后，再做下一个copyback或者erase操作
*进入gc函数不一定需要进行gc操作，需要进行一定的判断，当处于硬阈值以下时，必须进行gc操作；当处于软阈值以下时，
*需要判断，看这个channel上是否有子请求在等待(有写子请求等待就不行，gc的目标die处于busy状态也不行)，如果
*有就不执行gc，跳出，否则可以执行一步操作
************************************************************************************************************/
unsigned int gc(struct ssd_info *ssd,unsigned int channel, unsigned int flag)
{
	// printf("gc");
	unsigned int i;
	int flag_direct_erase=1,flag_gc=1,flag_invoke_gc=1;
	unsigned int flag_priority=0;
	struct gc_operation *gc_node=NULL,*gc_p=NULL;

	if (flag==1)/*整个ssd都是IDEL的情况*/
	{
		for (i=0;i<ssd->parameter->channel_number;i++)
		{
			flag_priority=0;
			flag_direct_erase=1;
			flag_gc=1;
			flag_invoke_gc=1;
			gc_node=NULL;
			gc_p=NULL;
			if((ssd->channel_head[i].current_state==CHANNEL_IDLE)||(ssd->channel_head[i].next_state==CHANNEL_IDLE&&ssd->channel_head[i].next_state_predict_time<=ssd->current_time))
			{
				channel=i;
				if (ssd->channel_head[channel].gc_command!=NULL)
				{
					gc_for_channel(ssd, channel);
				}
			}
		}
		return SUCCESS;

	}
	else/*只需针对某个特定的channel，chip，die进行gc请求的操作(只需对目标die进行判定，看是不是idle）*/
	{
		if ((ssd->parameter->allocation_scheme==1)||((ssd->parameter->allocation_scheme==0)&&(ssd->parameter->dynamic_allocation==1)))
		{
			if ((ssd->channel_head[channel].subs_r_head!=NULL)||(ssd->channel_head[channel].subs_w_head!=NULL))    /*队列上有请求，先服务请求*/
			{
				return 0;
			}
		}

		gc_for_channel(ssd,channel);
		return SUCCESS;
	}
}



/**********************************************************
*判断是否有子请求血药channel，若果没有返回1就可以发送gc操作
*如果有返回0，就不能执行gc操作，gc操作被中断
***********************************************************/
int decide_gc_invoke(struct ssd_info *ssd, unsigned int channel)
{
	struct sub_request *sub;
	struct local *location;

	if ((ssd->channel_head[channel].subs_r_head==NULL)&&(ssd->channel_head[channel].subs_w_head==NULL))    /*这里查找读写子请求是否需要占用这个channel，不用的话才能执行GC操作*/
	{
		return 1;                                                                        /*表示当前时间这个channel没有子请求需要占用channel*/
	}
	else
	{
		if (ssd->channel_head[channel].subs_w_head!=NULL)
		{
			return 0;
		}
		else if (ssd->channel_head[channel].subs_r_head!=NULL)
		{
			sub=ssd->channel_head[channel].subs_r_head;
			while (sub!=NULL)
			{
				if (sub->current_state==SR_WAIT)                                         /*这个读请求是处于等待状态，如果他的目标die处于idle，则不能执行gc操作，返回0*/
				{
					location=find_location(ssd,sub->ppn);
					if ((ssd->channel_head[location->channel].chip_head[location->chip].current_state==CHIP_IDLE)||((ssd->channel_head[location->channel].chip_head[location->chip].next_state==CHIP_IDLE)&&
						(ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time<=ssd->current_time)))
					{
						free(location);
						location=NULL;
						return 0;
					}
					free(location);
					location=NULL;
				}
				else if (sub->next_state==SR_R_DATA_TRANSFER)
				{
					location=find_location(ssd,sub->ppn);
					if (ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time<=ssd->current_time)
					{
						free(location);
						location=NULL;
						return 0;
					}
					free(location);
					location=NULL;
				}
				sub=sub->next_node;
			}
		}
		return 1;
	}
}

