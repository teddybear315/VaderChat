g++ -std=c++1z -Wall .\src\server.cpp -o Server.exe -Iinclude\ -static -lws2_32
g++ -std=c++1z -Wall .\src\client.cpp -o Client.exe -Iinclude\ -static -lws2_32