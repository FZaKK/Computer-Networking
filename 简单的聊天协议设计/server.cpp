#include<iostream>
#include<string.h>
#include<string>
#include<map>
#include<queue>
#include<WinSock2.h>
#include<WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")

#include <typeinfo>


#define DEFAULT_BUFLEN 100
#define DEFAULT_PORT 27015

using namespace std;
map<SOCKET, int> user_map;  // ���е�int���Ϊ1��Ϊ����
queue<string> message_queue; /*���������Ϣ���������Σ���Ҫ�����Ϣ����
                               ����һ����������Ϣת�����߳�*/

// Ϊÿһ�����ӵ��˶˿ڵ��û�����һ���߳�
DWORD WINAPI handlerRequest(LPVOID lparam)
{
    SOCKET ClientSocket = (SOCKET)(LPVOID)lparam;
    user_map[ClientSocket] = 1;

    char user_name[10];
    strcpy_s(user_name, to_string(ClientSocket).data());
    send(ClientSocket, user_name, 10, 0);

    SYSTEMTIME systime = { 0 };
    GetLocalTime(&systime);
    cout << endl << systime.wYear << "��" << systime.wMonth << "��" << systime.wDay << "��";
    cout << systime.wHour << "ʱ" << systime.wMinute << "��" << systime.wSecond << "��" << endl;
    cout << "Log���û�--" << ClientSocket << "--�������죡" << endl;
    cout << "-----------------------------------------------------" << endl;

    // ѭ�����ܿͻ�������
    int recvResult;
    int sendResult;
    int flag = 1; // �����Ƿ��˳���Socket�Ľ���ѭ��
    int judge_flag = 1; // Ĭ��Ⱥ��
    do {
        char recvBuf[DEFAULT_BUFLEN] = "";
        char sendBuf[DEFAULT_BUFLEN] = "";
        char special_message[DEFAULT_BUFLEN + 50] = "";
        recvResult = recv(ClientSocket, recvBuf, DEFAULT_BUFLEN, 0);
        judge_flag = 1;
        if (recvResult > 0) {
            char special_user_name[10] = "";
            for (int i = 0; i < DEFAULT_BUFLEN; i++) {
                if (recvBuf[i] == '|') {
                    judge_flag = 0;
                }
            }

            // temp_flagΪ0����Ҫ�������ͣ��ж��Ƿ�Ҫ����
            if (judge_flag == 0) {
                int j;
                for (j = 0; j < DEFAULT_BUFLEN; j++) {
                    if (recvBuf[j] == '|') {
                        special_user_name[j] = '\0';
                        for (int z = j + 1; z < DEFAULT_BUFLEN; z++) {
                            special_message[z - j - 1] = recvBuf[z];
                        }
                        strcat_s(special_message, "( from ");
                        strcat_s(special_message, to_string(ClientSocket).data());
                        strcat_s(special_message, ")\0");
                        break;
                    }
                    else {
                        special_user_name[j] = recvBuf[j];
                    }
                }
            }

            strcpy_s(sendBuf, "�û�--");
            string ClientID = to_string(ClientSocket);
            strcat_s(sendBuf, ClientID.data()); // data����ֱ��ת��Ϊchar*
            strcat_s(sendBuf, "--: ");
            strcat_s(sendBuf, recvBuf);
            // message_queue.push(sendBuf); //���ｫ��Ϣ�洢��������

            SYSTEMTIME Logtime = { 0 };
            GetLocalTime(&Logtime);
            cout << endl << Logtime.wYear << "��" << Logtime.wMonth << "��" << Logtime.wDay << "��";
            cout << Logtime.wHour << "ʱ" << Logtime.wMinute << "��" << Logtime.wSecond << "��" << endl;
            cout << "Log���û�--" << ClientSocket << "--����Ϣ��" << recvBuf << endl;
            cout << "-----------------------------------------------------" << endl;

            if (judge_flag == 1) {
                for (auto it : user_map) {
                    if (it.first != ClientSocket && it.second == 1) {
                        sendResult = send(it.first, sendBuf, DEFAULT_BUFLEN, 0);
                        if (sendResult == SOCKET_ERROR)
                            cout << "send failed with error: " << WSAGetLastError() << endl;
                    }
                }
            }
            else {
                for (auto it : user_map) {
                    if (to_string(it.first) == string(special_user_name) && it.second == 1) {
                        sendResult = send(it.first, special_message, DEFAULT_BUFLEN, 0);
                        if (sendResult == SOCKET_ERROR)
                            cout << "send failed with error: " << WSAGetLastError() << endl;
                    }
                }
            }
        }
        else {
            flag = 0;
        }
    } while (recvResult != SOCKET_ERROR && flag != 0);

    GetLocalTime(&systime);
    cout << endl << systime.wYear << "��" << systime.wMonth << "��" << systime.wDay << "��";
    cout << systime.wHour << "ʱ" << systime.wMinute << "��" << systime.wSecond << "��" << endl;
    cout << "Log���û�--" << ClientSocket << "--�뿪�����죨quq��" << endl;
    cout << "-----------------------------------------------------" << endl;

    closesocket(ClientSocket);
    return 0;
}

int main()
{

    //----------------------
    // ��ʼ��Winsock
    WSADATA wsaData;
    int iResult;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        cout << "WSAStartup failed with error: " << iResult << endl;
        return 1;
    }
    //----------------------
    // ����һ��������SOCKET
    // �����connect��������´���һ���߳�
    SOCKET ListenSocket;
    ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ListenSocket == INVALID_SOCKET) {
        cout << "socket failed with error: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }
    //----------------------
    // ����bind�����󶨵�IP��ַ�Ͷ˿ں�
    sockaddr_in service;
    service.sin_family = AF_INET;
    inet_pton(AF_INET, "10.130.151.45", &service.sin_addr.s_addr);
    service.sin_port = htons(27015);
    iResult = bind(ListenSocket, (SOCKADDR*)&service, sizeof(service));
    if (iResult == SOCKET_ERROR) {
        wprintf(L"bind failed with error: %ld\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }
    //----------------------
    // �������������������ź�
    if (listen(ListenSocket, 5) == SOCKET_ERROR) {
        cout << "listen failed with error: " << WSAGetLastError() << endl;
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }
    //----------------------
    // ����ÿ���µ�����ʹ�ö��̴߳���
    cout << "Waiting for client to connect..." << endl;

    while (1) {
        sockaddr_in addrClient;
        int len = sizeof(sockaddr_in);
        // ���ܳɹ�������clientͨѶ��Socket
        SOCKET AcceptSocket = accept(ListenSocket, (SOCKADDR*)&addrClient, &len);
        if (AcceptSocket == INVALID_SOCKET) {
            cout << "accept failed with error: " << WSAGetLastError() << endl;
            closesocket(ListenSocket);
            WSACleanup();
            return 1;
        }
        else {
            // �����̣߳����Ҵ�����clientͨѶ���׽���
            HANDLE hThread = CreateThread(NULL, 0, handlerRequest, (LPVOID)AcceptSocket, 0, NULL);
            CloseHandle(hThread); // �رն��̵߳�����
        }
    }

    // �رշ����SOCKET
    iResult = closesocket(ListenSocket);
    if (iResult == SOCKET_ERROR) {
        cout << "close failed with error: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    WSACleanup();
    return 0;
}
