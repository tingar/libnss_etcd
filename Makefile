all: libnss_etcd.so.2

libnss_etcd.so.2: nssrc.c
	gcc -shared -fPIC -o libnss_etcd.so.2 -Wl,-soname,libnss_etcd.so.2 nssrc.c
