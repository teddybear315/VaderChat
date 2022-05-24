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

string server_name = "My Server", server_motd = "A happy chat room.";
string ip = "127.0.0.1", server_password = "";
int server_port = 6969, user_color = 0;

bool exit_flag=false;
bool chatting = false, verbose = false;
std::mutex cout_mtx, cin_mtx;
thread t_send, t_recv;
string def_col="\033[0m";
string user_colors[]={"\033[38;5;"+std::to_string(COLORS::RED)+"m", "\033[38;5;"+std::to_string(COLORS::ORANGE)+"m", "\033[38;5;"+std::to_string(COLORS::GOLD)+"m", "\033[38;5;"+std::to_string(COLORS::GREEN)+"m","\033[38;5;"+std::to_string(COLORS::CYAN)+"m", "\033[38;5;"+std::to_string(COLORS::BLUE)+"m", "\033[38;5;"+std::to_string(COLORS::CORNFLOWER)+"m", "\033[38;5;"+std::to_string(COLORS::LAVENDER)+"m", "\033[38;5;"+std::to_string(COLORS::PURPLE)+"m", "\033[38;5;"+std::to_string(COLORS::MAGENTA)+"m"};
string username = "";

SOCKET client_socket;
sockaddr_in client;

void catch_ctrl_c(int signal);
string color(int code);
int eraseText(int cnt);
void shared_print(string str, bool endLine = true);
void send_message(int client_socket);
void recv_message(int client_socket);

int main(int argc, char** argv) {
	if (argc > 1) {
		for (int i = 1; i < argc; i++) {
			switch (argv[i][1]) {
			case 'i':
			case 'I':
				ip = string(argv[i]).substr(3);
				break;
			case 'u':
			case 'U':
				username = string(argv[i]).substr(3);
				break;
			case 'p':
			case 'P':
				server_port = stoi(string(argv[i]).substr(3));
				break;
			case 'w':
			case 'W':
				server_password = string(argv[i]).substr(3);
				break;
			case 'v':
			case 'V':
				verbose = true;
			default:
				break;
			}
		}
	}

	WSADATA wsData;
	WORD ver = MAKEWORD(2, 2);

	int wsOk = WSAStartup(ver, &wsData);
	if (wsOk != 0) {
		std::cerr << "Failed to initialize winsock. Quitting program..." << std::endl;
		return 1;
	}

	client_socket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(client_socket==INVALID_SOCKET) {
        wprintf(L"socket failed with error %u\n", WSAGetLastError());
        closesocket(client_socket);
        WSACleanup();
        return 1;
	}

	client.sin_family=AF_INET;
	client.sin_port=htons(server_port); // Port no. of server
	client.sin_addr.s_addr=inet_addr(ip.c_str()); // Provide IP address of server

	bool search = ip == "" || !server_port;
	while (connect(client_socket,(sockaddr *)&client,sizeof(client))==-1 || search) {
		search = false;
		char _port[6], _ip[16];
		cout <<"Enter an IP: ";
		cin.getline(_ip, 16);
		ip = _ip;
		client.sin_addr.s_addr=inet_addr(_ip); // Provide IP address of server
		cout <<"Enter a Port: ";
		cin.getline(_port, 6);
		server_port = std::stoi(_port);
		client.sin_port=htons(server_port); // Provide IP address of server
	}
	signal(SIGINT, catch_ctrl_c);
	char data[MAX_LEN];
	recv(client_socket,data,sizeof(data),0);
	int u = 0;
	while (strcmp(data, packet_types[PACKET_TYPE::PASSWORD_REQUEST].c_str()) == 0) {
		if (server_password == "") {
			cout<<"Enter the password: ";
			char pass[64];
			cin.getline(pass,64);
			server_password = pass;
		}
		string format = packet_types[PACKET_TYPE::DATA] + "|" + server_password;
		memset(data, 0, MAX_LEN);
		strcpy(data, format.c_str());
		send(client_socket,data,sizeof(data),0);
		recv(client_socket,data,sizeof(data),0);
		server_password = "";
	}
	char ubuff[32];
	if (strcmp(data, packet_types[PACKET_TYPE::USERNAME_REQUEST].c_str()) == 0) {
		memset(data, 0, MAX_LEN);
		if (username == "") {
			cout<<"Enter your username: ";
			cin.getline(ubuff,32);
			username = ubuff;
		}
		if (username == "") username = "Anonymous";
		string format = packet_types[PACKET_TYPE::DATA] + "|" + username;
		memset(data, 0, MAX_LEN);
		strcpy(data, format.c_str());
		send(client_socket,data,sizeof(data),0);
		recv(client_socket,data,sizeof(data),0);
	}
	while (strcmp(data, packet_types[PACKET_TYPE::USERNAME_REQUEST].c_str()) == 0) {
		username = "";
		cout<<"Enter a different username: ";
		cin.getline(ubuff,32);
		username = ubuff;
		if (username == "") username = "Anonymous";
		string format = packet_types[PACKET_TYPE::DATA] + "|" + username;
		memset(data, 0, MAX_LEN);
		strcpy(data, format.c_str());
		send(client_socket,data,sizeof(data),0);
		recv(client_socket,data,sizeof(data),0);
	}

	string sdata = data;
	int first = sdata.find('|'), second = sdata.find('|', first+1);
	cout << "Connecting..." << endl;
	while (sdata.substr(0, sdata.find('|')) != packet_types[PACKET_TYPE::CONNECTION_SUCCESS]) {
		if (verbose) cout << sdata.substr(0, sdata.find('|')) << endl;
		recv(client_socket, data, sizeof(data), 0);
		sdata = data;
	}
	server_name = sdata.substr(first+1, second-first-1);
	server_motd = sdata.substr(second+1);

	cout << "\033[2J\033[H\n";
	cout<<"\033[38;5;33m"<<endl<<"\t  ====== Welcome to "<<server_name<<" ======   "<<endl<<"\t\t"<<server_motd<<def_col<<endl;
	cout<<"\033[38;5;226m\t\t" << ip << ":" << server_port << def_col << endl;
	cout<<"\033[s";
	chatting = true;

	thread t2(recv_message, client_socket);
	thread t1(send_message, client_socket);

	t_recv=move(t2);
	t_send=move(t1);

	if(t_send.joinable())
		t_send.join();
	if(t_recv.joinable())
		t_recv.join();

	return 0;
}

// Handler for "Ctrl + C"
void catch_ctrl_c(int signal) {
	if (chatting) {
		char str[MAX_LEN] = "";
		strcpy(str, "#exit");
		send(client_socket,str,MAX_LEN,0);
	}
	exit_flag=true;
	t_send.detach();
	t_recv.detach();
	closesocket(client_socket);
	exit(signal);
}

string color(int code)
{
	return user_colors[code%NUM_UCOLORS];
}

// Erase text from terminal
int eraseText(int cnt) {
	char back_space=8;
	for(int i=0; i<cnt; i++) {
		cout<<back_space;
	}
}

// For synchronisation of cout statements
void shared_print(string str, bool endLine) {
	std::lock_guard<std::mutex> guard(cout_mtx);
	cout<<str;
	if(endLine) cout<<endl;
}

// Send message to everyone
void send_message(int client_socket) {
	srand(time(NULL));
	user_color = rand() % 10;
	while(!exit_flag) {
		char msgData[MAX_LEN] = "";
		shared_print("\033[H\033[K"+user_colors[user_color]+"You: \033[38;5;244mType '#exit' to exit.\033[0;6f"+def_col,false);
		memset(msgData, 0, sizeof(msgData));
		cin.getline(msgData, MAX_LEN);
		if (strcmp(msgData, "") != 0) send(client_socket,msgData,sizeof(msgData),0);
		if (cin.fail()) {
			cin.clear();
			cin.ignore(MAX_LEN, '\n');
		}
		if(strcmp(msgData, "#exit") == 0) {
			exit_flag=true;
			t_send.detach();
			t_recv.detach();
			closesocket(client_socket);
		}
	}
	catch_ctrl_c(0);
}

// Receive message
void recv_message(int client_socket) {
	char data[MAX_LEN] = "";
	string type = "";
	string sdata = "";
	while(!exit_flag) {
		memset(data, 0, MAX_LEN);
		int bytes_received=recv(client_socket,data,sizeof(data),0);
		if(bytes_received<=0) {
			exit_flag = true;
			t_send.detach();
			t_recv.detach();
		}
		sdata = string(data);
		if (sdata.find('|') == std::string::npos) {
			type = data;
			if (verbose) cout << "\033[u" << "packet:" << data << endl;
			if (type == packet_types[PACKET_TYPE::SHUTDOWN]) {
				cout << "\033[u" << "\033[38;5;" << std::to_string(COLORS::PINK) << "mServer shutdown..." << def_col << endl;
				exit_flag=true;
				t_send.detach();
				t_recv.detach();
				closesocket(client_socket);
				exit(0);
			}
		} else {
			int first_sep = sdata.find('|',0), second_sep, third_sep;
			type = sdata.substr(0, first_sep);
			if (verbose) cout << "\033[u" << "packet:" << type << "|" << data << endl;
			if (type == packet_types[PACKET_TYPE::MESSAGE]) {
				string username, message;
				int color_code;
				second_sep = sdata.find('|', first_sep+1);
				third_sep = sdata.find('|', second_sep+1);
				username = sdata.substr(first_sep+1, second_sep-first_sep-1);
				color_code = std::stoi(sdata.substr(second_sep+1, third_sep-second_sep-1));
				message = sdata.substr(third_sep+1);
				cout << "\033[u" << color(color_code) << username << ": " << def_col << message << endl;
			} else if (type == packet_types[PACKET_TYPE::USER_KICK]) {
				string username;
				int user_id;
				second_sep = sdata.find('|', first_sep+1);
				user_id = std::stoi(sdata.substr(first_sep+1, second_sep-first_sep-1));
				username = sdata.substr(second_sep+1);
				cout << "\033[u" << color(user_id) << username << " has been kicked." << def_col << endl;
			} else if (type == packet_types[PACKET_TYPE::USER_BAN]) {
				string username;
				int user_id;
				second_sep = sdata.find('|', first_sep+1);
				user_id = std::stoi(sdata.substr(first_sep+1, second_sep-first_sep-1));
				username = sdata.substr(second_sep+1);
				cout << "\033[u" << color(user_id) << username << " has been banned." << def_col << endl;
			} else if (type == packet_types[PACKET_TYPE::USER_JOIN]) {
				string username;
				int user_id;
				second_sep = sdata.find('|', first_sep+1);
				user_id = std::stoi(sdata.substr(first_sep+1, second_sep-first_sep-1));
				username = sdata.substr(second_sep+1);
				cout << "\033[u" << color(user_id) << username << " has joined." << def_col << endl;
			} else if (type == packet_types[PACKET_TYPE::USER_LEAVE]) {
				string username;
				int user_id;
				second_sep = sdata.find('|', first_sep+1);
				user_id = std::stoi(sdata.substr(first_sep+1, second_sep-first_sep-1));
				username = sdata.substr(second_sep+1);
				cout << "\033[u" << color(user_id) << username << " has left." << def_col << endl;
			}
			cout << "\033[s";
			cout << "\033[H\033[K" << user_colors[user_color] << "You: \033[38;5;244mType '#exit' to exit.\033[0;6f" << def_col;
			fflush(stdout);
		}
	}
}