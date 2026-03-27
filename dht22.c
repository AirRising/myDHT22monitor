#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "MQTTClient.h"

// --- 配置区 ---
#define ADDRESS     "tcp://192.168.10.100:1883" // 填入你 Ubuntu 的 IP
#define CLIENTID    "IMX6ULL_DHT22_Node"
#define TOPIC       "sensor/data"
#define READ_INTERVAL 5  // 每5秒上报一次

// --- 传感器查找逻辑 ---
int find_dht_device(char *temp_path, char *humi_path, size_t path_size) {
    DIR *dir;
    struct dirent *entry;
    char name_path[128], name[32];
    const char *base_path = "/sys/bus/iio/devices";
    
    dir = opendir(base_path);
    if (!dir) return -1;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "iio:device", 10) != 0) continue;
        
        snprintf(name_path, sizeof(name_path), "%s/%s/name", base_path, entry->d_name);
        FILE *fp = fopen(name_path, "r");
        if (fp) {
            if (fgets(name, sizeof(name), fp) != NULL) {
                name[strcspn(name, "\n")] = 0;
                if (strcmp(name, "dht11") == 0 || strcmp(name, "dht22") == 0) {
                    snprintf(temp_path, path_size, "%s/%s/in_temp_input", base_path, entry->d_name);
                    snprintf(humi_path, path_size, "%s/%s/in_humidityrelative_input", base_path, entry->d_name);
                    fclose(fp); closedir(dir); return 0;
                }
            }
            fclose(fp);
        }
    }
    closedir(dir); return -1;
}

// --- 安全读取逻辑 ---
int read_sensor_value(const char *path, float *value) {
    FILE *fp = fopen(path, "r");
    char buf[32];
    if (!fp) return -1;
    if (fgets(buf, sizeof(buf), fp) != NULL) {
        fclose(fp);
        if (strlen(buf) > 0 && buf[0] != '\n') {
            *value = atoi(buf) / 1000.0;
            return 0;
        }
    }
    fclose(fp); return -1;
}

// --- 主程序 ---
int main(int argc, char* argv[]) {
    char temp_path[128], humi_path[128];
    float temp, humi;
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    // 1. 初始化传感器路径
    if (find_dht_device(temp_path, humi_path, sizeof(temp_path)) < 0) {
        fprintf(stderr, "❌ 错误: 未找到 DHT22 设备，请检查驱动是否加载！\n");
        return EXIT_FAILURE;
    }

    // 2. 创建 MQTT 客户端
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    // 3. 连接 Ubuntu 中转站
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("❌ 无法连接到 Ubuntu 网关, 错误代码: %d\n", rc);
        return EXIT_FAILURE;
    }

    printf("🚀 DHT22 上报程序已启动！\n目标网关: %s\n", ADDRESS);

    while(1) {
        // 读取温湿度
        int t_ok = (read_sensor_value(temp_path, &temp) == 0);
        int h_ok = (read_sensor_value(humi_path, &humi) == 0);

        if (t_ok && h_ok) {
            char payload[64];
            // 打包成 "温度,湿度" 格式，由 Ubuntu 网关负责解析
            sprintf(payload, "%.1f,%.1f", temp, humi);

            MQTTClient_message pubmsg = MQTTClient_message_initializer;
            pubmsg.payload = payload;
            pubmsg.payloadlen = (int)strlen(payload);
            pubmsg.qos = 1;
            pubmsg.retained = 0;

            MQTTClient_publishMessage(client, TOPIC, &pubmsg, NULL);
            printf("✅ 数据上报成功: 温度 %.1f°C, 湿度 %.1f%%\n", temp, humi);
        } else {
            printf("⚠️ 读取失败，尝试下一轮采样...\n");
        }

        sleep(READ_INTERVAL); // 严格遵守 DHT22 读取频率限制
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return EXIT_SUCCESS;
}