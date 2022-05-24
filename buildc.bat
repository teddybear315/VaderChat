g++ -std=c++1z -Wall -Iinclude\ -c .\src\client.cpp
g++ .\src\client.o -o Client.exe -static -lws2_32
client -v