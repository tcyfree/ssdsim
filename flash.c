/*****************************************************************************************************************************
This project was supported by the National Basic Research 973 Program of China under Grant No.2011CB302301
Huazhong University of Science and Technology (HUST)   Wuhan National Laboratory for Optoelectronics

FileName： flash.c
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

#include "flash.h"
#include "ssd.h"

/**
 * @brief 增加计数
 * 
 * @param ssd 
 * @param opt 1 读 2 写 3 写hdd 4 热数据
 */
void opt_seq_write(struct seq_write *seq_write, unsigned int opt)
{
	switch (opt)
	{
	case 1:

		seq_write->read_num++;

		break;
	case 2:
		seq_write->write_num++;

		break;
	case 3:

		seq_write->hdd_num++;

		break;
	case 4:

		seq_write->hot_num++;

		break;

	default:
		break;
	}
}
/**
 * @brief 只记录被连续写下去的lpn
 *
 * @param ssd
 * @param lpn
 * @param opt 1 读 2 写 3 写hdd 4 热数据
 */
void record_seq_write(struct ssd_info *ssd, unsigned int lpn, unsigned int opt)
{
	struct seq_write *seq_write = NULL;
	if (ssd->seq_write_tail == NULL)
	{
		if (opt != 4)
		{
			return;
		}
		// printf("0-record_seq_write: %d lpn: %d\n", opt, lpn);
		ssd->seq_write_queue = (struct seq_write *)malloc(sizeof(struct seq_write));
		alloc_assert(ssd->seq_write_queue, "ssd->seq_write_queue");
		ssd->seq_write_queue->lpn = lpn;
		ssd->seq_write_queue->read_num = 0;
		ssd->seq_write_queue->write_num = 0;
		ssd->seq_write_queue->hdd_num = 0;
		ssd->seq_write_queue->hot_num = 0;
		opt_seq_write(ssd->seq_write_queue, opt);
		ssd->seq_write_queue->next = NULL;
		ssd->seq_write_tail = ssd->seq_write_queue;
		ssd->seq_write_head = ssd->seq_write_queue;
	}
	else
	{
		seq_write = (struct seq_write *)malloc(sizeof(struct seq_write));
		alloc_assert(seq_write, "seq_write");
		int flag = 0;
		struct seq_write *write = ssd->seq_write_head;
		while (write)
		{
			if (write->lpn == lpn)
			{
				// printf("2-record_seq_write: %d lpn: %d\n", opt, lpn);
				opt_seq_write(write, opt);
				flag = 1;
				break;
			}
			write = write->next;
		}
		if (flag != 1)
		{
			if (opt != 4)
			{
				return;
			}
			// printf("1-record_seq_write: %d lpn: %d\n", opt, lpn);
			seq_write->read_num = 0;
			seq_write->write_num = 0;
			seq_write->hdd_num = 0;
			seq_write->hot_num = 0;
			seq_write->lpn = lpn;
			opt_seq_write(seq_write, opt);
			seq_write->next = ssd->seq_write_tail->next;
			ssd->seq_write_tail->next = seq_write;
		}
	}
}

/**********************
*这个函数只作用于写请求
***********************/
Status allocate_location(struct ssd_info * ssd ,struct sub_request *sub_req)
{
	struct sub_request * update=NULL;
	unsigned int channel_num=0,chip_num=0,die_num=0,plane_num=0;
	struct local *location=NULL;

	channel_num=ssd->parameter->channel_number;
	chip_num=ssd->parameter->chip_channel[0];
	die_num=ssd->parameter->die_chip;
	plane_num=ssd->parameter->plane_die;


	if (ssd->parameter->allocation_scheme==0)                                          /*动态分配的情况*/
	{
		/******************************************************************
		* 在动态分配中，因为页的更新操作使用不了copyback操作，
		*需要产生一个读请求，并且只有这个读请求完成后才能进行这个页的写操作
		*******************************************************************/
		if (ssd->dram->map->map_entry[sub_req->lpn].state!=0)
		{
			if ((sub_req->state&ssd->dram->map->map_entry[sub_req->lpn].state)!=ssd->dram->map->map_entry[sub_req->lpn].state)
			{
				ssd->read_count++;
				ssd->update_read_count++;

				update=(struct sub_request *)malloc(sizeof(struct sub_request));
				alloc_assert(update,"update");
				memset(update,0, sizeof(struct sub_request));

				if(update==NULL)
				{
					return ERROR;
				}
				update->location=NULL;
				update->next_node=NULL;
				update->next_subs=NULL;
				update->update=NULL;
				location = find_location(ssd,ssd->dram->map->map_entry[sub_req->lpn].pn);
				update->location=location;
				update->begin_time = ssd->current_time;
				update->current_state = SR_WAIT;
				update->current_time=MAX_INT64;
				update->next_state = SR_R_C_A_TRANSFER;
				update->next_state_predict_time=MAX_INT64;
				update->lpn = sub_req->lpn;
				update->state=((ssd->dram->map->map_entry[sub_req->lpn].state^sub_req->state)&0x7fffffff);
				update->size=size(update->state);
				update->ppn = ssd->dram->map->map_entry[sub_req->lpn].pn;
				update->operation = READ;

				if (ssd->channel_head[location->channel].subs_r_tail!=NULL)            /*产生新的读请求，并且挂到channel的subs_r_tail队列尾*/
				{
						ssd->channel_head[location->channel].subs_r_tail->next_node=update;
						ssd->channel_head[location->channel].subs_r_tail=update;
				}
				else
				{
					ssd->channel_head[location->channel].subs_r_tail=update;
					ssd->channel_head[location->channel].subs_r_head=update;
				}
			}
		}
		/***************************************
		*一下是动态分配的几种情况
		*0：全动态分配
		*1：表示channel定package，die，plane动态
		****************************************/
		switch(ssd->parameter->dynamic_allocation)
		{
			case 0:
			{
				sub_req->location->channel=-1;
				sub_req->location->chip=-1;
				sub_req->location->die=-1;
				sub_req->location->plane=-1;
				sub_req->location->block=-1;
				sub_req->location->page=-1;

				if (ssd->subs_w_tail!=NULL)
				{
					ssd->subs_w_tail->next_node=sub_req;
					ssd->subs_w_tail=sub_req;
				}
				else
				{
					ssd->subs_w_tail=sub_req;
					ssd->subs_w_head=sub_req;
				}

				if (update!=NULL)
				{
					sub_req->update=update;
				}

				break;
			}
			case 1:
			{

				sub_req->location->channel=sub_req->lpn%ssd->parameter->channel_number;
				sub_req->location->chip=-1;
				sub_req->location->die=-1;
				sub_req->location->plane=-1;
				sub_req->location->block=-1;
				sub_req->location->page=-1;

				if (update!=NULL)
				{
					sub_req->update=update;
				}

				break;
			}
			case 2:
			{
				break;
			}
			case 3:
			{
				break;
			}
		}

	}
	else
	{   /***************************************************************************
		*是静态分配方式，所以可以将这个子请求的最终channel，chip，die，plane全部得出
		*总共有0,1,2,3,4,5,这六种静态分配方式。
		****************************************************************************/
		switch (ssd->parameter->static_allocation)
		{
			case 0:         //no striping static allocation
			{
				sub_req->location->channel=(sub_req->lpn/(plane_num*die_num*chip_num))%channel_num;
				sub_req->location->chip=sub_req->lpn%chip_num;
				sub_req->location->die=(sub_req->lpn/chip_num)%die_num;
				sub_req->location->plane=(sub_req->lpn/(die_num*chip_num))%plane_num;
				break;
			}
			case 1:
			{
				sub_req->location->channel=sub_req->lpn%channel_num;
				sub_req->location->chip=(sub_req->lpn/channel_num)%chip_num;
				sub_req->location->die=(sub_req->lpn/(chip_num*channel_num))%die_num;
				sub_req->location->plane=(sub_req->lpn/(die_num*chip_num*channel_num))%plane_num;

				break;
			}
			case 2:
			{
				sub_req->location->channel=sub_req->lpn%channel_num;
				sub_req->location->chip=(sub_req->lpn/(plane_num*channel_num))%chip_num;
				sub_req->location->die=(sub_req->lpn/(plane_num*chip_num*channel_num))%die_num;
				sub_req->location->plane=(sub_req->lpn/channel_num)%plane_num;
				break;
			}
			case 3:
			{
				sub_req->location->channel=sub_req->lpn%channel_num;
				sub_req->location->chip=(sub_req->lpn/(die_num*channel_num))%chip_num;
				sub_req->location->die=(sub_req->lpn/channel_num)%die_num;
				sub_req->location->plane=(sub_req->lpn/(die_num*chip_num*channel_num))%plane_num;
				break;
			}
			case 4:
			{
				sub_req->location->channel=sub_req->lpn%channel_num;
				sub_req->location->chip=(sub_req->lpn/(plane_num*die_num*channel_num))%chip_num;
				sub_req->location->die=(sub_req->lpn/(plane_num*channel_num))%die_num;
				sub_req->location->plane=(sub_req->lpn/channel_num)%plane_num;

				break;
			}
			case 5:
			{
				sub_req->location->channel=sub_req->lpn%channel_num;
				sub_req->location->chip=(sub_req->lpn/(plane_num*die_num*channel_num))%chip_num;
				sub_req->location->die=(sub_req->lpn/channel_num)%die_num;
				sub_req->location->plane=(sub_req->lpn/(die_num*channel_num))%plane_num;

				break;
			}
			default : return ERROR;

		}
		if (ssd->dram->map->map_entry[sub_req->lpn].state!=0)
		{                                                                              /*这个写回的子请求的逻辑页不可以覆盖之前被写回的数据 需要产生读请求*/
			if ((sub_req->state&ssd->dram->map->map_entry[sub_req->lpn].state)!=ssd->dram->map->map_entry[sub_req->lpn].state)
			{
				ssd->read_count++;
				ssd->update_read_count++;
				update=(struct sub_request *)malloc(sizeof(struct sub_request));
				alloc_assert(update,"update");
				memset(update,0, sizeof(struct sub_request));

				if(update==NULL)
				{
					return ERROR;
				}
				update->location=NULL;
				update->next_node=NULL;
				update->next_subs=NULL;
				update->update=NULL;
				location = find_location(ssd,ssd->dram->map->map_entry[sub_req->lpn].pn);
				update->location=location;
				update->begin_time = ssd->current_time;
				update->current_state = SR_WAIT;
				update->current_time=MAX_INT64;
				update->next_state = SR_R_C_A_TRANSFER;
				update->next_state_predict_time=MAX_INT64;
				update->lpn = sub_req->lpn;
				update->state=((ssd->dram->map->map_entry[sub_req->lpn].state^sub_req->state)&0x7fffffff);
				update->size=size(update->state);
				update->ppn = ssd->dram->map->map_entry[sub_req->lpn].pn;
				update->operation = READ;

				if (ssd->channel_head[location->channel].subs_r_tail!=NULL)
				{
					ssd->channel_head[location->channel].subs_r_tail->next_node=update;
					ssd->channel_head[location->channel].subs_r_tail=update;
				}
				else
				{
					ssd->channel_head[location->channel].subs_r_tail=update;
					ssd->channel_head[location->channel].subs_r_head=update;
				}
			}

			if (update!=NULL)
			{
				sub_req->update=update;

				sub_req->state=(sub_req->state|update->state);
				sub_req->size=size(sub_req->state);
			}

 		}
	}
	if ((ssd->parameter->allocation_scheme!=0)||(ssd->parameter->dynamic_allocation!=0))
	{
		if (ssd->channel_head[sub_req->location->channel].subs_w_tail!=NULL)
		{
			ssd->channel_head[sub_req->location->channel].subs_w_tail->next_node=sub_req;
			ssd->channel_head[sub_req->location->channel].subs_w_tail=sub_req;
		}
		else
		{
			ssd->channel_head[sub_req->location->channel].subs_w_tail=sub_req;
			ssd->channel_head[sub_req->location->channel].subs_w_head=sub_req;
		}
	}
	return SUCCESS;
}


/*******************************************************************************
*insert2buffer这个函数是专门为写请求分配子请求服务的在buffer_management中被调用。
********************************************************************************/
struct ssd_info * insert2buffer(struct ssd_info *ssd,unsigned int lpn,int state,struct sub_request *sub,struct request *req)
{
	int write_back_count,flag=0;                                                             /*flag表示为写入新数据腾空间是否完成，0表示需要进一步腾，1表示已经腾空*/
	unsigned int i,lsn,hit_flag,add_flag,sector_count,active_region_flag=0,free_sector=0;
	struct buffer_group *buffer_node=NULL,*pt,*new_node=NULL,key;
	struct sub_request *sub_req=NULL,*update=NULL;


	unsigned int sub_req_state=0, sub_req_size=0,sub_req_lpn=0;

	#ifdef DEBUG
	printf("enter insert2buffer,  current time:%lld, lpn:%d, state:%d,\n",ssd->current_time,lpn,state);
	#endif

	sector_count=size(state);                                                                /*需要写到buffer的sector个数*/
	key.group=lpn;
	buffer_node= (struct buffer_group*)avlTreeFind(ssd->dram->buffer, (TREE_NODE *)&key);    /*在平衡二叉树中寻找buffer node*/

	/************************************************************************************************
	*没有命中。
	*第一步根据这个lpn有多少子页需要写到buffer，去除已写回的lsn，为该lpn腾出位置，
	*首先即要计算出free sector（表示还有多少可以直接写的buffer节点）。
	*如果free_sector>=sector_count，即有多余的空间够lpn子请求写，不需要产生写回请求
	*否则，没有多余的空间供lpn子请求写，这时需要释放一部分空间，产生写回请求。就要creat_sub_request()
	*************************************************************************************************/
	if(buffer_node==NULL)//未命中（缓存里没有以前版本的数据，即不是在缓存里的更新操作）
	{
		free_sector=ssd->dram->buffer->max_buffer_sector-ssd->dram->buffer->buffer_sector_count;//计算缓存里有多少空闲空间
		if(free_sector>=sector_count)
		{
			flag=1;//空间足够，可以直接写进缓存，不需要腾空间
		}
		if(flag==0)//若空间不够
		{
			write_back_count=sector_count-free_sector;//缓存里面需要写回闪存的个数=不够的个数（尽量少）
			ssd->dram->buffer->write_miss_hit=ssd->dram->buffer->write_miss_hit+write_back_count;//更新写未命中的数量
			while(write_back_count>0)
			{
				sub_req=NULL;
				sub_req_state=ssd->dram->buffer->buffer_tail->stored;
				sub_req_size=size(ssd->dram->buffer->buffer_tail->stored);
				sub_req_lpn=ssd->dram->buffer->buffer_tail->group;
				sub_req=creat_sub_request(ssd,sub_req_lpn,sub_req_size,sub_req_state,req,WRITE,0);

				/**********************************************************************************
				*req不为空，表示这个insert2buffer函数是在buffer_management中调用，传递了request进来
				*req为空，表示这个函数是在process函数中处理一对多映射关系的读的时候，需要将这个读出
				*的数据加到buffer中，这可能产生实时的写回操作，需要将这个实时的写回操作的子请求挂在
				*这个读请求的总请求上
				***********************************************************************************/
				if(req!=NULL)
				{
				}
				else
				{
					sub_req->next_subs=sub->next_subs;
					sub->next_subs=sub_req;
				}

				/*********************************************************************
				*写请求插入到了平衡二叉树，这时就要修改dram的buffer_sector_count；
				*维持平衡二叉树调用avlTreeDel()和AVL_TREENODE_FREE()函数；维持LRU算法；
				**********************************************************************/
				ssd->dram->buffer->buffer_sector_count=ssd->dram->buffer->buffer_sector_count-sub_req->size;
				pt = ssd->dram->buffer->buffer_tail;
				avlTreeDel(ssd->dram->buffer, (TREE_NODE *) pt);
				if(ssd->dram->buffer->buffer_head->LRU_link_next == NULL){
					ssd->dram->buffer->buffer_head = NULL;
					ssd->dram->buffer->buffer_tail = NULL;
				}else{
					ssd->dram->buffer->buffer_tail=ssd->dram->buffer->buffer_tail->LRU_link_pre;
					ssd->dram->buffer->buffer_tail->LRU_link_next=NULL;
				}
				pt->LRU_link_next=NULL;
				pt->LRU_link_pre=NULL;
				AVL_TREENODE_FREE(ssd->dram->buffer, (TREE_NODE *) pt);
				pt = NULL;

				write_back_count=write_back_count-sub_req->size; /*因为产生了实时写回操作，需要将主动写回操作区域增加*/
			}
		}

		/******************************************************************************
		*生成一个buffer node，根据这个页的情况分别赋值个各个成员，添加到队首和二叉树中
		*******************************************************************************/
		new_node=NULL;
		new_node=(struct buffer_group *)malloc(sizeof(struct buffer_group));
		alloc_assert(new_node,"buffer_group_node");
		memset(new_node,0, sizeof(struct buffer_group));

		new_node->group=lpn;//把该lpn设置为缓存新节点
		new_node->stored=state;//判断扇区是不是在缓存里面，1在，0不在
		new_node->dirty_clean=state;//判断子页是否被更新了
		new_node->LRU_link_pre = NULL;
		new_node->LRU_link_next=ssd->dram->buffer->buffer_head;//新节点插到队头
		if(ssd->dram->buffer->buffer_head != NULL){//有队头，就插到队头前面
			ssd->dram->buffer->buffer_head->LRU_link_pre=new_node;
		}else{//没有队头，该节点就是队头,同时也是队尾。因为队列就一个节点
			ssd->dram->buffer->buffer_tail = new_node;
		}
		ssd->dram->buffer->buffer_head=new_node;
		new_node->LRU_link_pre=NULL;
		avlTreeAdd(ssd->dram->buffer, (TREE_NODE *) new_node);
		ssd->dram->buffer->buffer_sector_count += sector_count;
	}
	/****************************************************************************************
	*在buffer中命中的情况
	*虽然命中了，但是命中的只是lpn，有可能新来的写请求，只是需要写lpn这一page的某几个sub_page
	*这时有需要进一步的判断
	*****************************************************************************************/
	else
	{
		for(i=0;i<ssd->parameter->subpage_page;i++)
		{
			/*************************************************************
			*判断state第i位是不是1（第i个子页要不要处理，1要处理。0不处理）
			*并且判断第i个sector是否存在buffer中，1表示存在，0表示不存在。
			**************************************************************/
			if((state>>i)%2!=0)//=1要处理
			{
				lsn=lpn*ssd->parameter->subpage_page+i;
				hit_flag=0;//判断第i个lsn有没有命中的变量，1为命中，0为未命中
				hit_flag=(buffer_node->stored)&(0x00000001<<i);

				if(hit_flag!=0)	//lsn命中了			           /*命中了，需要将该节点移到buffer的队首，并且将命中的lsn进行标记*/
				{
					active_region_flag=1;              /*用来记录在这个buffer node中的lsn是否被命中，用于后面对阈值的判定*/

					/*（我觉得跟上面的判断req一致）*********************************************************************************
					*req不为空，表示这个insert2buffer函数是在buffer_management中调用，传递了request进来
					*req为空，表示这个函数是在process函数中处理一对多映射关系的读的时候，需要将这个读出
					*的数据加到buffer中，这可能产生实时的写回操作，需要将这个实时的写回操作的子请求挂在
					*这个读请求的总请求上
					**********************************************************************************)*/
					if(req!=NULL)//
					{
						if(ssd->dram->buffer->buffer_head!=buffer_node)
						{
							if(ssd->dram->buffer->buffer_tail==buffer_node)
							{
								ssd->dram->buffer->buffer_tail=buffer_node->LRU_link_pre;
								buffer_node->LRU_link_pre->LRU_link_next=NULL;
							}
							else if(buffer_node != ssd->dram->buffer->buffer_head)
							{
								buffer_node->LRU_link_pre->LRU_link_next=buffer_node->LRU_link_next;
								buffer_node->LRU_link_next->LRU_link_pre=buffer_node->LRU_link_pre;
							}
							//将该节点移到buffer的队首
							buffer_node->LRU_link_next=ssd->dram->buffer->buffer_head;//双向链表重新设置队头 的后继设置
							ssd->dram->buffer->buffer_head->LRU_link_pre=buffer_node;//双向链表重新设置队头 的前驱设置
							buffer_node->LRU_link_pre=NULL;
							ssd->dram->buffer->buffer_head=buffer_node;
						}
						ssd->dram->buffer->write_hit++;//该lsn命中了，缓存命中率++
						req->complete_lsn_count++;  /*关键 当在buffer中命中时 就用req->complete_lsn_count++表示往buffer中写了数据。*/
					}
					else
					{
					}
				}
				else
				{
					/************************************************************************************************************
					*该lsn没有命中，但是节点在buffer中，需要将这个lsn加到buffer的对应节点中
					*从buffer的末端找一个节点，将一个已经写回的lsn从节点中删除(如果找到的话)，更改这个节点的状态，同时将这个新的
					*lsn加到相应的buffer节点中，该节点可能在buffer头，不在的话，将其移到头部。如果没有找到已经写回的lsn，在buffer
					*节点找一个group整体写回，将这个子请求挂在这个请求上。可以提前挂在一个channel上。
					*第一步:将buffer队尾的已经写回的节点删除一个，为新的lsn腾出空间，这里需要修改队尾某节点的stored状态这里还需要
					*      增加，当没有可以之间删除的lsn时，需要产生新的写子请求，写回LRU最后的节点。
					*第二步:将新的lsn加到所述的buffer节点中。
					*************************************************************************************************************/
					ssd->dram->buffer->write_miss_hit++;//该lsn未命中，则未命中率++

					if(ssd->dram->buffer->buffer_sector_count>=ssd->dram->buffer->max_buffer_sector)//缓存的扇区数目达到上限
					{
						if (buffer_node==ssd->dram->buffer->buffer_tail)  /*如果命中的节点是buffer中最后一个节点，交换最后两个节点*/
						{
							pt = ssd->dram->buffer->buffer_tail->LRU_link_pre;
							ssd->dram->buffer->buffer_tail->LRU_link_pre=pt->LRU_link_pre;
							ssd->dram->buffer->buffer_tail->LRU_link_pre->LRU_link_next=ssd->dram->buffer->buffer_tail;
							ssd->dram->buffer->buffer_tail->LRU_link_next=pt;
							pt->LRU_link_next=NULL;
							pt->LRU_link_pre=ssd->dram->buffer->buffer_tail;
							ssd->dram->buffer->buffer_tail=pt;

						}
						sub_req=NULL;
						sub_req_state=ssd->dram->buffer->buffer_tail->stored;
						sub_req_size=size(ssd->dram->buffer->buffer_tail->stored);
						sub_req_lpn=ssd->dram->buffer->buffer_tail->group;
						sub_req=creat_sub_request(ssd,sub_req_lpn,sub_req_size,sub_req_state,req,WRITE,0);

						if(req!=NULL)
						{

						}
						else if(req==NULL)
						{
							sub_req->next_subs=sub->next_subs;
							sub->next_subs=sub_req;
						}

						ssd->dram->buffer->buffer_sector_count=ssd->dram->buffer->buffer_sector_count-sub_req->size;
						pt = ssd->dram->buffer->buffer_tail;
						avlTreeDel(ssd->dram->buffer, (TREE_NODE *) pt);//平衡树中删除节点

						/************************************************************************/
						/* 改:  挂在了子请求，buffer的节点不应立即删除，						*/
						/*			需等到写回了之后才能删除									*/
						/************************************************************************/
						if(ssd->dram->buffer->buffer_head->LRU_link_next == NULL)
						{
							ssd->dram->buffer->buffer_head = NULL;
							ssd->dram->buffer->buffer_tail = NULL;
						}
						else
						{
							ssd->dram->buffer->buffer_tail=ssd->dram->buffer->buffer_tail->LRU_link_pre;
							ssd->dram->buffer->buffer_tail->LRU_link_next=NULL;
						}
						pt->LRU_link_next=NULL;
						pt->LRU_link_pre=NULL;
						AVL_TREENODE_FREE(ssd->dram->buffer, (TREE_NODE *) pt);
						pt = NULL;
					}

					                                                                     /*第二步:将新的lsn加到所述的buffer节点中*/
					add_flag=0x00000001<<(lsn%ssd->parameter->subpage_page);

					if(ssd->dram->buffer->buffer_head!=buffer_node)                      /*如果该buffer节点不在buffer的队首，需要将这个节点提到队首*/
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
					buffer_node->stored=buffer_node->stored|add_flag;
					buffer_node->dirty_clean=buffer_node->dirty_clean|add_flag;
					ssd->dram->buffer->buffer_sector_count++;
				}

			}
		}
	}

	return ssd;
}

/*=====================================================================================
*优先寻找有空闲LSB page的活跃块
=====================================================================================*/
Status  find_active_block_lf(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane)
{
	unsigned int active_lsb_block;
	unsigned int free_lsb_num=0;
	unsigned int last_write_lsb = 0;
	unsigned int fast_erase, dr_state;
	unsigned int count=0;
	unsigned int temp_block;

	struct plane_info candidate_plane = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane];

	active_lsb_block = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_lsb_block;
	/*
	free_lsb_num = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_lsb_block].free_lsb_num;
	fast_erase = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_lsb_block].fast_erase;
	dr_state = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_lsb_block].dr_state;
	*/
	for(count=0;count<ssd->parameter->block_plane; count++){
		temp_block = (active_lsb_block+count)%ssd->parameter->block_plane;
		if(candidate_plane.blk_head[temp_block].free_lsb_num <= 0) continue;
		if(candidate_plane.blk_head[temp_block].fast_erase == TRUE) continue;
		//if(candidate_plane.blk_head[temp_block].free_lsb_num != 0) continue;
		if(candidate_plane.blk_head[temp_block].dr_state == DR_STATE_OUTPUT) continue;
		if(candidate_plane.blk_head[temp_block].last_write_lsb+3 > ssd->parameter->page_block) continue;
		break;
		}
	active_lsb_block = temp_block;
	/*
	while((free_lsb_num <= 0)&&(count < ssd->parameter->block_plane)){
		active_lsb_block = (active_lsb_block+1)%ssd->parameter->block_plane;
		free_lsb_num = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_lsb_block].free_lsb_num;
		last_write_lsb = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_lsb_block].last_write_lsb;
		fast_erase = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_lsb_block].fast_erase;
	    dr_state = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_lsb_block].dr_state;
		if (last_write_lsb != (ssd->parameter->page_block-2*free_lsb_num-2)){
			printf("ERROR, the free page number is NOT corresponding to the last write page.\n");
			}
		count++;
		}
	*/

	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_lsb_block = active_lsb_block;

	if(count < ssd->parameter->block_plane){
		//printf("find active_lsb_block SUCCESS, %d, %d, %d, %d, %d,free_lsb: %d.\n", channel,chip,die,plane,active_lsb_block,free_lsb_num);
		return SUCCESS_LSB;
		}
	else{
		return find_active_block_csb(ssd, channel, chip, die, plane);
		}
}

/*=====================================================================================
*没有成功找到有空闲LSB page的块，接下来寻找有空闲CSB page的块
=====================================================================================*/
Status  find_active_block_csb(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane)
{
	unsigned int active_csb_block, temp_block;
	unsigned int free_csb_num=0;
	unsigned int count=0;
	struct plane_info candidate_plane;
	candidate_plane = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane];
	active_csb_block = candidate_plane.active_csb_block;
	//free_msb_num = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_msb_block].free_msb_num;

	for(count=0; count < ssd->parameter->block_plane; count++){
		temp_block = (active_csb_block+count)%ssd->parameter->block_plane;
		if(candidate_plane.blk_head[temp_block].free_csb_num <= 0) continue;
		if(candidate_plane.blk_head[temp_block].fast_erase == TRUE) continue;
		//if(candidate_plane.blk_head[temp_block].free_lsb_num != 0) continue;
		if(candidate_plane.blk_head[temp_block].last_write_lsb < candidate_plane.blk_head[temp_block].last_write_csb) continue;
		if(candidate_plane.blk_head[temp_block].dr_state == DR_STATE_OUTPUT) continue;
		if(candidate_plane.blk_head[temp_block].last_write_csb+3 > ssd->parameter->page_block) continue;
		break;
		}
	active_csb_block = temp_block;
	/*
	while(((free_msb_num <= 0)||(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_msb_block].fast_erase == TRUE))&&(count < ssd->parameter->block_plane)){
		active_msb_block = (active_msb_block+1)%ssd->parameter->block_plane;
		free_msb_num = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_msb_block].free_msb_num;
		count++;
		}
	*/
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_csb_block = active_csb_block;

	if(count < ssd->parameter->block_plane){
		//printf("find active_CSB_block SUCCESS, %d.\n", active_csb_block);
		//printf("free_csb_num: %d, last_write_csb: %d.\n", candidate_plane.blk_head[temp_block].free_csb_num, candidate_plane.blk_head[temp_block].last_write_csb);
		return SUCCESS_CSB;
		}
	else{
		return find_active_block_msb(ssd, channel, chip, die, plane);
		}
}


/*=====================================================================================
*没有成功找到有空闲LSB page的块，接下来寻找有空闲MSB page的块
=====================================================================================*/
Status  find_active_block_msb(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane)
{
	unsigned int active_msb_block, temp_block;
	unsigned int free_msb_num=0;
	unsigned int count=0;
	struct plane_info candidate_plane;
	candidate_plane = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane];
	active_msb_block = candidate_plane.active_msb_block;
	//free_msb_num = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_msb_block].free_msb_num;

	for(count=0; count < ssd->parameter->block_plane; count++){
		temp_block = (active_msb_block+count)%ssd->parameter->block_plane;
		if(candidate_plane.blk_head[temp_block].free_msb_num <= 0) continue;
		if(candidate_plane.blk_head[temp_block].fast_erase == TRUE) continue;
		//if(candidate_plane.blk_head[temp_block].free_lsb_num != 0) continue;
		if(candidate_plane.blk_head[temp_block].last_write_csb < candidate_plane.blk_head[temp_block].last_write_msb) continue;
		if(candidate_plane.blk_head[temp_block].dr_state == DR_STATE_OUTPUT) continue;
		if(candidate_plane.blk_head[temp_block].last_write_msb+3 > ssd->parameter->page_block) continue;
		break;
		}
	active_msb_block = temp_block;
	/*
	while(((free_msb_num <= 0)||(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_msb_block].fast_erase == TRUE))&&(count < ssd->parameter->block_plane)){
		active_msb_block = (active_msb_block+1)%ssd->parameter->block_plane;
		free_msb_num = ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_msb_block].free_msb_num;
		count++;
		}
	*/
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_msb_block = active_msb_block;

	if(count < ssd->parameter->block_plane){
		return SUCCESS_MSB;
		}
	else{
		return FAILURE;
		}
}

/**************************************************************************************
*函数的功能是寻找活跃快，因为每个plane中都只有一个活跃块，只有这个活跃块中才能进行操作
***************************************************************************************/
Status  find_active_block(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane)
{
	unsigned int active_block;
	unsigned int free_page_num=0;
	unsigned int count=0;
	unsigned int gc_processing=FALSE;

	active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;
	free_page_num=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num;//（该块里的空闲页数）
	gc_processing=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].fast_erase;
	//last_write_page=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num;
	while((free_page_num==0)&&(count<ssd->parameter->block_plane)&&(!gc_processing))//判断当前选中的块的free_page_num是否为0且活跃块active_block的计数是否超过了该plane的block数量上限。若是则看下一个块
	{
		active_block=(active_block+1)%ssd->parameter->block_plane;
		free_page_num=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num;
		gc_processing=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].fast_erase;
		count++;
	}
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block=active_block;
	if(count<ssd->parameter->block_plane)
	{
		return SUCCESS;
	}
	else
	{
		return FAILURE;
	}
}

Status  find_active_block_dr(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane)
{
	unsigned int active_block;
	unsigned int free_page_num=0;
	unsigned int invalid_page_num=0;
	unsigned int dr_state;
	unsigned int count=0;

	active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;
	free_page_num=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num;
	invalid_page_num=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].invalid_page_num;
	dr_state=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].dr_state;
	//last_write_page=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num;
	while((free_page_num==0)&&(count<ssd->parameter->block_plane)&&(invalid_page_num!=0)&&(dr_state==DR_STATE_OUTPUT))
	{
		active_block=(active_block+1)%ssd->parameter->block_plane;
		free_page_num=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num;
		invalid_page_num=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].invalid_page_num;
		count++;
	}
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block=active_block;
	if(count<ssd->parameter->block_plane)
	{
		//ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].dr_state=DR_STATE_RECIEVE;
		return SUCCESS;
	}
	else
	{
		return FAILURE;
	}
}


/*************************************************
*这个函数的功能就是一个模拟一个实实在在的写操作
*就是更改这个page的相关参数，以及整个ssd的统计参数
**************************************************/
Status write_page(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane,unsigned int active_block,unsigned int *ppn)
{
	int last_write_page=0;
	last_write_page=++(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page);
	if(last_write_page>=(int)(ssd->parameter->page_block))
	{
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page=0;
		printf("AAA error! the last write page larger than 64!!\n");
		return ERROR;
	}

	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page--;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[last_write_page].written_count++;
	ssd->write_flash_count++;
	//====================================================================================
	if(last_write_page%3 == 0){
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_lsb_num--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_lsb_num--;
		ssd->free_lsb_count--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_lsb = last_write_page;
		ssd->write_lsb_count++;
		ssd->newest_write_lsb_count++;
		}
	else if(last_write_page%3==2){
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_msb_num--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_msb = last_write_page;
		ssd->write_msb_count++;
		ssd->free_msb_count--;
		ssd->newest_write_msb_count++;
		}
	else{
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_csb_num--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_csb = last_write_page;
		ssd->write_csb_count++;
		ssd->free_csb_count--;
		ssd->newest_write_csb_count++;
		}
	//====================================================================================
	*ppn=find_ppn(ssd,channel,chip,die,plane,active_block,last_write_page);

	return SUCCESS;
}

/**
 * @brief 记录热写数据
 * 
 * @param ssd 
 * @param lpn 
 */
void record_write_hot(struct ssd_info *ssd, unsigned int lpn)
{
	int flag=0;                                                             /*flag表示为写入新数据腾空间是否完成，0表示需要进一步腾，1表示已经腾空*/
	struct buffer_group *buffer_node=NULL,*pt,*new_node=NULL,key;
	unsigned int lsn;
	key.group=lpn;//记录当前的lpn
	buffer_node= (struct buffer_group*)avlTreeFind(ssd->avl_write->buffer, (TREE_NODE *)&key);    /*在平衡二叉树中寻找buffer node*/

	/************************************************************************************************
	 *没有命中。
	 *否则，没有多余的空间供lpn子请求写，这时需要释放一部分空间
	 *************************************************************************************************/
	if (buffer_node == NULL) //未命中（缓存里没有以前版本的数据，即不是在缓存里的更新操作）
	{
		int avl_count = avlTreeCount(ssd->avl_write->buffer);
		// printf("avl-count:%d\n", avl_count);
		if (avl_count < HOT_QUEUE_LEN)
		{
			flag = 1; //空间足够，可以直接写进缓存，不需要腾空间
		}
		if (flag == 0) //若空间不够
		{
			/*********************************************************************
				 *写请求插入到了平衡二叉树，这时就要修改avl_write的buffer_sector_count；
				 *维持平衡二叉树调用avlTreeDel()和AVL_TREENODE_FREE()函数；维持LRU算法；
			**********************************************************************/
			pt = ssd->avl_write->buffer->buffer_tail;
			avlTreeDel(ssd->avl_write->buffer, (TREE_NODE *)pt);
			if (ssd->avl_write->buffer->buffer_head->LRU_link_next == NULL)
			{
				ssd->avl_write->buffer->buffer_head = NULL;
				ssd->avl_write->buffer->buffer_tail = NULL;
			}
			else
			{
				ssd->avl_write->buffer->buffer_tail = ssd->avl_write->buffer->buffer_tail->LRU_link_pre;
				ssd->avl_write->buffer->buffer_tail->LRU_link_next = NULL;
			}
			pt->LRU_link_next = NULL;
			pt->LRU_link_pre = NULL;
			AVL_TREENODE_FREE(ssd->avl_write->buffer, (TREE_NODE *)pt);
			pt = NULL;
		}

		/******************************************************************************
		 *生成一个buffer node，根据这个页的情况分别赋值个各个成员，添加到队首和二叉树中
		 *******************************************************************************/
		new_node = NULL;
		new_node = (struct buffer_group *)malloc(sizeof(struct buffer_group));
		alloc_assert(new_node, "buffer_group_node");
		memset(new_node, 0, sizeof(struct buffer_group));

		new_node->group = lpn;		   //把该lpn设置为缓存新节点
		new_node->LRU_link_pre = NULL;
		new_node->LRU_link_next = ssd->avl_write->buffer->buffer_head; //新节点插到队头
		if (ssd->avl_write->buffer->buffer_head != NULL)
		{ //有队头，就插到队头前面
			ssd->avl_write->buffer->buffer_head->LRU_link_pre = new_node;
		}
		else
		{ //没有队头，该节点就是队头,同时也是队尾。因为队列就一个节点
			ssd->avl_write->buffer->buffer_tail = new_node;
		}
		ssd->avl_write->buffer->buffer_head = new_node;
		new_node->LRU_link_pre = NULL;
		avlTreeAdd(ssd->avl_write->buffer, (TREE_NODE *)new_node);
	}
	/****************************************************************************************
	 *在buffer中命中的情况
	 *虽然命中了，但是命中的只是lpn，有可能新来的写请求，只是需要写lpn这一page的某几个sub_page
	 *这时有需要进一步的判断
	 *****************************************************************************************/
	else
	{
		if (ssd->avl_write->buffer->buffer_head != buffer_node)
		{
			if (ssd->avl_write->buffer->buffer_tail == buffer_node)
			{
				ssd->avl_write->buffer->buffer_tail = buffer_node->LRU_link_pre;
				buffer_node->LRU_link_pre->LRU_link_next = NULL;
			}
			else if (buffer_node != ssd->avl_write->buffer->buffer_head)
			{
				buffer_node->LRU_link_pre->LRU_link_next = buffer_node->LRU_link_next;
				buffer_node->LRU_link_next->LRU_link_pre = buffer_node->LRU_link_pre;
			}
			//将该节点移到buffer的队首
			buffer_node->LRU_link_next = ssd->avl_write->buffer->buffer_head; //双向链表重新设置队头 的后继设置
			ssd->avl_write->buffer->buffer_head->LRU_link_pre = buffer_node;	 //双向链表重新设置队头 的前驱设置
			buffer_node->LRU_link_pre = NULL;
			ssd->avl_write->buffer->buffer_head = buffer_node;
		}
	}
	// struct write_hot *write_hot = NULL;
	// if (ssd->write_hot_tail == NULL)
	// {
	// 	printf("write-1 lpn:%d\n",lpn);
	// 	write_hot = (struct write_hot *)malloc(sizeof(struct write_hot));
	// 	alloc_assert(write_hot, "write_hot");
	// 	write_hot->lpn = lpn;
	// 	write_hot->next = NULL;
	// 	ssd->write_hot_tail = write_hot;
	// 	ssd->write_hot_head = write_hot;
	// 	ssd->write_hot_queue_length = 1;
	// }
	// else
	// {
	// 	write_hot = (struct write_hot *)malloc(sizeof(struct write_hot));
	// 	alloc_assert(write_hot, "write_hot");
	// 	struct write_hot *hot = ssd->write_hot_head;
	// 	struct write_hot *pre = NULL;
	// 	int flag = 0;
	// 	//该lpn是否已经存在在队了
	// 	// while (hot)
	// 	// {
	// 	// 	if (hot->lpn == lpn)
	// 	// 	{
	// 	// 		//说明是头
	// 	// 		if (pre == NULL)
	// 	// 		{
	// 	// 			ssd->write_hot_head = ssd->write_hot_head->next; 
	// 	// 		}else
	// 	// 		{
	// 	// 			pre->next = pre->next->next;
	// 	// 		}
	// 	// 		//移动到队尾
	// 	// 		ssd->write_hot_tail->next = hot;
	// 	// 		hot->next = NULL;
	// 	// 		ssd->write_hot_tail = hot;
	// 	// 		flag = 1;
	// 	// 		break;
	// 	// 	}
	// 	// 	pre = hot;
	// 	// 	hot = hot->next;
	// 	// }

	// 	if (flag == 0)
	// 	{
	// 		// printf("new write lpn:%d\n",lpn);
	// 		if (ssd->write_hot_queue_length > HOT_QUEUE_LEN)
	// 		{
	// 			// printf("len:%d\n", ssd->write_hot_queue_length);
	// 			struct write_hot *temp = NULL;
	// 			temp = ssd->write_hot_head;
	// 			ssd->write_hot_head = ssd->write_hot_head->next;
	// 			free(temp);
	// 		}
	// 		else
	// 		{
	// 			ssd->write_hot_queue_length++;
	// 		}
	// 		write_hot->lpn = lpn;
	// 		write_hot->next = NULL;
	// 		ssd->write_hot_tail->next = write_hot;
	// 		ssd->write_hot_tail = write_hot;
	// 	}
	// }
}

/**********************************************
*这个函数的功能是根据lpn，size，state创建子请求
**********************************************/
struct sub_request * creat_sub_request(struct ssd_info * ssd,unsigned int lpn,int size,unsigned int state,struct request * req,unsigned int operation, unsigned int target_page_type)
{
	struct sub_request* sub=NULL,* sub_r=NULL;
	struct channel_info * p_ch=NULL;
	struct local * loc=NULL;
	unsigned int flag=0;

	sub = (struct sub_request*)malloc(sizeof(struct sub_request));     /*申请一个子请求sub的空间*/
	alloc_assert(sub,"sub_request");
	memset(sub,0, sizeof(struct sub_request));

	if(sub==NULL)//分配失败的情况
	{
		return NULL;
	}
	//若分配成功，初始化子请求sub的成员变量值
	sub->location=NULL;
	sub->next_node=NULL;
	sub->next_subs=NULL;
	sub->update=NULL;
	sub->target_page_type = target_page_type;

	//随后，程序会进一步判断req也就是IO请求项是否为空，若非空则将新定义的sub挂接到req的sub队列队头上去
	//否则若为空则说明创建的子请求是要挂到channel中的，程序会直接进行操作类型operation的判断。
	//把当前请求插入到req的自请求队列里
	if(req!=NULL)
	{
		sub->next_subs = req->subs;
		req->subs = sub;
	}

	/*************************************************************************************
	*在读操作的情况下，有一点非常重要就是要预先判断读子请求队列中是否有与这个子请求相同的，
	*有的话，新子请求就不必再执行了，将新的子请求直接赋为完成
	**************************************************************************************/
	if (operation == READ)
	{
		//首先根据lpn在映射表里找ppn，然后根据ppn查找具体的地址（在哪一个channel、chip等）
		loc = find_location(ssd,ssd->dram->map->map_entry[lpn].pn);
		sub->location=loc;
		sub->begin_time = ssd->current_time;
		sub->current_state = SR_WAIT;
		sub->current_time=MAX_INT64;
		sub->next_state = SR_R_C_A_TRANSFER;
		sub->next_state_predict_time=MAX_INT64;
		sub->lpn = lpn;
		sub->size=size;                         /*需要计算出该子请求的请求大小*/

		p_ch = &ssd->channel_head[loc->channel];	//p_ch指向sub操作所在的通道
		sub->ppn = ssd->dram->map->map_entry[lpn].pn;
		sub->operation = READ;
		//并设置sub->state即子页状态标志等于当前dram中lpn映射表结构中的state即子页映射有效标志位
		//即让映射成功的子页可以直接提供读请求操作，
		//此处之所以映射表中有读请求的映射关系可以直接使用是因为在经过读请求与处理函数的处理后，
		//所有读请求对应的映射关系都已经经过了一定的初始化了
		sub->state=(ssd->dram->map->map_entry[lpn].state&0x7fffffff);
		sub_r=p_ch->subs_r_head; //sub_r指向目标channel的读子请求队列的队头 /*以下几行包括flag用于判断该读子请求队列中是否有与这个子请求相同的，有的话，将新的子请求直接赋为完成*/
		//开始在channel的读请求队列中寻找是否存在有对应ppn的子请求节点
		flag=0;
		while (sub_r!=NULL)
		{
			if (sub_r->ppn==sub->ppn)//在channel的读请求队列中寻找是否存在有对应ppn的子请求节点
			{						 //因为创建读子请求时需要考虑到是否在该channel下的读子请求队列中已经存在具有目标ppn的读子请求节点
				flag=1;//flag为1表示可以直接利用已经存在的读子请求节点(因为读取的都是同一个页的数据，没必要做两次重复的读取操作)
				break;
			}
			sub_r=sub_r->next_node;
		}
		if (flag==0)//如果在channel下的sub_r读子请求队列中遍历完毕后尚未找到存在对应ppn的节点
		{
			//就判断p_ch所指的channel下的读子请求队列是否为空
			if (p_ch->subs_r_tail!=NULL)//若不为空
			{
				//直接将新创建的sub插到队尾
				p_ch->subs_r_tail->next_node=sub;
				p_ch->subs_r_tail=sub;
			}
			else//若为空
			{
				//直接将新创建的sub插入空读子请求队列中
				p_ch->subs_r_head=sub;
				p_ch->subs_r_tail=sub;
			}
			record_read_hot(ssd, lpn);
			// record_seq_write(ssd, lpn, 1);
		}
		else//flag=1，可以直接利用已经存在的读子请求节点。可以直接设置sub的相关状态标识
		{
			sub->current_state = SR_R_DATA_TRANSFER;//当前状态为数据传输状态SR_R_DATA_TRANSFER
			sub->current_time=ssd->current_time;//当前时间为系统当前时间代表立即执行这个读子请求
			sub->next_state = SR_COMPLETE;//下一状态为完成状态SR_COMPLETE
			sub->next_state_predict_time=ssd->current_time+1000;//下一状态预计时间为当前时间偏移1000等
			sub->complete_time=ssd->current_time+1000;
		}
	}
	/*************************************************************************************
	*写请求的情况下，就需要利用到函数allocate_location(ssd ,sub)来处理静态分配和动态分配了
	**************************************************************************************/
	else if(operation == WRITE)
	{
		//设置相关的成员信息
		sub->ppn=0;
		sub->operation = WRITE;
		sub->location=(struct local *)malloc(sizeof(struct local));
		alloc_assert(sub->location,"sub->location");
		memset(sub->location,0, sizeof(struct local));

		sub->current_state=SR_WAIT;
		sub->current_time=ssd->current_time;
		sub->lpn=lpn;
		sub->size=size;
		sub->state=state;
		sub->begin_time=ssd->current_time;

		//调用allocate_location()函数为sub分配物理地址
		//若非ERROR则创建写子请求成功直接返回sub
		//若allocate_location()返回ERROR则说明分配失败此时需要释放malloc动态申请到的location空间，且释放sub和返回NULL表示创建写子请求失败。
		if (allocate_location(ssd ,sub)==ERROR)
		{
			free(sub->location);
			sub->location=NULL;
			free(sub);
			sub=NULL;
			return NULL;
		}
		record_write_hot(ssd, lpn);
		// record_seq_write(ssd, lpn, 2);
	}

	//operation既不为READ也不为WRITE，那么说明传入的operation有误，此时需要释放前面动态申请的location且释放sub，打印出错信息并返回NULL。
	else
	{
		free(sub->location);
		sub->location=NULL;
		free(sub);
		sub=NULL;
		printf("\nERROR ! Unexpected command.\n");
		return NULL;
	}

	return sub;
}

struct sub_request * creat_sub_request_read_hdd(struct ssd_info * ssd,unsigned int lpn,int size,unsigned int state,struct request * req,unsigned int operation, unsigned int target_page_type, int read_hdd_time)
{
	struct sub_request* sub=NULL,* sub_r=NULL;
	struct channel_info * p_ch=NULL;
	struct local * loc=NULL;
	unsigned int flag=0;

	sub = (struct sub_request*)malloc(sizeof(struct sub_request));     /*申请一个子请求sub的空间*/
	alloc_assert(sub,"sub_request");
	memset(sub,0, sizeof(struct sub_request));

	if(sub==NULL)//分配失败的情况
	{
		return NULL;
	}
	//若分配成功，初始化子请求sub的成员变量值
	sub->location=NULL;
	sub->next_node=NULL;
	sub->next_subs=NULL;
	sub->update=NULL;
	sub->target_page_type = target_page_type;

	//随后，程序会进一步判断req也就是IO请求项是否为空，若非空则将新定义的sub挂接到req的sub队列队头上去
	//否则若为空则说明创建的子请求是要挂到channel中的，程序会直接进行操作类型operation的判断。
	//把当前请求插入到req的自请求队列里
	if(req!=NULL)
	{
		sub->next_subs = req->subs;
		req->subs = sub;
	}

	/*************************************************************************************
	*在读操作的情况下，有一点非常重要就是要预先判断读子请求队列中是否有与这个子请求相同的，
	*有的话，新子请求就不必再执行了，将新的子请求直接赋为完成
	**************************************************************************************/
	if (operation == READ)
	{
		//首先根据lpn在映射表里找ppn，然后根据ppn查找具体的地址（在哪一个channel、chip等）
		loc = find_location(ssd,ssd->dram->map->map_entry[lpn].pn);
		sub->location=loc;
		sub->begin_time = ssd->current_time;
		sub->current_state = SR_WAIT;
		sub->current_time=MAX_INT64;
		sub->next_state = SR_R_C_A_TRANSFER;
		sub->next_state_predict_time=MAX_INT64;
		sub->lpn = lpn;
		sub->size=size;                         /*需要计算出该子请求的请求大小*/

		p_ch = &ssd->channel_head[loc->channel];	//p_ch指向sub操作所在的通道
		sub->ppn = ssd->dram->map->map_entry[lpn].pn;
		sub->operation = READ;
		//并设置sub->state即子页状态标志等于当前dram中lpn映射表结构中的state即子页映射有效标志位
		//即让映射成功的子页可以直接提供读请求操作，
		//此处之所以映射表中有读请求的映射关系可以直接使用是因为在经过读请求与处理函数的处理后，
		//所有读请求对应的映射关系都已经经过了一定的初始化了
		sub->state=(ssd->dram->map->map_entry[lpn].state&0x7fffffff);
		sub_r=p_ch->subs_r_head; //sub_r指向目标channel的读子请求队列的队头 /*以下几行包括flag用于判断该读子请求队列中是否有与这个子请求相同的，有的话，将新的子请求直接赋为完成*/
		//是否从HDD读数据
		if (ssd->dram->map->map_entry[lpn].hdd_flag == 1)
		{
			// 打包page被读
			ssd->read_hdd_count++;
			sub->current_state = SR_R_DATA_TRANSFER;//当前状态为数据传输状态SR_R_DATA_TRANSFER
			sub->current_time=ssd->current_time;//当前时间为系统当前时间代表立即执行这个读子请求
			sub->next_state = SR_COMPLETE;//下一状态为完成状态SR_COMPLETE
			sub->next_state_predict_time=ssd->current_time+read_hdd_time;//下一状态预计时间为当前时间偏移read_hdd_time
			sub->complete_time=ssd->current_time+read_hdd_time;
			return sub;
		}

		//开始在channel的读请求队列中寻找是否存在有对应ppn的子请求节点
		flag=0;
		while (sub_r!=NULL)
		{
			if (sub_r->ppn==sub->ppn)//在channel的读请求队列中寻找是否存在有对应ppn的子请求节点
			{						 //因为创建读子请求时需要考虑到是否在该channel下的读子请求队列中已经存在具有目标ppn的读子请求节点
				flag=1;//flag为1表示可以直接利用已经存在的读子请求节点(因为读取的都是同一个页的数据，没必要做两次重复的读取操作)
				break;
			}
			sub_r=sub_r->next_node;
		}
		if (flag==0)//如果在channel下的sub_r读子请求队列中遍历完毕后尚未找到存在对应ppn的节点
		{
			//就判断p_ch所指的channel下的读子请求队列是否为空
			if (p_ch->subs_r_tail!=NULL)//若不为空
			{
				//直接将新创建的sub插到队尾
				p_ch->subs_r_tail->next_node=sub;
				p_ch->subs_r_tail=sub;
			}
			else//若为空
			{
				//直接将新创建的sub插入空读子请求队列中
				p_ch->subs_r_head=sub;
				p_ch->subs_r_tail=sub;
			}
			record_read_hot(ssd, lpn);
			// record_seq_write(ssd, lpn, 1);
		}
		else//flag=1，可以直接利用已经存在的读子请求节点。可以直接设置sub的相关状态标识
		{
			sub->current_state = SR_R_DATA_TRANSFER;//当前状态为数据传输状态SR_R_DATA_TRANSFER
			sub->current_time=ssd->current_time;//当前时间为系统当前时间代表立即执行这个读子请求
			sub->next_state = SR_COMPLETE;//下一状态为完成状态SR_COMPLETE
			sub->next_state_predict_time=ssd->current_time+1000;//下一状态预计时间为当前时间偏移1000等
			sub->complete_time=ssd->current_time+1000;
		}
	}
	//operation既不为READ也不为WRITE，那么说明传入的operation有误，此时需要释放前面动态申请的location且释放sub，打印出错信息并返回NULL。
	else
	{
		free(sub->location);
		sub->location=NULL;
		free(sub);
		sub=NULL;
		printf("\nERROR ! Unexpected command.\n");
		return NULL;
	}

	return sub;
}

/**
 * @brief creat-sub-req-pro
 * 
 * @param ssd 
 * @param lpn 
 * @param size 
 * @param state 
 * @param req 
 * @param operation 
 * @param target_page_type 
 * @return struct sub_request* 
 */
struct sub_request * creat_sub_request_pro(struct ssd_info * ssd,unsigned int lpn,int size,unsigned int state,struct request * req,unsigned int operation, unsigned int target_page_type, unsigned flag)
{
	struct sub_request* sub=NULL,* sub_r=NULL;
	struct channel_info * p_ch=NULL;
	struct local * loc=NULL;

	sub = (struct sub_request*)malloc(sizeof(struct sub_request));     /*申请一个子请求sub的空间*/
	alloc_assert(sub,"sub_request");
	memset(sub,0, sizeof(struct sub_request));

	if(sub==NULL)//分配失败的情况
	{
		return NULL;
	}
	//若分配成功，初始化子请求sub的成员变量值
	sub->location=NULL;
	sub->next_node=NULL;
	sub->next_subs=NULL;
	sub->update=NULL;
	sub->target_page_type = target_page_type;

	//随后，程序会进一步判断req也就是IO请求项是否为空，若非空则将新定义的sub挂接到req的sub队列队头上去
	//否则若为空则说明创建的子请求是要挂到channel中的，程序会直接进行操作类型operation的判断。
	//把当前请求插入到req的自请求队列里
	if(req!=NULL)
	{
		sub->next_subs = req->subs;
		req->subs = sub;
	}

	if(operation == WRITE)
	{
		//设置相关的成员信息
		sub->ppn=0;
		sub->operation = WRITE;
		sub->location=(struct local *)malloc(sizeof(struct local));
		alloc_assert(sub->location,"sub->location");
		memset(sub->location,0, sizeof(struct local));

		sub->current_state=SR_WAIT;
		sub->current_time=ssd->current_time;
		sub->lpn=lpn;
		sub->size=size;
		sub->state=state;
		sub->begin_time=ssd->current_time;
		if (flag == 1)
		{
			sub->is_writeback = 1; //writeback flag
		} else if (flag == 2)
		{
			sub->no_ope = 1;
		} else
		{
			printf("flag is error: %d\n", flag);
		}
		
		//调用allocate_location()函数为sub分配物理地址
		//若非ERROR则创建写子请求成功直接返回sub
		//若allocate_location()返回ERROR则说明分配失败此时需要释放malloc动态申请到的location空间，且释放sub和返回NULL表示创建写子请求失败。
		if (allocate_location(ssd ,sub)==ERROR)
		{
			free(sub->location);
			sub->location=NULL;
			free(sub);
			sub=NULL;
			return NULL;
		}
		record_write_hot(ssd, lpn);
		// record_seq_write(ssd, lpn, 2);
	}

	//operation既不为READ也不为WRITE，那么说明传入的operation有误，此时需要释放前面动态申请的location且释放sub，打印出错信息并返回NULL。
	else
	{
		free(sub->location);
		sub->location=NULL;
		free(sub);
		sub=NULL;
		printf("\nERROR ! Unexpected command.\n");
		return NULL;
	}

	return sub;
}
// /**
//  * @brief 查找热度数最低的第一个，删除然后再在最后加上现在的热度数据
//  * 
//  * @param ssd 
//  * @param cold_num 
//  */
// void delete_cold_data(struct ssd_info *ssd, int cold_num)
// {
//     struct read_hot *hot = ssd->read_hot_head;
//     int flag = 0;
// 	while (hot != NULL)
// 	{
// 		if (hot->num == cold_num)
// 		{
// 			flag = 1;
// 			break;
// 		}
// 		hot = hot->next;
// 	}
// 	//删除第一个冷数据
// 	if (flag == 1)
// 	{
// 		struct read_hot *del = hot->next;
// 		hot->next = hot->next->next;
// 		free(del);
//         // if (pre_read_hot == NULL) {
//         //     hot = hot->next;
//         // }
// 		// //链表尾部
// 		// if (hot->next != NULL)
// 		// {
// 		// 	pre_read_hot->next = hot->next;
// 		// }
// 		// // printf("delete_cold_data:%d\n", cold_num);
// 		// free(hot);
// 		ssd->read_hot_queue_length--;
// //        abort();
// 		return;
// 	}
// 	cold_num ++;
// 	delete_cold_data(ssd, cold_num);
// }

// /**
//  * @brief 记录热数据
//  * 
//  * @param ssd 
//  * @param lpn 
//  */
// void record_read_hot(struct ssd_info *ssd, unsigned int lpn)
// {
// 	struct read_hot *read_hot = NULL;
// 	if (ssd->read_hot_queue_length < 4096)
// 	{
// 		if (ssd->read_tail == NULL)
// 		{
// 			ssd->read_queue = (struct read_hot *)malloc(sizeof(struct read_hot));
// 			alloc_assert(ssd->read_queue, "ssd->read_queue");
// 			ssd->read_queue->num = 1;
// 			ssd->read_queue->lpn = lpn;
// 			ssd->read_hot_queue_length = 1;
// 			ssd->read_queue->next = NULL;
// 			ssd->read_tail = ssd->read_queue;
// 			ssd->read_hot_head = ssd->read_queue;
// 		}
// 		else
// 		{
// 			read_hot = (struct read_hot *)malloc(sizeof(struct read_hot));
// 			alloc_assert(read_hot, "read_hot");
// 			int flag = 0;
// 			//是指针变量所以能修改原队列数据
// 			struct read_hot *hot = ssd->read_hot_head;
// 			while (hot)
// 			{
// 				if (hot->lpn == lpn)
// 				{
// 					hot->num++;
// 					flag = 1;
// 					break;
// 				}
// 				hot = hot->next;
// 			}
// 			if (flag != 1)
// 			{
// 				read_hot->num = 1;
// 				read_hot->lpn = lpn;
// 				ssd->read_hot_queue_length++;
// 				read_hot->next = NULL;
// 				ssd->read_tail->next = read_hot;
// 				ssd->read_tail = read_hot;
// 			}
// 		}
// 	}
// 	else
// 	{
// 		//删除一个热度最低的数据
// 		int cold_num = 1;
// 		delete_cold_data(ssd, cold_num);
// 		//插入新节点到队尾
// 		read_hot = (struct read_hot *)malloc(sizeof(struct read_hot));
// 		alloc_assert(read_hot, "read_hot");
// 		read_hot->num = 1;
// 		read_hot->lpn = lpn;
// 		ssd->read_hot_queue_length++;
// 		read_hot->next = NULL;
// 		ssd->read_tail->next = read_hot;
// 		ssd->read_tail = read_hot;
// 	}
// }

/**
 * @brief 记录热读数据
 * 
 * @param ssd 
 * @param lpn 
 */
void record_read_hot(struct ssd_info *ssd, unsigned int lpn)
{
	int flag=0;                                                             /*flag表示为写入新数据腾空间是否完成，0表示需要进一步腾，1表示已经腾空*/
	struct buffer_group *buffer_node=NULL,*pt,*new_node=NULL,key;
	unsigned int lsn;
	key.group=lpn;//记录当前的lpn
	buffer_node= (struct buffer_group*)avlTreeFind(ssd->avl_read->buffer, (TREE_NODE *)&key);    /*在平衡二叉树中寻找buffer node*/

	/************************************************************************************************
	 *没有命中。
	 *否则，没有多余的空间供lpn子请求写，这时需要释放一部分空间
	 *************************************************************************************************/
	if (buffer_node == NULL) //未命中（缓存里没有以前版本的数据，即不是在缓存里的更新操作）
	{
		int avl_count = avlTreeCount(ssd->avl_read->buffer);
		// printf("avl-count:%d\n", avl_count);
		if (avl_count < HOT_QUEUE_LEN)
		{
			flag = 1; //空间足够，可以直接写进缓存，不需要腾空间
		}
		if (flag == 0) //若空间不够
		{
			/*********************************************************************
				 *写请求插入到了平衡二叉树，这时就要修改avl_read的buffer_sector_count；
				 *维持平衡二叉树调用avlTreeDel()和AVL_TREENODE_FREE()函数；维持LRU算法；
			**********************************************************************/
			pt = ssd->avl_read->buffer->buffer_tail;
			avlTreeDel(ssd->avl_read->buffer, (TREE_NODE *)pt);
			if (ssd->avl_read->buffer->buffer_head->LRU_link_next == NULL)
			{
				ssd->avl_read->buffer->buffer_head = NULL;
				ssd->avl_read->buffer->buffer_tail = NULL;
			}
			else
			{
				ssd->avl_read->buffer->buffer_tail = ssd->avl_read->buffer->buffer_tail->LRU_link_pre;
				ssd->avl_read->buffer->buffer_tail->LRU_link_next = NULL;
			}
			pt->LRU_link_next = NULL;
			pt->LRU_link_pre = NULL;
			AVL_TREENODE_FREE(ssd->avl_read->buffer, (TREE_NODE *)pt);
			pt = NULL;
		}

		/******************************************************************************
		 *生成一个buffer node，根据这个页的情况分别赋值个各个成员，添加到队首和二叉树中
		 *******************************************************************************/
		new_node = NULL;
		new_node = (struct buffer_group *)malloc(sizeof(struct buffer_group));
		alloc_assert(new_node, "buffer_group_node");
		memset(new_node, 0, sizeof(struct buffer_group));

		new_node->group = lpn;		   //把该lpn设置为缓存新节点
		new_node->LRU_link_pre = NULL;
		new_node->LRU_link_next = ssd->avl_read->buffer->buffer_head; //新节点插到队头
		if (ssd->avl_read->buffer->buffer_head != NULL)
		{ //有队头，就插到队头前面
			ssd->avl_read->buffer->buffer_head->LRU_link_pre = new_node;
		}
		else
		{ //没有队头，该节点就是队头,同时也是队尾。因为队列就一个节点
			ssd->avl_read->buffer->buffer_tail = new_node;
		}
		ssd->avl_read->buffer->buffer_head = new_node;
		new_node->LRU_link_pre = NULL;
		avlTreeAdd(ssd->avl_read->buffer, (TREE_NODE *)new_node);
	}
	/****************************************************************************************
	 *在buffer中命中的情况
	 *虽然命中了，但是命中的只是lpn，有可能新来的写请求，只是需要写lpn这一page的某几个sub_page
	 *这时有需要进一步的判断
	 *****************************************************************************************/
	else
	{
		if (ssd->avl_read->buffer->buffer_head != buffer_node)
		{
			if (ssd->avl_read->buffer->buffer_tail == buffer_node)
			{
				ssd->avl_read->buffer->buffer_tail = buffer_node->LRU_link_pre;
				buffer_node->LRU_link_pre->LRU_link_next = NULL;
			}
			else if (buffer_node != ssd->avl_read->buffer->buffer_head)
			{
				buffer_node->LRU_link_pre->LRU_link_next = buffer_node->LRU_link_next;
				buffer_node->LRU_link_next->LRU_link_pre = buffer_node->LRU_link_pre;
			}
			//将该节点移到buffer的队首
			buffer_node->LRU_link_next = ssd->avl_read->buffer->buffer_head; //双向链表重新设置队头 的后继设置
			ssd->avl_read->buffer->buffer_head->LRU_link_pre = buffer_node;	 //双向链表重新设置队头 的前驱设置
			buffer_node->LRU_link_pre = NULL;
			ssd->avl_read->buffer->buffer_head = buffer_node;
		}
	}

	// struct read_hot *read_hot = NULL;
	// read_hot = (struct read_hot *)malloc(sizeof(struct read_hot));
	// alloc_assert(read_hot, "read_hot");
	// if (ssd->read_hot_tail == NULL)
	// {
	// 	printf("read-1 lpn:%d\n",lpn);
	// 	read_hot->lpn = lpn;
	// 	read_hot->next = NULL;
	// 	ssd->read_hot_tail = read_hot;
	// 	ssd->read_hot_head = read_hot;
	// 	ssd->read_hot_queue_length = 1;
	// }
	// else
	// {
	// 	struct read_hot *hot = ssd->read_hot_head;
	// 	struct read_hot *pre = NULL;
	// 	int flag = 0;
	// 	//该lpn是否已经存在在队了
	// 	// while (hot)
	// 	// {
	// 	// 	printf("hot lpn %d\n", hot->lpn);
	// 	// 	if (hot->lpn == lpn)
	// 	// 	{
	// 	// 		printf("exits lpn:%d\n",lpn);
	// 	// 		//说明是头
	// 	// 		if (pre == NULL)
	// 	// 		{
	// 	// 			ssd->read_hot_head = ssd->read_hot_head->next; 
	// 	// 		}else
	// 	// 		{
	// 	// 			pre->next = pre->next->next;
	// 	// 		}
	// 	// 		//移动到队尾
	// 	// 		read_hot->lpn = lpn;
	// 	// 		read_hot->next = NULL;
	// 	// 		ssd->read_hot_tail->next = read_hot;
	// 	// 		ssd->read_hot_tail = read_hot;
	// 	// 		flag = 1;
	// 	// 		break;
	// 	// 	}
	// 	// 	pre = hot;
	// 	// 	hot = hot->next;
	// 	// }
	// 	if (flag == 0)
	// 	{
	// 		// printf("new lpn:%d\n",lpn);
	// 		if (ssd->read_hot_queue_length > HOT_QUEUE_LEN)
	// 		{
	// 			struct read_hot *temp = NULL;
	// 			temp = ssd->read_hot_head;
	// 			// printf("len:%d\n", ssd->read_hot_queue_length);
	// 			// while (temp)
	// 			// {
	// 			// 	printf("lpn:%d\n",temp->lpn);
	// 			// 	temp = temp->next;
	// 			// }
	// 			// printf("delete lpn:%d %d\n",ssd->read_hot_head->lpn,lpn);
	// 			// abort();
	// 			temp = ssd->read_hot_head;
	// 			ssd->read_hot_head = ssd->read_hot_head->next;
	// 			free(temp);
	// 		}
	// 		else
	// 		{
	// 			ssd->read_hot_queue_length++;
	// 		}
	// 		read_hot->lpn = lpn;
	// 		read_hot->next = NULL;
	// 		ssd->read_hot_tail->next = read_hot;
	// 		ssd->read_hot_tail = read_hot;
	// 		// printf("new tail lpn:%d\n", ssd->read_hot_tail->lpn);
	// 	}
	// }
}

/**
 * @brief 记录更新写
 * 
 * @param ssd 
 * @param lpn 
 */
void record_update_write(struct ssd_info *ssd, unsigned int lpn)
{
	// printf("recode_update_write lpn:%d\n", lpn);
	struct update_write *update_write = NULL;
	if (ssd->update_write_tail == NULL)
	{
		ssd->update_write_queue = (struct update_write *)malloc(sizeof(struct update_write));
		alloc_assert(ssd->update_write_queue, "ssd->update_write_queue");
		ssd->update_write_queue->num = 1;
		ssd->update_write_queue->lpn = lpn;
		ssd->update_write_queue->next = NULL;
		ssd->update_write_tail = ssd->update_write_queue;
		ssd->update_write_head = ssd->update_write_queue;
	}
	else
	{
		update_write = (struct update_write *)malloc(sizeof(struct update_write));
		alloc_assert(update_write, "update_write");
		int flag = 0;
		//是指针变量所以能修改原队列数据
		struct update_write *update_write_queue = ssd->update_write_head;
		while (update_write_queue)
		{
			if (update_write_queue->lpn == lpn)
			{
				update_write_queue->num++;
				flag = 1;
				break;
			}
			update_write_queue = update_write_queue->next;
		}
		if (flag != 1)
		{
			update_write->num = 1;
			update_write->lpn = lpn;
			update_write->next = NULL;
			ssd->update_write_tail->next = update_write;
			ssd->update_write_tail = update_write;
		}
	}
}

/******************************************************
*函数的功能是在给出的channel，chip，die上面寻找读子请求
*这个子请求的ppn要与相应的plane的寄存器里面的ppn相符
*******************************************************/
struct sub_request * find_read_sub_request(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die)
{
	unsigned int plane=0;
	unsigned int address_ppn=0;
	struct sub_request *sub=NULL,* p=NULL;

	for(plane=0;plane<ssd->parameter->plane_die;plane++)
	{
		address_ppn=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].add_reg_ppn;
		if(address_ppn!=-1)
		{
			sub=ssd->channel_head[channel].subs_r_head;
			if(sub->ppn==address_ppn)
			{
				if(sub->next_node==NULL)
				{
					ssd->channel_head[channel].subs_r_head=NULL;
					ssd->channel_head[channel].subs_r_tail=NULL;
				}
				ssd->channel_head[channel].subs_r_head=sub->next_node;
			}
			while((sub->ppn!=address_ppn)&&(sub->next_node!=NULL))
			{
				if(sub->next_node->ppn==address_ppn)
				{
					p=sub->next_node;
					if(p->next_node==NULL)
					{
						sub->next_node=NULL;
						ssd->channel_head[channel].subs_r_tail=sub;
					}
					else
					{
						sub->next_node=p->next_node;
					}
					sub=p;
					break;
				}
				sub=sub->next_node;
			}
			if(sub->ppn==address_ppn)
			{
				sub->next_node=NULL;
				return sub;
			}
			else
			{
				printf("Error! Can't find the sub request.");
			}
		}
	}
	return NULL;
}

/*******************************************************************************
*函数的功能是寻找写子请求。
*分两种情况1，要是是完全动态分配就在ssd->subs_w_head队列上找
*2，要是不是完全动态分配那么就在ssd->channel_head[channel].subs_w_head队列上查找
********************************************************************************/
struct sub_request * find_write_sub_request(struct ssd_info * ssd, unsigned int channel)
{
	struct sub_request * sub=NULL,* p=NULL;
	if ((ssd->parameter->allocation_scheme==0)&&(ssd->parameter->dynamic_allocation==0))    /*是完全的动态分配*/
	{
		sub=ssd->subs_w_head;
		while(sub!=NULL)
		{
			if(sub->current_state==SR_WAIT)
			{
				if (sub->update!=NULL)                                                      /*如果有需要提前读出的页*/
				{
					if ((sub->update->current_state==SR_COMPLETE)||((sub->update->next_state==SR_COMPLETE)&&(sub->update->next_state_predict_time<=ssd->current_time)))   //被更新的页已经被读出
					{
						break;
					}
				}
				else
				{
					break;
				}
			}
			p=sub;
			sub=sub->next_node;
		}

		if (sub==NULL)
		{
			return NULL;
		}

		if (sub!=ssd->subs_w_head)
		{
			if (sub!=ssd->subs_w_tail)
			{
				p->next_node=sub->next_node;
			}
			else
			{
				ssd->subs_w_tail=p;
				ssd->subs_w_tail->next_node=NULL;
			}
		}
		else
		{
			if (sub->next_node!=NULL)
			{
				ssd->subs_w_head=sub->next_node;
			}
			else
			{
				ssd->subs_w_head=NULL;
				ssd->subs_w_tail=NULL;
			}
		}
		sub->next_node=NULL;
		if (ssd->channel_head[channel].subs_w_tail!=NULL)
		{
			ssd->channel_head[channel].subs_w_tail->next_node=sub;
			ssd->channel_head[channel].subs_w_tail=sub;
		}
		else
		{
			ssd->channel_head[channel].subs_w_tail=sub;
			ssd->channel_head[channel].subs_w_head=sub;
		}
	}
	/**********************************************************
	*除了全动态分配方式，其他方式的请求已经分配到特定的channel，
	*就只需要在channel上找出准备服务的子请求
	***********************************************************/
	else
	{
		sub=ssd->channel_head[channel].subs_w_head;
		while(sub!=NULL)
		{
			if(sub->current_state==SR_WAIT)
			{
				if (sub->update!=NULL)
				{
					if ((sub->update->current_state==SR_COMPLETE)||((sub->update->next_state==SR_COMPLETE)&&(sub->update->next_state_predict_time<=ssd->current_time)))   //被更新的页已经被读出
					{
						break;
					}
				}
				else
				{
					break;
				}
			}
			p=sub;
			sub=sub->next_node;
		}

		if (sub==NULL)
		{
			return NULL;
		}
	}

	return sub;
}

/*********************************************************************************************
*专门为读子请求服务的函数
*1，只有当读子请求的当前状态是SR_R_C_A_TRANSFER命令地址传输状态
*2，读子请求的当前状态是SR_COMPLETE或者下一状态是SR_COMPLETE并且下一状态到达的时间比当前时间小（表示事情做完了）
**********************************************************************************************/
Status services_2_r_cmd_trans_and_complete(struct ssd_info * ssd)
{
	unsigned int i=0;
	struct sub_request * sub=NULL, * p=NULL;//sub是是当前结点，p是前面的一个结点
	for(i=0;i<ssd->parameter->channel_number;i++)/*这个循环处理不需要channel的时间(读命令已经到达chip，chip由ready变为busy)，当读请求完成时，将其从channel的队列中取出*/
	{
		sub=ssd->channel_head[i].subs_r_head;//从channel上子请求队列的第一个子请求开始处理

		while(sub!=NULL)
		{//SubRequest_Read_Command_Address_Transfer(读子请求的命令地址传输状态)
			if(sub->current_state==SR_R_C_A_TRANSFER)		/*读命令发送完毕，将对应的die置为busy，同时修改sub的状态; 这个部分专门处理读请求由当前状态为传命令变为die开始busy，die开始busy不需要channel为空，所以单独列出*/
			{
				if(sub->next_state_predict_time<=ssd->current_time)//下一状态预计时间小于等于当前时间，说明它的事情做完了
				{
					go_one_step(ssd, sub,NULL, SR_R_READ,NORMAL);//状态由命令地址传输状态-->读状态    /*状态跳变处理函数*/

				}
			}
			//（当前状态是读完了）或（当前状态是读数据传输，且传输完毕）	完成后，删除此请求
			else if((sub->current_state==SR_COMPLETE)||((sub->next_state==SR_COMPLETE)&&(sub->next_state_predict_time<=ssd->current_time)))
			{
				if(sub!=ssd->channel_head[i].subs_r_head)/*if the request is completed, we delete it from read queue */
				{
					p->next_node=sub->next_node;
				}
				else
				{
					if (ssd->channel_head[i].subs_r_head!=ssd->channel_head[i].subs_r_tail)
					{
						ssd->channel_head[i].subs_r_head=sub->next_node;
					}
					else
					{
						ssd->channel_head[i].subs_r_head=NULL;
						ssd->channel_head[i].subs_r_tail=NULL;
					}
				}
			}
			p=sub;
			sub=sub->next_node;
		}
	}

	return SUCCESS;
}

/**************************************************************************
*这个函数也是只处理读子请求，处理chip当前状态是CHIP_WAIT，
*或者下一个状态是CHIP_DATA_TRANSFER并且下一状态的预计时间小于当前时间的chip
***************************************************************************/
Status services_2_r_data_trans(struct ssd_info * ssd,unsigned int channel,unsigned int * channel_busy_flag, unsigned int * change_current_time_flag)
{
	int chip=0;
	unsigned int die=0,plane=0,address_ppn=0,die1=0;
	struct sub_request * sub=NULL, * p=NULL,*sub1=NULL;
	struct sub_request * sub_twoplane_one=NULL, * sub_twoplane_two=NULL;
	struct sub_request * sub_interleave_one=NULL, * sub_interleave_two=NULL;
	for(chip=0;chip<ssd->channel_head[channel].chip;chip++)
	{
			if((ssd->channel_head[channel].chip_head[chip].current_state==CHIP_WAIT)||((ssd->channel_head[channel].chip_head[chip].next_state==CHIP_DATA_TRANSFER)&&
				(ssd->channel_head[channel].chip_head[chip].next_state_predict_time<=ssd->current_time)))
			{
				for(die=0;die<ssd->parameter->die_chip;die++)
				{
					sub=find_read_sub_request(ssd,channel,chip,die);                   /*在channel,chip,die中找到读子请求*/
					if(sub!=NULL)
					{
						break;
					}
				}

				if(sub==NULL)
				{
					continue;
				}

				/**************************************************************************************
				*如果ssd支持高级命令，那没我们可以一起处理支持AD_TWOPLANE_READ，AD_INTERLEAVE的读子请求
				*1，有可能产生了two plane操作，在这种情况下，将同一个die上的两个plane的数据依次传出
				*2，有可能产生了interleave操作，在这种情况下，将不同die上的两个plane的数据依次传出
				***************************************************************************************/
				if(((ssd->parameter->advanced_commands&AD_TWOPLANE_READ)==AD_TWOPLANE_READ)||((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE))
				{
					if ((ssd->parameter->advanced_commands&AD_TWOPLANE_READ)==AD_TWOPLANE_READ)     /*有可能产生了two plane操作，在这种情况下，将同一个die上的两个plane的数据依次传出*/
					{
						sub_twoplane_one=sub;
						sub_twoplane_two=NULL;
						                                                                            /*为了保证找到的sub_twoplane_two与sub_twoplane_one不同，令add_reg_ppn=-1*/
						ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[sub->location->plane].add_reg_ppn=-1;
						sub_twoplane_two=find_read_sub_request(ssd,channel,chip,die);               /*在相同的channel,chip,die中寻找另外一个读子请求*/

						/******************************************************
						*如果找到了那么就执行TWO_PLANE的状态转换函数go_one_step
						*如果没找到那么就执行普通命令的状态转换函数go_one_step
						******************************************************/
						if (sub_twoplane_two==NULL)
						{
							go_one_step(ssd, sub_twoplane_one,NULL, SR_R_DATA_TRANSFER,NORMAL);
							*change_current_time_flag=0;
							*channel_busy_flag=1;

						}
						else
						{
							go_one_step(ssd, sub_twoplane_one,sub_twoplane_two, SR_R_DATA_TRANSFER,TWO_PLANE);
							*change_current_time_flag=0;
							*channel_busy_flag=1;

						}
					}
					else if ((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE)      /*有可能产生了interleave操作，在这种情况下，将不同die上的两个plane的数据依次传出*/
					{
						sub_interleave_one=sub;
						sub_interleave_two=NULL;
						ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[sub->location->plane].add_reg_ppn=-1;

						for(die1=0;die1<ssd->parameter->die_chip;die1++)
						{
							if(die1!=die)
							{
								sub_interleave_two=find_read_sub_request(ssd,channel,chip,die1);    /*在相同的channel，chhip不同的die上面找另外一个读子请求*/
								if(sub_interleave_two!=NULL)
								{
									break;
								}
							}
						}
						if (sub_interleave_two==NULL)
						{
							go_one_step(ssd, sub_interleave_one,NULL, SR_R_DATA_TRANSFER,NORMAL);

							*change_current_time_flag=0;
							*channel_busy_flag=1;

						}
						else
						{
							go_one_step(ssd, sub_twoplane_one,sub_interleave_two, SR_R_DATA_TRANSFER,INTERLEAVE);

							*change_current_time_flag=0;
							*channel_busy_flag=1;

						}
					}
				}
				else                                                                                 /*如果ssd不支持高级命令那么就执行一个一个的执行读子请求*/
				{

					go_one_step(ssd, sub,NULL, SR_R_DATA_TRANSFER,NORMAL);
					*change_current_time_flag=0;
					*channel_busy_flag=1;

				}
				break;
			}

		if(*channel_busy_flag==1)
		{
			break;
		}
	}
	return SUCCESS;
}


/******************************************************
*这个函数也是只服务读子请求，并且处于等待状态的读子请求
*******************************************************/
int services_2_r_wait(struct ssd_info * ssd,unsigned int channel,unsigned int * channel_busy_flag, unsigned int * change_current_time_flag)
{
	unsigned int plane=0,address_ppn=0;
	struct sub_request * sub=NULL, * p=NULL;
	struct sub_request * sub_twoplane_one=NULL, * sub_twoplane_two=NULL;
	struct sub_request * sub_interleave_one=NULL, * sub_interleave_two=NULL;


	sub=ssd->channel_head[channel].subs_r_head;


	if ((ssd->parameter->advanced_commands&AD_TWOPLANE_READ)==AD_TWOPLANE_READ)         /*to find whether there are two sub request can be served by two plane operation*/
	{
		sub_twoplane_one=NULL;
		sub_twoplane_two=NULL;
		                                                                                /*寻找能执行two_plane的两个读子请求*/
		find_interleave_twoplane_sub_request(ssd,channel,sub_twoplane_one,sub_twoplane_two,TWO_PLANE);

		if (sub_twoplane_two!=NULL)                                                     /*可以执行two plane read 操作*/
		{
			go_one_step(ssd, sub_twoplane_one,sub_twoplane_two, SR_R_C_A_TRANSFER,TWO_PLANE);

			*change_current_time_flag=0;
			*channel_busy_flag=1;                                                       /*已经占用了这个周期的总线，不用执行die中数据的回传*/
		}
		else if((ssd->parameter->advanced_commands&AD_INTERLEAVE)!=AD_INTERLEAVE)       /*没有满足条件的两个page，，并且没有interleave read命令时，只能执行单个page的读*/
		{
			while(sub!=NULL)                                                            /*if there are read requests in queue, send one of them to target die*/
			{
				if(sub->current_state==SR_WAIT)
				{	                                                                    /*注意下个这个判断条件与services_2_r_data_trans中判断条件的不同
																						*/
					if((ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].current_state==CHIP_IDLE)||((ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].next_state==CHIP_IDLE)&&
						(ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].next_state_predict_time<=ssd->current_time)))
					{
						go_one_step(ssd, sub,NULL, SR_R_C_A_TRANSFER,NORMAL);

						*change_current_time_flag=0;
						*channel_busy_flag=1;                                           /*已经占用了这个周期的总线，不用执行die中数据的回传*/
						break;
					}
					else
					{
						                                                                /*因为die的busy导致的阻塞*/
					}
				}
				sub=sub->next_node;
			}
		}
	}
	if ((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE)               /*to find whether there are two sub request can be served by INTERLEAVE operation*/
	{
		sub_interleave_one=NULL;
		sub_interleave_two=NULL;
		find_interleave_twoplane_sub_request(ssd,channel,sub_interleave_one,sub_interleave_two,INTERLEAVE);

		if (sub_interleave_two!=NULL)                                                  /*可以执行interleave read 操作*/
		{

			go_one_step(ssd, sub_interleave_one,sub_interleave_two, SR_R_C_A_TRANSFER,INTERLEAVE);

			*change_current_time_flag=0;
			*channel_busy_flag=1;                                                      /*已经占用了这个周期的总线，不用执行die中数据的回传*/
		}
		else                                                                           /*没有满足条件的两个page，只能执行单个page的读*/
		{
			while(sub!=NULL)                                                           /*if there are read requests in queue, send one of them to target die*/
			{
				if(sub->current_state==SR_WAIT)
				{
					if((ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].current_state==CHIP_IDLE)||((ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].next_state==CHIP_IDLE)&&
						(ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].next_state_predict_time<=ssd->current_time)))
					{

						go_one_step(ssd, sub,NULL, SR_R_C_A_TRANSFER,NORMAL);

						*change_current_time_flag=0;
						*channel_busy_flag=1;                                          /*已经占用了这个周期的总线，不用执行die中数据的回传*/
						break;
					}
					else
					{
						                                                               /*因为die的busy导致的阻塞*/
					}
				}
				sub=sub->next_node;
			}
		}
	}

	/*******************************
	*ssd不能执行执行高级命令的情况下
	*******************************/
	if (((ssd->parameter->advanced_commands&AD_INTERLEAVE)!=AD_INTERLEAVE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE_READ)!=AD_TWOPLANE_READ))
	{
		while(sub!=NULL)                                                               /*if there are read requests in queue, send one of them to target chip*/
		{
			if(sub->current_state==SR_WAIT)
			{
				//chip空闲
				if((ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].current_state==CHIP_IDLE)||((ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].next_state==CHIP_IDLE)&&
					(ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].next_state_predict_time<=ssd->current_time)))
				{

					go_one_step(ssd, sub,NULL, SR_R_C_A_TRANSFER,NORMAL);

					*change_current_time_flag=0;
					*channel_busy_flag=1;                                              /*已经占用了这个周期的总线，不用执行die中数据的回传*/
					break;
				}
				else
				{
					                                                                   /*因为die的busy导致的阻塞*/
				}
			}
			sub=sub->next_node;
		}
	}

	return SUCCESS;
}

/*********************************************************************
*当一个写子请求处理完后，要从请求队列上删除，这个函数就是执行这个功能。
**********************************************************************/
int delete_w_sub_request(struct ssd_info * ssd, unsigned int channel, struct sub_request * sub )
{
	struct sub_request * p=NULL;
	if (sub==ssd->channel_head[channel].subs_w_head)                                   /*将这个子请求从channel队列中删除*/
	{
		if (ssd->channel_head[channel].subs_w_head!=ssd->channel_head[channel].subs_w_tail)
		{
			ssd->channel_head[channel].subs_w_head=sub->next_node;
		}
		else
		{
			ssd->channel_head[channel].subs_w_head=NULL;
			ssd->channel_head[channel].subs_w_tail=NULL;
		}
	}
	else
	{
		p=ssd->channel_head[channel].subs_w_head;
		while(p->next_node !=sub)
		{
			p=p->next_node;
		}

		if (sub->next_node!=NULL)
		{
			p->next_node=sub->next_node;
		}
		else
		{
			p->next_node=NULL;
			ssd->channel_head[channel].subs_w_tail=p;
		}
	}

	return SUCCESS;
}

/*
*函数的功能就是执行copyback命令的功能，
*/
Status copy_back(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die,struct sub_request * sub)
{
	int old_ppn=-1, new_ppn=-1;
	long long time=0;
	if (ssd->parameter->greed_CB_ad==1)                                               /*允许贪婪使用copyback高级命令*/
	{
		old_ppn=-1;
		if (ssd->dram->map->map_entry[sub->lpn].state!=0)                             /*说明这个逻辑页之前有写过，需要使用copyback+random input命令，否则直接写下去即可*/
		{
			if ((sub->state&ssd->dram->map->map_entry[sub->lpn].state)==ssd->dram->map->map_entry[sub->lpn].state)
			{
				sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
			}
			else
			{
				sub->next_state_predict_time=ssd->current_time+19*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
				ssd->copy_back_count++;
				ssd->read_count++;
				ssd->update_read_count++;
				old_ppn=ssd->dram->map->map_entry[sub->lpn].pn;                       /*记录原来的物理页，用于在copyback时，判断是否满足同为奇地址或者偶地址*/
			}
		}
		else
		{
			sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
		}
		sub->complete_time=sub->next_state_predict_time;
		time=sub->complete_time;

		get_ppn(ssd,sub->location->channel,sub->location->chip,sub->location->die,sub->location->plane,sub);

		if (old_ppn!=-1)                                                              /*采用了copyback操作，需要??断是否满足了奇偶地址的限制*/
		{
			new_ppn=ssd->dram->map->map_entry[sub->lpn].pn;
			while (old_ppn%2!=new_ppn%2)                                              /*没有满足奇偶地址限制，需要再往下找一页*/
			{
				get_ppn(ssd,sub->location->channel,sub->location->chip,sub->location->die,sub->location->plane,sub);
				ssd->program_count--;
				ssd->write_flash_count--;
				ssd->waste_page_count++;
				new_ppn=ssd->dram->map->map_entry[sub->lpn].pn;
			}
		}
	}
	else                                                                              /*不能贪婪的使用copyback高级命令*/
	{
		if (ssd->dram->map->map_entry[sub->lpn].state!=0)
		{
			if ((sub->state&ssd->dram->map->map_entry[sub->lpn].state)==ssd->dram->map->map_entry[sub->lpn].state)
			{
				sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
				get_ppn(ssd,sub->location->channel,sub->location->chip,sub->location->die,sub->location->plane,sub);
			}
			else
			{
				old_ppn=ssd->dram->map->map_entry[sub->lpn].pn;                       /*记录原来的物理页，用于在copyback时，判断是否满足同为奇地址或者偶地址*/
				get_ppn(ssd,sub->location->channel,sub->location->chip,sub->location->die,sub->location->plane,sub);
				new_ppn=ssd->dram->map->map_entry[sub->lpn].pn;
				if (old_ppn%2==new_ppn%2)
				{
					ssd->copy_back_count++;
					sub->next_state_predict_time=ssd->current_time+19*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
				}
				else
				{
					sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(size(ssd->dram->map->map_entry[sub->lpn].state))*ssd->parameter->time_characteristics.tRC+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
				}
				ssd->read_count++;
				ssd->update_read_count++;
			}
		}
		else
		{
			sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
			get_ppn(ssd,sub->location->channel,sub->location->chip,sub->location->die,sub->location->plane,sub);
		}
		sub->complete_time=sub->next_state_predict_time;
		time=sub->complete_time;
	}

	/****************************************************************
	*执行copyback高级命令时，需要修改channel，chip的状态，以及时间等
	*****************************************************************/
	ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;
	ssd->channel_head[channel].current_time=ssd->current_time;
	ssd->channel_head[channel].next_state=CHANNEL_IDLE;
	ssd->channel_head[channel].next_state_predict_time=time;

	ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;
	ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
	ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;
	ssd->channel_head[channel].chip_head[chip].next_state_predict_time=time+ssd->parameter->time_characteristics.tPROG;
	// if (new_ppn%2 == 0)
	// 	{
	// 		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=time+ssd->parameter->time_characteristics.tPROG_L;
	// 		//printf("LSB programming\n");
	// 	}
	// else
	// 	{
	// 		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=time+ssd->parameter->time_characteristics.tPROG_M;
	// 		//printf("MSB programming\n");
	// 	}

	return SUCCESS;
}

/*****************
*静态写操作的实现
******************/
Status static_write(struct ssd_info * ssd, unsigned int channel,unsigned int chip, unsigned int die,struct sub_request * sub)
{
	long long time=0;
	if (ssd->dram->map->map_entry[sub->lpn].state!=0)                                    /*说明这个逻辑页之前有写过，需要使用先读出来，再写下去，否则直接写下去即可*/
	{
		if ((sub->state&ssd->dram->map->map_entry[sub->lpn].state)==ssd->dram->map->map_entry[sub->lpn].state)   /*可以覆盖*/
		{
			sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
		}
		else
		{
			sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(size((ssd->dram->map->map_entry[sub->lpn].state^sub->state)))*ssd->parameter->time_characteristics.tRC+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
			ssd->read_count++;
			ssd->update_read_count++;
		}
	}
	else
	{
		sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
	}
	sub->complete_time=sub->next_state_predict_time;
	time=sub->complete_time;

	get_ppn(ssd,sub->location->channel,sub->location->chip,sub->location->die,sub->location->plane,sub);

    /****************************************************************
	*执行copyback高级命令时，需要修改channel，chip的状态，以及时间等
	*****************************************************************/
	ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;
	ssd->channel_head[channel].current_time=ssd->current_time;
	ssd->channel_head[channel].next_state=CHANNEL_IDLE;
	ssd->channel_head[channel].next_state_predict_time=time;

	ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;
	ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
	ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;
	//**************************************************************
	int new_ppn;
	new_ppn=ssd->dram->map->map_entry[sub->lpn].pn;
	ssd->channel_head[channel].chip_head[chip].next_state_predict_time=time+ssd->parameter->time_characteristics.tPROG;
	// if (new_ppn%2 == 0)
	// 	{
	// 		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=time+ssd->parameter->time_characteristics.tPROG_L;
	// 		//printf("LSB programming\n");
	// 	}
	// else
	// 	{
	// 		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=time+ssd->parameter->time_characteristics.tPROG_M;
	// 		//printf("MSB programming\n");
	// 	}
	//****************************************************************
	return SUCCESS;
}

/********************
写子请求的处理函数
*********************/
Status services_2_write(struct ssd_info * ssd,unsigned int channel,unsigned int * channel_busy_flag, unsigned int * change_current_time_flag)
{
	int j=0,chip=0;
	unsigned int k=0;
	unsigned int  old_ppn=0,new_ppn=0;
	unsigned int chip_token=0,die_token=0,plane_token=0,address_ppn=0;
	unsigned int  die=0,plane=0;
	long long time=0;
	struct sub_request * sub=NULL, * p=NULL;
	struct sub_request * sub_twoplane_one=NULL, * sub_twoplane_two=NULL;
	struct sub_request * sub_interleave_one=NULL, * sub_interleave_two=NULL;

	/************************************************************************************************************************
	*写子请求挂在两个地方一个是channel_head[channel].subs_w_head，另外一个是ssd->subs_w_head，所以要保证至少有一个队列不为空
	*同时子请求的处理还分为动态分配和静态分配。
	*************************************************************************************************************************/
	if((ssd->channel_head[channel].subs_w_head!=NULL)||(ssd->subs_w_head!=NULL)) //有子写请求
	{
		if (ssd->parameter->allocation_scheme==0)                                       /*动态分配*/
		{
			for(j=0;j<ssd->channel_head[channel].chip;j++)
			{
				if((ssd->channel_head[channel].subs_w_head==NULL)&&(ssd->subs_w_head==NULL)) //该channel上没有子请求，跳出循环，到下一个channel找
				{
					break;
				}
				//channel上有子请求，处理
				chip_token=ssd->channel_head[channel].token; //从某一个chip开始分配                /*令牌*/
				if (*channel_busy_flag==0)   //此通道不忙
				{
					//chip不忙
					if((ssd->channel_head[channel].chip_head[chip_token].current_state==CHIP_IDLE)||((ssd->channel_head[channel].chip_head[chip_token].next_state==CHIP_IDLE)&&(ssd->channel_head[channel].chip_head[chip_token].next_state_predict_time<=ssd->current_time)))
					{
							if((ssd->channel_head[channel].subs_w_head==NULL)&&(ssd->subs_w_head==NULL))
							{
								break;
							}
							//该chip上有子请求，处理
							die_token=ssd->channel_head[channel].chip_head[chip_token].token;
							if (((ssd->parameter->advanced_commands&AD_INTERLEAVE)!=AD_INTERLEAVE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)!=AD_TWOPLANE))       //can't use advanced commands
							{
								sub=find_write_sub_request(ssd,channel);
								if(sub==NULL)
								{
									break;
								}

								if(sub->current_state==SR_WAIT)
								{
									plane_token=ssd->channel_head[channel].chip_head[chip_token].die_head[die_token].token;

									get_ppn(ssd,channel,chip_token,die_token,plane_token,sub);

									ssd->channel_head[channel].chip_head[chip_token].die_head[die_token].token=(ssd->channel_head[channel].chip_head[chip_token].die_head[die_token].token+1)%ssd->parameter->plane_die;

									*change_current_time_flag=0;

									if(ssd->parameter->ad_priority2==0)
									{
										ssd->real_time_subreq--;
									}
									go_one_step(ssd,sub,NULL,SR_W_TRANSFER,NORMAL);       /*执行普通的状态的转变。*/
									delete_w_sub_request(ssd,channel,sub);                /*删掉处理完后的写子请求*/

									*channel_busy_flag=1;
									/**************************************************************************
									*跳出for循环前，修改令牌
									*这里的token的变化完全取决于在这个channel chip die plane下写是否成功
									*成功了就break 没成功token就要变化直到找到能写成功的channel chip die plane
									***************************************************************************/
									ssd->channel_head[channel].chip_head[chip_token].token=(ssd->channel_head[channel].chip_head[chip_token].token+1)%ssd->parameter->die_chip;
									ssd->channel_head[channel].token=(ssd->channel_head[channel].token+1)%ssd->parameter->chip_channel[channel];
									break;
								}
							}
							else                                                          /*use advanced commands*/
							{
								if (dynamic_advanced_process(ssd,channel,chip_token)==NULL)
								{
									*channel_busy_flag=0;
								}
								else
								{
									*channel_busy_flag=1;                                 /*执行了一个请求，传输了数据，占用了总线，需要跳出到下一个channel*/
                                    ssd->channel_head[channel].chip_head[chip_token].token=(ssd->channel_head[channel].chip_head[chip_token].token+1)%ssd->parameter->die_chip;
                                    ssd->channel_head[channel].token=(ssd->channel_head[channel].token+1)%ssd->parameter->chip_channel[channel];
									break;
								}
							}

						ssd->channel_head[channel].chip_head[chip_token].token=(ssd->channel_head[channel].chip_head[chip_token].token+1)%ssd->parameter->die_chip;
					}
				}

				ssd->channel_head[channel].token=(ssd->channel_head[channel].token+1)%ssd->parameter->chip_channel[channel];
			}
		}
		else if(ssd->parameter->allocation_scheme==1)                                     /*静态分配*/
		{
			for(chip=0;chip<ssd->channel_head[channel].chip;chip++)
			{
				if((ssd->channel_head[channel].chip_head[chip].current_state==CHIP_IDLE)||((ssd->channel_head[channel].chip_head[chip].next_state==CHIP_IDLE)&&(ssd->channel_head[channel].chip_head[chip].next_state_predict_time<=ssd->current_time)))
				{
					if(ssd->channel_head[channel].subs_w_head==NULL)
					{
						break;
					}
					if (*channel_busy_flag==0)
					{

							if (((ssd->parameter->advanced_commands&AD_INTERLEAVE)!=AD_INTERLEAVE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)!=AD_TWOPLANE))     /*不执行高级命令*/
							{
								for(die=0;die<ssd->channel_head[channel].chip_head[chip].die_num;die++)
								{
									if(ssd->channel_head[channel].subs_w_head==NULL)
									{
										break;
									}
									sub=ssd->channel_head[channel].subs_w_head;
									while (sub!=NULL)
									{
										if ((sub->current_state==SR_WAIT)&&(sub->location->channel==channel)&&(sub->location->chip==chip)&&(sub->location->die==die))      /*该子请求就是当前die的请求*/
										{
											break;
										}
										sub=sub->next_node;
									}
									if (sub==NULL)
									{
										continue;
									}

									if(sub->current_state==SR_WAIT)
									{
										sub->current_time=ssd->current_time;
										sub->current_state=SR_W_TRANSFER;
										sub->next_state=SR_COMPLETE;

										if ((ssd->parameter->advanced_commands&AD_COPYBACK)==AD_COPYBACK)
										{
											copy_back(ssd, channel,chip, die,sub);      /*如果可以执行copyback高级命令，那么就用函数copy_back(ssd, channel,chip, die,sub)处理写子请求*/
											*change_current_time_flag=0;
										}
										else
										{
											static_write(ssd, channel,chip, die,sub);   /*不能执行copyback高级命令，那么就用static_write(ssd, channel,chip, die,sub)函数来处理写子请求*/
											*change_current_time_flag=0;
										}

										delete_w_sub_request(ssd,channel,sub);
										*channel_busy_flag=1;
										break;
									}
								}
							}
							else                                                        /*不能处理高级命令*/
							{
								if (dynamic_advanced_process(ssd,channel,chip)==NULL)
								{
									*channel_busy_flag=0;
								}
								else
								{
									*channel_busy_flag=1;                               /*执行了一个请求，传输了数据，占用了总线，需要跳出到下一个channel*/
									break;
								}
							}

					}
				}
			}
		}
	}
	return SUCCESS;
}


/********************************************************
*这个函数的主要功能是主控读子请求和写子请求的状态变化处理
*********************************************************/

struct ssd_info *process(struct ssd_info *ssd)
{
	/*********************************************************************************************************
	*flag_die表示是否因为die的busy，阻塞了时间前进，-1表示没有，非-1表示有阻塞，
	*flag_die的值表示die号,old ppn记录在copyback之前的物理页号，用于判断copyback是否遵守了奇偶地址的限制；
	*two_plane_bit[8],two_plane_place[8]数组成员表示同一个channel上每个die的请求分配情况；
	*chg_cur_time_flag作为是否需要调整当前时间的标志位，当因为channel处于busy导致请求阻塞时，需要调整当前时间；
	*初始认为需要调整，置为1，当任何一个channel处理了传送命令或者数据时，这个值置为0，表示不需要调整；
	**********************************************************************************************************/
	int old_ppn=-1,flag_die=-1;
	unsigned int i,chan,random_num;
	unsigned int flag=0,new_write=0,chg_cur_time_flag=1,flag2=0,flag_gc=0;//flag_gc有没有执行过gc，1是，0否
	int64_t time, channel_time=MAX_INT64;
	struct sub_request *sub;

#ifdef DEBUG
	printf("enter process,  current time:%lld\n",ssd->current_time);
#endif

	/*********************************************************
	*判断是否有读写子请求，如果有那么flag令为0，没有flag就为1
	*当flag为1时，若ssd中有gc操作这时就可以执行gc操作
	**********************************************************/
	for(i=0;i<ssd->parameter->channel_number;i++)
	{
		if((ssd->channel_head[i].subs_r_head==NULL)&&(ssd->channel_head[i].subs_w_head==NULL)&&(ssd->subs_w_head==NULL))
		{
			flag=1;
		}
		else
		{
			flag=0;
			break;
		}
	}
	if(flag==1)//没有读写子请求，代表是空闲时间段，可进行GC
	{
		ssd->flag=1;
		if (ssd->gc_request>0) /*SSD中有gc操作的请求*/
		{
			gc(ssd,0,1);     /*这个gc要求所有channel都必须遍历到*/
		}
		return ssd;
	}
	else
	{
		ssd->flag=0;
	}

	//上述情况判断完(或者说是判断完，因为可能不满足条件没有进入具体执行路径)
	time = ssd->current_time;
	services_2_r_cmd_trans_and_complete(ssd);//专门为读子请求服务的函数/*处理当前状态是SR_R_C_A_TRANSFER或者当前状态是SR_COMPLETE，或者下一状态是SR_COMPLETE并且下一状态预计时间小于当前状态时间*/

	random_num=ssd->program_count%ssd->parameter->channel_number;   /*产生一个随机数，保证每次从不同的channel开始查询*/

	/*****************************************
	*循环处理所有channel上的读写子请求
	*发读请求命令，传读写数据，都需要占用总线，
	******************************************/
	for(chan=0;chan<ssd->parameter->channel_number;chan++)
	{
		i=(random_num+chan)%ssd->parameter->channel_number;//表示通道数，每次从不同的通道开始工作
		flag=0;	//flag表示通道是否被占用(是否忙状态),0表示未被占用，1表示被占用(处于忙状态)
		flag_gc=0;   /*每次进入channel时，将gc的标志位置为0，默认认为没有进行gc操作*/
		//该channel正空闲状态
		if((ssd->channel_head[i].current_state==CHANNEL_IDLE)||(ssd->channel_head[i].next_state==CHANNEL_IDLE&&ssd->channel_head[i].next_state_predict_time<=ssd->current_time))
		{
			if (ssd->gc_request>0)/*有gc操作，需要进行一定的判断*/
			{
				if (ssd->channel_head[i].gc_command!=NULL)
				{
					flag_gc=gc(ssd,i,0);/*gc函数返回一个值，表示是否执行了gc操作，如果执行了gc操作(gc成功执行返回1，没有执行成功(即要先服务读写请求)返回0)，这个channel在这个时刻不能服务其他的请求*/
				}
				if (flag_gc==1)/*执行过gc操作，需要跳出此次循环*/ //即此通道正忙，可以执行下一个通道的操作
				{
					continue;
				}
			}

			//若没有GC操作
			sub=ssd->channel_head[i].subs_r_head;                                        /*先处理读请求*/
			services_2_r_wait(ssd,i,&flag,&chg_cur_time_flag);                           /*处理处于等待状态的读子请求*/

			if((flag==0)&&(ssd->channel_head[i].subs_r_head!=NULL))                      /*if there are no new read request and data is ready in some dies, send these data to controller and response this request*/
			{
				services_2_r_data_trans(ssd,i,&flag,&chg_cur_time_flag);
			}
			if(flag==0)                                                                  /*if there are no read request to take channel, we can serve write requests*/
			{
				services_2_write(ssd,i,&flag,&chg_cur_time_flag);		//i表示通道channel；flag表示通道是否被占用(忙状态),0表示未被占用
			}
		}
	}
	return ssd;
}

/****************************************************************************************************************************
*当ssd支持高级命令时，这个函数的作用就是处理高级命令的写子请求
*根据请求的个数，决定选择哪种高级命令（这个函数只处理写请求，读请求已经分配到每个channel，所以在执行时之间进行选取相应的命令）
*****************************************************************************************************************************/
struct ssd_info *dynamic_advanced_process(struct ssd_info *ssd,unsigned int channel,unsigned int chip)
{
	unsigned int die=0,plane=0;
	unsigned int subs_count=0;
	int flag;
	unsigned int gate;                                                                    /*record the max subrequest that can be executed in the same channel. it will be used when channel-level priority order is highest and allocation scheme is full dynamic allocation*/
	unsigned int plane_place;                                                             /*record which plane has sub request in static allocation*/
	struct sub_request *sub=NULL,*p=NULL,*sub0=NULL,*sub1=NULL,*sub2=NULL,*sub3=NULL,*sub0_rw=NULL,*sub1_rw=NULL,*sub2_rw=NULL,*sub3_rw=NULL;
	struct sub_request ** subs=NULL;
	unsigned int max_sub_num=0;
	unsigned int die_token=0,plane_token=0;
	unsigned int * plane_bits=NULL;
	unsigned int interleaver_count=0;

	unsigned int mask=0x00000001;
	unsigned int i=0,j=0;

	max_sub_num=(ssd->parameter->die_chip)*(ssd->parameter->plane_die);
	gate=max_sub_num;
	subs=(struct sub_request **)malloc(max_sub_num*sizeof(struct sub_request *));
	alloc_assert(subs,"sub_request");

	for(i=0;i<max_sub_num;i++)
	{
		subs[i]=NULL;
	}

	if((ssd->parameter->allocation_scheme==0)&&(ssd->parameter->dynamic_allocation==0)&&(ssd->parameter->ad_priority2==0))
	{
		gate=ssd->real_time_subreq/ssd->parameter->channel_number;

		if(gate==0)
		{
			gate=1;
		}
		else
		{
			if(ssd->real_time_subreq%ssd->parameter->channel_number!=0)
			{
				gate++;
			}
		}
	}

	if ((ssd->parameter->allocation_scheme==0))                                           /*全动态分配，需要从ssd->subs_w_head上选取等待服务的子请求*/
	{
		if(ssd->parameter->dynamic_allocation==0)
		{
			sub=ssd->subs_w_head;
		}
		else
		{
			sub=ssd->channel_head[channel].subs_w_head;
		}

		subs_count=0;

		while ((sub!=NULL)&&(subs_count<max_sub_num)&&(subs_count<gate))
		{
			if(sub->current_state==SR_WAIT)
			{
				if ((sub->update==NULL)||((sub->update!=NULL)&&((sub->update->current_state==SR_COMPLETE)||((sub->update->next_state==SR_COMPLETE)&&(sub->update->next_state_predict_time<=ssd->current_time)))))    //没有需要提前读出的页
				{
					subs[subs_count]=sub;
					subs_count++;
				}
			}

			p=sub;
			sub=sub->next_node;
		}

		if (subs_count==0)                                                               /*没有请求可以服务，返回NULL*/
		{
			for(i=0;i<max_sub_num;i++)
			{
				subs[i]=NULL;
			}
			free(subs);

			subs=NULL;
			free(plane_bits);
			return NULL;
		}
		if(subs_count>=2)
		{
		    /*********************************************
			*two plane,interleave都可以使用
			*在这个channel上，选用interleave_two_plane执行
			**********************************************/
			if (((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE)&&((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE))
			{
				get_ppn_for_advanced_commands(ssd,channel,chip,subs,subs_count,INTERLEAVE_TWO_PLANE);
			}
			else if (((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE)&&((ssd->parameter->advanced_commands&AD_INTERLEAVE)!=AD_INTERLEAVE))
			{
				if(subs_count>ssd->parameter->plane_die)
				{
					for(i=ssd->parameter->plane_die;i<subs_count;i++)
					{
						subs[i]=NULL;
					}
					subs_count=ssd->parameter->plane_die;
				}
				get_ppn_for_advanced_commands(ssd,channel,chip,subs,subs_count,TWO_PLANE);
			}
			else if (((ssd->parameter->advanced_commands&AD_TWOPLANE)!=AD_TWOPLANE)&&((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE))
			{

				if(subs_count>ssd->parameter->die_chip)
				{
					for(i=ssd->parameter->die_chip;i<subs_count;i++)
					{
						subs[i]=NULL;
					}
					subs_count=ssd->parameter->die_chip;
				}
				get_ppn_for_advanced_commands(ssd,channel,chip,subs,subs_count,INTERLEAVE);
			}
			else
			{
				for(i=1;i<subs_count;i++)
				{
					subs[i]=NULL;
				}
				subs_count=1;
				get_ppn_for_normal_command(ssd,channel,chip,subs[0]);
			}

		}//if(subs_count>=2)
		else if(subs_count==1)     //only one request
		{
			get_ppn_for_normal_command(ssd,channel,chip,subs[0]);
		}

	}//if ((ssd->parameter->allocation_scheme==0))
	else                                                                                  /*静态分配方式，只需从这个特定的channel上选取等待服务的子请求*/
	{
		                                                                                  /*在静态分配方式中，根据channel上的请求落在同一个die上的那些plane来确定使用什么命令*/

			sub=ssd->channel_head[channel].subs_w_head;
			plane_bits=(unsigned int * )malloc((ssd->parameter->die_chip)*sizeof(unsigned int));
			alloc_assert(plane_bits,"plane_bits");
			memset(plane_bits,0, (ssd->parameter->die_chip)*sizeof(unsigned int));

			for(i=0;i<ssd->parameter->die_chip;i++)
			{
				plane_bits[i]=0x00000000;
			}
			subs_count=0;

			while ((sub!=NULL)&&(subs_count<max_sub_num))
			{
				if(sub->current_state==SR_WAIT)
				{
					if ((sub->update==NULL)||((sub->update!=NULL)&&((sub->update->current_state==SR_COMPLETE)||((sub->update->next_state==SR_COMPLETE)&&(sub->update->next_state_predict_time<=ssd->current_time)))))
					{
						if (sub->location->chip==chip)
						{
							plane_place=0x00000001<<(sub->location->plane);

							if ((plane_bits[sub->location->die]&plane_place)!=plane_place)      //we have not add sub request to this plane
							{
								subs[sub->location->die*ssd->parameter->plane_die+sub->location->plane]=sub;
								subs_count++;
								plane_bits[sub->location->die]=(plane_bits[sub->location->die]|plane_place);
							}
						}
					}
				}
				sub=sub->next_node;
			}//while ((sub!=NULL)&&(subs_count<max_sub_num))

			if (subs_count==0)                                                            /*没有请求可以服务，返回NULL*/
			{
				for(i=0;i<max_sub_num;i++)
				{
					subs[i]=NULL;
				}
				free(subs);
				subs=NULL;
				free(plane_bits);
				return NULL;
			}

			flag=0;
			if (ssd->parameter->advanced_commands!=0)
			{
				if ((ssd->parameter->advanced_commands&AD_COPYBACK)==AD_COPYBACK)        /*全部高级命令都可以使用*/
				{
					if (subs_count>1)                                                    /*有1个以上可以直接服务的写请求*/
					{
						get_ppn_for_advanced_commands(ssd,channel,chip,subs,subs_count,COPY_BACK);
					}
					else
					{
						for(i=0;i<max_sub_num;i++)
						{
							if(subs[i]!=NULL)
							{
								break;
							}
						}
						get_ppn_for_normal_command(ssd,channel,chip,subs[i]);
					}

				}// if ((ssd->parameter->advanced_commands&AD_COPYBACK)==AD_COPYBACK)
				else                                                                     /*不能执行copyback*/
				{
					if (subs_count>1)                                                    /*有1个以上可以直接服务的写请求*/
					{
						if (((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE))
						{
							get_ppn_for_advanced_commands(ssd,channel,chip,subs,subs_count,INTERLEAVE_TWO_PLANE);
						}
						else if (((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)!=AD_TWOPLANE))
						{
							for(die=0;die<ssd->parameter->die_chip;die++)
							{
								if(plane_bits[die]!=0x00000000)
								{
									for(i=0;i<ssd->parameter->plane_die;i++)
									{
										plane_token=ssd->channel_head[channel].chip_head[chip].die_head[die].token;
										ssd->channel_head[channel].chip_head[chip].die_head[die].token=(plane_token+1)%ssd->parameter->plane_die;
										mask=0x00000001<<plane_token;
										if((plane_bits[die]&mask)==mask)
										{
											plane_bits[die]=mask;
											break;
										}
									}
									for(i=i+1;i<ssd->parameter->plane_die;i++)
									{
										plane=(plane_token+1)%ssd->parameter->plane_die;
										subs[die*ssd->parameter->plane_die+plane]=NULL;
										subs_count--;
									}
									interleaver_count++;
								}//if(plane_bits[die]!=0x00000000)
							}//for(die=0;die<ssd->parameter->die_chip;die++)
							if(interleaver_count>=2)
							{
								get_ppn_for_advanced_commands(ssd,channel,chip,subs,subs_count,INTERLEAVE);
							}
							else
							{
								for(i=0;i<max_sub_num;i++)
								{
									if(subs[i]!=NULL)
									{
										break;
									}
								}
								get_ppn_for_normal_command(ssd,channel,chip,subs[i]);
							}
						}//else if (((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)!=AD_TWOPLANE))
						else if (((ssd->parameter->advanced_commands&AD_INTERLEAVE)!=AD_INTERLEAVE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE))
						{
							for(i=0;i<ssd->parameter->die_chip;i++)
							{
								die_token=ssd->channel_head[channel].chip_head[chip].token;
								ssd->channel_head[channel].chip_head[chip].token=(die_token+1)%ssd->parameter->die_chip;
								if(size(plane_bits[die_token])>1)
								{
									break;
								}

							}

							if(i<ssd->parameter->die_chip)
							{
								for(die=0;die<ssd->parameter->die_chip;die++)
								{
									if(die!=die_token)
									{
										for(plane=0;plane<ssd->parameter->plane_die;plane++)
										{
											if(subs[die*ssd->parameter->plane_die+plane]!=NULL)
											{
												subs[die*ssd->parameter->plane_die+plane]=NULL;
												subs_count--;
											}
										}
									}
								}
								get_ppn_for_advanced_commands(ssd,channel,chip,subs,subs_count,TWO_PLANE);
							}//if(i<ssd->parameter->die_chip)
							else
							{
								for(i=0;i<ssd->parameter->die_chip;i++)
								{
									die_token=ssd->channel_head[channel].chip_head[chip].token;
									ssd->channel_head[channel].chip_head[chip].token=(die_token+1)%ssd->parameter->die_chip;
									if(plane_bits[die_token]!=0x00000000)
									{
										for(j=0;j<ssd->parameter->plane_die;j++)
										{
											plane_token=ssd->channel_head[channel].chip_head[chip].die_head[die_token].token;
											ssd->channel_head[channel].chip_head[chip].die_head[die_token].token=(plane_token+1)%ssd->parameter->plane_die;
											if(((plane_bits[die_token])&(0x00000001<<plane_token))!=0x00000000)
											{
												sub=subs[die_token*ssd->parameter->plane_die+plane_token];
												break;
											}
										}
									}
								}//for(i=0;i<ssd->parameter->die_chip;i++)
								get_ppn_for_normal_command(ssd,channel,chip,sub);
							}//else
						}//else if (((ssd->parameter->advanced_commands&AD_INTERLEAVE)!=AD_INTERLEAVE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE))
					}//if (subs_count>1)
					else
					{
						for(i=0;i<ssd->parameter->die_chip;i++)
						{
							die_token=ssd->channel_head[channel].chip_head[chip].token;
							ssd->channel_head[channel].chip_head[chip].token=(die_token+1)%ssd->parameter->die_chip;
							if(plane_bits[die_token]!=0x00000000)
							{
								for(j=0;j<ssd->parameter->plane_die;j++)
								{
									plane_token=ssd->channel_head[channel].chip_head[chip].die_head[die_token].token;
									ssd->channel_head[channel].chip_head[chip].die_head[die_token].token=(plane_token+1)%ssd->parameter->plane_die;
									if(((plane_bits[die_token])&(0x00000001<<plane_token))!=0x00000000)
									{
										sub=subs[die_token*ssd->parameter->plane_die+plane_token];
										break;
									}
								}
								if(sub!=NULL)
								{
									break;
								}
							}
						}//for(i=0;i<ssd->parameter->die_chip;i++)
						get_ppn_for_normal_command(ssd,channel,chip,sub);
					}//else
				}
			}//if (ssd->parameter->advanced_commands!=0)
			else
			{
				for(i=0;i<ssd->parameter->die_chip;i++)
				{
					die_token=ssd->channel_head[channel].chip_head[chip].token;
					ssd->channel_head[channel].chip_head[chip].token=(die_token+1)%ssd->parameter->die_chip;
					if(plane_bits[die_token]!=0x00000000)
					{
						for(j=0;j<ssd->parameter->plane_die;j++)
						{
							plane_token=ssd->channel_head[channel].chip_head[chip].die_head[die_token].token;
							ssd->channel_head[channel].chip_head[chip].die_head[die].token=(plane_token+1)%ssd->parameter->plane_die;
							if(((plane_bits[die_token])&(0x00000001<<plane_token))!=0x00000000)
							{
								sub=subs[die_token*ssd->parameter->plane_die+plane_token];
								break;
							}
						}
						if(sub!=NULL)
						{
							break;
						}
					}
				}//for(i=0;i<ssd->parameter->die_chip;i++)
				get_ppn_for_normal_command(ssd,channel,chip,sub);
			}//else

	}//else

	for(i=0;i<max_sub_num;i++)
	{
		subs[i]=NULL;
	}
	free(subs);
	subs=NULL;
	free(plane_bits);
	return ssd;
}

/****************************************
*执行写子请求时，为普通的写子请求获取ppn
*****************************************/
Status get_ppn_for_normal_command(struct ssd_info * ssd, unsigned int channel,unsigned int chip, struct sub_request * sub)
{
	unsigned int die=0;
	unsigned int plane=0;
	if(sub==NULL)
	{
		return ERROR;
	}

	if (ssd->parameter->allocation_scheme==DYNAMIC_ALLOCATION)
	{
		die=ssd->channel_head[channel].chip_head[chip].token;
		plane=ssd->channel_head[channel].chip_head[chip].die_head[die].token;
		get_ppn(ssd,channel,chip,die,plane,sub);
		ssd->channel_head[channel].chip_head[chip].die_head[die].token=(plane+1)%ssd->parameter->plane_die;
		ssd->channel_head[channel].chip_head[chip].token=(die+1)%ssd->parameter->die_chip;

		compute_serve_time(ssd,channel,chip,die,&sub,1,NORMAL);
		return SUCCESS;
	}
	else
	{
		die=sub->location->die;
		plane=sub->location->plane;
		get_ppn(ssd,channel,chip,die,plane,sub);
		compute_serve_time(ssd,channel,chip,die,&sub,1,NORMAL);
		return SUCCESS;
	}

}



/************************************************************************************************
*为高级命令获取ppn
*根据不同的命令，遵从在同一个block中顺序写的要求，选取可以进行写操作的ppn，跳过的ppn全部置为失效。
*在使用two plane操作时，为了寻找相同水平位置的页，可能需要直接找到两个完全空白的块，这个时候原来
*的块没有用完，只能放在这，等待下次使用，同时修改查找空白page的方法，将以前首先寻找free块改为，只
*要invalid block!=64即可。
*except find aim page, we should modify token and decide gc operation
*************************************************************************************************/
Status get_ppn_for_advanced_commands(struct ssd_info *ssd,unsigned int channel,unsigned int chip,struct sub_request * * subs ,unsigned int subs_count,unsigned int command)
{
	unsigned int die=0,plane=0;
	unsigned int die_token=0,plane_token=0;
	struct sub_request * sub=NULL;
	unsigned int i=0,j=0,k=0;
	unsigned int unvalid_subs_count=0;
	unsigned int valid_subs_count=0;
	unsigned int interleave_flag=FALSE;
	unsigned int multi_plane_falg=FALSE;
	unsigned int max_subs_num=0;
	struct sub_request * first_sub_in_chip=NULL;
	struct sub_request * first_sub_in_die=NULL;
	struct sub_request * second_sub_in_die=NULL;
	unsigned int state=SUCCESS;
	unsigned int multi_plane_flag=FALSE;

	max_subs_num=ssd->parameter->die_chip*ssd->parameter->plane_die;

	if (ssd->parameter->allocation_scheme==DYNAMIC_ALLOCATION)                         /*动态分配操作*/
	{
		if((command==INTERLEAVE_TWO_PLANE)||(command==COPY_BACK))                      /*INTERLEAVE_TWO_PLANE以及COPY_BACK的情况*/
		{
			for(i=0;i<subs_count;i++)
			{
				die=ssd->channel_head[channel].chip_head[chip].token;
				if(i<ssd->parameter->die_chip)                                         /*为每个subs[i]获取ppn，i小于die_chip*/
				{
					plane=ssd->channel_head[channel].chip_head[chip].die_head[die].token;
					get_ppn(ssd,channel,chip,die,plane,subs[i]);
					ssd->channel_head[channel].chip_head[chip].die_head[die].token=(plane+1)%ssd->parameter->plane_die;
				}
				else
				{
					/*********************************************************************************************************************************
					*超过die_chip的i所指向的subs[i]与subs[i%ssd->parameter->die_chip]获取相同位置的ppn
					*如果成功的获取了则令multi_plane_flag=TRUE并执行compute_serve_time(ssd,channel,chip,0,subs,valid_subs_count,INTERLEAVE_TWO_PLANE);
					*否则执行compute_serve_time(ssd,channel,chip,0,subs,valid_subs_count,INTERLEAVE);
					***********************************************************************************************************************************/
					state=make_level_page(ssd,subs[i%ssd->parameter->die_chip],subs[i]);
					if(state!=SUCCESS)
					{
						subs[i]=NULL;
						unvalid_subs_count++;
					}
					else
					{
						multi_plane_flag=TRUE;
					}
				}
				ssd->channel_head[channel].chip_head[chip].token=(die+1)%ssd->parameter->die_chip;
			}
			valid_subs_count=subs_count-unvalid_subs_count;
			ssd->interleave_count++;
			if(multi_plane_flag==TRUE)
			{
				ssd->inter_mplane_count++;
				compute_serve_time(ssd,channel,chip,0,subs,valid_subs_count,INTERLEAVE_TWO_PLANE);/*计算写子请求的处理时间，以写子请求的状态转变*/
			}
			else
			{
				compute_serve_time(ssd,channel,chip,0,subs,valid_subs_count,INTERLEAVE);
			}
			return SUCCESS;
		}//if((command==INTERLEAVE_TWO_PLANE)||(command==COPY_BACK))
		else if(command==INTERLEAVE)
		{
			/***********************************************************************************************
			*INTERLEAVE高级命令的处理，这个处理比TWO_PLANE高级命令的处理简单
			*因为two_plane的要求是同一个die里面不同plane的同一位置的page，而interleave要求则是不同die里面的。
			************************************************************************************************/
			for(i=0;(i<subs_count)&&(i<ssd->parameter->die_chip);i++)
			{
				die=ssd->channel_head[channel].chip_head[chip].token;
				plane=ssd->channel_head[channel].chip_head[chip].die_head[die].token;
				get_ppn(ssd,channel,chip,die,plane,subs[i]);
				ssd->channel_head[channel].chip_head[chip].die_head[die].token=(plane+1)%ssd->parameter->plane_die;
				ssd->channel_head[channel].chip_head[chip].token=(die+1)%ssd->parameter->die_chip;
				valid_subs_count++;
			}
			ssd->interleave_count++;
			compute_serve_time(ssd,channel,chip,0,subs,valid_subs_count,INTERLEAVE);
			return SUCCESS;
		}//else if(command==INTERLEAVE)
		else if(command==TWO_PLANE)
		{
			if(subs_count<2)
			{
				return ERROR;
			}
			die=ssd->channel_head[channel].chip_head[chip].token;
			for(j=0;j<subs_count;j++)
			{
				if(j==1)
				{
					state=find_level_page(ssd,channel,chip,die,subs[0],subs[1]);        /*寻找与subs[0]的ppn位置相同的subs[1]，执行TWO_PLANE高级命令*/
					if(state!=SUCCESS)
					{
						get_ppn_for_normal_command(ssd,channel,chip,subs[0]);           /*没找到，那么就当普通命令来处理*/
						return FAILURE;
					}
					else
					{
						valid_subs_count=2;
					}
				}
				else if(j>1)
				{
					state=make_level_page(ssd,subs[0],subs[j]);                         /*寻找与subs[0]的ppn位置相同的subs[j]，执行TWO_PLANE高级命令*/
					if(state!=SUCCESS)
					{
						for(k=j;k<subs_count;k++)
						{
							subs[k]=NULL;
						}
						subs_count=j;
						break;
					}
					else
					{
						valid_subs_count++;
					}
				}
			}//for(j=0;j<subs_count;j++)
			ssd->channel_head[channel].chip_head[chip].token=(die+1)%ssd->parameter->die_chip;
			ssd->m_plane_prog_count++;
			compute_serve_time(ssd,channel,chip,die,subs,valid_subs_count,TWO_PLANE);
			return SUCCESS;
		}//else if(command==TWO_PLANE)
		else
		{
			return ERROR;
		}
	}//if (ssd->parameter->allocation_scheme==DYNAMIC_ALLOCATION)
	else                                                                              /*静态分配的情况*/
	{
		if((command==INTERLEAVE_TWO_PLANE)||(command==COPY_BACK))
		{
			for(die=0;die<ssd->parameter->die_chip;die++)
			{
				first_sub_in_die=NULL;
				for(plane=0;plane<ssd->parameter->plane_die;plane++)
				{
					sub=subs[die*ssd->parameter->plane_die+plane];
					if(sub!=NULL)
					{
						if(first_sub_in_die==NULL)
						{
							first_sub_in_die=sub;
							get_ppn(ssd,channel,chip,die,plane,sub);
						}
						else
						{
							state=make_level_page(ssd,first_sub_in_die,sub);
							if(state!=SUCCESS)
							{
								subs[die*ssd->parameter->plane_die+plane]=NULL;
								subs_count--;
								sub=NULL;
							}
							else
							{
								multi_plane_flag=TRUE;
							}
						}
					}
				}
			}
			if(multi_plane_flag==TRUE)
			{
				ssd->inter_mplane_count++;
				compute_serve_time(ssd,channel,chip,0,subs,valid_subs_count,INTERLEAVE_TWO_PLANE);
				return SUCCESS;
			}
			else
			{
				compute_serve_time(ssd,channel,chip,0,subs,valid_subs_count,INTERLEAVE);
				return SUCCESS;
			}
		}//if((command==INTERLEAVE_TWO_PLANE)||(command==COPY_BACK))
		else if(command==INTERLEAVE)
		{
			for(die=0;die<ssd->parameter->die_chip;die++)
			{
				first_sub_in_die=NULL;
				for(plane=0;plane<ssd->parameter->plane_die;plane++)
				{
					sub=subs[die*ssd->parameter->plane_die+plane];
					if(sub!=NULL)
					{
						if(first_sub_in_die==NULL)
						{
							first_sub_in_die=sub;
							get_ppn(ssd,channel,chip,die,plane,sub);
							valid_subs_count++;
						}
						else
						{
							subs[die*ssd->parameter->plane_die+plane]=NULL;
							subs_count--;
							sub=NULL;
						}
					}
				}
			}
			if(valid_subs_count>1)
			{
				ssd->interleave_count++;
			}
			compute_serve_time(ssd,channel,chip,0,subs,valid_subs_count,INTERLEAVE);
		}//else if(command==INTERLEAVE)
		else if(command==TWO_PLANE)
		{
			for(die=0;die<ssd->parameter->die_chip;die++)
			{
				first_sub_in_die=NULL;
				second_sub_in_die=NULL;
				for(plane=0;plane<ssd->parameter->plane_die;plane++)
				{
					sub=subs[die*ssd->parameter->plane_die+plane];
					if(sub!=NULL)
					{
						if(first_sub_in_die==NULL)
						{
							first_sub_in_die=sub;
						}
						else if(second_sub_in_die==NULL)
						{
							second_sub_in_die=sub;
							state=find_level_page(ssd,channel,chip,die,first_sub_in_die,second_sub_in_die);
							if(state!=SUCCESS)
							{
								subs[die*ssd->parameter->plane_die+plane]=NULL;
								subs_count--;
								second_sub_in_die=NULL;
								sub=NULL;
							}
							else
							{
								valid_subs_count=2;
							}
						}
						else
						{
							state=make_level_page(ssd,first_sub_in_die,sub);
							if(state!=SUCCESS)
							{
								subs[die*ssd->parameter->plane_die+plane]=NULL;
								subs_count--;
								sub=NULL;
							}
							else
							{
								valid_subs_count++;
							}
						}
					}//if(sub!=NULL)
				}//for(plane=0;plane<ssd->parameter->plane_die;plane++)
				if(second_sub_in_die!=NULL)
				{
					multi_plane_flag=TRUE;
					break;
				}
			}//for(die=0;die<ssd->parameter->die_chip;die++)
			if(multi_plane_flag==TRUE)
			{
				ssd->m_plane_prog_count++;
				compute_serve_time(ssd,channel,chip,die,subs,valid_subs_count,TWO_PLANE);
				return SUCCESS;
			}//if(multi_plane_flag==TRUE)
			else
			{
				i=0;
				sub=NULL;
				while((sub==NULL)&&(i<max_subs_num))
				{
					sub=subs[i];
					i++;
				}
				if(sub!=NULL)
				{
					get_ppn_for_normal_command(ssd,channel,chip,sub);
					return FAILURE;
				}
				else
				{
					return ERROR;
				}
			}//else
		}//else if(command==TWO_PLANE)
		else
		{
			return ERROR;
		}
	}//elseb 静态分配的情况
}


/***********************************************
*函数的作用是让sub0，sub1的ppn所在的page位置相同
************************************************/
Status make_level_page(struct ssd_info * ssd, struct sub_request * sub0,struct sub_request * sub1)
{
	unsigned int i=0,j=0,k=0;
	unsigned int channel=0,chip=0,die=0,plane0=0,plane1=0,block0=0,block1=0,page0=0,page1=0;
	unsigned int active_block0=0,active_block1=0;
	unsigned int old_plane_token=0;

	if((sub0==NULL)||(sub1==NULL)||(sub0->location==NULL))
	{
		return ERROR;
	}
	channel=sub0->location->channel;
	chip=sub0->location->chip;
	die=sub0->location->die;
	plane0=sub0->location->plane;
	block0=sub0->location->block;
	page0=sub0->location->page;
	old_plane_token=ssd->channel_head[channel].chip_head[chip].die_head[die].token;

	/***********************************************************************************************
	*动态分配的情况下
	*sub1的plane是根据sub0的ssd->channel_head[channel].chip_head[chip].die_head[die].token令牌获取的
	*sub1的channel，chip，die，block，page都和sub0的相同
	************************************************************************************************/
	if(ssd->parameter->allocation_scheme==DYNAMIC_ALLOCATION)
	{
		old_plane_token=ssd->channel_head[channel].chip_head[chip].die_head[die].token;
		for(i=0;i<ssd->parameter->plane_die;i++)
		{
			plane1=ssd->channel_head[channel].chip_head[chip].die_head[die].token;
			if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane1].add_reg_ppn==-1)
			{
				find_active_block(ssd,channel,chip,die,plane1);                               /*在plane1中找到活跃块*/
				block1=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane1].active_block;

				/*********************************************************************************************
				*只有找到的block1与block0相同，才能继续往下寻找相同的page
				*在寻找page时比较简单，直接用last_write_page（上一次写的page）+1就可以了。
				*如果找到的page不相同，那么如果ssd允许贪婪的使用高级命令，这样就可以让小的page 往大的page靠拢
				*********************************************************************************************/
				if(block1==block0)
				{
					page1=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane1].blk_head[block1].last_write_page+1;
					if(page1==page0)
					{
						break;
					}
					else if(page1<page0)
					{
						if (ssd->parameter->greed_MPW_ad==1)                                  /*允许贪婪的使用高级命令*/
						{
							//make_same_level(ssd,channel,chip,die,plane1,active_block1,page0); /*小的page地址往大的page地址靠*/
							make_same_level(ssd,channel,chip,die,plane1,block1,page0);
							break;
						}
					}
				}//if(block1==block0)
			}
			ssd->channel_head[channel].chip_head[chip].die_head[die].token=(plane1+1)%ssd->parameter->plane_die;
		}//for(i=0;i<ssd->parameter->plane_die;i++)
		if(i<ssd->parameter->plane_die)
		{
			flash_page_state_modify(ssd,sub1,channel,chip,die,plane1,block1,page0);          /*这个函数的作用就是更新page1所对应的物理页以及location还有map表*/
			//flash_page_state_modify(ssd,sub1,channel,chip,die,plane1,block1,page1);
			ssd->channel_head[channel].chip_head[chip].die_head[die].token=(plane1+1)%ssd->parameter->plane_die;
			return SUCCESS;
		}
		else
		{
			ssd->channel_head[channel].chip_head[chip].die_head[die].token=old_plane_token;
			return FAILURE;
		}
	}
	else                                                                                      /*静态分配的情况*/
	{
		if((sub1->location==NULL)||(sub1->location->channel!=channel)||(sub1->location->chip!=chip)||(sub1->location->die!=die))
		{
			return ERROR;
		}
		plane1=sub1->location->plane;
		if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane1].add_reg_ppn==-1)
		{
			find_active_block(ssd,channel,chip,die,plane1);
			block1=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane1].active_block;
			if(block1==block0)
			{
				page1=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane1].blk_head[block1].last_write_page+1;
				if(page1>page0)
				{
					return FAILURE;
				}
				else if(page1<page0)
				{
					if (ssd->parameter->greed_MPW_ad==1)
					{
						//make_same_level(ssd,channel,chip,die,plane1,active_block1,page0);    /*小的page地址往大的page地址靠*/
                        make_same_level(ssd,channel,chip,die,plane1,block1,page0);
						flash_page_state_modify(ssd,sub1,channel,chip,die,plane1,block1,page0);
						//flash_page_state_modify(ssd,sub1,channel,chip,die,plane1,block1,page1);
						return SUCCESS;
					}
					else
					{
						return FAILURE;
					}
				}
				else
				{
					flash_page_state_modify(ssd,sub1,channel,chip,die,plane1,block1,page0);
					//flash_page_state_modify(ssd,sub1,channel,chip,die,plane1,block1,page1);
					return SUCCESS;
				}

			}
			else
			{
				return FAILURE;
			}

		}
		else
		{
			return ERROR;
		}
	}

}

/******************************************************************************************************
*函数的功能是为two plane命令寻找出两个相同水平位置的页，并且修改统计值，修改页的状态
*注意这个函数与上一个函数make_level_page函数的区别，make_level_page这个函数是让sub1与sub0的page位置相同
*而find_level_page函数的作用是在给定的channel，chip，die中找两个位置相同的subA和subB。
*******************************************************************************************************/
Status find_level_page(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,struct sub_request *subA,struct sub_request *subB)
{
	unsigned int i,planeA,planeB,active_blockA,active_blockB,pageA,pageB,aim_page,old_plane;
	struct gc_operation *gc_node;

	old_plane=ssd->channel_head[channel].chip_head[chip].die_head[die].token;

	/************************************************************
	*在动态分配的情况下
	*planeA赋初值为die的令牌，如果planeA是偶数那么planeB=planeA+1
	*planeA是奇数，那么planeA+1变为偶数，再令planeB=planeA+1
	*************************************************************/
	if (ssd->parameter->allocation_scheme==0)
	{
		planeA=ssd->channel_head[channel].chip_head[chip].die_head[die].token;
		if (planeA%2==0)
		{
			planeB=planeA+1;
			ssd->channel_head[channel].chip_head[chip].die_head[die].token=(ssd->channel_head[channel].chip_head[chip].die_head[die].token+2)%ssd->parameter->plane_die;
		}
		else
		{
			planeA=(planeA+1)%ssd->parameter->plane_die;
			planeB=planeA+1;
			ssd->channel_head[channel].chip_head[chip].die_head[die].token=(ssd->channel_head[channel].chip_head[chip].die_head[die].token+3)%ssd->parameter->plane_die;
		}
	}
	else                                                                                     /*静态分配的情况，就直接赋值给planeA和planeB*/
	{
		planeA=subA->location->plane;
		planeB=subB->location->plane;
	}
	find_active_block(ssd,channel,chip,die,planeA);                                          /*寻找active_block*/
	find_active_block(ssd,channel,chip,die,planeB);
	active_blockA=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].active_block;
	active_blockB=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].active_block;



	/*****************************************************
	*如果active_block相同，那么就在这两个块中找相同的page
	*或者使用贪婪的方法找到两个相同的page
	******************************************************/
	if (active_blockA==active_blockB)
	{
		pageA=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].blk_head[active_blockA].last_write_page+1;
		pageB=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].blk_head[active_blockB].last_write_page+1;
		if (pageA==pageB)                                                                    /*两个可用的页正好在同一个水平位置上*/
		{
			flash_page_state_modify(ssd,subA,channel,chip,die,planeA,active_blockA,pageA);
			flash_page_state_modify(ssd,subB,channel,chip,die,planeB,active_blockB,pageB);
		}
		else
		{
			if (ssd->parameter->greed_MPW_ad==1)                                             /*贪婪地使用高级命令*/
			{
				if (pageA<pageB)
				{
					aim_page=pageB;
					make_same_level(ssd,channel,chip,die,planeA,active_blockA,aim_page);     /*小的page地址往大的page地址靠*/
				}
				else
				{
					aim_page=pageA;
					make_same_level(ssd,channel,chip,die,planeB,active_blockB,aim_page);
				}
				flash_page_state_modify(ssd,subA,channel,chip,die,planeA,active_blockA,aim_page);
				flash_page_state_modify(ssd,subB,channel,chip,die,planeB,active_blockB,aim_page);
			}
			else                                                                             /*不能贪婪的使用高级命令*/
			{
				subA=NULL;
				subB=NULL;
				ssd->channel_head[channel].chip_head[chip].die_head[die].token=old_plane;
				return FAILURE;
			}
		}
	}
	/*********************************
	*如果找到的两个active_block不相同
	**********************************/
	else
	{
		pageA=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].blk_head[active_blockA].last_write_page+1;
		pageB=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].blk_head[active_blockB].last_write_page+1;
		if (pageA<pageB)
		{
			if (ssd->parameter->greed_MPW_ad==1)                                             /*贪婪地使用高级命令*/
			{
				/*******************************************************************************
				*在planeA中，与active_blockB相同位置的的block中，与pageB相同位置的page是可用的。
				*也就是palneA中的相应水平位置是可用的，将其最为与planeB中对应的页。
				*那么可也让planeA，active_blockB中的page往pageB靠拢
				********************************************************************************/
				if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].blk_head[active_blockB].page_head[pageB].free_state==PG_SUB)
				{
					make_same_level(ssd,channel,chip,die,planeA,active_blockB,pageB);
					flash_page_state_modify(ssd,subA,channel,chip,die,planeA,active_blockB,pageB);
					flash_page_state_modify(ssd,subB,channel,chip,die,planeB,active_blockB,pageB);
				}
                /********************************************************************************
				*在planeA中，与active_blockB相同位置的的block中，与pageB相同位置的page是可用的。
				*那么就要重新寻找block，需要重新找水平位置相同的一对页
				*********************************************************************************/
				else
				{
					for (i=0;i<ssd->parameter->block_plane;i++)
					{
						pageA=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].blk_head[i].last_write_page+1;
						pageB=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].blk_head[i].last_write_page+1;
						if ((pageA<ssd->parameter->page_block)&&(pageB<ssd->parameter->page_block))
						{
							if (pageA<pageB)
							{
								if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].blk_head[i].page_head[pageB].free_state==PG_SUB)
								{
									aim_page=pageB;
									make_same_level(ssd,channel,chip,die,planeA,i,aim_page);
									break;
								}
							}
							else
							{
								if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].blk_head[i].page_head[pageA].free_state==PG_SUB)
								{
									aim_page=pageA;
									make_same_level(ssd,channel,chip,die,planeB,i,aim_page);
									break;
								}
							}
						}
					}//for (i=0;i<ssd->parameter->block_plane;i++)
					if (i<ssd->parameter->block_plane)
					{
						flash_page_state_modify(ssd,subA,channel,chip,die,planeA,i,aim_page);
						flash_page_state_modify(ssd,subB,channel,chip,die,planeB,i,aim_page);
					}
					else
					{
						subA=NULL;
						subB=NULL;
						ssd->channel_head[channel].chip_head[chip].die_head[die].token=old_plane;
						return FAILURE;
					}
				}
			}//if (ssd->parameter->greed_MPW_ad==1)
			else
			{
				subA=NULL;
				subB=NULL;
				ssd->channel_head[channel].chip_head[chip].die_head[die].token=old_plane;
				return FAILURE;
			}
		}//if (pageA<pageB)
		else
		{
			if (ssd->parameter->greed_MPW_ad==1)
			{
				if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].blk_head[active_blockA].page_head[pageA].free_state==PG_SUB)
				{
					make_same_level(ssd,channel,chip,die,planeB,active_blockA,pageA);
					flash_page_state_modify(ssd,subA,channel,chip,die,planeA,active_blockA,pageA);
					flash_page_state_modify(ssd,subB,channel,chip,die,planeB,active_blockA,pageA);
				}
				else
				{
					for (i=0;i<ssd->parameter->block_plane;i++)
					{
						pageA=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].blk_head[i].last_write_page+1;
						pageB=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].blk_head[i].last_write_page+1;
						if ((pageA<ssd->parameter->page_block)&&(pageB<ssd->parameter->page_block))
						{
							if (pageA<pageB)
							{
								if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].blk_head[i].page_head[pageB].free_state==PG_SUB)
								{
									aim_page=pageB;
									make_same_level(ssd,channel,chip,die,planeA,i,aim_page);
									break;
								}
							}
							else
							{
								if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].blk_head[i].page_head[pageA].free_state==PG_SUB)
								{
									aim_page=pageA;
									make_same_level(ssd,channel,chip,die,planeB,i,aim_page);
									break;
								}
							}
						}
					}//for (i=0;i<ssd->parameter->block_plane;i++)
					if (i<ssd->parameter->block_plane)
					{
						flash_page_state_modify(ssd,subA,channel,chip,die,planeA,i,aim_page);
						flash_page_state_modify(ssd,subB,channel,chip,die,planeB,i,aim_page);
					}
					else
					{
						subA=NULL;
						subB=NULL;
						ssd->channel_head[channel].chip_head[chip].die_head[die].token=old_plane;
						return FAILURE;
					}
				}
			} //if (ssd->parameter->greed_MPW_ad==1)
			else
			{
				if ((pageA==pageB)&&(pageA==0))
				{
					/*******************************************************************************************
					*下面是两种情况
					*1，planeA，planeB中的active_blockA，pageA位置都可用，那么不同plane 的相同位置，以blockA为准
					*2，planeA，planeB中的active_blockB，pageA位置都可用，那么不同plane 的相同位置，以blockB为准
					********************************************************************************************/
					if ((ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].blk_head[active_blockA].page_head[pageA].free_state==PG_SUB)
					  &&(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].blk_head[active_blockA].page_head[pageA].free_state==PG_SUB))
					{
						flash_page_state_modify(ssd,subA,channel,chip,die,planeA,active_blockA,pageA);
						flash_page_state_modify(ssd,subB,channel,chip,die,planeB,active_blockA,pageA);
					}
					else if ((ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].blk_head[active_blockB].page_head[pageA].free_state==PG_SUB)
						   &&(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].blk_head[active_blockB].page_head[pageA].free_state==PG_SUB))
					{
						flash_page_state_modify(ssd,subA,channel,chip,die,planeA,active_blockB,pageA);
						flash_page_state_modify(ssd,subB,channel,chip,die,planeB,active_blockB,pageA);
					}
					else
					{
						subA=NULL;
						subB=NULL;
						ssd->channel_head[channel].chip_head[chip].die_head[die].token=old_plane;
						return FAILURE;
					}
				}
				else
				{
					subA=NULL;
					subB=NULL;
					ssd->channel_head[channel].chip_head[chip].die_head[die].token=old_plane;
					return ERROR;
				}
			}
		}
	}

	if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].free_page<(ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->gc_hard_threshold))
	{
		gc_node=(struct gc_operation *)malloc(sizeof(struct gc_operation));
		alloc_assert(gc_node,"gc_node");
		memset(gc_node,0, sizeof(struct gc_operation));

		gc_node->next_node=NULL;
		gc_node->chip=chip;
		gc_node->die=die;
		gc_node->plane=planeA;
		gc_node->block=0xffffffff;
		gc_node->page=0;
		gc_node->state=GC_WAIT;
		gc_node->priority=GC_UNINTERRUPT;
		gc_node->next_node=ssd->channel_head[channel].gc_command;
		ssd->channel_head[channel].gc_command=gc_node;
		ssd->gc_request++;
	}
	if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].free_page<(ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->gc_hard_threshold))
	{
		gc_node=(struct gc_operation *)malloc(sizeof(struct gc_operation));
		alloc_assert(gc_node,"gc_node");
		memset(gc_node,0, sizeof(struct gc_operation));

		gc_node->next_node=NULL;
		gc_node->chip=chip;
		gc_node->die=die;
		gc_node->plane=planeB;
		gc_node->block=0xffffffff;
		gc_node->page=0;
		gc_node->state=GC_WAIT;
		gc_node->priority=GC_UNINTERRUPT;
		gc_node->next_node=ssd->channel_head[channel].gc_command;
		ssd->channel_head[channel].gc_command=gc_node;
		ssd->gc_request++;
	}

	return SUCCESS;
}

/*
*函数的功能是修改找到的page页的状态以及相应的dram中映射表的值
*/
struct ssd_info *flash_page_state_modify(struct ssd_info *ssd,struct sub_request *sub,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane,unsigned int block,unsigned int page)
{
	unsigned int ppn,full_page;
	struct local *location;
	struct direct_erase *new_direct_erase,*direct_erase_node;

	full_page=~(0xffffffff<<ssd->parameter->subpage_page);
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_page=page;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_page_num--;
	if(page%2==0){
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_lsb_num--;
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_lsb_num--;
		ssd->free_lsb_count--;
		}
	else{
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_msb_num--;
		}

	if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_page>=ssd->parameter->page_block)
	{
		printf("BBB error! the last write page larger than 64!!\n");
		while(1){}
	}

	if(ssd->dram->map->map_entry[sub->lpn].state==0)                                          /*this is the first logical page*/
	{
		ssd->dram->map->map_entry[sub->lpn].pn=find_ppn(ssd,channel,chip,die,plane,block,page);
		ssd->dram->map->map_entry[sub->lpn].state=sub->state;
	}
	else                                                                                      /*这个逻辑页进行了更新，需要将原来的页置为失效*/
	{
		ppn=ssd->dram->map->map_entry[sub->lpn].pn;
		location=find_location(ssd,ppn);
		ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state=0;        //表示某一页失效，同时标记valid和free状态都为0
		ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state=0;         //表示某一页失效，同时标记valid和free状态都为0
		ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn=0;
		ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num++;
		if((location->page)%2==0){
			ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_lsb_num++;
			}
		if (ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num==ssd->parameter->page_block)    //该block中全是invalid的页，可以直接删除
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
		ssd->dram->map->map_entry[sub->lpn].pn=find_ppn(ssd,channel,chip,die,plane,block,page);
		ssd->dram->map->map_entry[sub->lpn].state=(ssd->dram->map->map_entry[sub->lpn].state|sub->state);
	}

	sub->ppn=ssd->dram->map->map_entry[sub->lpn].pn;
	sub->location->channel=channel;
	sub->location->chip=chip;
	sub->location->die=die;
	sub->location->plane=plane;
	sub->location->block=block;
	sub->location->page=page;

	ssd->program_count++;
	ssd->channel_head[channel].program_count++;
	ssd->channel_head[channel].chip_head[chip].program_count++;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page--;
	if(page%2==0){
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_lsb_num--;
		ssd->free_lsb_count--;
		}
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].lpn=sub->lpn;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].valid_state=sub->state;
	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].free_state=((~(sub->state))&full_page);
	ssd->write_flash_count++;

	return ssd;
}


/********************************************
*函数的功能就是让两个位置不同的page位置相同
*********************************************/
struct ssd_info *make_same_level(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane,unsigned int block,unsigned int aim_page)
{
	int i=0,step,page;
	struct direct_erase *new_direct_erase,*direct_erase_node;

	page=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_page+1;                  /*需要调整的当前块的可写页号*/
	step=aim_page-page;
	while (i<step)
	{
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page+i].valid_state=0;     /*表示某一页失效，同时标记valid和free状态都为0*/
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page+i].free_state=0;      /*表示某一页失效，同时标记valid和free状态都为0*/
		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page+i].lpn=0;

		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].invalid_page_num++;

		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_page_num--;

		ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page--;
		/*
		if((page+i)%2==0){
			ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].invalid_lsb_num++;
			ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_lsb_num--;
			ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_lsb_num--;
			}
		else{
			ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_msb_num--;
			}
		*/
		i++;
	}

	ssd->waste_page_count+=step;

	ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_page=aim_page-1;

	if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].invalid_page_num==ssd->parameter->page_block)    /*该block中全是invalid的页，可以直接删除*/
	{
		new_direct_erase=(struct direct_erase *)malloc(sizeof(struct direct_erase));
		alloc_assert(new_direct_erase,"new_direct_erase");
		memset(new_direct_erase,0, sizeof(struct direct_erase));

		direct_erase_node=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node;
		if (direct_erase_node==NULL)
		{
			ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node=new_direct_erase;
		}
		else
		{
			new_direct_erase->next_node=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node;
			ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node=new_direct_erase;
		}
	}

	if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_page>=ssd->parameter->page_block)
		{
		printf("CCC error! the last write page larger than 64!!\n");
		while(1){}
		}

	return ssd;
}


/****************************************************************************
*在处理高级命令的写子请求时，这个函数的功能就是计算处理时间以及处理的状态转变
*功能还不是很完善，需要完善，修改时注意要分为静态分配和动态分配两种情况
*****************************************************************************/
struct ssd_info *compute_serve_time(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,struct sub_request **subs, unsigned int subs_count,unsigned int command)
{
	unsigned int i=0;
	unsigned int max_subs_num=0;
	struct sub_request *sub=NULL,*p=NULL;
	struct sub_request * last_sub=NULL;
	max_subs_num=ssd->parameter->die_chip*ssd->parameter->plane_die;

	if((command==INTERLEAVE_TWO_PLANE)||(command==COPY_BACK))
	{
		for(i=0;i<max_subs_num;i++)
		{
			if(subs[i]!=NULL)
			{
				last_sub=subs[i];
				subs[i]->current_state=SR_W_TRANSFER;
				subs[i]->current_time=ssd->current_time;
				subs[i]->next_state=SR_COMPLETE;
				subs[i]->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+(subs[i]->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
				subs[i]->complete_time=subs[i]->next_state_predict_time;

				delete_from_channel(ssd,channel,subs[i]);
			}
		}
		ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;
		ssd->channel_head[channel].current_time=ssd->current_time;
		ssd->channel_head[channel].next_state=CHANNEL_IDLE;
		ssd->channel_head[channel].next_state_predict_time=last_sub->complete_time;

		ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;
		ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
		ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;
	}
	else if(command==TWO_PLANE)
	{
		for(i=0;i<max_subs_num;i++)
		{
			if(subs[i]!=NULL)
			{

				subs[i]->current_state=SR_W_TRANSFER;
				if(last_sub==NULL)
				{
					subs[i]->current_time=ssd->current_time;
				}
				else
				{
					subs[i]->current_time=last_sub->complete_time+ssd->parameter->time_characteristics.tDBSY;
				}

				subs[i]->next_state=SR_COMPLETE;
				subs[i]->next_state_predict_time=subs[i]->current_time+7*ssd->parameter->time_characteristics.tWC+(subs[i]->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
				subs[i]->complete_time=subs[i]->next_state_predict_time;
				last_sub=subs[i];

				delete_from_channel(ssd,channel,subs[i]);
			}
		}
		ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;
		ssd->channel_head[channel].current_time=ssd->current_time;
		ssd->channel_head[channel].next_state=CHANNEL_IDLE;
		ssd->channel_head[channel].next_state_predict_time=last_sub->complete_time;

		ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;
		ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
		ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;
	}
	else if(command==INTERLEAVE)
	{
		for(i=0;i<max_subs_num;i++)
		{
			if(subs[i]!=NULL)
			{

				subs[i]->current_state=SR_W_TRANSFER;
				if(last_sub==NULL)
				{
					subs[i]->current_time=ssd->current_time;
				}
				else
				{
					subs[i]->current_time=last_sub->complete_time;
				}
				subs[i]->next_state=SR_COMPLETE;
				subs[i]->next_state_predict_time=subs[i]->current_time+7*ssd->parameter->time_characteristics.tWC+(subs[i]->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
				subs[i]->complete_time=subs[i]->next_state_predict_time;
				last_sub=subs[i];

				delete_from_channel(ssd,channel,subs[i]);
			}
		}
		ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;
		ssd->channel_head[channel].current_time=ssd->current_time;
		ssd->channel_head[channel].next_state=CHANNEL_IDLE;
		ssd->channel_head[channel].next_state_predict_time=last_sub->complete_time;

		ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;
		ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
		ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;
	}
	else if(command==NORMAL)
	{
		subs[0]->current_state=SR_W_TRANSFER;
		subs[0]->current_time=ssd->current_time;
		subs[0]->next_state=SR_COMPLETE;
		subs[0]->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+(subs[0]->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
		subs[0]->complete_time=subs[0]->next_state_predict_time;

		delete_from_channel(ssd,channel,subs[0]);

		ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;
		ssd->channel_head[channel].current_time=ssd->current_time;
		ssd->channel_head[channel].next_state=CHANNEL_IDLE;
		ssd->channel_head[channel].next_state_predict_time=subs[0]->complete_time;

		ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;
		ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
		ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;
		//**************************************************************
		// int new_ppn;
		// new_ppn=subs[0]->ppn;
		// if (new_ppn%2 == 0)
		// 	{
		// 		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG_L;
		// 		printf("LSB programming\n");
		// 	}
		// else
		// 	{
		// 		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG_M;
		// 		printf("MSB programming\n");
		// 	}
		//****************************************************************
	}
	else
	{
		return NULL;
	}

	return ssd;

}


/*****************************************************************************************
*函数的功能就是把子请求从ssd->subs_w_head或者ssd->channel_head[channel].subs_w_head上删除
******************************************************************************************/
struct ssd_info *delete_from_channel(struct ssd_info *ssd,unsigned int channel,struct sub_request * sub_req)
{
	struct sub_request *sub,*p;

	/******************************************************************
	*完全动态分配子请求就在ssd->subs_w_head上
	*不是完全动态分配子请求就在ssd->channel_head[channel].subs_w_head上
	*******************************************************************/
	if ((ssd->parameter->allocation_scheme==0)&&(ssd->parameter->dynamic_allocation==0))
	{
		sub=ssd->subs_w_head;
	}
	else
	{
		sub=ssd->channel_head[channel].subs_w_head;
	}
	p=sub;

	while (sub!=NULL)
	{
		if (sub==sub_req)
		{
			if ((ssd->parameter->allocation_scheme==0)&&(ssd->parameter->dynamic_allocation==0))
			{
				if(ssd->parameter->ad_priority2==0)
				{
					ssd->real_time_subreq--;
				}

				if (sub==ssd->subs_w_head)                                                     /*将这个子请求从sub request队列中删除*/
				{
					if (ssd->subs_w_head!=ssd->subs_w_tail)
					{
						ssd->subs_w_head=sub->next_node;
						sub=ssd->subs_w_head;
						continue;
					}
					else
					{
						ssd->subs_w_head=NULL;
						ssd->subs_w_tail=NULL;
						p=NULL;
						break;
					}
				}//if (sub==ssd->subs_w_head)
				else
				{
					if (sub->next_node!=NULL)
					{
						p->next_node=sub->next_node;
						sub=p->next_node;
						continue;
					}
					else
					{
						ssd->subs_w_tail=p;
						ssd->subs_w_tail->next_node=NULL;
						break;
					}
				}
			}//if ((ssd->parameter->allocation_scheme==0)&&(ssd->parameter->dynamic_allocation==0))
			else
			{
				if (sub==ssd->channel_head[channel].subs_w_head)                               /*将这个子请求从channel队列中删除*/
				{
					if (ssd->channel_head[channel].subs_w_head!=ssd->channel_head[channel].subs_w_tail)
					{
						ssd->channel_head[channel].subs_w_head=sub->next_node;
						sub=ssd->channel_head[channel].subs_w_head;
						continue;;
					}
					else
					{
						ssd->channel_head[channel].subs_w_head=NULL;
						ssd->channel_head[channel].subs_w_tail=NULL;
						p=NULL;
						break;
					}
				}//if (sub==ssd->channel_head[channel].subs_w_head)
				else
				{
					if (sub->next_node!=NULL)
					{
						p->next_node=sub->next_node;
						sub=p->next_node;
						continue;
					}
					else
					{
						ssd->channel_head[channel].subs_w_tail=p;
						ssd->channel_head[channel].subs_w_tail->next_node=NULL;
						break;
					}
				}//else
			}//else
		}//if (sub==sub_req)
		p=sub;
		sub=sub->next_node;
	}//while (sub!=NULL)

	return ssd;
}


struct ssd_info *un_greed_interleave_copyback(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,struct sub_request *sub1,struct sub_request *sub2)
{
	unsigned int old_ppn1,ppn1,old_ppn2,ppn2,greed_flag=0;

	old_ppn1=ssd->dram->map->map_entry[sub1->lpn].pn;
	get_ppn(ssd,channel,chip,die,sub1->location->plane,sub1);                                  /*找出来的ppn一定是发生在与子请求相同的plane中,才能使用copyback操作*/
	ppn1=sub1->ppn;

	old_ppn2=ssd->dram->map->map_entry[sub2->lpn].pn;
	get_ppn(ssd,channel,chip,die,sub2->location->plane,sub2);                                  /*找出来的ppn一定是发生在与子请求相同的plane中,才能使用copyback操作*/
	ppn2=sub2->ppn;

	if ((old_ppn1%2==ppn1%2)&&(old_ppn2%2==ppn2%2))
	{
		ssd->copy_back_count++;
		ssd->copy_back_count++;

		sub1->current_state=SR_W_TRANSFER;
		sub1->current_time=ssd->current_time;
		sub1->next_state=SR_COMPLETE;
		sub1->next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(sub1->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
		sub1->complete_time=sub1->next_state_predict_time;

		sub2->current_state=SR_W_TRANSFER;
		sub2->current_time=sub1->complete_time;
		sub2->next_state=SR_COMPLETE;
		sub2->next_state_predict_time=sub2->current_time+14*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(sub2->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
		sub2->complete_time=sub2->next_state_predict_time;

		ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;
		ssd->channel_head[channel].current_time=ssd->current_time;
		ssd->channel_head[channel].next_state=CHANNEL_IDLE;
		ssd->channel_head[channel].next_state_predict_time=sub2->complete_time;

		ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;
		ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
		ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;

		delete_from_channel(ssd,channel,sub1);
		delete_from_channel(ssd,channel,sub2);
	} //if ((old_ppn1%2==ppn1%2)&&(old_ppn2%2==ppn2%2))
	else if ((old_ppn1%2==ppn1%2)&&(old_ppn2%2!=ppn2%2))
	{
		ssd->interleave_count--;
		ssd->copy_back_count++;

		sub1->current_state=SR_W_TRANSFER;
		sub1->current_time=ssd->current_time;
		sub1->next_state=SR_COMPLETE;
		sub1->next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(sub1->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
		sub1->complete_time=sub1->next_state_predict_time;

		ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;
		ssd->channel_head[channel].current_time=ssd->current_time;
		ssd->channel_head[channel].next_state=CHANNEL_IDLE;
		ssd->channel_head[channel].next_state_predict_time=sub1->complete_time;

		ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;
		ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
		ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;

		delete_from_channel(ssd,channel,sub1);
	}//else if ((old_ppn1%2==ppn1%2)&&(old_ppn2%2!=ppn2%2))
	else if ((old_ppn1%2!=ppn1%2)&&(old_ppn2%2==ppn2%2))
	{
		ssd->interleave_count--;
		ssd->copy_back_count++;

		sub2->current_state=SR_W_TRANSFER;
		sub2->current_time=ssd->current_time;
		sub2->next_state=SR_COMPLETE;
		sub2->next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(sub2->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
		sub2->complete_time=sub2->next_state_predict_time;

		ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;
		ssd->channel_head[channel].current_time=ssd->current_time;
		ssd->channel_head[channel].next_state=CHANNEL_IDLE;
		ssd->channel_head[channel].next_state_predict_time=sub2->complete_time;

		ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;
		ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
		ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;

		delete_from_channel(ssd,channel,sub2);
	}//else if ((old_ppn1%2!=ppn1%2)&&(old_ppn2%2==ppn2%2))
	else
	{
		ssd->interleave_count--;

		sub1->current_state=SR_W_TRANSFER;
		sub1->current_time=ssd->current_time;
		sub1->next_state=SR_COMPLETE;
		sub1->next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+2*(ssd->parameter->subpage_page*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
		sub1->complete_time=sub1->next_state_predict_time;

		ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;
		ssd->channel_head[channel].current_time=ssd->current_time;
		ssd->channel_head[channel].next_state=CHANNEL_IDLE;
		ssd->channel_head[channel].next_state_predict_time=sub1->complete_time;

		ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;
		ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
		ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;

		delete_from_channel(ssd,channel,sub1);
	}//else

	return ssd;
}


struct ssd_info *un_greed_copyback(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,struct sub_request *sub1)
{
	unsigned int old_ppn,ppn;

	old_ppn=ssd->dram->map->map_entry[sub1->lpn].pn;
	get_ppn(ssd,channel,chip,die,0,sub1);                                                     /*找出来的ppn一定是发生在与子请求相同的plane中,才能使用copyback操作*/
	ppn=sub1->ppn;

	if (old_ppn%2==ppn%2)
	{
		ssd->copy_back_count++;
		sub1->current_state=SR_W_TRANSFER;
		sub1->current_time=ssd->current_time;
		sub1->next_state=SR_COMPLETE;
		sub1->next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(sub1->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
		sub1->complete_time=sub1->next_state_predict_time;

		ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;
		ssd->channel_head[channel].current_time=ssd->current_time;
		ssd->channel_head[channel].next_state=CHANNEL_IDLE;
		ssd->channel_head[channel].next_state_predict_time=sub1->complete_time;

		ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;
		ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
		ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;
	}//if (old_ppn%2==ppn%2)
	else
	{
		sub1->current_state=SR_W_TRANSFER;
		sub1->current_time=ssd->current_time;
		sub1->next_state=SR_COMPLETE;
		sub1->next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+2*(ssd->parameter->subpage_page*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
		sub1->complete_time=sub1->next_state_predict_time;

		ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;
		ssd->channel_head[channel].current_time=ssd->current_time;
		ssd->channel_head[channel].next_state=CHANNEL_IDLE;
		ssd->channel_head[channel].next_state_predict_time=sub1->complete_time;

		ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;
		ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;
		ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;
		ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;
	}//else

	delete_from_channel(ssd,channel,sub1);

	return ssd;
}


/****************************************************************************************
*函数的功能是在处理读子请求的高级命令时，需要找与one_page相匹配的另外一个page即two_page
*没有找到可以和one_page执行two plane或者interleave操作的页,需要将one_page向后移一个节点
*****************************************************************************************/
struct sub_request *find_interleave_twoplane_page(struct ssd_info *ssd, struct sub_request *one_page,unsigned int command)
{
	struct sub_request *two_page;

	if (one_page->current_state!=SR_WAIT)
	{
		return NULL;
	}
	if (((ssd->channel_head[one_page->location->channel].chip_head[one_page->location->chip].current_state==CHIP_IDLE)||((ssd->channel_head[one_page->location->channel].chip_head[one_page->location->chip].next_state==CHIP_IDLE)&&
		(ssd->channel_head[one_page->location->channel].chip_head[one_page->location->chip].next_state_predict_time<=ssd->current_time))))
	{
		two_page=one_page->next_node;
		if(command==TWO_PLANE)
		{
			while (two_page!=NULL)
		    {
				if (two_page->current_state!=SR_WAIT)
				{
					two_page=two_page->next_node;
				}
				else if ((one_page->location->chip==two_page->location->chip)&&(one_page->location->die==two_page->location->die)&&(one_page->location->block==two_page->location->block)&&(one_page->location->page==two_page->location->page))
				{
					if (one_page->location->plane!=two_page->location->plane)
					{
						return two_page;                                                       /*找到了与one_page可以执行two plane操作的页*/
					}
					else
					{
						two_page=two_page->next_node;
					}
				}
				else
				{
					two_page=two_page->next_node;
				}
		     }//while (two_page!=NULL)
		    if (two_page==NULL)                                                               /*没有找到可以和one_page执行two_plane操作的页,需要将one_page向后移一个节点*/
		    {
				return NULL;
			}
		}//if(command==TWO_PLANE)
		else if(command==INTERLEAVE)
		{
			while (two_page!=NULL)
		    {
				if (two_page->current_state!=SR_WAIT)
				{
					two_page=two_page->next_node;
				}
				else if ((one_page->location->chip==two_page->location->chip)&&(one_page->location->die!=two_page->location->die))
				{
					return two_page;                                                           /*找到了与one_page可以执行interleave操作的页*/
				}
				else
				{
					two_page=two_page->next_node;
				}
		     }
		    if (two_page==NULL)                                                                /*没有找到可以和one_page执行interleave操作的页,需要将one_page向后移一个节点*/
		    {
				return NULL;
			}//while (two_page!=NULL)
		}//else if(command==INTERLEAVE)

	}
	{
		return NULL;
	}
}


/*************************************************************************
*在处理读子请求高级命令时，利用这个还是查找可以执行高级命令的sub_request
**************************************************************************/
int find_interleave_twoplane_sub_request(struct ssd_info * ssd, unsigned int channel,struct sub_request * sub_request_one,struct sub_request * sub_request_two,unsigned int command)
{
	sub_request_one=ssd->channel_head[channel].subs_r_head;
	while (sub_request_one!=NULL)
	{
		sub_request_two=find_interleave_twoplane_page(ssd,sub_request_one,command);                /*找出两个可以做two_plane或者interleave的read子请求，包括位置条件和时间条件*/
		if (sub_request_two==NULL)
		{
			sub_request_one=sub_request_one->next_node;
		}
		else if (sub_request_two!=NULL)                                                            /*找到了两个可以执行two plane操作的页*/
		{
			break;
		}
	}

	if (sub_request_two!=NULL)
	{
		if (ssd->request_queue!=ssd->request_tail)
		{                                                                                         /*确保interleave read的子请求是第一个请求的子请求*/
			if ((ssd->request_queue->lsn-ssd->parameter->subpage_page)<(sub_request_one->lpn*ssd->parameter->subpage_page))
			{
				if ((ssd->request_queue->lsn+ssd->request_queue->size+ssd->parameter->subpage_page)>(sub_request_one->lpn*ssd->parameter->subpage_page))
				{
				}
				else
				{
					sub_request_two=NULL;
				}
			}
			else
			{
				sub_request_two=NULL;
			}
		}//if (ssd->request_queue!=ssd->request_tail)
	}//if (sub_request_two!=NULL)

	if(sub_request_two!=NULL)
	{
		return SUCCESS;
	}
	else
	{
		return FAILURE;
	}

}


/**************************************************************************
*这个函数非常重要，读子请求的状态转变，以及时间的计算都通过这个函数来处理
*还有写子请求的执行普通命令时的状态，以及时间的计算也是通过这个函数来处理的
****************************************************************************/
Status go_one_step(struct ssd_info * ssd, struct sub_request * sub1,struct sub_request *sub2, unsigned int aim_state,unsigned int command)
{
	unsigned int i=0,j=0,k=0,m=0;
	long long time=0;
	struct sub_request * sub=NULL ;
	struct sub_request * sub_twoplane_one=NULL, * sub_twoplane_two=NULL;
	struct sub_request * sub_interleave_one=NULL, * sub_interleave_two=NULL;
	struct local * location=NULL;
	if(sub1==NULL)
	{
		return ERROR;
	}

	/***************************************************************************************************
	*处理普通命令时，读子请求的目标状态分为以下几种情况SR_R_READ，SR_R_C_A_TRANSFER，SR_R_DATA_TRANSFER
	*写子请求的目标状态只有SR_W_TRANSFER
	****************************************************************************************************/
	if(command==NORMAL)
	{
		sub=sub1;
		location=sub1->location;
		switch(aim_state)//目标状态
		{
			case SR_R_READ://目标状态是读
			{
				/*****************************************************************************************************
			    *这个目标状态是指flash处于读数据的状态，sub的下一状态就应该是传送数据SR_R_DATA_TRANSFER
			    *这时与channel无关，只与chip有关所以要修改chip的状态为CHIP_READ_BUSY，下一个状态就是CHIP_DATA_TRANSFER
			    ******************************************************************************************************/
				sub->current_time=ssd->current_time;
				sub->current_state=SR_R_READ;//状态重新赋值为下一下状态
				sub->next_state=SR_R_DATA_TRANSFER;
				sub->next_state_predict_time=ssd->current_time+ssd->parameter->time_characteristics.tR;
				
				ssd->channel_head[location->channel].chip_head[location->chip].current_state=CHIP_READ_BUSY;
				ssd->channel_head[location->channel].chip_head[location->chip].current_time=ssd->current_time;
				ssd->channel_head[location->channel].chip_head[location->chip].next_state=CHIP_DATA_TRANSFER;
				ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=ssd->current_time+ssd->parameter->time_characteristics.tR;
				break;
			}
			case SR_R_C_A_TRANSFER://目标状态是读命令地址传输
			{
				/*******************************************************************************************************
				*目标状态是命令地址传输时，sub的下一个状态就是SR_R_READ
				*这个状态与channel，chip有关，所以要修改channel，chip的状态分别为CHANNEL_C_A_TRANSFER，CHIP_C_A_TRANSFER
				*下一状态分别为CHANNEL_IDLE，CHIP_READ_BUSY
				*******************************************************************************************************/
				sub->current_time=ssd->current_time;
				sub->current_state=SR_R_C_A_TRANSFER;
				sub->next_state=SR_R_READ;
				sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC;
				sub->begin_time=ssd->current_time;

				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].add_reg_ppn=sub->ppn;
				ssd->read_count++;

				ssd->channel_head[location->channel].current_state=CHANNEL_C_A_TRANSFER;
				ssd->channel_head[location->channel].current_time=ssd->current_time;
				ssd->channel_head[location->channel].next_state=CHANNEL_IDLE;
				ssd->channel_head[location->channel].next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC;

				ssd->channel_head[location->channel].chip_head[location->chip].current_state=CHIP_C_A_TRANSFER;
				ssd->channel_head[location->channel].chip_head[location->chip].current_time=ssd->current_time;
				ssd->channel_head[location->channel].chip_head[location->chip].next_state=CHIP_READ_BUSY;
				ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC;

				break;

			}
			case SR_R_DATA_TRANSFER://目标状态是读数据传输
			{
				/**************************************************************************************************************
				*目标状态是数据传输时，sub的下一个状态就是完成状态SR_COMPLETE
				*这个状态的处理也与channel，chip有关，所以channel，chip的当前状态变为CHANNEL_DATA_TRANSFER，CHIP_DATA_TRANSFER
				*下一个状态分别为CHANNEL_IDLE，CHIP_IDLE。
				***************************************************************************************************************/
				sub->current_time=ssd->current_time;
				sub->current_state=SR_R_DATA_TRANSFER;
				sub->next_state=SR_COMPLETE;
				sub->next_state_predict_time=ssd->current_time+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tRC;
				sub->complete_time=sub->next_state_predict_time;

				ssd->channel_head[location->channel].current_state=CHANNEL_DATA_TRANSFER;
				ssd->channel_head[location->channel].current_time=ssd->current_time;
				ssd->channel_head[location->channel].next_state=CHANNEL_IDLE;
				ssd->channel_head[location->channel].next_state_predict_time=sub->next_state_predict_time;

				ssd->channel_head[location->channel].chip_head[location->chip].current_state=CHIP_DATA_TRANSFER;
				ssd->channel_head[location->channel].chip_head[location->chip].current_time=ssd->current_time;
				ssd->channel_head[location->channel].chip_head[location->chip].next_state=CHIP_IDLE;
				ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=sub->next_state_predict_time;

				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].add_reg_ppn=-1;

				break;
			}
			case SR_W_TRANSFER://目标状态是写
			{
				/******************************************************************************************************
				*这是处理写子请求时，状态的转变以及时间的计算
				*虽然写子请求的处理状态也像读子请求那么多，但是写请求都是从上往plane中传输数据
				*这样就可以把几个状态当一个状态来处理，就当成SR_W_TRANSFER这个状态来处理，sub的下一个状态就是完成状态了
				*此时channel，chip的当前状态变为CHANNEL_TRANSFER，CHIP_WRITE_BUSY
				*下一个状态变为CHANNEL_IDLE，CHIP_IDLE
				*******************************************************************************************************/
				sub->current_time=ssd->current_time;
				sub->current_state=SR_W_TRANSFER;
				sub->next_state=SR_COMPLETE;
				sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
				//sub->complete_time=sub->next_state_predict_time;
				time=sub->next_state_predict_time;
				if (sub->is_writeback == 1)
				{
					// printf("is-writeback is 1!\n");
					ssd->writeback_count++;
					ssd->dram->map->map_entry[sub->lpn].hdd_flag == 0;
				}
				if (sub->no_ope == 1)
				{
					printf("no_ope is 1!\n");
					abort();
				}

				ssd->channel_head[location->channel].current_state=CHANNEL_TRANSFER;
				ssd->channel_head[location->channel].current_time=ssd->current_time;
				ssd->channel_head[location->channel].next_state=CHANNEL_IDLE;
				ssd->channel_head[location->channel].next_state_predict_time=time;

				ssd->channel_head[location->channel].chip_head[location->chip].current_state=CHIP_WRITE_BUSY;
				ssd->channel_head[location->channel].chip_head[location->chip].current_time=ssd->current_time;
				ssd->channel_head[location->channel].chip_head[location->chip].next_state=CHIP_IDLE;
				//***************************************************************************
				ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=time+ssd->parameter->time_characteristics.tPROG;
				// if (sub->ppn%3 == 0){
				// 	//printf("LSB\n");
				// 	ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=time+ssd->parameter->time_characteristics.tPROG_L;
				// 	}
				// else if(sub->ppn%3 == 2){
				// 	//printf("MSB\n");
				// 	ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=time+ssd->parameter->time_characteristics.tPROG_M;
				// 	}
				// else{
				// 	ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=time+ssd->parameter->time_characteristics.tPROG_C;
				// 	}
				sub->next_state_predict_time=ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time;
				sub->complete_time=sub->next_state_predict_time;
				//***************************************************************************
				break;
			}
			default :  return ERROR;

		}//switch(aim_state)
	}//if(command==NORMAL)
	else if(command==TWO_PLANE)
	{
		/**********************************************************************************************
		*高级命令TWO_PLANE的处理，这里的TWO_PLANE高级命令是读子请求的高级命令
		*状态转变与普通命令一样，不同的是在SR_R_C_A_TRANSFER时计算时间是串行的，因为共用一个通道channel
		*还有SR_R_DATA_TRANSFER也是共用一个通道
		**********************************************************************************************/
		if((sub1==NULL)||(sub2==NULL))
		{
			return ERROR;
		}
		sub_twoplane_one=sub1;
		sub_twoplane_two=sub2;
		location=sub1->location;

		switch(aim_state)
		{
			case SR_R_C_A_TRANSFER:
			{
				sub_twoplane_one->current_time=ssd->current_time;
				sub_twoplane_one->current_state=SR_R_C_A_TRANSFER;
				sub_twoplane_one->next_state=SR_R_READ;
				sub_twoplane_one->next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC;
				sub_twoplane_one->begin_time=ssd->current_time;

				ssd->channel_head[sub_twoplane_one->location->channel].chip_head[sub_twoplane_one->location->chip].die_head[sub_twoplane_one->location->die].plane_head[sub_twoplane_one->location->plane].add_reg_ppn=sub_twoplane_one->ppn;
				ssd->read_count++;

				sub_twoplane_two->current_time=ssd->current_time;
				sub_twoplane_two->current_state=SR_R_C_A_TRANSFER;
				sub_twoplane_two->next_state=SR_R_READ;
				sub_twoplane_two->next_state_predict_time=sub_twoplane_one->next_state_predict_time;
				sub_twoplane_two->begin_time=ssd->current_time;

				ssd->channel_head[sub_twoplane_two->location->channel].chip_head[sub_twoplane_two->location->chip].die_head[sub_twoplane_two->location->die].plane_head[sub_twoplane_two->location->plane].add_reg_ppn=sub_twoplane_two->ppn;
				ssd->read_count++;
				ssd->m_plane_read_count++;

				ssd->channel_head[location->channel].current_state=CHANNEL_C_A_TRANSFER;
				ssd->channel_head[location->channel].current_time=ssd->current_time;
				ssd->channel_head[location->channel].next_state=CHANNEL_IDLE;
				ssd->channel_head[location->channel].next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC;

				ssd->channel_head[location->channel].chip_head[location->chip].current_state=CHIP_C_A_TRANSFER;
				ssd->channel_head[location->channel].chip_head[location->chip].current_time=ssd->current_time;
				ssd->channel_head[location->channel].chip_head[location->chip].next_state=CHIP_READ_BUSY;
				ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC;


				break;
			}
			case SR_R_DATA_TRANSFER:
			{
				sub_twoplane_one->current_time=ssd->current_time;
				sub_twoplane_one->current_state=SR_R_DATA_TRANSFER;
				sub_twoplane_one->next_state=SR_COMPLETE;
				sub_twoplane_one->next_state_predict_time=ssd->current_time+(sub_twoplane_one->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tRC;
				sub_twoplane_one->complete_time=sub_twoplane_one->next_state_predict_time;

				sub_twoplane_two->current_time=sub_twoplane_one->next_state_predict_time;
				sub_twoplane_two->current_state=SR_R_DATA_TRANSFER;
				sub_twoplane_two->next_state=SR_COMPLETE;
				sub_twoplane_two->next_state_predict_time=sub_twoplane_two->current_time+(sub_twoplane_two->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tRC;
				sub_twoplane_two->complete_time=sub_twoplane_two->next_state_predict_time;

				ssd->channel_head[location->channel].current_state=CHANNEL_DATA_TRANSFER;
				ssd->channel_head[location->channel].current_time=ssd->current_time;
				ssd->channel_head[location->channel].next_state=CHANNEL_IDLE;
				ssd->channel_head[location->channel].next_state_predict_time=sub_twoplane_one->next_state_predict_time;

				ssd->channel_head[location->channel].chip_head[location->chip].current_state=CHIP_DATA_TRANSFER;
				ssd->channel_head[location->channel].chip_head[location->chip].current_time=ssd->current_time;
				ssd->channel_head[location->channel].chip_head[location->chip].next_state=CHIP_IDLE;
				ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=sub_twoplane_one->next_state_predict_time;

				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].add_reg_ppn=-1;

				break;
			}
			default :  return ERROR;
		}//switch(aim_state)
	}//else if(command==TWO_PLANE)
	else if(command==INTERLEAVE)
	{
		if((sub1==NULL)||(sub2==NULL))
		{
			return ERROR;
		}
		sub_interleave_one=sub1;
		sub_interleave_two=sub2;
		location=sub1->location;

		switch(aim_state)
		{
			case SR_R_C_A_TRANSFER:
			{
				sub_interleave_one->current_time=ssd->current_time;
				sub_interleave_one->current_state=SR_R_C_A_TRANSFER;
				sub_interleave_one->next_state=SR_R_READ;
				sub_interleave_one->next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC;
				sub_interleave_one->begin_time=ssd->current_time;

				ssd->channel_head[sub_interleave_one->location->channel].chip_head[sub_interleave_one->location->chip].die_head[sub_interleave_one->location->die].plane_head[sub_interleave_one->location->plane].add_reg_ppn=sub_interleave_one->ppn;
				ssd->read_count++;

				sub_interleave_two->current_time=ssd->current_time;
				sub_interleave_two->current_state=SR_R_C_A_TRANSFER;
				sub_interleave_two->next_state=SR_R_READ;
				sub_interleave_two->next_state_predict_time=sub_interleave_one->next_state_predict_time;
				sub_interleave_two->begin_time=ssd->current_time;

				ssd->channel_head[sub_interleave_two->location->channel].chip_head[sub_interleave_two->location->chip].die_head[sub_interleave_two->location->die].plane_head[sub_interleave_two->location->plane].add_reg_ppn=sub_interleave_two->ppn;
				ssd->read_count++;
				ssd->interleave_read_count++;

				ssd->channel_head[location->channel].current_state=CHANNEL_C_A_TRANSFER;
				ssd->channel_head[location->channel].current_time=ssd->current_time;
				ssd->channel_head[location->channel].next_state=CHANNEL_IDLE;
				ssd->channel_head[location->channel].next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC;

				ssd->channel_head[location->channel].chip_head[location->chip].current_state=CHIP_C_A_TRANSFER;
				ssd->channel_head[location->channel].chip_head[location->chip].current_time=ssd->current_time;
				ssd->channel_head[location->channel].chip_head[location->chip].next_state=CHIP_READ_BUSY;
				ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC;

				break;

			}
			case SR_R_DATA_TRANSFER:
			{
				sub_interleave_one->current_time=ssd->current_time;
				sub_interleave_one->current_state=SR_R_DATA_TRANSFER;
				sub_interleave_one->next_state=SR_COMPLETE;
				sub_interleave_one->next_state_predict_time=ssd->current_time+(sub_interleave_one->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tRC;
				sub_interleave_one->complete_time=sub_interleave_one->next_state_predict_time;

				sub_interleave_two->current_time=sub_interleave_one->next_state_predict_time;
				sub_interleave_two->current_state=SR_R_DATA_TRANSFER;
				sub_interleave_two->next_state=SR_COMPLETE;
				sub_interleave_two->next_state_predict_time=sub_interleave_two->current_time+(sub_interleave_two->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tRC;
				sub_interleave_two->complete_time=sub_interleave_two->next_state_predict_time;

				ssd->channel_head[location->channel].current_state=CHANNEL_DATA_TRANSFER;
				ssd->channel_head[location->channel].current_time=ssd->current_time;
				ssd->channel_head[location->channel].next_state=CHANNEL_IDLE;
				ssd->channel_head[location->channel].next_state_predict_time=sub_interleave_two->next_state_predict_time;

				ssd->channel_head[location->channel].chip_head[location->chip].current_state=CHIP_DATA_TRANSFER;
				ssd->channel_head[location->channel].chip_head[location->chip].current_time=ssd->current_time;
				ssd->channel_head[location->channel].chip_head[location->chip].next_state=CHIP_IDLE;
				ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=sub_interleave_two->next_state_predict_time;

				ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].add_reg_ppn=-1;

				break;
			}
			default :  return ERROR;
		}//switch(aim_state)
	}//else if(command==INTERLEAVE)
	else
	{
		printf("\nERROR: Unexpected command !\n" );
		return ERROR;
	}

	return SUCCESS;
}

