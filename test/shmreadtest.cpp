#include <stdio.h>
#include "shmconfigloader.h"

using namespace std;

int main()
{
	ShmConfigLoader configLoader(123456, MODE_READ);
	int ret = configLoader.LoadConfig("./test.ini");
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