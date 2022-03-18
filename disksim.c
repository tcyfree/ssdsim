#include <disksim.h>

/**
 * @brief 调用disksim接口，返回时间
 * 执行次数、读写、随机/顺序
 * @param times 
 * @param is_read 
 * @param is_sequential 
 * @return char* 
 */
char* exec_disksim_syssim(int times, int is_read, int is_sequential) 
{
	char average[1024], command[1024];
	FILE * temp;
	// sprintf(command, "docker exec ssd-disksim bash -c cd '/var/www/disksim/valid/ &&  ../src/syssim %d %d > temp.txt'", times, is_sequential);
	//容器里面执行
	sprintf(command, "cd ../disksim/valid/ && ../src/syssim %d %d %d > temp.txt", times, is_read, is_sequential);
	// printf("%s\n", command);
	int i = system(command);
	// printf("i: %d\n", i);
	temp = fopen("../disksim/valid/temp.txt","r");
	if(temp == NULL )      /*打开trace文件从中读取请求*/
	{
		printf("the trace temp can't open\n");
	}
	fgets(average, 200, temp);
	// printf("average: %d\n",average);
	return atoi(average);
}