#include <disksim.h>

/**
 * @brief call disksim
 * 
 * @param time 
 * @param lpn 
 * @param size 
 * @param isread 
 * @return char* 
 */
char* exec_disksim_syssim(double time, long lpn, int size, int isread) 
{
	char average[1024], command[1024];
	FILE * temp;
	// sprintf(command, "docker exec ssd-disksim bash -c cd '/var/www/disksim/valid/ &&  ../src/syssim %d %d > temp.txt'", times, is_sequential);
	//容器里面执行
	// printf("%lf %d %ld %d %d\n", time, devno, logical_block_number,size, isread);
	sprintf(command, "cd ../disksim/valid/ && ../src/syssim cheetah4LP.parv  %lf %d %d > temp.txt", time, lpn, size, isread);
	// printf("%s\n", command);
	int i = system(command);
	// printf("i: %d\n", i);
	temp = fopen("../disksim/valid/temp.txt","r");
	if(temp == NULL )      /*打开trace文件从中读取请求*/
	{
		printf("the trace temp can't open\n");
	}
	fgets(average, 200, temp);
	// printf("average: %d\n", atoi(average));
	return atoi(average);
}