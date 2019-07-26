libwsclient
===========


WebSocket client demo for C

Also, to compile:

./autogen.sh

./configuer --prefix=$PWD/install

make && make install

Then link your C program against wsclient: 
 gcc -g -O0 -I../install/include -o test_voice sample_voice_8000.c g711.c g711_table.c -lwsclient -L../install/lib -lasound -lpthread


run:
# export LD_LIBRARY_PATH=$(ROOT)/install/lib
 ./test_voice ws://127.0.0.1:9002 pcm_8000_1.pcm


