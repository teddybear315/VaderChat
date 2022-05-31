#include <sys/types.h>
#include <errno.h>
#include <vector>
#include <iostream>
#include <string>
#include <WS2tcpip.h>
#include <thread>
#include <mutex>
#include <signal.h>

#define MAX_LEN 16384
#define MSG_LEN 256
#define NUM_UCOLORS 10

using std::string, std::vector, std::thread, std::cout, std::cin, std::endl;

struct terminal {
	int id;
	string name;
	SOCKET socket;
	thread th;
};

enum COLORS {
	RED = 160,
	ORANGE = 208,
	GOLD = 220,
	YELLOW = 226,
	LIME = 46,
	GREEN = 22,
	CYAN = 51,
	LIGHT_BLUE = 33,
	BLUE = 21,
	CORNFLOWER = 69,
	LAVENDER = 99,
	PURPLE = 92,
	MAGENTA = 200,
	PINK = 198
};
string user_colors[]={"\033[38;5;"+std::to_string(COLORS::RED)+"m", "\033[38;5;"+std::to_string(COLORS::ORANGE)+"m", "\033[38;5;"+std::to_string(COLORS::GOLD)+"m", "\033[38;5;"+std::to_string(COLORS::GREEN)+"m","\033[38;5;"+std::to_string(COLORS::CYAN)+"m", "\033[38;5;"+std::to_string(COLORS::BLUE)+"m", "\033[38;5;"+std::to_string(COLORS::CORNFLOWER)+"m", "\033[38;5;"+std::to_string(COLORS::LAVENDER)+"m", "\033[38;5;"+std::to_string(COLORS::PURPLE)+"m", "\033[38;5;"+std::to_string(COLORS::MAGENTA)+"m"};

enum PACKET_TYPE {
	PASSWORD_REQUEST = 0,
	USERNAME_REQUEST = 1,
	CONNECTION_SUCCESS = 2,
	CONNECTION_FAILURE = 3,
	MESSAGE = 4,
	USER_KICK = 5,
	USER_BAN = 6,
	SHUTDOWN = 7,
	USER_JOIN = 8,
	USER_LEAVE = 9,
	DATA = 10,
};
string packet_types[] = {"PasswordRequest", "UsernameRequest", "ConnectSuccess", "ConnectFail", "UserMessage", "UserKick", "UserBan", "Shutdown", "UserJoin", "UserLeave", "Data"};

string server_name = "My Server", server_motd = "A happy chat room.", server_password = "";
string server_ip = "127.0.0.1";
int server_port = 6969;

vector<terminal> clients;
string def_col="\033[0m";
int seed=0;
std::mutex cout_mtx,clients_mtx;
SOCKET server_socket;
sockaddr_in server;
thread t_input, t_listen;
bool exit_flag = false, verbose = false;

string color(int code);
int eraseText(int cnt);
void catch_ctrl_c(int signal);
void set_name(int id, char name[]);
string get_name(int id);
void shared_print(string str);
void print_caret();
void broadcast_message(PACKET_TYPE type, int sender_id, string message = "");
void end_connection(int id);
void handle_client(SOCKET client_socket, int id);
void tf_input(SOCKET server_socket);
void tf_listen(SOCKET client_socket, sockaddr_in client, int len);

int main(int argc, char **argv) {
	if (argc > 1) {
		for (int i = 1; i < argc; i++) {
			switch (argv[i][1]) {
			case 'n':
			case 'N':
				server_name = string(argv[i]).substr(3);
				break;
			case 'm':
			case 'M':
				server_motd = string(argv[i]).substr(3);
				break;
			case 'p':
			case 'P':
				server_port = stoi(string(argv[i]).substr(3));
				break;
			case 'w':
			case 'W':
				server_password = string(argv[i]).substr(3);
				break;
			case 'i':
			case 'I':
				server_ip = string(argv[i]).substr(3);
				break;
			case 'v':
			case 'V':
				verbose = true;
				break;
			default:
				break;
			}
		}
	}

	signal(SIGINT, catch_ctrl_c);

	// Winsock initialization
	WSADATA wsData;
	WORD ver = MAKEWORD(2, 2);

	int wsOk = WSAStartup(ver, &wsData);
	if (wsOk != 0) {
		std::cerr << "Failed to initialize winsock. Quitting program..." << std::endl;
		return 1;
	}

	server_socket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(server_socket==INVALID_SOCKET) {
        wprintf(L"socket failed with error %u\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return 1;
	}

	server.sin_family=AF_INET;
	server.sin_port=htons(server_port);
	server.sin_addr.s_addr=inet_addr(server_ip.c_str());

    if (bind(server_socket, (sockaddr *) &server, sizeof(server)) == SOCKET_ERROR) {
        wprintf(L"bind failed with error %u\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

	if(listen(server_socket,SOMAXCONN)==-1) {
        wprintf(L"listen failed with error %u\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return 1;
	}

	sockaddr_in client;
	SOCKET client_socket;
	int len=sizeof(sockaddr_in);
	cout<<"\033[2J\033[H\n\033[38;5;33m"<<endl<<"\t  ====== Welcome to "<<server_name<<" ======   "<<endl<<"\t\t"<<server_motd<<endl<<def_col;
	cout<<"\033[38;5;226m\t\t" << server_ip << ":" << server_port << def_col << endl;
	cout<<"\033[s";

	thread t1(tf_input, server_socket);
	thread t2(tf_listen, client_socket, client, len);

	t_input=move(t1);
	t_listen=move(t2);

	if(t_input.joinable())
		t_input.join();
	if(t_listen.joinable())
		t_listen.join();

	return 0;
}

void tf_input(SOCKET server_socket) {
	while(!exit_flag) {
		char msgData[MAX_LEN] = "";
		print_caret();
		cin.getline(msgData, MAX_LEN);
		string smsg = msgData;
		if (msgData[0] == '#') {
			if(smsg == "#exit") {
				exit_flag=true;
				broadcast_message(PACKET_TYPE::SHUTDOWN, -1);
				t_input.detach();
				t_listen.detach();
				closesocket(server_socket);
			} else if (smsg.find("#kick") == 0) {
				int s1 = smsg.find(' ');
				string username = smsg.substr(s1+1);
				for (int i = 0; i < clients.size(); i++) {
					if (clients[i].name == username) {
						broadcast_message(PACKET_TYPE::USER_KICK, clients[i].id);
						shared_print("\033[u"+color(clients[i].id)+clients[i].name+" has been kicked\n\033[s"+def_col);
						end_connection(clients[i].id);
					}
				}
			}
		} else if (strcmp(msgData, "") != 0) {
			broadcast_message(PACKET_TYPE::MESSAGE, -1, msgData);
		}
		if (cin.fail()) {
			cin.clear();
			cin.ignore(MAX_LEN, '\n');
		}
	}
	catch_ctrl_c(0);
}

void tf_listen(SOCKET client_socket, sockaddr_in client, int len) {
	while(!exit_flag) {
		client_socket=accept(server_socket,(sockaddr *)&client,&len);
		if(client_socket==INVALID_SOCKET) {
			wprintf(L"listen failed with error %u\n", WSAGetLastError());
			closesocket(server_socket);
			WSACleanup();
			exit(1);
		}
		seed++;
		thread t(handle_client,client_socket,seed);
		std::lock_guard<std::mutex> guard(clients_mtx);
		clients.push_back({seed, string("Anonymous"),client_socket,(move(t))});
	}

	for(int i=0; i<clients.size(); i++) {
		if(clients[i].th.joinable()) {
			clients[i].th.join();
			closesocket(clients[i].socket);
		}
	}

	closesocket(server_socket);
	WSACleanup();
}

string color(int code) {
	return user_colors[code%NUM_UCOLORS];
}

int eraseText(int cnt) {
	char back_space=8;
	for(int i=0; i<cnt; i++) {
		cout<<back_space;
	}
}

// Set name of client
void set_name(int id, char name[]) {
	for(int i=0; i<clients.size(); i++) {
		if(clients[i].id==id) {
			clients[i].name=string(name);
		}
	}
}

// get name of client
string get_name(int id) {
	if (id == -1) return "Server";
	for(int i=0; i<clients.size(); i++) {
		if(clients[i].id==id) {
			return clients[i].name;
		}
	}
}

// Handler for "Ctrl + C"
void catch_ctrl_c(int signal) {
	exit_flag = true;
	shared_print("Broadcasting shutdown...");
	broadcast_message(PACKET_TYPE::SHUTDOWN, -1);

	for(int i=0; i<clients.size(); i++) {
		if(clients[i].th.joinable()) {
			clients[i].th.join();
			closesocket(clients[i].socket);
			clients.erase(clients.begin()+i);
			shared_print("Disconnecting client " + clients[i].name);
		}
	}

	closesocket(server_socket);
	WSACleanup();
	exit(0);
}

void print_caret() {
	shared_print("\033[H\033[K> \033[38;5;244mType '#exit' to exit.\033[0;3f"+def_col);
	fflush(stdout);
}

// For synchronisation of cout statements
void shared_print(string str) {
	std::lock_guard<std::mutex> guard(cout_mtx);
	cout<<str;
}

// Broadcast message to all clients except the sender
void broadcast_message(PACKET_TYPE type, int sender_id, string message) {
	if (clients.size() == 0) return;
	string format = packet_types[type];
	if (type == PACKET_TYPE::MESSAGE)
		format += '|' + get_name(sender_id) + '|' + std::to_string(sender_id) + '|' + message;
	else if (type == PACKET_TYPE::USER_JOIN || type == PACKET_TYPE::USER_LEAVE || type == PACKET_TYPE::USER_KICK || type == PACKET_TYPE::USER_BAN)
		format += '|' + std::to_string(sender_id) + '|' + get_name(sender_id);
	else if (message != "") format += '|' + message;
	char temp[MAX_LEN];
	strcpy(temp,format.c_str());
	for(int i=0; i<clients.size(); i++) {
		if(clients[i].id!=sender_id || type == PACKET_TYPE::MESSAGE)
			send(clients[i].socket,temp,sizeof(temp),0);
	}
}

void end_connection(int id) {
	for(int i=0; i<clients.size(); i++) {
		if(clients[i].id==id) {
			std::lock_guard<std::mutex> guard(clients_mtx);
			clients[i].th.detach();
			closesocket(clients[i].socket);
			clients.erase(clients.begin()+i);
			return;
		}
	}
}

void handle_client(SOCKET client_socket, int id) {
	char data[MAX_LEN];
	string sdata = "";
	if (server_password != "") {
		bool logged_in = false;
		while (!logged_in) {
			memset(data, 0, MAX_LEN);
			strcpy(data, packet_types[PACKET_TYPE::PASSWORD_REQUEST].c_str());
			send(client_socket, data, sizeof(data), 0);
			recv(client_socket, data, sizeof(data), 0);
			sdata = string(data);
			string password = "";
			if (sdata.substr(0, sdata.find('|')) == packet_types[PACKET_TYPE::DATA]) password = sdata.substr(sdata.find('|')+1);
			if (password == server_password) {
				logged_in = true;
			} else {
				shared_print("\033[u\033[38;5;"+std::to_string(COLORS::PINK)+"mAttempt "+sdata.substr(sdata.find('|')+1)+"/"+server_password+"\n\033[s");
				print_caret();
			}
		}
	}
	memset(data, 0, MAX_LEN);
	strcpy(data, packet_types[PACKET_TYPE::USERNAME_REQUEST].c_str());
	send(client_socket,data,sizeof(data),0);
	recv(client_socket,data,sizeof(data),0);
	sdata = string(data);
	string name = "Anonymous";
	if (sdata.substr(0, sdata.find('|')) == packet_types[PACKET_TYPE::DATA]) name = sdata.substr(sdata.find('|')+1);
	bool found = false;
	for (int i = 0; i < clients.size(); i++) {
		if (name == clients[i].name) {
			found = true;
			sdata = "";
			break;
		}
	}
	while (found && name != "Anonymous") {
		memset(data, 0, MAX_LEN);
		strcpy(data, packet_types[PACKET_TYPE::USERNAME_REQUEST].c_str());
		send(client_socket,data,sizeof(data), 0);
		recv(client_socket,data,sizeof(data),0);
		sdata = string(data);
		if (sdata.substr(0, sdata.find('|')) == packet_types[PACKET_TYPE::DATA]) name = sdata.substr(sdata.find('|')+1);
		found = false;
		for (int i = 0; i < clients.size(); i++) {
			if (name == clients[i].name) {
				found = true;
				break;
			}
		}
	}
	char _name[32];
	strcpy(_name, name.c_str());
	set_name(id,_name);

	string msg = packet_types[PACKET_TYPE::CONNECTION_SUCCESS] + '|' + server_name + '|' + server_motd;
	memset(data, 0, MAX_LEN);
	strcpy(data, msg.c_str());
	send(client_socket, data, sizeof(data), 0);

	// Display welcome message
	string welcome_message= name+(verbose ? "("+std::to_string(id)+")" : "")+" has joined";
	broadcast_message(PACKET_TYPE::USER_JOIN,id);
	// broadcast_message(PACKET_TYPE::MESSAGE,id,welcome_message);
	shared_print("\033[u"+color(id)+welcome_message+def_col+"\n\033[s");
	print_caret();
	
	while(!exit_flag) {
		int bytes_received=recv(client_socket,data,sizeof(data),0);
		if(bytes_received<=0)
			return;
		else if (verbose) shared_print("\033[upacket:"+string(data)+"\n\033[s");
		if(strcmp(data, "#exit") == 0) {
			// Display leaving message
			string message=name+" has left";
			broadcast_message(PACKET_TYPE::USER_LEAVE,id);
			shared_print("\033[u"+color(id)+message+def_col+"\n\033[s");
			end_connection(id);
		} else {
			broadcast_message(PACKET_TYPE::MESSAGE, id, data);
			shared_print("\033[u"+color(id)+name+": "+def_col+data+"\n\033[s");
		}
		print_caret();
	}
}