#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define MAXLINE 1024
#define PORT 3000

typedef struct
{
	char name[100];
	int normalMode;
	int savingMode;
} device;

enum mode
{
	OFF,
	NORMAL,
	SAVING
};

device *deviceList;
int N;
int clientSocket;
char serverResponse[MAXLINE];
int *shm;
char *shm2;
char info[1000];
char systemStatus[10], equipStatus[10];

int kbhit();
int getch();
void showMenuDevices();
void showMenu();
void getResponse();
void makeCommand(char *command, char *code, char *param1, char *param2);
void showMenuAction(int deviceID);
int runDevice(int deviceID, int isSaving);
void stopDevice(char *deviceName);

int main()
{
	// khởi tạo kết nối IP
	struct sockaddr_in serverAddr;
	clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket < 0)
	{
		printf("[-]Lỗi kết nối.\n");
		exit(1);
	}

	memset(&serverAddr, '\0', sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
	{
		printf("[-]Lỗi kết nối.\n");
		exit(1);
	}
	printf("[+]Đã kết nối với connectMng.\n");

	while (1)
	{
		showMenuDevices();
	}

	return 0;
}

void showMenuDevices()
{
	int choice;
	char c;
	while (1)
	{
		choice = 0;
		printf("-----Menu-----\n");
		printf("Chọn thiết bị kết nối:\n");
		int i;
		printf("1. TV\n");
		printf("2. AR\n");
		printf("3. PC\n");
		printf("4. IRON\n");
		printf("5. LIGHT\n");
		printf("6. Quit\n\n");
		printf("-----------------\n");
		printf("Lựa chọn của bạn: \n");
		while (choice == 0)
		{
			if (scanf("%d", &choice) < 1)
			{
				choice = 0;
			}
			if (choice < 1 || choice > 6)
			{
				choice = 0;
				printf("Không hợp lệ.\n");
				printf("Nhập lại lựa chọn: ");
			}
			while ((c = getchar()) != '\n')
				;
		}
		if (1 <= choice && choice <= 5)
		{
			showMenuAction(choice);
		}
		else
		{
			exit(0);
		}
	}
}

void showMenuAction(int deviceID)
{
	int choice = 0;
	char c;
	int mode = OFF;
	// first get system status
	mode = runDevice(deviceID - 1, mode);
	while (1)
	{
		choice = 0;
		while (choice == 0)
		{
			printf("Chọn hành động:\n");
			printf("1. Chạy chế độ mặc định \n");
			printf("2. Chạy chế độ tiết kiệm điện\n");
			printf("3. Tắt thiết bị và thoát\n");
			printf("Your choice: ");
			if (kbhit())
			{
				c = getch();
				choice = c - '0';
				if (choice < 1 || choice > 4)
				{
					choice = 0;
				}
			}
			else
			{
				mode = runDevice(deviceID - 1, mode);
				choice = 0;
			}
		}

		switch (choice)
		{
		case 1:
			mode = runDevice(deviceID - 1, NORMAL);
			break;
		case 2:
			mode = runDevice(deviceID - 1, SAVING);
			break;
		default:
			mode = runDevice(deviceID - 1, OFF);
			exit(0);
		}
	}
}
void getResponse()
{
	memset(serverResponse, 0, sizeof(serverResponse));
	int n = recv(clientSocket, serverResponse, MAXLINE, 0);
	if (n == 0)
	{
		perror("connectMng đã kết thúc\n\n");
		exit(4);
	}
	serverResponse[n] = '\0';
}

void makeCommand(char *command, char *code, char *param1, char *param2)
{
	strcpy(command, code);
	strcat(command, "|");
	if (param1 != NULL)
	{
		strcat(command, param1);
		strcat(command, "|");
	}
	if (param2 != NULL)
	{
		strcat(command, param2);
		strcat(command, "|");
	}
}

// chạy thiết bị, gửi về server sau khi select action
int runDevice(int deviceID, int mode)
{
	char command[100];
	char response[MAXLINE];
	char buffer[20];
	char param[20];
	int countDown = 10;
	char deviceName[100];
	char *token;
	sprintf(deviceName, "%d", deviceID);
	int voltage;

	switch (mode)
	{
	case OFF:
		strcat(deviceName, "|OFF|");
		break;
	case NORMAL:
		strcat(deviceName, "|NORMAL|");
		break;
	case SAVING:
		strcat(deviceName, "|SAVING|");
		break;
	default:
		break;
	}

	printf("Sending command mode %d to server...\n\n", mode);
	if (mode == OFF)
	{
		stopDevice(deviceName);
	}
	else if (mode == NORMAL || mode == SAVING)
	{
		makeCommand(command, "ON", deviceName, buffer);
		send(clientSocket, command, strlen(command), 0);
		getResponse();
	}

	printf("Đã nhận thành công kết quả trả về từ connectMng\n");
	memset(response, 0, sizeof(response));
	strcpy(response, serverResponse);
	token = strtok(response, "|");
	// trạng thái hệ thống
	strcpy(systemStatus, token);
	token = strtok(NULL, "|");
	// current supply
	voltage = atoi(token);
	token = strtok(NULL, "|");
	// trạng thái thiết bị
	strcpy(equipStatus, token);

	if (strcmp(systemStatus, "OVER") == 0)
	{
		if (strcmp(equipStatus, "OFF") == 0)
		{
			while (countDown > 0)
			{
				printf("Hệ thống quá tải.\nThiết bị sẽ bị tắt trong %d giây.\n", countDown);
				sleep(1);
				countDown--;
				if (countDown == 0)
				{
					stopDevice(deviceName);
					return OFF;
				}
			}
		}
		else if (strcmp(equipStatus, "SAVING") == 0)
		{
			printf("Hệ thống quá tải.\nThiết bị sẽ chạy ở chế độ %s mode.\n", equipStatus);
		}
	}
	else
	{
		printf("Hệ thống có trạng thái %s.\n Thiết bị đang chạy ở chế độ %s mode và %d W\n", systemStatus, equipStatus, voltage);
	}

	if (strcmp(equipStatus, "NORMAL") == 0)
	{
		return NORMAL;
	}
	else if (strcmp(equipStatus, "SAVING") == 0)
	{
		return SAVING;
	}
	else
		return OFF;
}

void stopDevice(char *deviceName)
{
	char command[100];
	makeCommand(command, "STOP", deviceName, NULL);
	send(clientSocket, command, strlen(command), 0);
	getResponse();
}

int kbhit()
{
	struct timeval tv = {10L, 0L};
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	return select(1, &fds, NULL, NULL, &tv);
}

int getch()
{
	int r;
	unsigned char c;
	if ((r = read(0, &c, sizeof(c))) < 0)
	{
		return r;
	}
	else
	{
		return c;
	}
}