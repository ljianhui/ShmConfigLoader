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

	ShmConfigLoader configLoader(123456, MODE_WRITE);
	int ret = configLoader.LoadConfig(configFile);
	if (ret != 0)
	{
		printf("Load config failed, ret = %d, errmsg[%s]\n", ret, configLoader.GetErrMsg().c_str());
		return 0;
	}

	configLoader.PrintConfig();

	string value = configLoader.GetValue("website", "baidu");
	printf("websize.baidu=%s\n", value.c_str());
	return 0;

}