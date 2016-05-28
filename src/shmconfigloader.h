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
	ShmConfigLoader(key_t shmKey, unsigned int mode);
	~ShmConfigLoader();
	int LoadConfig(const char *conf = NULL);
	std::string GetValue(const char *sessionName, const char *key)const;
	void PrintConfig()const;
	int FreeShm();

	const std::string& GetErrMsg()const
	{
		return m_errMsg;
	}

private:
	int AttachConfigShm();
	int CreateConfigShm(const char *conf);
	int AnalyseConfig(const char *conf, size_t &sessionCount, size_t &kvCount, size_t &bytes);
	int LoadToShm(const char *conf, size_t sessionCount, size_t kvCount, size_t shmBytes);
	int GetShm(size_t size);
	int DetShm();
	void TrimString(char *str);
	ConfigSession* IncConfigSession(ConfigSession *session)const;

private:
	int m_shmId;
	unsigned int m_mode;
	key_t m_shmKey;
	Config *m_configPtr;
	std::string m_errMsg;
};

#endif
