# CompactIndex
this is a demo of CoNEXT 17's paper Compact-Index

Prerequesites
=============
|  dependency  |  version  |
|  ----------  |  -----    |
|  gcc         |  7.5.0    |
|  mysql       |  5.7.35   |
|  ubuntu      |  18.04    |

Besids, you need install mysql C api, like this:
>sudo apt-get install libmysqlclient15-dev

Install
=======
this demo is divided into two parts:**storage** and **retrieval**, and they both depend on a dynamic link library: **libtdms**. **libtdms** provides apis for 
storage program and retrival program, and implements algorithms we proposed in Paper Compct-Index.
1. Compile libtdms
>   - cd /path/to/CompactIndex
>   - make
>   - sudo cp -f lib/libtdms.so /usr/lib/
2. Compile storage program
>   - gcc s_test.c -o store -ltdms -lmysqlclient
3. Compile retrieval program
>   - gcc q_test.c -o query -ltdms -lmysqlclient
  
**tips:**
there are some configurations hard coded in source file:s_test.c and q_test.c, you should modify them to adapt your environment.

RUN
===
1. Run store program
>   - sudo ./store(the path to traffic data is hard coded in s_test.c, you should modify it to your real path to traffic data)
2. Run query program
>   - sudo ./query(the query expression is alse hard coded in source file q_test.c, you can modify it to query what you want)

Recommendation
==============
As this just is a test demo, the source code is a little confusing and difficult to understand, we suggest that you only focus on the parts related to implemention 
of algorithm proposed in paper, these includes:
1. the index of packtets, it located in ./src/index.c:
  - buildIndexForLongKey()
  - buildIndexForShortKey()
2. the persistence of index, it located in ./src/flush.c:
  - dumpShortIndexToFile()
  - dumpLongIndexToFile()
3. the parse of index file when retrieval, it located in ./src/fileOperation.c
4. the compression and decompression of index, it located in ./src/compress.c and ./src/decompress.c
