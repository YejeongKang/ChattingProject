#pragma comment(lib, "ws2_32.lib") 

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <algorithm>

#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>

#include <WinSock2.h>

#define MAX_SIZE 1024
#define MAX_CLIENT 3

using std::cout;
using std::cin;
using std::endl;
using std::string;
using std::vector;

// MySQL Connector/C++ 초기화
sql::Driver* driver;
sql::Connection* con;
sql::Statement* stmt;
sql::PreparedStatement* pstmt;

// 데이터베이스 주소, 사용자, 비밀번호
const string server = "tcp://127.0.0.1:3306";
const string username = "user";
const string password = "yj1130";

struct SOCKET_INFO {
    SOCKET sck;
    string user;
};

vector<SOCKET_INFO> sck_list;
SOCKET_INFO server_sock;
int client_count = 0;

// socket functions
void server_init();
void add_client();
void send_msg(const char* msg, string to);
void recv_msg(int idx);
void del_client(int idx);

int main() {
    try {
        // MySQL 데이터베이스에 연결
        driver = get_driver_instance();
        con = driver->connect(server, username, password);
    }
    catch (sql::SQLException& e) {
        cout << "Could not connect to server. Error message: " << e.what() << endl;
        exit(1);
    }

    // 데이터베이스 선택
    con->setSchema("chatting_project");

    // 한글 저장을 위한 셋팅
    stmt = con->createStatement();
    stmt->execute("set names euckr");
    if (stmt) { delete stmt; stmt = nullptr; }

    // Windows 소켓 초기화
    WSADATA wsa;
    int code = WSAStartup(MAKEWORD(2, 2), &wsa);

    if (!code) {
        // 서버 소켓 초기화
        server_init();

        // 최대 클라이언트 수 만큼 스레드 생성하여 클라이언트 연결 처리
        std::thread th1[MAX_CLIENT];
        for (int i = 0; i < MAX_CLIENT; i++) {
            th1[i] = std::thread(add_client);
        }

        // 각 스레드의 종료를 대기
        for (int i = 0; i < MAX_CLIENT; i++) {
            th1[i].join();
        }

        // 서버 소켓 종료
        closesocket(server_sock.sck);
    }
    else {
        cout << "프로그램 종료. (Error code : " << code << ")";
    }

    // Windows 소켓 라이브러리 정리
    WSACleanup();

    return 0;
}

// 디비 저장
void insert(string from, string to, string msg) {
    pstmt = con->prepareStatement("INSERT INTO chat(id_from, id_to, message) VALUES(?,?,?)");

    pstmt->setString(1, from);
    pstmt->setString(2, to);
    pstmt->setString(3, msg);
    pstmt->execute();
}

// 서버 소켓을 초기화하는 함수
void server_init() {
    server_sock.sck = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    SOCKADDR_IN server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(7777);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(server_sock.sck, (sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock.sck, SOMAXCONN);

    server_sock.user = "server";

    cout << "Server On" << endl;
}

void add_client() {
    SOCKADDR_IN addr = {};
    int addrsize = sizeof(addr);
    char buf[MAX_SIZE] = { };

    ZeroMemory(&addr, addrsize);

    SOCKET_INFO new_client = {};

    // 클라이언트의 연결을 받아들임
    new_client.sck = accept(server_sock.sck, (sockaddr*)&addr, &addrsize);
    recv(new_client.sck, buf, MAX_SIZE, 0);
    
    // 클라이언트가 보낸 사용자 이름을 받아서 저장
    // new_client = {sck: socket, user: id}
    new_client.user = string(buf);
    sck_list.push_back(new_client);

    // 서버에 클라이언트 입장 메시지 출력 및 모든 클라이언트에게 전송
    string msg = "[공지] " + new_client.user + " 님이 입장했습니다.";
    cout << msg << endl;
    send_msg(msg.c_str(), "");

    // 클라이언트가 보낸 메시지를 수신하고 처리하는 스레드 시작
    std::thread th(recv_msg, client_count);
    client_count++;

    cout << "[공지] 현재 접속자 수 : " << client_count << "명" << endl;

    th.join();
}

// 클라이언트에게 메세지 보내기
void send_msg(const char* msg, string to) {
    if (to == "") {
        // 모든 클라이언트에게 메시지를 전송
        for (int i = 0; i < sck_list.size(); i++) {
            send(sck_list[i].sck, msg, MAX_SIZE, 0);
        }
    }
    else {
        // 특정 클라이언트에게 메시지를 전송 (if문 추가)
        for (int i = 0; i < sck_list.size(); i++) {
            if (sck_list[i].user == to) send(sck_list[i].sck, msg, MAX_SIZE, 0);
        }
    }
}

void recv_msg(int idx) {
    char buf[MAX_SIZE] = { };
    string msg = "";

    while (1) {
        ZeroMemory(&buf, MAX_SIZE);

        // 클라이언트로부터 메시지를 수신
        if (recv(sck_list[idx].sck, buf, MAX_SIZE, 0) > 0) {
            string from = sck_list[idx].user;
            string msg = buf;
            string to = "";

            // 귓속말 처리
            if (msg.substr(0, 2) == "->") {
                std::stringstream ss(msg);
                // 공백 기준 두번째 단어 to에 저장, 임시로 msg에 ':' 저장
                ss >> to >> to >> msg;
                // 나머지 문자열 msg에 저장
                std::getline(ss, msg);
            }

            // 디엠 전송 시 왼쪽 공백 제거
            if (msg.at(0) == ' ') msg = msg.substr(1);

            // 디비 저장
            insert(from, to, msg);

            msg = from + " : " + msg;
            send_msg(msg.c_str(), to);
        }
        else {
            msg = "[공지] " + sck_list[idx].user + " 님이 퇴장했습니다.";
            cout << msg << endl;
            send_msg(msg.c_str(), "");
            del_client(idx);
            return;
        }
    }
}

void del_client(int idx) {
    closesocket(sck_list[idx].sck);
    client_count--;
}