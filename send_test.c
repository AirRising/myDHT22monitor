#include <stdio.h>
#include <string.h>
#include "MQTTClient.h"

int main() {
    MQTTClient client;
    // 这里填你 Ubuntu 的地址
    MQTTClient_create(&client, "tcp://192.168.10.100:1883", "IMX6ULL_TEST", 0, NULL);
    
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    if (MQTTClient_connect(client, &conn_opts) != 0) {
        printf("连接仓库失败！\n");
        return -1;
    }

    // char* payload = "Hello, I am IMX6ULL!";
    char* payload = "26.5";
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = payload;
    pubmsg.payloadlen = strlen(payload);
    
    // 发送到 sensor/data 柜台
    MQTTClient_publishMessage(client, "sensor/data", &pubmsg, NULL);
    printf("消息已发出！\n");

    MQTTClient_disconnect(client, 1000);
    MQTTClient_destroy(&client);
    return 0;
}