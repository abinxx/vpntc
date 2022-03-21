/*
 *	OpenVPN用户流量限速插件
 *
 *	@使用方法
 *	1.编译生成动态库 libvpntc.so
 *	2.编写限速配置文件命名 vpntc.conf
 *	3.把上面的文件复制到 /etc/openvpn
 *	4.配置文件中添加 plugin "/etc/openvpn/libvpntc.so"
 *
 *	By 阿斌	2022年3月17日	QQ: 1797106720 
 */
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "openvpn-plugin.h"

struct context {
    const char *dev;
    const char *ip;
    const char *username;
    char *classid;
};

static const char * get_env(const char *name, const char *envp[])
{
    if (envp)
    {
        int i;
        const int namelen = strlen(name);
        for (i = 0; envp[i]; ++i)
        {
            if (!strncmp(envp[i], name, namelen))
            {
                const char *cp = envp[i] + namelen;
                if (*cp == '=')
                {
                    return cp + 1;
                }
            }
        }
    }
    return NULL;
}


static char * delchar(char *str, const char ch)
{
	char *p, *q;
	q = p = str;
	
    while(*str) {
		if(*str != ch) {
			*p++ = *str;
		}
		str++;
	}
    *p = '\0';

	return q;
}


static char * get_value(const char *key) 
{
	FILE *fp = fopen("vpntc.conf", "r");
	if(!fp) return NULL;
	
	char k[32], line[128];
	char *v = malloc(32);
	
	while(fgets(line, sizeof(line), fp))
	{
		if(*line == '\n') continue;
		
		if(*line != '#' && *line != ';') 
		{
			sscanf(line, "%s%s", k, v);
			if(!strcmp(k, key)) {		
				fclose(fp);
				return v;
			}
		}
	}
	fclose(fp);
	free(v);
	return NULL;
}


OPENVPN_EXPORT openvpn_plugin_handle_t
	openvpn_plugin_open_v1(unsigned int *type_mask, const char *argv[], const char *envp[])
{
    struct context *ctx;

	ctx = (struct context *) calloc(1, sizeof(struct context));

	*type_mask = OPENVPN_PLUGIN_MASK(OPENVPN_PLUGIN_UP)
				|OPENVPN_PLUGIN_MASK(OPENVPN_PLUGIN_CLIENT_CONNECT)
				|OPENVPN_PLUGIN_MASK(OPENVPN_PLUGIN_CLIENT_DISCONNECT);

    return (openvpn_plugin_handle_t) ctx;
}

OPENVPN_EXPORT int openvpn_plugin_func_v1 
	(openvpn_plugin_handle_t handle, const int type, const char *argv[], const char *envp[])
{
	struct context *ctx = (struct context *) handle;
	char cmdStr[256];
	
	if (type == OPENVPN_PLUGIN_UP) {
		ctx->dev = get_env("dev", envp);
		
		if(!ctx->dev) {
			return OPENVPN_PLUGIN_FUNC_ERROR;	//获取网卡失败
		}
		
		// 创建一个htb队列 默认不限速
		sprintf(cmdStr, "tc qdisc add dev %s root handle 1: htb r2q 1", ctx->dev);
	}
	else if (type == OPENVPN_PLUGIN_CLIENT_CONNECT) 
	{
		ctx->username = get_env("username", envp);
		
		char *maxp = get_value(ctx->username);
		if(!maxp) return OPENVPN_PLUGIN_FUNC_SUCCESS;	//没找到则未限速
		
		ctx->ip = get_env("ifconfig_pool_remote_ip", envp);
		if(!ctx->ip) {
			return OPENVPN_PLUGIN_FUNC_ERROR;	//获取IP失败
		}
		
		ctx->classid = strdup(ctx->ip + 3);
		delchar(ctx->classid, '.');	//用ip做id保证唯一性	10.8.0.2 -> 802

		sprintf(cmdStr, "tc class replace dev %s parent 1: classid 1:%s htb rate %s && \
tc filter add dev %s parent 1: protocol ip prio 1 u32 match ip dst %s flowid 1:%s", 
		ctx->dev, ctx->classid, maxp, ctx->dev, ctx->ip, ctx->classid);	//连接后添加限速规则到过滤器
		free(maxp); //释放获取的值
	}
	else if (type == OPENVPN_PLUGIN_CLIENT_DISCONNECT) 
	{
		//下线立即清除规则 防止误限
		sprintf(cmdStr, "handle=`tc filter ls dev %s|grep 'flowid 1:%s '|awk '{print $12}'` \
&& tc filter del dev %s parent 1: prio 1 handle $handle u32", ctx->dev, ctx->classid, ctx->dev);
	}
	
	if (cmdStr)	system(cmdStr);

	return OPENVPN_PLUGIN_FUNC_SUCCESS;
}


OPENVPN_EXPORT void openvpn_plugin_close_v1(openvpn_plugin_handle_t handle)
{
    struct context *ctx = (struct context *) handle;
    free(ctx);	//释放资源
}