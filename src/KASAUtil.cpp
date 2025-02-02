//
// Created by ianzh on 1/21/2025.
//

#include "KASAUtil.h"

const char *KASAUtil::get_kasa_info = "{\"system\":{\"get_sysinfo\":null}}";
const char *KASAUtil::relay_on = "{\"system\":{\"set_relay_state\":{\"state\":1}}}";
const char *KASAUtil::relay_off = "{\"system\":{\"set_relay_state\":{\"state\":0}}}";
const char *KASAUtil::brightness = "{\"smartlife.iot.dimmer\":{\"set_brightness\":{\"brightness\":%d}}}";

uint16_t KASAUtil::Encrypt(const char *data, int length, uint8_t addLengthByte, char *encryped_data)
{
    uint8_t key = KASA_ENCRYPTED_KEY;
    uint8_t en_b;
    int index = 0;
    if (addLengthByte)
    {
        encryped_data[index++] = 0;
        encryped_data[index++] = 0;
        encryped_data[index++] = (char)(length >> 8);
        encryped_data[index++] = (char)(length & 0xFF);
    }

    for (int i = 0; i < length; i++)
    {
        en_b = data[i] ^ key;
        encryped_data[index++] = en_b;
        key = en_b;
    }

    return index;
}

uint16_t KASAUtil::Decrypt(char *data, int length, char *decryped_data, int startIndex)
{
    uint8_t key = KASA_ENCRYPTED_KEY;
    uint8_t dc_b;
    int retLength = 0;
    for (int i = startIndex; i < length; i++)
    {
        dc_b = data[i] ^ key;
        key = data[i];
        decryped_data[retLength++] = dc_b;
    }

    return retLength;
}

void KASAUtil::closeSock(int sock)
{
    if (sock != -1)
    {
        // ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }
}

KASAUtil::KASAUtil()
{
    deviceCount = 0;
}

int KASAUtil::ScanDevices(int timeoutMs)
{
    struct sockaddr_in dest_addr;
    int ret = 0;
    int boardCaseEnable = 1;
    int retValue = 0;
    int sock;

    int err = 1;
    char sendbuf[128];
    char addrbuf[32] = {0};
    int len;
    const char *string_value;
    const char *model;
    int relay_state;
    int brightness;

    JsonDocument doc;

    len = strlen(get_kasa_info);

    dest_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(9999);

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        retValue = -1;
        closeSock(sock);
        return retValue;
    }

    ret = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &boardCaseEnable, sizeof(boardCaseEnable));
    if (ret < 0) {
        ESP_LOGE(TAG, "Unable to set broadcase option %d", errno);
        retValue = -2;
        closeSock(sock);
        return retValue;
    }

    len = Encrypt(get_kasa_info, len, 0, sendbuf);
    if (len > sizeof(sendbuf)) {
        ESP_LOGE(TAG, "Overflowed multicast sendfmt buffer!!");

        retValue = -3;
        closeSock(sock);
        return retValue;
    }

    // Sending the first broadcase message out..
    ESP_LOGI(TAG, "Send Query Message length  %s %d", get_kasa_info, len);

    err = sendto(sock, sendbuf, len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        closeSock(sock);
        return -4;
    }
    // Serial.println("Query Message sent");
    int send_loop = 0;
    long time_out_us = (long)timeoutMs * 1000;
    while (err > 0 && send_loop < 1) {
        timeval tv = {
            .tv_sec = 0,
            .tv_usec = time_out_us,
        };
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        // Serial.println("Enter select function...");

        int s = select(sock + 1, &rfds, NULL, NULL, &tv);
        // Serial.printf("Select value = %d\n", s);

        if (s < 0) {
            ESP_LOGE(TAG, "Select failed: errno %d", errno);
            err = -1;
            break;
        }
        if (s > 0) {
            if (FD_ISSET(sock, &rfds))
            {
                // Incoming datagram received
                char recvbuf[1024];
                char raddr_name[32] = {0};

                sockaddr_storage raddr; // Large enough for both IPv4 or IPv6
                socklen_t socklen = sizeof(raddr);
                // Serial.println("Waiting incomming package...");
                int len = recvfrom(sock, recvbuf, sizeof(recvbuf) - 1, 0,
                                   (struct sockaddr *)&raddr, &socklen);
                if (len < 0)
                {
                    ESP_LOGE(TAG, "multicast recvfrom failed: errno %d", errno);
                    err = -1;
                    break;
                }
                len = Decrypt(recvbuf, len, recvbuf, 0);

                if (raddr.ss_family == PF_INET)
                {
                    inet_ntoa_r(((struct sockaddr_in *)&raddr)->sin_addr,
                                raddr_name, sizeof(raddr_name) - 1);
                }

                // Serial.printf("received %d bytes from %s: \r\n", len, raddr_name);

                recvbuf[len] = 0; // Null-terminate whatever we received and treat like a string...

                // We got the response from the broadcast message
                // I found HS103 plug would response around 500 to 700 bytes of JSON data
                if (len > 500)
                {
                    // Serial.println("Parsing info...");
                    DeserializationError error = deserializeJson(doc, recvbuf, len);

                    if (error)
                    {
                        Serial.print("deserializeJson() failed: ");
                        Serial.println(error.c_str());
                    }
                    else
                    {
                        JsonObject get_sysinfo = doc["system"]["get_sysinfo"];
                        string_value = get_sysinfo["alias"];
                        relay_state = get_sysinfo["relay_state"];
                        brightness = get_sysinfo["brightness"].isNull() ? -1 : get_sysinfo["brightness"];
                        model = get_sysinfo["model"];

                        // if (!IsStartWith("HS",model) && !IsStartWith("KP",model))
                        // {
                        //     Serial.println("Found a valid Kasa Device, but we don't know if it works with this library just yet. You are in unprecedented territory, proceed with caution.");
                        // }

                        // Limit the number of devices and make sure no duplicate device.
                        if (IsContainPlug(raddr_name) == -1)
                        {
                            // New device has been found
                            if (deviceCount < MAX_PLUG_ALLOW)
                            {
                                ptr_plugs[deviceCount] = new KASASmartPlug(string_value, raddr_name);
                                ptr_plugs[deviceCount]->state = relay_state;
                                ptr_plugs[deviceCount]->brightness = brightness;
                                ptr_plugs[deviceCount]->init = true;

                                strcpy(ptr_plugs[deviceCount]->model, model);
                                deviceCount++;
                            } else
                            {
                                Serial.printf("\r\n Error unable to add more plug");
                            }
                        }
                    }
                }
            }
            else
            {

                // int len = snprintf(sendbuf, sizeof(sendbuf), sendfmt, send_count++);

                // Serial.println("Send Query Message");

                err = sendto(sock, sendbuf, len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                if (err < 0)
                {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    retValue = -5;
                }
                // Serial.println("Query Message sent");
            }
        }
        else if (s == 0) // Timeout package
        {
            // Serial.println("S Timeout Send Query Message");
            send_loop++;

            err = sendto(sock, sendbuf, len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err < 0)
            {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                retValue = -1;
                closeSock(sock);
                return retValue;
            }
            // Serial.println("Query Message sent");
        }

        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
    closeSock(sock);
    return deviceCount;
}

int KASAUtil::IsContainPlug(const char *ip)
{
    if (deviceCount == 0) return -1;
    for (int i = 0; i < deviceCount; i++) {
        if (strcmp(ip, ptr_plugs[i]->ip_address) == 0)
            return i;
    }

    return -1;
}

KASASmartPlug *KASAUtil::GetSmartPlugByIndex(int index)
{
    if (index < -0)
        return NULL;

    if (index < deviceCount)
    {
        return ptr_plugs[index];
    }
    else
        return NULL;
}

KASASmartPlug *KASAUtil::GetSmartPlug(const char *alias_name)
{
    for (int i = 0; i < deviceCount; i++)
    {
        if (strcmp(alias_name, ptr_plugs[i]->alias) == 0)
            return ptr_plugs[i];
    }
    return NULL;
}

KASASmartPlug *KASAUtil::GetSmartPlugByIp(const char *ip)
{
    for (int i = 0; i < deviceCount; i++)
    {
        if (strcmp(ip, ptr_plugs[i]->ip_address) == 0)
            return ptr_plugs[i];
    }
    return NULL;
}