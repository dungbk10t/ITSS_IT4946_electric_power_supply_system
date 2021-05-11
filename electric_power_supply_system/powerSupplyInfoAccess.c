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

struct mesg_buffer
{
    long mesg_type;
    char mesg_text[100];
} message1, message2, message3, message4, message5;

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
    key_t key1, key2, key3, key4, key5;
    int msgId1, msgId2, msgId3, msgId4, msgId5;
    int deviceID;
    FILE *fp = NULL;
    char messageBuffer[100] = "", infoBuffer[100] = "";
    char *infoToken, *messageToken;
    t_systemInfo systemInfo;
    t_deviceInfo deviceInfo;
    long msgtype = 1;
    int lineNo = 0;
    int totalPower = 0;

    // ftok để tạo khoá System V IPC dùng cho message queue
    key1 = ftok("keyfile", 1); // queue để gửi tới elePowerCtrl
    key2 = ftok("keyfile", 2); // queue để gửi tới powerSupplyInfoAccess
    key3 = ftok("keyfile", 3); // queue đọc từ elePowerCtrl
    key4 = ftok("keyfile", 4); // queue đọc từ powerSupplyInfoAccess
    key5 = ftok("keyfile", 5); // queue để gửi tới writeLogProcess
    // printf("Success: Getting message queue keys %d %d %d %d %d\n", key1, key2, key3, key4, key5);

    // msgget tạo 1 message queue hoặc get existing one
    // trả về message queue id
    msgId1 = msgget(key1, 0666 | IPC_CREAT);
    msgId2 = msgget(key2, 0666 | IPC_CREAT);
    msgId3 = msgget(key3, 0666 | IPC_CREAT);
    msgId4 = msgget(key4, 0666 | IPC_CREAT);
    msgId5 = msgget(key5, 0666 | IPC_CREAT);
    // printf("Success: Getting message ID %d %d %d %d\n", msgId1, msgId2, msgId3, msgId4);

    while (1)
    {
        // msgrcv to receive message
        message2.mesg_type = msgtype;
        memset(message2.mesg_text, 0, sizeof(message2.mesg_text));
        if (msgrcv(msgId2, &message2, sizeof(message2), 1, 0) != -1)
        {
            printf("Success: Received Message from Power Control\n");
            int accessType, infoType;

            strcpy(messageBuffer, message2.mesg_text);
            messageToken = strtok(messageBuffer, "|");
            accessType = atoi(messageToken);
            messageToken = strtok(NULL, "|");
            infoType = atoi(messageToken);
            // printf("Access Type: %d - info type: %d\n", accessType, infoType);

            if (accessType == T_READ)
            {
                if (infoType == T_SYSTEM)
                {
                    printf("Pending: Opening System Info for READING...\n");
                    if ((fp = fopen("./sysInfo", "r")) != NULL)
                        printf("Success: Opening System Info for READING\n");
                    else
                    {
                        perror(NULL);
                        exit(-1);
                    }
                    memset(infoBuffer, 0, sizeof(infoBuffer));
                    fgets(infoBuffer, 100, fp);

                    printf("%s\n", infoBuffer);
                    memset(message4.mesg_text, 0, sizeof(message4.mesg_text));
                    strcpy(message4.mesg_text, infoBuffer);
                    message4.mesg_type = msgtype;
                    msgsnd(msgId4, &message4, sizeof(message4.mesg_text), 0);
                    fclose(fp);
                    fp = NULL;
                }
                else if (infoType == T_DEVICE)
                {
                    printf("Opening Equip Info for READING...\n");
                    messageToken = strtok(NULL, "|");
                    deviceID = atoi(messageToken);
                    fp = fopen("./deviceInfo", "r");
                    int i;
                    for (i = 0; i <= deviceID; i++)
                    {
                        memset(infoBuffer, 0, sizeof(infoBuffer));
                        fgets(infoBuffer, 100, fp);
                        printf("%s", infoBuffer);
                    }

                    memset(message4.mesg_text, 0, sizeof(message4.mesg_text));
                    strcpy(message4.mesg_text, infoBuffer);
                    message4.mesg_type = msgtype;
                    msgsnd(msgId4, &message4, sizeof(message4.mesg_text), 0);
                    printf("Success: Sending Message to Power Control\n\n");
                    fclose(fp);
                    fp = NULL;
                }
            }
            else if (accessType == T_WRITE)
            {
                if (infoType == T_SYSTEM)
                {
                    // read from message
                    messageToken = strtok(NULL, "|");
                    systemInfo.currentSupply = atoi(messageToken);
                    messageToken = strtok(NULL, "|");
                    strcpy(systemInfo.status, messageToken);

                    // read from systemInfo
                    fp = fopen("./sysInfo", "r");
                    memset(infoBuffer, 0, sizeof(infoBuffer));
                    fgets(infoBuffer, 100, fp);
                    // extract info
                    infoToken = strtok(infoBuffer, "|");
                    systemInfo.warningThreshold = atoi(infoToken);
                    infoToken = strtok(NULL, "|");
                    systemInfo.maxThreshold = atoi(infoToken);
                    // reopen
                    freopen("./sysInfo", "w", fp);
                    fprintf(fp, "%d|%d|%d|%s|", systemInfo.warningThreshold, systemInfo.maxThreshold, systemInfo.currentSupply, systemInfo.status);
                    printf("Success: Writing to System Info\n");
                    fclose(fp);
                    fp = NULL;

                    // log
                    memset(message5.mesg_text, 0, sizeof(message5.mesg_text));
                    message5.mesg_type = msgtype;
                    sprintf(message5.mesg_text, "%d|%d|%s|", T_SYSTEM, systemInfo.currentSupply, systemInfo.status);
                    msgsnd(msgId5, &message5, sizeof(message5.mesg_text), 0);
                    printf("Success: Sending Message to LogWrite...\n\n");
                }
                else if (infoType == T_DEVICE)
                {
                    // read from message
                    messageToken = strtok(NULL, "|");
                    deviceID = atoi(messageToken);
                    // printf("%s", infoBuffer);

                    messageToken = strtok(NULL, "|");
                    deviceInfo.currentSupply = atoi(messageToken);
                    // printf("Device ID: %d - Current Supply: %d\n", deviceID, deviceInfo.currentSupply);
                    if (deviceInfo.currentSupply == -1) // OVERLOAD, reduce every device to SAVING
                    {
                        printf("System is OVERLOADED\n");
                        FILE *fin = fopen("./deviceInfo", "r");
                        FILE *fout = fopen("./temp", "w");
                        lineNo = 0;
                        totalPower = 0;
                        while (!feof(fin))
                        {
                            memset(infoBuffer, 0, sizeof(infoBuffer));
                            fgets(infoBuffer, 100, fin);
                            if (strlen(infoBuffer) == 0)
                                break;
                            char deviceName[100];
                            infoToken = strtok(infoBuffer, "|");
                            strcpy(deviceName, infoToken);
                            infoToken = strtok(NULL, "|");
                            deviceInfo.normalVoltage = atoi(infoToken);
                            infoToken = strtok(NULL, "|");
                            deviceInfo.savingVoltage = atoi(infoToken);
                            infoToken = strtok(NULL, "|");
                            memset(deviceInfo.status, 0, sizeof(deviceInfo.status));
                            strcpy(deviceInfo.status, infoToken);
                            if (lineNo == deviceID || (strcmp(deviceInfo.status, "OFF") != 0))
                            {
                                memset(deviceInfo.status, 0, sizeof(deviceInfo.status));
                                strcpy(deviceInfo.status, "SAVING");
                                deviceInfo.currentSupply = deviceInfo.savingVoltage;

                                // print to Device Info
                                fprintf(fout, "%s|%d|%d|%s|\n", deviceName, deviceInfo.normalVoltage, deviceInfo.savingVoltage, "SAVING");
                                totalPower += deviceInfo.savingVoltage;

                                // log
                                memset(message5.mesg_text, 0, sizeof(message5.mesg_text));
                                message5.mesg_type = msgtype;
                                sprintf(message5.mesg_text, "%d|%s|%d|%s|", T_DEVICE, deviceName, deviceInfo.currentSupply, deviceInfo.status);
                                msgsnd(msgId5, &message5, sizeof(message5.mesg_text), 0);
                                printf("Success: Sending Message to LogWrite...\n\n");
                            }
                            else
                            {
                                fprintf(fout, "%s|%d|%d|%s|\n", deviceName, deviceInfo.normalVoltage, deviceInfo.savingVoltage, "OFF");
                            }
                            lineNo++;
                            infoToken = NULL;
                        }
                        fclose(fin);
                        fin = NULL;
                        fclose(fout);
                        fout = NULL;
                        remove("./deviceInfo");
                        rename("./temp", "./deviceInfo");
                        printf("Success: Reduce Device Voltage to SAVING mode\n\n");

                        // sending total power to Power Control
                        memset(message4.mesg_text, 0, sizeof(message4.mesg_text));
                        message4.mesg_type = msgtype;
                        sprintf(message4.mesg_text, "%d|", totalPower);
                        msgsnd(msgId4, &message4, sizeof(message4.mesg_text), 0);
                        printf("Success: Sending Total Saving Power to Power Control\n\n");
                    }
                    else
                    {

                        messageToken = strtok(NULL, "|");
                        strcpy(deviceInfo.status, messageToken);

                        // copy contents from deviceInfo to temp
                        FILE *fin = fopen("./deviceInfo", "r");
                        FILE *fout = fopen("./temp", "w");
                        lineNo = 0;
                        while (!feof(fin))
                        {
                            memset(infoBuffer, 0, sizeof(infoBuffer));
                            fgets(infoBuffer, 100, fin);

                            if (lineNo == deviceID)
                            {
                                char deviceName[100];
                                infoToken = strtok(infoBuffer, "|");
                                strcpy(deviceName, infoToken);
                                infoToken = strtok(NULL, "|");
                                deviceInfo.normalVoltage = atoi(infoToken);
                                infoToken = strtok(NULL, "|");
                                deviceInfo.savingVoltage = atoi(infoToken);
                                fprintf(fout, "%s|%d|%d|%s|\n", deviceName, deviceInfo.normalVoltage, deviceInfo.savingVoltage, deviceInfo.status);

                                // log
                                memset(message5.mesg_text, 0, sizeof(message5.mesg_text));
                                message5.mesg_type = msgtype;
                                sprintf(message5.mesg_text, "%d|%s|%d|%s|", T_DEVICE, deviceName, deviceInfo.currentSupply, deviceInfo.status);
                                msgsnd(msgId5, &message5, sizeof(message5.mesg_text), 0);
                                printf("Success: Sending Message to LogWrite...\n\n");
                            }
                            else
                            {
                                fprintf(fout, "%s", infoBuffer);
                            }
                            lineNo++;
                        }
                        fclose(fin);
                        fin = NULL;
                        fclose(fout);
                        fout = NULL;
                        remove("./deviceInfo");
                        rename("./temp", "./deviceInfo");
                        printf("Success: Writing to Device Info\n\n");
                    }
                }
            }
            messageToken = NULL;
            infoToken = NULL;
        }
    }

    // to destroy the message queue
    msgctl(msgId2, IPC_RMID, NULL);
    msgctl(msgId4, IPC_RMID, NULL);
    msgctl(msgId5, IPC_RMID, NULL);

    return 0;
}