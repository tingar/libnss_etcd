libnss_etcd
===========

A libnss resolver based off etcdctl.

The resolver will look for hosts specified in the /hosts/ key. For example, if you have /hosts/foo with the value "10.0.0.2", you can do:
```
$ ping foo
PING foo (10.0.0.2) 56(84) bytes of data.
```

Installation
------------

*IMPORTANT*

This plugin requires that you have [etcdctl](https://github.com/coreos/etcdctl/) installed somewhere in your path. It will not work without it. For best results, you'll probably also want a working etcd installation.

```
$ make
# cp libnss_etcd.so.2 /lib/libnss_etcd.so.2
# ln -s /lib/libnss_etcd.so.2 /lib/libnss_etcd.so
```

Then, include "etcd" in your /etc/nsswitch.conf files section:
```
...
hosts = files etcd dns
...
```
