#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shmconfigloader.h"

using namespace std;

int main(int argc, char *argv[])
{
	char configFile[256] = {'\0'};
	char path[256] = {'\0'};
	char *pc = strrchr(argv[0], '/');
	memcpy(path, argv[0], pc - argv[0] + 1);
	path[pc - argv[0] + 1] = '\0';
	snprintf(configFile, sizeof(configFile) - 1, "%s/test.ini", path);

	ShmConfigLoader configLoader(123456, MODE_READ);
	int ret = configLoader.LoadConfig(configFile);
	if (ret != 0)
	{
		printf("Load config failed, ret = %d, errmsg[%s]\n", ret, configLoader.GetErrMsg().c_str());
		return 0;
	}

	printf("websize.baidu=%s\n", configLoader.GetValue("website", "baidu").c_str());
	printf("network.protocol=%s\n", configLoader.GetValue("network", "protocol").c_str());
	printf("ext.py=%s\n", configLoader.GetValue("ext", "py").c_str());
	printf("ext.txt=%s\n", configLoader.GetValue("ext", "txt").c_str());
	configLoader.FreeShm();
	return 0;

}