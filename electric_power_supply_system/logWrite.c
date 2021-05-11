#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>
#include <time.h>

struct mesg_buffer
{
    long mesg_type;
    char mesg_text[100];
} message5;

typedef struct
{
    int warningThreshold, maxThreshold, currentSupply;
    char status[10];
} t_systemInfo;

typedef struct
{
    int deviceID, normalVoltage, savingVoltage, currentSupply;
    char status[10];
} t_deviceInfo;

enum t_infoType
{
    T_SYSTEM,
    T_DEVICE
};

enum t_accessType
{
    T_READ,
    T_WRITE
};

int main()
{
    key_t key5;
    int msgId5;
    int deviceID;
    FILE *fp = NULL;
    char messageBuffer[100] = "", infoBuffer[100] = "";
    char *infoToken, *messageToken;
    t_systemInfo systemInfo;
    t_deviceInfo deviceInfo;
    long msgtype = 1;
    int lineNo = 0;
    int totalPower = 0;
    time_t rawtime;
    struct tm *timeinfo;
    char *dateTime = NULL;
    char deviceName[100] = "";
    char deviceLogFilename[100] = "";
    int infoType;

    // ftok to generate unique key
    key5 = ftok("keyfile", 5); // to writeLogProcess
    // printf("Success: Getting message queue keys %d\n", key5);

    // msgget creates a message queue
    // and returns identifier
    msgId5 = msgget(key5, 0666 | IPC_CREAT);
    // printf("Success: Getting message ID %d\n", msgId5);

    while (1)
    {
        // msgrcv to receive message
        message5.mesg_type = msgtype;
        memset(message5.mesg_text, 0, sizeof(message5.mesg_text));
        if (msgrcv(msgId5, &message5, sizeof(message5), 1, 0) != -1)
        {
            printf("Success: Received Message from Power Supply Info Access\n");
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            dateTime = asctime(timeinfo);
            dateTime[strlen(dateTime) - 1] = 0;

            strcpy(messageBuffer, message5.mesg_text);
            messageToken = strtok(messageBuffer, "|");
            infoType = atoi(messageToken);
            // printf("Info type: %d\n", infoType);

            if (infoType == T_SYSTEM)
            {
                // read from message
                messageToken = strtok(NULL, "|");
                systemInfo.currentSupply = atoi(messageToken);
                messageToken = strtok(NULL, "|");
                strcpy(systemInfo.status, messageToken);

                // open System Log
                fp = fopen("systemLog", "a");
                fprintf(fp, "%s\t|\t%d\t|\t%s\n", dateTime, systemInfo.currentSupply, systemInfo.status);
                printf("Success: Writing to System Info\n");
                fclose(fp);
                fp = NULL;
            }
            else if (infoType == T_DEVICE)
            {
                memset(deviceName, 0, sizeof(deviceName));
                // read from message
                messageToken = strtok(NULL, "|");
                strcpy(deviceName, messageToken);
                messageToken = strtok(NULL, "|");
                deviceInfo.currentSupply = atoi(messageToken);
                messageToken = strtok(NULL, "|");
                strcpy(deviceInfo.status, messageToken);

                // printf("Device ID: %d - Current Supply: %d\n", deviceID, deviceInfo.currentSupply);
                // write to device log
                sprintf(deviceLogFilename, "log%s.txt", deviceName);
                fp = fopen(deviceLogFilename, "a");
                fprintf(fp, "%s\t|\t%s\t|\t%d\t|\t%s\n", dateTime, deviceName, deviceInfo.currentSupply, deviceInfo.status);
                printf("Success: Writing to Device Info\n\n");
                fclose(fp);
                fp = NULL;
            }
            messageToken = NULL;
            dateTime = NULL;
        }
    }

    // to destroy the message queue
    msgctl(msgId5, IPC_RMID, NULL);

    return 0;
}