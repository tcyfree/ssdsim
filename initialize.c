/*****************************************************************************************************************************
This project was supported by the National Basic Research 973 Program of China under Grant No.2011CB302301
Huazhong University of Science and Technology (HUST)   Wuhan National Laboratory for Optoelectronics

FileName： initialize.c
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

#include <stdlib.h>
#include "initialize.h"
#include "pagemap.h"

#define FALSE		0
#define TRUE		1

#define ACTIVE_FIXED 0
#define ACTIVE_ADJUST 1


/************************************************************************
* Compare function for AVL Tree
************************************************************************/
extern int keyCompareFunc(TREE_NODE *p , TREE_NODE *p1)
{
	struct buffer_group *T1=NULL,*T2=NULL;

	T1=(struct buffer_group*)p;
	T2=(struct buffer_group*)p1;


	if(T1->group< T2->group) return 1;
	if(T1->group> T2->group) return -1;

	return 0;
}


extern int freeFunc(TREE_NODE *pNode)
{

	if(pNode!=NULL)
	{
		free((void *)pNode);
	}


	pNode=NULL;
	return 1;
}

struct avl_write_info * initialize_avl_write(struct ssd_info * ssd)
{
	unsigned int page_num;
	struct avl_write_info *avl_write=ssd->avl_write;
	avl_write->dram_capacity = 131074;
	avl_write->buffer = (tAVLTree *)avlTreeCreate((void*)keyCompareFunc , (void *)freeFunc);
	avl_write->buffer->max_buffer_sector=avl_write->dram_capacity/SECTOR; //512

	return avl_write;
}

struct avl_read_info * initialize_avl_read(struct ssd_info * ssd)
{
	unsigned int page_num;
	struct avl_read_info *avl_read=ssd->avl_read;
	avl_read->dram_capacity = 131074;
	avl_read->buffer = (tAVLTree *)avlTreeCreate((void*)keyCompareFunc , (void *)freeFunc);
	avl_read->buffer->max_buffer_sector=avl_read->dram_capacity/SECTOR; //512

	return avl_read;
}

/**********   initiation   ******************
*modify by zhouwen
*November 08,2011
*initialize the ssd struct to simulate the ssd hardware
*1.this function allocate memory for ssd structure
*2.set the infomation according to the parameter file
*******************************************/
struct ssd_info *initiation(struct ssd_info *ssd)
{
	//SSD->channel->chip->die->plane->block->page->subpage
	unsigned int x=0,y=0,i=0,j=0,k=0,l=0,m=0,n=0; //这里定义的八个uint变量应该就是指为了循环遍历初始化上述这八个层次中的所有结构参数而设定的
	//errno_t err;
	char buffer[300]; //缓冲区初始化，大小为300Byte
	struct parameter_value *parameters; //声明输入参数文件
	FILE *fp=NULL; //指向文件指针的一个指针，FILE是C语言stdio.h头文件中定义的一个结构体类型，该结构体包含了与文件相关的一系列成员参数，如文件属性等。
    unsigned int lpn = 0;
    unsigned int lsn = 100000;
    unsigned int ppn, full_page;
    int sub_size = 4;
	int large_lsn=0;

	/*printf("input parameter file name:");
	gets(ssd->parameterfilename);
 	strcpy_s(ssd->parameterfilename,16,"page.parameters");

	printf("\ninput trace file name:");
	gets(ssd->tracefilename);
	strcpy_s(ssd->tracefilename,25,"Exchange.ascii");

	printf("\ninput output file name:");
	gets(ssd->outputfilename);
	strcpy_s(ssd->outputfilename,7,"ex.out");

	printf("\ninput statistic file name:");
	gets(ssd->statisticfilename);
	strcpy_s(ssd->statisticfilename,16,"statistic10.dat");

	strcpy_s(ssd->statisticfilename2 ,16,"statistic2.dat");*/

	strncpy(ssd->parameterfilename,"page.parameters",16);
	//strncpy(ssd->tracefilename,"example.ascii",25);
	//printf("\ninput trace file name:");
	//scanf("%s",ssd->tracefilename);
	strncpy(ssd->outputfilename,"ex.out",7);
	strncpy(ssd->statisticfilename,"statistic10.dat",16);
	strncpy(ssd->statisticfilename2,"statistic2.dat",15);

	//导入ssd的配置文件
	parameters=load_parameters(ssd->parameterfilename);  //将ssd结构体的参数导入至定义的参数文件中，而后赋值给参数结构体变量
	ssd->parameter=parameters;  //直接将保存参数的结构体变量进行赋值 
	ssd->min_lsn=0x7fffffff;
	ssd->page=ssd->parameter->chip_num*ssd->parameter->die_chip*ssd->parameter->plane_die*ssd->parameter->block_plane*ssd->parameter->page_block;

	//初始化 dram
	ssd->dram = (struct dram_info *)malloc(sizeof(struct dram_info));
	alloc_assert(ssd->dram,"ssd->dram");
	memset(ssd->dram,0,sizeof(struct dram_info));
	initialize_dram(ssd);

	//初始化 avl_read
	ssd->avl_read = (struct avl_read_info *)malloc(sizeof(struct avl_read_info));
	alloc_assert(ssd->avl_read,"ssd->avl_read");
	memset(ssd->avl_read,0,sizeof(struct avl_read_info));
	initialize_avl_read(ssd);

	//初始化 avl_write
	ssd->avl_write = (struct avl_write_info *)malloc(sizeof(struct avl_write_info));
	alloc_assert(ssd->avl_write,"ssd->avl_write");
	memset(ssd->avl_write,0,sizeof(struct avl_write_info));
	initialize_avl_write(ssd);

	//初始化通道
	ssd->channel_head=(struct channel_info*)malloc(ssd->parameter->channel_number * sizeof(struct channel_info));
	alloc_assert(ssd->channel_head,"ssd->channel_head");
	memset(ssd->channel_head,0,ssd->parameter->channel_number * sizeof(struct channel_info));
	initialize_channels(ssd );
	//====================================
	ssd->time_day = 0;
	ssd->current_time = 0;
	ssd->completed_request_count = 0;
	ssd->total_request_num = 0;
	ssd->moved_page_count = 0;
	ssd->idle_fast_gc_count = 0;
	ssd->write_lat_anchor = 0;

	ssd->performance_mode = 0;
	ssd->max_queue_depth = 0;
	ssd->newest_write_request_completed_with_same_type_pages = 0;
	ssd->newest_req_with_msb = 0;
	ssd->newest_req_with_csb = 0;
	ssd->newest_req_with_lsb = 0;
	ssd->newest_write_request_num_l = 0;
	ssd->newest_write_request_completed_with_same_type_pages_l = 0;
	ssd->newest_req_with_msb_l = 0;
	ssd->newest_req_with_csb_l = 0;
	ssd->newest_req_with_lsb_l = 0;
	ssd->sub_request_success = 0;
	ssd->sub_request_all = 0;
	ssd->r_queue_length = HOT_QUEUE_LEN;
	ssd->p_queue_length = HOT_QUEUE_LEN;

	for(i=0;i<10;i++){
		ssd->last_ten_write_lat[i]=0;
		}
	//printf("%d, %d, %d, %d, %d, %d\n",ssd->parameter->page_block,)
	ssd->free_lsb_count = ((ssd->parameter->page_block)*(ssd->parameter->block_plane)*(ssd->parameter->plane_die)*(ssd->parameter->die_chip)*(ssd->parameter->chip_channel[0])*(ssd->parameter->channel_number))/3;
	ssd->free_csb_count = ssd->free_lsb_count;
	ssd->free_msb_count = ssd->free_lsb_count;
	printf("FREE_LSB_COUNT: %d\n",ssd->free_lsb_count);
	//====================================
	printf("\n");
	ssd->outputfile=fopen(ssd->outputfilename,"w");
	if(ssd->outputfile==NULL)
	{
		printf("the output file can't open\n");
		return NULL;
	}

	printf("\n");
	ssd->statisticfile=fopen(ssd->statisticfilename,"w");
	if(ssd->statisticfile==NULL)
	{
		printf("the statistic file can't open\n");
		return NULL;
	}

	printf("\n");


	fprintf(ssd->outputfile,"parameter file: %s\n",ssd->parameterfilename);
	fprintf(ssd->outputfile,"trace file: %s\n",ssd->tracefilename);
	fprintf(ssd->statisticfile,"parameter file: %s\n",ssd->parameterfilename);
	fprintf(ssd->statisticfile,"trace file: %s\n",ssd->tracefilename);

	fflush(ssd->outputfile);
	fflush(ssd->statisticfile);

	fp=fopen(ssd->parameterfilename,"r");
	if(fp==NULL)
	{
		printf("\nthe parameter file can't open!\n");
		return NULL;
	}

	//fp=fopen(ssd->parameterfilename,"r");

	fprintf(ssd->outputfile,"-----------------------parameter file----------------------\n");
	fprintf(ssd->statisticfile,"-----------------------parameter file----------------------\n");
	while(fgets(buffer,300,fp))
	{
		fprintf(ssd->outputfile,"%s",buffer);
		fflush(ssd->outputfile);
		fprintf(ssd->statisticfile,"%s",buffer);
		fflush(ssd->statisticfile);
	}

	fprintf(ssd->outputfile,"\n");
	fprintf(ssd->outputfile,"-----------------------simulation output----------------------\n");
	fflush(ssd->outputfile);

	fprintf(ssd->statisticfile,"\n");
	fprintf(ssd->statisticfile,"-----------------------simulation output----------------------\n");
	fflush(ssd->statisticfile);

	fclose(fp);
	printf("initiation is completed!\n");
	large_lsn=(int)((ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->plane_die*ssd->parameter->die_chip*ssd->parameter->chip_num)*(1-ssd->parameter->overprovide));
    //init valid pages
    // lpn = 0;
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
	// 						// printf("enter 2\n");
	// 						lpn = 0; //??
	// 						while(ssd->dram->map->map_entry[lpn].state != 0){
	// 							lpn = random();
	// 							lpn %= large_lsn;
	// 						}
	// 						// printf("%d\n", lpn);
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


struct dram_info * initialize_dram(struct ssd_info * ssd)
{
	unsigned int page_num;

	struct dram_info *dram=ssd->dram;
	dram->dram_capacity = ssd->parameter->dram_capacity;
	dram->buffer = (tAVLTree *)avlTreeCreate((void*)keyCompareFunc , (void *)freeFunc);
	dram->buffer->max_buffer_sector=ssd->parameter->dram_capacity/SECTOR; //512

	dram->map = (struct map_info *)malloc(sizeof(struct map_info));
	alloc_assert(dram->map,"dram->map");
	memset(dram->map,0, sizeof(struct map_info));

	page_num = ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->plane_die*ssd->parameter->die_chip*ssd->parameter->chip_num;

	dram->map->map_entry = (struct entry *)malloc(sizeof(struct entry) * page_num); //每个物理页和逻辑页都有对应关系
	alloc_assert(dram->map->map_entry,"dram->map->map_entry");
	memset(dram->map->map_entry,0,sizeof(struct entry) * page_num);

	return dram;
}

struct page_info * initialize_page(struct page_info * p_page )
{
	p_page->valid_state =0;
	p_page->free_state = PG_SUB;
	p_page->lpn = -1;
	p_page->written_count=0;
	return p_page;
}

struct blk_info * initialize_block(struct blk_info * p_block,struct parameter_value *parameter)
{
	unsigned int i;
	struct page_info * p_page;

	p_block->free_page_num = parameter->page_block;	// all pages are free
	p_block->last_write_page = -1;	// no page has been programmed

	//================================================
	p_block->free_lsb_num = (int)(parameter->page_block/3);
	p_block->free_msb_num = (int)(parameter->page_block/3);
	p_block->free_csb_num = (int)(parameter->page_block/3);
	p_block->last_write_lsb = -3;
	p_block->last_write_csb = -2;
	p_block->last_write_msb = -1;
	p_block->fast_erase = FALSE;
	p_block->dr_state = DR_STATE_NULL;
	//================================================

	p_block->page_head = (struct page_info *)malloc(parameter->page_block * sizeof(struct page_info));
	alloc_assert(p_block->page_head,"p_block->page_head");
	memset(p_block->page_head,0,parameter->page_block * sizeof(struct page_info));

	for(i = 0; i<parameter->page_block; i++)
	{
		p_page = &(p_block->page_head[i]);
		initialize_page(p_page );
	}
	return p_block;
}

struct plane_info * initialize_plane(struct plane_info * p_plane,struct parameter_value *parameter )
{
	unsigned int i;
	struct blk_info * p_block;
	p_plane->add_reg_ppn = -1;  //plane 里面的额外寄存器additional register -1 表示无数据
	p_plane->free_page=parameter->block_plane*parameter->page_block;
	//*********************************
	p_plane->free_lsb_num=parameter->block_plane*(parameter->page_block/3);//（一个plane里页的数量/3）
	p_plane->active_lsb_block = 0;
	p_plane->active_csb_block = 0;
	p_plane->active_msb_block = 0;
	//*********************************

	p_plane->blk_head = (struct blk_info *)malloc(parameter->block_plane * sizeof(struct blk_info));
	alloc_assert(p_plane->blk_head,"p_plane->blk_head");
	memset(p_plane->blk_head,0,parameter->block_plane * sizeof(struct blk_info));

	//用for循环初始化各个plane上的block队列
	for(i = 0; i<parameter->block_plane; i++)
	{
		p_block = &(p_plane->blk_head[i]);//将该plane上的block队列首地址复制给p_block
		initialize_block( p_block ,parameter);
	}
	return p_plane;
}

struct die_info * initialize_die(struct die_info * p_die,struct parameter_value *parameter,long long current_time )
{
	unsigned int i;
	struct plane_info * p_plane;

	p_die->token=0;

	p_die->plane_head = (struct plane_info*)malloc(parameter->plane_die * sizeof(struct plane_info));
	alloc_assert(p_die->plane_head,"p_die->plane_head");
	memset(p_die->plane_head,0,parameter->plane_die * sizeof(struct plane_info));

	//用for循环初始化各die上的plane队列
	for (i = 0; i<parameter->plane_die; i++)
	{
		p_plane = &(p_die->plane_head[i]);//将该die上的plane队列首地址复制给p_plane
		initialize_plane(p_plane,parameter );
	}

	return p_die;
}

struct chip_info * initialize_chip(struct chip_info * p_chip,struct parameter_value *parameter,long long current_time )
{
	unsigned int i;
	struct die_info *p_die;

	//初始化chip中各个成员（除了chip上的die队列）
	p_chip->current_state = CHIP_IDLE;
	p_chip->next_state = CHIP_IDLE;
	p_chip->current_time = current_time;
	p_chip->next_state_predict_time = 0;
	p_chip->die_num = parameter->die_chip;//chip上die的数量
	p_chip->plane_num_die = parameter->plane_die;
	p_chip->block_num_plane = parameter->block_plane;
	p_chip->page_num_block = parameter->block_plane;
	p_chip->subpage_num_page = parameter->subpage_page;
	p_chip->ers_limit = parameter->ers_limit;
	p_chip->token=0;
	p_chip->ac_timing = parameter->time_characteristics;
	p_chip->read_count = 0;
	p_chip->program_count = 0;
	p_chip->erase_count = 0;

	p_chip->die_head = (struct die_info *)malloc(parameter->die_chip * sizeof(struct die_info));
	alloc_assert(p_chip->die_head,"p_chip->die_head");
	memset(p_chip->die_head,0,parameter->die_chip * sizeof(struct die_info));

	//用for循环初始化各个chip上的die队列
	for (i = 0; i<parameter->die_chip; i++)
	{
		p_die = &(p_chip->die_head[i]);//将该chip的die队列的首地址赋值给p_die
		initialize_die( p_die,parameter,current_time );
	}

	return p_chip;
}

struct ssd_info * initialize_channels(struct ssd_info * ssd )
{
	unsigned int i,j;
	struct channel_info * p_channel;
	struct chip_info * p_chip;

	// set the parameter of each channel
	for (i = 0; i< ssd->parameter->channel_number; i++)//从第一个channel开始，初始化每一个channel
	{
		p_channel = &(ssd->channel_head[i]);//把channel队列的首地址赋值给p_channel
		p_channel->chip = ssd->parameter->chip_channel[i];//把参数中的当前channel上的颗粒数设置为（实际）该channel上的颗粒数量
		p_channel->current_state = CHANNEL_IDLE;
		p_channel->next_state = CHANNEL_IDLE;

		p_channel->chip_head = (struct chip_info *)malloc(ssd->parameter->chip_channel[i]* sizeof(struct chip_info));//为当前channel上的chip队列分配空间
		alloc_assert(p_channel->chip_head,"p_channel->chip_head");
		memset(p_channel->chip_head,0,ssd->parameter->chip_channel[i]* sizeof(struct chip_info));

		for (j = 0; j< ssd->parameter->chip_channel[i]; j++)//从第一个chip开始，初始化每一个chip
		{
			p_chip = &(p_channel->chip_head[j]);//把当前channel的chip队列的首地址赋给p_chip
			initialize_chip(p_chip,ssd->parameter,ssd->current_time );
		}
	}

	return ssd;
}


/*************************************************
*将page.parameters里面的参数导入到ssd->parameter里
*modify by zhouwen
*November 8,2011
**************************************************/
struct parameter_value *load_parameters(char parameter_file[30])
{
	FILE * fp;
	FILE * fp1;
	FILE * fp2;
	//errno_t ferr;
	struct parameter_value *p;
	char buf[BUFSIZE];
	int i;
	int pre_eql,next_eql;
	int res_eql;
	char *ptr;

	p = (struct parameter_value *)malloc(sizeof(struct parameter_value));
	alloc_assert(p,"parameter_value");
	memset(p,0,sizeof(struct parameter_value));
	//p->queue_length=5;
	p->queue_length=1024;
	memset(buf,0,BUFSIZE);

	fp=fopen(parameter_file,"r");
	if(fp == NULL)
	{
		printf("the file parameter_file error!\n");
		return p;
	}
	fp1=fopen("parameters_name.txt","w");
	if(fp1==NULL)
	{
		printf("the file parameter_name error!\n");
		return p;
	}
	fp2=fopen("parameters_value.txt","w");
	if(fp2==NULL)
	{
		printf("the file parameter_value error!\n");
		return p;
	}

	p->dr_filter = 0.25;

	while(fgets(buf,200,fp)){
		if(buf[0] =='#' || buf[0] == ' ') continue;
		ptr=strchr(buf,'=');
		if(!ptr) continue;

		pre_eql = ptr - buf;
		next_eql = pre_eql + 1;

		while(buf[pre_eql-1] == ' ') pre_eql--;
		buf[pre_eql] = 0;
		if((res_eql=strcmp(buf,"chip number")) ==0){
			sscanf(buf + next_eql,"%d",&p->chip_num);
		}else if((res_eql=strcmp(buf,"dram capacity")) ==0){
			sscanf(buf + next_eql,"%d",&p->dram_capacity);
		}else if((res_eql=strcmp(buf,"cpu sdram")) ==0){
			sscanf(buf + next_eql,"%d",&p->cpu_sdram);
		}else if((res_eql=strcmp(buf,"channel number")) ==0){
			sscanf(buf + next_eql,"%d",&p->channel_number);
		}else if((res_eql=strcmp(buf,"die number")) ==0){
			sscanf(buf + next_eql,"%d",&p->die_chip);
		}else if((res_eql=strcmp(buf,"plane number")) ==0){
			sscanf(buf + next_eql,"%d",&p->plane_die);
		}else if((res_eql=strcmp(buf,"block number")) ==0){
			sscanf(buf + next_eql,"%d",&p->block_plane);
		}else if((res_eql=strcmp(buf,"page number")) ==0){
			sscanf(buf + next_eql,"%d",&p->page_block);
		}else if((res_eql=strcmp(buf,"subpage page")) ==0){
			sscanf(buf + next_eql,"%d",&p->subpage_page);
		}else if((res_eql=strcmp(buf,"page capacity")) ==0){
			sscanf(buf + next_eql,"%d",&p->page_capacity);
		}else if((res_eql=strcmp(buf,"subpage capacity")) ==0){
			sscanf(buf + next_eql,"%d",&p->subpage_capacity);
		}else if((res_eql=strcmp(buf,"t_PROG")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tPROG);
		//*******************************************************
		}else if((res_eql=strcmp(buf,"t_PROG_L")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tPROG_L);
		}else if((res_eql=strcmp(buf,"t_PROG_C")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tPROG_C);
		}else if((res_eql=strcmp(buf,"t_PROG_M")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tPROG_M);
		}else if((res_eql=strcmp(buf,"turbo_mode")) ==0){
			sscanf(buf + next_eql,"%d",&p->turbo_mode);
		}else if((res_eql=strcmp(buf,"turbo_mode_factor")) ==0){
			sscanf(buf + next_eql,"%d",&p->turbo_mode_factor);
		}else if((res_eql=strcmp(buf,"turbo_mode_factor_2")) ==0){
			sscanf(buf + next_eql,"%d",&p->turbo_mode_factor_2);
		}else if((res_eql=strcmp(buf,"fast_gc")) ==0){
			sscanf(buf + next_eql,"%d",&p->fast_gc);
		}else if((res_eql=strcmp(buf,"fast_gc_thr_1")) ==0){
			sscanf(buf + next_eql,"%f",&p->fast_gc_thr_1);
		}else if((res_eql=strcmp(buf,"fast_gc_filter_1")) ==0){
			sscanf(buf + next_eql,"%f",&p->fast_gc_filter_1);
		}else if((res_eql=strcmp(buf,"fast_gc_thr_2")) ==0){
			sscanf(buf + next_eql,"%f",&p->fast_gc_thr_2);
		}else if((res_eql=strcmp(buf,"fast_gc_filter_2")) ==0){
			sscanf(buf + next_eql,"%f",&p->fast_gc_filter_2);
		}else if((res_eql=strcmp(buf,"fast_gc_filter_idle")) ==0){
			sscanf(buf + next_eql,"%f",&p->fast_gc_filter_idle);
		}else if((res_eql=strcmp(buf,"dr_filter")) ==0){
			sscanf(buf + next_eql,"%f",&p->dr_filter);
		}else if((res_eql=strcmp(buf,"dr_switch")) ==0){
			sscanf(buf + next_eql,"%d",&p->dr_switch);
		}else if((res_eql=strcmp(buf,"dr_cycle")) ==0){
			sscanf(buf + next_eql,"%d",&p->dr_cycle);
		//*******************************************************
		}else if((res_eql=strcmp(buf,"t_DBSY")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tDBSY);
		}else if((res_eql=strcmp(buf,"t_BERS")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tBERS);
		}else if((res_eql=strcmp(buf,"t_CLS")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tCLS);
		}else if((res_eql=strcmp(buf,"t_CLH")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tCLH);
		}else if((res_eql=strcmp(buf,"t_CS")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tCS);
		}else if((res_eql=strcmp(buf,"t_CH")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tCH);
		}else if((res_eql=strcmp(buf,"t_WP")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tWP);
		}else if((res_eql=strcmp(buf,"t_ALS")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tALS);
		}else if((res_eql=strcmp(buf,"t_ALH")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tALH);
		}else if((res_eql=strcmp(buf,"t_DS")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tDS);
		}else if((res_eql=strcmp(buf,"t_DH")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tDH);
		}else if((res_eql=strcmp(buf,"t_WC")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tWC);
		}else if((res_eql=strcmp(buf,"t_WH")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tWH);
		}else if((res_eql=strcmp(buf,"t_ADL")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tADL);
		}else if((res_eql=strcmp(buf,"t_R")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tR);
		}else if((res_eql=strcmp(buf,"t_AR")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tAR);
		}else if((res_eql=strcmp(buf,"t_CLR")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tCLR);
		}else if((res_eql=strcmp(buf,"t_RR")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tRR);
		}else if((res_eql=strcmp(buf,"t_RP")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tRP);
		}else if((res_eql=strcmp(buf,"t_WB")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tWB);
		}else if((res_eql=strcmp(buf,"t_RC")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tRC);
		}else if((res_eql=strcmp(buf,"t_REA")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tREA);
		}else if((res_eql=strcmp(buf,"t_CEA")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tCEA);
		}else if((res_eql=strcmp(buf,"t_RHZ")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tRHZ);
		}else if((res_eql=strcmp(buf,"t_CHZ")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tCHZ);
		}else if((res_eql=strcmp(buf,"t_RHOH")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tRHOH);
		}else if((res_eql=strcmp(buf,"t_RLOH")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tRLOH);
		}else if((res_eql=strcmp(buf,"t_COH")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tCOH);
		}else if((res_eql=strcmp(buf,"t_REH")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tREH);
		}else if((res_eql=strcmp(buf,"t_IR")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tIR);
		}else if((res_eql=strcmp(buf,"t_RHW")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tRHW);
		}else if((res_eql=strcmp(buf,"t_WHR")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tWHR);
		}else if((res_eql=strcmp(buf,"t_WH")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tWH);
		}else if((res_eql=strcmp(buf,"t_RH")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tRH);
		}else if((res_eql=strcmp(buf,"t_RST")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_characteristics.tRST);
		}else if((res_eql=strcmp(buf,"erase limit")) ==0){
			sscanf(buf + next_eql,"%d",&p->ers_limit);
		}else if((res_eql=strcmp(buf,"flash operating current")) ==0){
			sscanf(buf + next_eql,"%lf",&p->operating_current);
		}else if((res_eql=strcmp(buf,"flash supply voltage")) ==0){
			sscanf(buf + next_eql,"%lf",&p->supply_voltage);
		}else if((res_eql=strcmp(buf,"dram active current")) ==0){
			sscanf(buf + next_eql,"%lf",&p->dram_active_current);
		}else if((res_eql=strcmp(buf,"dram standby current")) ==0){
			sscanf(buf + next_eql,"%lf",&p->dram_standby_current);
		}else if((res_eql=strcmp(buf,"dram refresh current")) ==0){
			sscanf(buf + next_eql,"%lf",&p->dram_refresh_current);
		}else if((res_eql=strcmp(buf,"dram voltage")) ==0){
			sscanf(buf + next_eql,"%lf",&p->dram_voltage);
		}else if((res_eql=strcmp(buf,"address mapping")) ==0){
			sscanf(buf + next_eql,"%d",&p->address_mapping);
		}else if((res_eql=strcmp(buf,"wear leveling")) ==0){
			sscanf(buf + next_eql,"%d",&p->wear_leveling);
		}else if((res_eql=strcmp(buf,"gc")) ==0){
			sscanf(buf + next_eql,"%d",&p->gc);
		}else if((res_eql=strcmp(buf,"clean in background")) ==0){
			sscanf(buf + next_eql,"%d",&p->clean_in_background);
		}else if((res_eql=strcmp(buf,"overprovide")) ==0){
			sscanf(buf + next_eql,"%f",&p->overprovide);
		}else if((res_eql=strcmp(buf,"gc threshold")) ==0){
			sscanf(buf + next_eql,"%f",&p->gc_threshold);
		}else if((res_eql=strcmp(buf,"buffer management")) ==0){
			sscanf(buf + next_eql,"%d",&p->buffer_management);
		}else if((res_eql=strcmp(buf,"scheduling algorithm")) ==0){
			sscanf(buf + next_eql,"%d",&p->scheduling_algorithm);
		}else if((res_eql=strcmp(buf,"quick table radio")) ==0){
			sscanf(buf + next_eql,"%f",&p->quick_radio);
		}else if((res_eql=strcmp(buf,"related mapping")) ==0){
			sscanf(buf + next_eql,"%d",&p->related_mapping);
		}else if((res_eql=strcmp(buf,"striping")) ==0){
			sscanf(buf + next_eql,"%d",&p->striping);
		}else if((res_eql=strcmp(buf,"interleaving")) ==0){
			sscanf(buf + next_eql,"%d",&p->interleaving);
		}else if((res_eql=strcmp(buf,"pipelining")) ==0){
			sscanf(buf + next_eql,"%d",&p->pipelining);
		}else if((res_eql=strcmp(buf,"time_step")) ==0){
			sscanf(buf + next_eql,"%d",&p->time_step);
		}else if((res_eql=strcmp(buf,"small large write")) ==0){
			sscanf(buf + next_eql,"%d",&p->small_large_write);
		}else if((res_eql=strcmp(buf,"active write threshold")) ==0){
			sscanf(buf + next_eql,"%d",&p->threshold_fixed_adjust);
		}else if((res_eql=strcmp(buf,"threshold value")) ==0){
			sscanf(buf + next_eql,"%f",&p->threshold_value);
		}else if((res_eql=strcmp(buf,"active write")) ==0){
			sscanf(buf + next_eql,"%d",&p->active_write);
		}else if((res_eql=strcmp(buf,"gc hard threshold")) ==0){
			sscanf(buf + next_eql,"%f",&p->gc_hard_threshold);
		}else if((res_eql=strcmp(buf,"allocation")) ==0){
			sscanf(buf + next_eql,"%d",&p->allocation_scheme);
		}else if((res_eql=strcmp(buf,"static_allocation")) ==0){
			sscanf(buf + next_eql,"%d",&p->static_allocation);
		}else if((res_eql=strcmp(buf,"dynamic_allocation")) ==0){
			sscanf(buf + next_eql,"%d",&p->dynamic_allocation);
		}else if((res_eql=strcmp(buf,"advanced command")) ==0){
			sscanf(buf + next_eql,"%d",&p->advanced_commands);
		}else if((res_eql=strcmp(buf,"advanced command priority")) ==0){
			sscanf(buf + next_eql,"%d",&p->ad_priority);
		}else if((res_eql=strcmp(buf,"advanced command priority2")) ==0){
			sscanf(buf + next_eql,"%d",&p->ad_priority2);
		}else if((res_eql=strcmp(buf,"greed CB command")) ==0){
			sscanf(buf + next_eql,"%d",&p->greed_CB_ad);
		}else if((res_eql=strcmp(buf,"greed MPW command")) ==0){
			sscanf(buf + next_eql,"%d",&p->greed_MPW_ad);
		}else if((res_eql=strcmp(buf,"aged")) ==0){
			sscanf(buf + next_eql,"%d",&p->aged);
		}else if((res_eql=strcmp(buf,"aged ratio")) ==0){
			sscanf(buf + next_eql,"%f",&p->aged_ratio);
		}else if((res_eql=strcmp(buf,"queue_length")) ==0){
			sscanf(buf + next_eql,"%d",&p->queue_length);
		}else if((res_eql=strncmp(buf,"chip number",11)) ==0)
		{
			sscanf(buf+12,"%d",&i);
			sscanf(buf + next_eql,"%d",&p->chip_channel[i]);
		}else{
			printf("don't match\t %s\n",buf);
		}

		memset(buf,0,BUFSIZE);

	}
	fclose(fp);
	fclose(fp1);
	fclose(fp2);

	return p;
}


