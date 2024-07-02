#pragma comment(lib, "ws2_32.lib")

#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <conio.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>

#define MAX_SIZE 1024
#define ENTER 13
#define BACK_SPACE 8

using std::cout;
using std::cin;
using std::endl;
using std::string;

SOCKET client_sock;
string my_id;

// MySQL Connector/C++ 초기화
sql::Driver* driver;
sql::Connection* con;
sql::Statement* stmt;
sql::PreparedStatement* pstmt;
sql::ResultSet* result;

// 데이터베이스 주소, 사용자, 비밀번호
const string server = "tcp://127.0.0.1:3306";
const string username = "user";
const string password = "yj1130";

bool login(string id, string pw);
bool idCheck(string id);
void signup(string id, string pw);
void chatHistory();

int chat_recv();

int main() {

    // 데이터베이스 서버 연결
    try {
        driver = get_driver_instance();
        con = driver->connect(server, username, password);
    }
    catch (sql::SQLException& e) {
        cout << "Could not connect to server. Error message: " << e.what() << endl;
        exit(1);
    }

    // 데이터베이스 선택
    con->setSchema("chatting_project");

    // db 한글 저장을 위한 셋팅 
    stmt = con->createStatement();
    stmt->execute("set names euckr");
    if (stmt) { delete stmt; stmt = nullptr; }

    // 로그인 또는 회원가입
    while (1) {
        bool isSignup = false;

        cout << "----------------------------------------------------------" << endl;
        cout << "---------------------------로그인-------------------------" << endl;
        cout << "----------------------------------------------------------" << endl;

        while (1) {
            string id, pw;
            cout << "아이디를 입력하세요: (아이디가 없을 경우 signup 입력) ";
            cin >> id;
            if (id == "signup") {
                isSignup = true;
                break;
            }

            // 비밀번호 *표시
            cout << "비밀번호를 입력하세요: ";
            while (1) {
                char c = _getch();
                if (c == ENTER) break;
                else if (c >= 33 && c <= 126) {
                    pw += c;
                    cout << "*";
                }
                else if (c == BACK_SPACE) {
                    pw = pw.substr(0, pw.size() - 1);
                    cout << "\b \b";
                }
            }
            cout << endl;

            if (login(id, pw)) {
                my_id = id;
                break;
            }
            else {
                cout << "아이디 또는 비밀번호가 틀렸습니다." << endl;
                cout << "----------------------------------------------------------" << endl;
            }
        }

        if (isSignup) {
            string id_in, pw_in;

            cout << "----------------------------------------------------------" << endl;
            cout << "--------------------------회원가입------------------------" << endl;
            cout << "----------------------------------------------------------" << endl;

            while (1) {
                cout << "아이디를 입력하세요: ";
                cin >> id_in;
                if (!idCheck(id_in)) break;
                cout << "이미 존재하는 아이디입니다." << endl;
            }
            cout << "비밀번호를 입력하세요: ";
            cin >> pw_in;
            signup(id_in, pw_in);
            continue;
        }
        break;
    }

    // 콘솔 창 클리어
    system("cls");

    // 채팅 내역 불러오기
    chatHistory();
    cout << "----------------------------------------------------------" << endl;

    WSADATA wsa;
    int code = WSAStartup(MAKEWORD(2, 2), &wsa);

    // code == 0 : 실행 성공
    if (!code) {

        client_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

        SOCKADDR_IN client_addr = {};
        client_addr.sin_family = AF_INET;
        client_addr.sin_port = htons(7777);
        InetPton(AF_INET, TEXT("127.0.0.1"), &client_addr.sin_addr);

        while (1) {
            if (!connect(client_sock, (SOCKADDR*)&client_addr, sizeof(client_addr))) {
                // 입장 시 서버로 아이디 전송
                send(client_sock, my_id.c_str(), my_id.length(), 0);
                break;
            }
            cout << "Connecting..." << endl;
        }

        std::thread th2(chat_recv);

        while (1) {
            string text;
            std::getline(cin, text);
            const char* buffer = text.c_str(); // string형을 char* 타입으로 변환
            send(client_sock, buffer, strlen(buffer), 0);
        }

        th2.join();
        // join()이 while문 뒤에 위치한 이유:
        // chat_recv 스레드가 종료될 때까지 메인 스레드가 블록되어 
        // 사용자 입력을 처리하지 못할 수 있는 문제 예방.
        closesocket(client_sock);
    }

    WSACleanup();
    return 0;
}

// 로그인
bool login(string id, string pw) {
    pstmt = con->prepareStatement("SELECT * FROM user WHERE id = ? AND pw = ?");
    pstmt->setString(1, id);
    pstmt->setString(2, pw);
    result = pstmt->executeQuery();

    bool isSuccess = false;

    while (result->next()) {
        isSuccess = true;
    }

    return isSuccess;
}

// 아이디 중복 체크
bool idCheck(string id) {
    pstmt = con->prepareStatement("SELECT * FROM user WHERE id = ?");
    pstmt->setString(1, id);
    result = pstmt->executeQuery();

    bool isExist = false;

    while (result->next()) {
        isExist = true;
    }

    return isExist;
}

// 회원가입
void signup(string id, string pw) {
    pstmt = con->prepareStatement("INSERT INTO user(id, pw) VALUES(?,?)");

    pstmt->setString(1, id);
    pstmt->setString(2, pw);
    pstmt->execute();
    cout << "회원가입이 완료되었습니다." << endl;
}

// 채팅 내역 불러오기
void chatHistory() {
    string from, to, msg;
    stmt = con->createStatement();
    result = stmt->executeQuery("SELECT * FROM chat");

    while (result->next()) {
        from = result->getString(2).c_str();
        to = result->getString(3).c_str();
        msg = result->getString(4).c_str();

        if (to == "") cout << from << " : " << msg << endl;
        else cout << from << "(" << to << ") : " << msg << endl;
    }

    delete stmt;
}

// 서버로부터 받은 메세지 출력
int chat_recv() {
    char buf[MAX_SIZE] = { };
    string msg;

    while (1) {
        ZeroMemory(&buf, MAX_SIZE);
        if (recv(client_sock, buf, MAX_SIZE, 0) > 0) {
            msg = buf;
            std::stringstream ss(msg);  // 문자열을 스트림화
            string user;
            ss >> user; // 스트림을 통해, 문자열을 공백 분리해 변수에 할당
            if (user != my_id) cout << buf << endl; // 내가 보낸 게 아닐 경우에만 출력하도록.
        }
        else {
            cout << "Server Off" << endl;
            return -1;
        }
    }
}