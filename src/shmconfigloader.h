#ifndef _SHMCONFIGLOADER_H
#define _SHMConfigLoader_H

#include <sys/types.h>
#include <sys/shm.h>
#include <string>

#define MODE_READ 0444
#define MODE_WRITE (0644|IPC_CREAT)

#define ERR_OPEN_CONFIG -1001
#define ERR_CONFIG_FORMAT -1002

#define ERR_INVALID_MODE -2001
#define ERR_SHMGET -2002
#define ERR_SHMAT -2003
#define ERR_SHMDT -2004
#define ERR_SHMCTL_RMID -2005

typedef struct Config Config;
typedef struct ConfigSession ConfigSession;

class ShmConfigLoader
{
public:
	/*
	 * ShmConfigLoader的构造函数,构造函数并不会申请共享内存,申请操作是在LoadConfig中进行的
	 * @Params shmKey 创建共享内存时的key
	 * @Params mode 创建共享内存时的模式,为MODE_READ, MODE_WRITE之一
	 */
	ShmConfigLoader(key_t shmKey, unsigned int mode);

	/*
	 * ShmConfigLoader的析构函数,
	 * 该函数detaches共享内存,但并不会释放共享内存,若需要释放,请调用FreeShm函数
	 */
	~ShmConfigLoader();

	/*
	 * 若创建该对象时选择的是MODE_WRITE模式,则会创建共享内存,并把文件conf的内容转换成对象,导入到共享内存中
	 * 若创建该对象时选择的是MODE_READ模式,则只会Attach共享内存中的配置对象,conf参数会被忽略
	 * @Params conf MODE_WRITE模式时的配置文件名
	 * @Return 0 if succussful
	 */
	int LoadConfig(const char *conf = NULL);

	std::string GetValue(const char *sessionName, const char *key)const;
	void PrintConfig()const;

	/*
	 * 该函数detaches共享内存,并释放共享内存
	 */
	int FreeShm();

	const std::string& GetErrMsg()const
	{
		return m_errMsg;
	}

private:
	/*
	 * MODE_READ模式下,被LoadConfig函数调用,Attach共享内存中的配置对象
	 */
	int AttachConfigShm();

	/*
	 * MODE_READ模式下,被LoadConfig函数调用,
	 * 分析文件conf,并根据文件分析的结果,创建共享内存,把文件conf的内容转换成对象,导入到共享内存中
	 */
	int CreateConfigShm(const char *conf);
	
	/*
	 * 分析配置文件conf的内容,计算文件中的总session数,kv数和有效的总字节数
	 */
	int AnalyseConfig(const char *conf, size_t &sessionCount, size_t &kvCount, size_t &bytes);

	/*
	 * 根据配置文件conf的session数,kv数和字节数,把配置文件转换成转换对象,保存在共享内存中
	 */
	int LoadToShm(const char *conf, size_t sessionCount, size_t kvCount, size_t shmBytes);

	int GetShm(size_t size);
	int DetShm();

	/*
	 * 删除字符串两端的空白字符
	 */
	void TrimString(char *str);

	/*
	 * 相当于++sessin,因为ConfigSession为变长结构,该函数计算++session的偏移字节数
	 */
	ConfigSession* IncConfigSession(ConfigSession *session)const;

private:
	int m_shmId;
	unsigned int m_mode;
	key_t m_shmKey;
	Config *m_configPtr;
	std::string m_errMsg;
};

#endif
