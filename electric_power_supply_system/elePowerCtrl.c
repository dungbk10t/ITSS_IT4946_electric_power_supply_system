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

// message
struct mesg_buffer
{
    long mesg_type;
    char mesg_text[100];
} message1, message2, message3, message4, message5;

// thông tin hệ thống
typedef struct
{
    int warningThreshold, maxThreshold, currentSupply;
    char status[10];
} t_systemInfo;

// thông tin thiết bị
typedef struct
{
    int deviceID, normalVoltage, savingVoltage, currentSupply;
    char status[10];
} t_deviceInfo;

// kiểu thông tin
enum t_infoType
{
    T_SYSTEM,
    T_DEVICE
};

// kiểu truy cập
enum t_accessType
{
    T_READ,
    T_WRITE
};

// chế độ thiết bị
enum t_deviceMode
{
    D_OFF,
    D_NORMAL,
    D_SAVING
};

// trạng thái hệ thống
enum t_systemStatus
{
    S_OFF,
    S_NORMAL,
    S_WARNING,
    S_OVER
};

int main()
{
    printf("Đang khởi động...\n");
    key_t key1, key2, key3, key4;
    int msgId1, msgId2, msgId3, msgId4;
    char messageBuffer[100] = "", infoBuffer[100] = "";
    char *infoToken;
    long msgtype = 1;

    // ftok để tạo khoá System V IPC dùng cho message queue
    key1 = ftok("keyfile", 1); // queue nhận từ connectMng
    key2 = ftok("keyfile", 2); //  queue để gửi tới powerSupplyInfoAccess
    key3 = ftok("keyfile", 3); // queue gửi tới powerSupply
    key4 = ftok("keyfile", 4); // queue đọc từ powerSupplyInfoAccess
    //printf("Success: Getting message queue keys %d %d %d %d\n", key1, key2, key3, key4);

    // msgget tạo 1 message queue hoặc get existing one
    // trả về message queue id
    msgId1 = msgget(key1, 0666 | IPC_CREAT);
    msgId2 = msgget(key2, 0666 | IPC_CREAT);
    msgId3 = msgget(key3, 0666 | IPC_CREAT);
    msgId4 = msgget(key4, 0666 | IPC_CREAT);
    // printf("Success: Getting message ID %d %d %d %d\n", msgId1, msgId2, msgId3, msgId4);

    while (1)
    {
        // msgrcv để lấy message từ powerSupply
        message1.mesg_type = msgtype;
        memset(message1.mesg_text, 0, sizeof(message1.mesg_text));
        if (msgrcv(msgId1, &message1, sizeof(message1), 1, 0) != -1)
        {
            printf("\nSuccess: Nhận được message từ powerSupply\n");
            int deviceID, requestedSupply;
            char requestedMode[10];
            t_systemInfo systemInfo;
            t_deviceInfo equipInfo;
            char *token;
            // lấy deviceId và mode từ message
            memset(messageBuffer, 0, sizeof(messageBuffer));
            strcpy(messageBuffer, message1.mesg_text);
            token = strtok(messageBuffer, "|");
            // id thiết bị
            deviceID = atoi(token);
            token = strtok(NULL, "|");
            // chế độ chạy đc yêu cầu
            strcpy(requestedMode, token);

            // Gửi yêu cầu lấy thông tin hệ thống từ powerSupplyInfoAccess
            printf("Pending: Đang lấy thông tin hệ thống...\n");
            sprintf(message2.mesg_text, "%d|%d|", T_READ, T_SYSTEM);
            message2.mesg_type = msgtype;
            // msgsnd để gửi message tới powerSupplyInfoAccess
            msgsnd(msgId2, &message2, sizeof(message2.mesg_text), 0);
            printf("Success: Gửi message tới powerSupplyInfoAccess\n");

            // nhận message từ powerSupplyInfoAccess
            message4.mesg_type = msgtype;
            memset(message4.mesg_text, 0, sizeof(message4.mesg_text));
            if (msgrcv(msgId4, &message4, sizeof(message4), 1, 0) != -1)
            {
                printf("Success: Nhận được thông tin hệ thống từ powerSupplyInfoAccess\n");
                memset(infoBuffer, 0, sizeof(infoBuffer));
                strcpy(infoBuffer, message4.mesg_text);
                // lấy thông tin
                infoToken = strtok(infoBuffer, "|");
                // tải warning
                systemInfo.warningThreshold = atoi(infoToken);
                infoToken = strtok(NULL, "|");
                // tải tối đa
                systemInfo.maxThreshold = atoi(infoToken);
                infoToken = strtok(NULL, "|");
                // lượng cung cấp hiện tại
                systemInfo.currentSupply = atoi(infoToken);
                infoToken = strtok(NULL, "|");
                // trạng thái hệ thống hiện tại
                strcpy(systemInfo.status, infoToken);
            }

            // Gửi yêu cầu thông tin thiết bị tới powerSupplyInfoAccess
            printf("Pending: Đang lấy thông tin thiết bị...\n");
            sprintf(message2.mesg_text, "%d|%d|%d|", T_READ, T_DEVICE, deviceID);
            message2.mesg_type = msgtype;
            // msgsnd để gửi message tới powerSupplyInfoAccess
            msgsnd(msgId2, &message2, sizeof(message2.mesg_text), 0);
            printf("Success: Sending Message to Power Supply Info Access...\n");

            // nhận message từ powerSupplyInfoAccess
            message4.mesg_type = msgtype;
            memset(message4.mesg_text, 0, sizeof(message4.mesg_text));
            if (msgrcv(msgId4, &message4, sizeof(message4), 1, 0) != -1)
            {
                printf("Success: Nhận được thông tin thiết bị từ powerSupplyInfoAccess..\n");
                memset(infoBuffer, 0, sizeof(infoBuffer));
                strcpy(infoBuffer, message4.mesg_text);
                // lấy thông tin thiết bị
                infoToken = strtok(infoBuffer, "|");
                // id thiết bị
                equipInfo.deviceID = deviceID;
                infoToken = strtok(NULL, "|");
                // mức công suất NORMAL của thiết bị
                equipInfo.normalVoltage = atoi(infoToken);
                infoToken = strtok(NULL, "|");
                // mức công suất SAVING của thiết bị
                equipInfo.savingVoltage = atoi(infoToken);
                infoToken = strtok(NULL, "|");
                // trạng thái thiết bị
                strcpy(equipInfo.status, infoToken);

                // Thiết lập mức cung cấp cho thiết bị hiện tại
                if (strcmp(equipInfo.status, "OFF") == 0)
                    equipInfo.currentSupply = 0;
                else if (strcmp(equipInfo.status, "NORMAL") == 0)
                    equipInfo.currentSupply = equipInfo.normalVoltage;
                else if (strcmp(equipInfo.status, "SAVING") == 0)
                    equipInfo.currentSupply = equipInfo.savingVoltage;

                // Thiết lập mức cung cấp được yêu cầu
                if (strcmp(requestedMode, "OFF") == 0)
                    requestedSupply = 0;
                else if (strcmp(requestedMode, "NORMAL") == 0)
                    requestedSupply = equipInfo.normalVoltage;
                else if (strcmp(requestedMode, "SAVING") == 0)
                    requestedSupply = equipInfo.savingVoltage;
            }

            // so sánh status của thiết bị với yêu cầu
            if (strcmp(requestedMode, equipInfo.status) == 0)
            {
                memset(message3.mesg_text, 0, sizeof(message3.mesg_text));
                message3.mesg_type = msgtype;
                sprintf(message3.mesg_text, "%s|%d|%s|", systemInfo.status, equipInfo.currentSupply, equipInfo.status);
                // msgsnd gửi message tới powerSupply
                msgsnd(msgId3, &message3, sizeof(message3.mesg_text), 0);
                printf("Success: Đã gửi message tới powerSupply...\n");
            }
            // kiểm tra hạn mức cung cấp
            else
            {
                int predictedSupply = systemInfo.currentSupply - equipInfo.currentSupply + requestedSupply;

                // nếu nhỏ hơn mức tối đa
                if (predictedSupply <= systemInfo.maxThreshold)
                {
                    // nếu lớn hơn mức cảnh báo
                    if (predictedSupply >= systemInfo.warningThreshold)
                    {
                        // chuyển trạng thái hệ thống sang WARNING
                        memset(systemInfo.status, 0, sizeof(systemInfo.status));
                        strcpy(systemInfo.status, "WARNING");
                        printf("System: WARNING\n");
                    }
                    // ko lớn hơn mức cảnh bảo
                    else
                    {
                        // chuyển trạng thái hệ thống sang NORMAL
                        memset(systemInfo.status, 0, sizeof(systemInfo.status));
                        strcpy(systemInfo.status, "NORMAL");
                        printf("System: NORMAL\n");
                    }

                    // cho current supply = requested
                    equipInfo.currentSupply = requestedSupply;
                    strcpy(equipInfo.status, requestedMode);
                    // gửi message tới powerSupplyInfoAccess để viết sysInfo
                    sprintf(message2.mesg_text, "%d|%d|%d|%s|", T_WRITE, T_SYSTEM, predictedSupply, systemInfo.status);
                    message2.mesg_type = msgtype;
                    msgsnd(msgId2, &message2, sizeof(message2.mesg_text), 0);
                    printf("Success: Sending Request to Write System Info...\n");
                    // gửi message tới powerSupplyInfoAccess để viết deviceInfo
                    sprintf(message2.mesg_text, "%d|%d|%d|%d|%s|", T_WRITE, T_DEVICE, deviceID, equipInfo.currentSupply, equipInfo.status);
                    message2.mesg_type = msgtype;
                    msgsnd(msgId2, &message2, sizeof(message2.mesg_text), 0);
                    printf("Success: Sending Request to Write Device Info...\n");
                    // send message to powerSupply
                    memset(message3.mesg_text, 0, sizeof(message3.mesg_text));
                    message3.mesg_type = msgtype;
                    sprintf(message3.mesg_text, "%s|%d|%s|", systemInfo.status, equipInfo.currentSupply, equipInfo.status);
                    msgsnd(msgId3, &message3, sizeof(message3.mesg_text), 0);
                    printf("Success: Sending Message to Power Supply...\n\n");
                }
                else // nếu cần cung cấp > hạn mức
                {
                    int totalPower = 0;
                    message2.mesg_type = msgtype;
                    memset(message2.mesg_text, 0, sizeof(message2.mesg_text));
                    // cho tất cả các thiết bị sang chế độ SAVING
                    // gửi message tới powerSupplyInfoAccess
                    sprintf(message2.mesg_text, "%d|%d|%d|%d|", T_WRITE, T_DEVICE, deviceID, -1);
                    msgsnd(msgId2, &message2, sizeof(message2.mesg_text), 0);
                    printf("Success: Đã gửi message tới powerSupplyInfoAccess\n");

                    // nhận message từ powerSupplyInfoAccess
                    if (msgrcv(msgId4, &message4, sizeof(message4), 1, 0) != -1)
                    {
                        printf("Success: Đã nhận message từ powerSupplyInfoAccess\n");
                        memset(infoBuffer, 0, sizeof(infoBuffer));
                        strcpy(infoBuffer, message4.mesg_text);

                        infoToken = strtok(infoBuffer, "|");
                        // tổng
                        totalPower = atoi(infoToken);
                        if (totalPower > systemInfo.maxThreshold) // vẫn quá tải
                        {
                            memset(equipInfo.status, 0, sizeof(equipInfo.status));
                            strcpy(equipInfo.status, "OFF");
                            equipInfo.currentSupply = 0;
                            // gửi message tới powerSupplyInfoAccess để tắt thiết bị đó
                            message2.mesg_type = msgtype;
                            memset(message2.mesg_text, 0, sizeof(message2.mesg_text));
                            sprintf(message2.mesg_text, "%d|%d|%d|%d|%s|", T_WRITE, T_DEVICE, deviceID, equipInfo.currentSupply, equipInfo.status);
                            msgsnd(msgId2, &message2, sizeof(message2.mesg_text), 0);
                            printf("Success: Đã gửi message tới powerSupplyInfoAccess\n");

                            totalPower -= equipInfo.savingVoltage;
                            // nếu tổng > warning
                            if (totalPower >= systemInfo.warningThreshold)
                            {
                                // chuyển trạng thái hệ thống sang WARNING
                                memset(systemInfo.status, 0, sizeof(systemInfo.status));
                                strcpy(systemInfo.status, "WARNING");
                                printf("System: WARNING\n");
                            }
                            else
                            {
                                // chuyển trạng thái hệ thống sang NORMAL
                                memset(systemInfo.status, 0, sizeof(systemInfo.status));
                                strcpy(systemInfo.status, "NORMAL");
                                printf("System: NORMAL\n");
                            }

                            // gửi message tới powerSupplyInfoAccess để viết sysInfo
                            sprintf(message2.mesg_text, "%d|%d|%d|%s|", T_WRITE, T_SYSTEM, totalPower, systemInfo.status);
                            message2.mesg_type = msgtype;
                            msgsnd(msgId2, &message2, sizeof(message2.mesg_text), 0);
                            printf("Success: Đã gửi message tới powerSupplyInfoAccess\n");

                            // gửi message tới powerSupply
                            memset(message3.mesg_text, 0, sizeof(message3.mesg_text));
                            message3.mesg_type = msgtype;
                            sprintf(message3.mesg_text, "%s|%d|%s|", "OVER", equipInfo.currentSupply, equipInfo.status);
                            msgsnd(msgId3, &message3, sizeof(message3), 0);
                            printf("Success: Đã gửi message tới powerSupplyInfoAccess\n");
                        }
                        else
                        {
                            if (totalPower >= systemInfo.warningThreshold)
                            {
                                // chuyển trạng thái hệ thống sang WARNING
                                memset(systemInfo.status, 0, sizeof(systemInfo.status));
                                strcpy(systemInfo.status, "WARNING");
                                printf("System: WARNING\n");
                            }
                            else
                            {
                                // chuyển trạng thái hệ thống sang NORMAL
                                memset(systemInfo.status, 0, sizeof(systemInfo.status));
                                strcpy(systemInfo.status, "NORMAL");
                                printf("System: NORMAL\n");
                            }

                            // gửi message tới powerSupplyInfoAccess để viết sysInfo
                            sprintf(message2.mesg_text, "%d|%d|%d|%s|", T_WRITE, T_SYSTEM, totalPower, systemInfo.status);
                            message2.mesg_type = msgtype;
                            msgsnd(msgId2, &message2, sizeof(message2.mesg_text), 0);
                            printf("Success: Đã gửi message tới powerSupplyInfoAccess\n");

                            // gửi message tới powerSupply
                            memset(equipInfo.status, 0, sizeof(equipInfo.status));
                            strcpy(equipInfo.status, "SAVING");
                            equipInfo.currentSupply = equipInfo.savingVoltage;

                            memset(message3.mesg_text, 0, sizeof(message3.mesg_text));
                            message3.mesg_type = msgtype;
                            sprintf(message3.mesg_text, "%s|%d|%s|", "OVER", equipInfo.currentSupply, equipInfo.status);
                            msgsnd(msgId3, &message3, sizeof(message3.mesg_text), 0);
                            printf("Success: Đã gửi message tới powerSupplyInfoAccess\n");
                        }
                    }
                }
            }
        }
    }

    // Huỷ các message queue
    msgctl(msgId1, IPC_RMID, NULL);
    msgctl(msgId2, IPC_RMID, NULL);
    msgctl(msgId3, IPC_RMID, NULL);
    msgctl(msgId4, IPC_RMID, NULL);

    return 0;
}