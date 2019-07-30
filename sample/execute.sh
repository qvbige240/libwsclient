export LD_LIBRARY_PATH=/home/zouqing/osource/network/src/rt/libwsclient/install/lib

for i in `seq 1 8`
do
    #./test ws://localhost:9002 &
    ./voice_test ws://127.0.0.1:9002 pcm_8000_1.pcm &

done

