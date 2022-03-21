##OpenVPN用户流量限速插件

####编译动态库  

安装gcc编译器 ```apt insatll gcc -y```  
安装完成执行下面代码生成动态库  
```
gcc -shared -fPIC -o libvpntc.so vpntc.c
```

####限速配置文件  

编写限速配置文件命名 vpntc.conf  
```
#用户限速配置示列文件
#前面用户名 后面限速值 

123		100Kbps
124		1Mbps
125		1Gbps
```
####加载插件  
将动态库和限速配置文件复制到 /etc/openvpn  
在OpenVpn配置文件中添加 
```plugin "/etc/openvpn/libvpntc.so"```   

[官网地址 或 技术支持](http://vpn.bincs.cn)  
