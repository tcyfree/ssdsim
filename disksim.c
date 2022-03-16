#include <disksim.h>

/**
 * @brief 调用disksim接口，返回时间
 * 
 * @param times 
 * @param random 
 * @return char* 
 */
char* exec_disksim_syssim(int times, int random) 
{
	char average[1024], command[1024];
	FILE * temp;
	// sprintf(command, "docker exec ssd-disksim bash -c cd '/var/www/disksim/valid/ &&  ../src/syssim %d %d > temp.txt'", times, random);
	sprintf(command, "cd ../disksim/valid/ && ../src/syssim %d %d > temp.txt", times, random);
	printf("%s\n", command);
	int i = system(command);
	printf("i: %d\n", i);
	temp = fopen("../disksim/valid/temp.txt","r");
	if(temp == NULL )      /*打开trace文件从中读取请求*/
	{
		printf("the trace temp can't open\n");
	}
	fgets(average, 200, temp);
	return average;
}