#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>

#define SHMSZ 8
#define KEY "1234"
#define MAXLINE 1024   /*max text line length*/
#define SERV_PORT 3000 /*port*/
#define LISTENQ 10     /*maximum number of client connections*/

struct mesg_buffer
{
    long mesg_type;
    char mesg_text[100];
} message1, message2, message3;

// struct cho command
typedef struct
{
    // ON | STOP
    char code[100];
    // params[0]: deviceId, params[1]: mode
    char params[2][256];
} command;

// thông tin hệ thống
typedef struct
{
    int warningThreshold, maxThreshold, currentSupply, status;
} t_systemInfo;

// thông tin thiết bị
typedef struct
{
    int deviceID, normalVoltage, savingVoltage, status;
} t_deviceInfo;

// 3 chế độ của thiết bị yêu cầu điện
enum deviceMode
{
    D_OFF,
    D_NORMAL,
    D_SAVING
};

// 4 chế độ hệ thống
enum systemStatus
{
    S_OFF,
    S_NORMAL,
    S_WARNING,
    S_OVER
};

// socket lắng nghe và socket kết nối tới
int listenSock,
    connectSock, n;
// id tiến trình con cho mỗi kết nối
pid_t pid;
// request payload
char request[MAXLINE];
struct sockaddr_in serverAddr, clientAddr;
socklen_t clilen;
command cmd;
char *shm2;
t_systemInfo systemInfo;

// tiến trình con
void powerSupply(command cmd)
{
    int deviceId = atoi(cmd.params[0]);
    t_deviceInfo deviceInfo;
    int voltage, mode;
    char response[100], infoBuffer[100];
    char *infoToken;
    key_t key1, key2, key3;
    int msgId1, msgId2, msgId3;
    long msgtype = 1;

    // ftok để tạo khoá System V IPC dùng cho message queue
    key1 = ftok("keyfile", 1); // queue để gửi tới elePowerCtrl
    key2 = ftok("keyfile", 2); // queue để gửi tới powerSupplyInfoAccess
    key3 = ftok("keyfile", 3); // queue đọc từ elePowerCtrl

    // msgget tạo 1 message queue hoặc get existing one
    // trả về message queue id
    msgId1 = msgget(key1, 0666 | IPC_CREAT);
    msgId2 = msgget(key2, 0666 | IPC_CREAT);
    msgId3 = msgget(key3, 0666 | IPC_CREAT);

    printf("Đang gửi message tới elePowerCtrl...\n\n");
    // nếu code nhận từ client là STOP
    if (strcmp(cmd.code, "STOP") == 0)
    {
        sprintf(message1.mesg_text, "%d|%s|", deviceId, "OFF");
        // msgsnd để gửi message tới elecPowerCtrl
        message1.mesg_type = msgtype;
        if(msgsnd(msgId1, &message1, sizeof(message1.mesg_text), 0) == -1) printf("Failed: Gửi message tới elePowerCtrl\n\n");
    }
    // nếu code là ON
    else
    {
        sprintf(message1.mesg_text, "%d|%s|", deviceId, cmd.params[1]);
        // msgsnd để gửi message tới elePowerCtrl
        message1.mesg_type = msgtype;
        if(msgsnd(msgId1, &message1, sizeof(message1.mesg_text), 0) == -1) printf("Failed: Gửi message tới elePowerCtrl\n\n");
    }
    // msgrcv để lấy message từ elePowerCtrl: "[systemInfo.status]|[equipInfo.currentSupply]|[equipInfo.status]"
    if (msgrcv(msgId3, &message3, sizeof(message3), 1, 0) != -1)
    {
        printf("Success: Lấy message từ elePowerCtrl\n\n");
        memset(infoBuffer, 0, sizeof(infoBuffer));
        strcpy(infoBuffer, message3.mesg_text);
        // gửi message3 nhận được cho client
        send(connectSock, infoBuffer, sizeof(infoBuffer), 0);
        printf("Success: Trả về kết quả cho client\n\n");
    }

    //send(connectSock, KEY, 4, 0);
}

void sig_chld(int singno)
{
    pid_t pid;
    int stat;
    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
        printf("Huỷ tiến trình con %d\n", pid);
    return;
}

// chuyển request sang command
command convertRequestToCommand(char *request)
{
    char copy[100];
    strcpy(copy, request);
    command cmd;
    char *token;
    int i = 0;
    token = strtok(copy, "|");
    strcpy(cmd.code, token);
    while (token != NULL)
    {
        token = strtok(NULL, "|");
        if (token != NULL)
        {
            strcpy(cmd.params[i++], token);
        }
    }
    return cmd;
}

int main()
{
    // shared memory id
    int shmid;
    key_t key;
    // con trỏ shared memory
    int *shm;
    key = atoi(KEY);
    int currentVoltage = 0;
    // connectMng
    // khởi tạo kết nối IP
    printf("Đang khởi tạo kết nối IP...\n\n");
    if ((listenSock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Xảy ra lỗi khi khởi tạo socket\n");
        exit(1);
    }
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(SERV_PORT);

    int enable = 1;
    if (setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    if (bind(listenSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        printf("Xảy ra lỗi khi bind địa chỉ\n");
        exit(2);
    }

    listen(listenSock, LISTENQ);
    clilen = sizeof(clientAddr);

    // nhận kết nối từ client
    while (1)
    {
        printf("Đang chấp nhận kết nối...\n\n");
        connectSock = accept(listenSock, (struct sockaddr *)&clientAddr, &clilen);
        // tạo tiến trình con
        if ((pid = fork()) == 0)
        {
            close(listenSock);
            // nhận request từ client
            // "[code]|[deviceId]||[mode]"
            while ((n = recv(connectSock, request, MAXLINE, 0)) > 0)
            {
                request[n] = '\0';
                cmd = convertRequestToCommand(request);
                printf("command: %s\n\n", request);
                // tiến trình powerSupply
                powerSupply(cmd);
            }

            close(connectSock);
            exit(0);
        }
        signal(SIGCHLD, sig_chld);
        close(connectSock);
    }
}