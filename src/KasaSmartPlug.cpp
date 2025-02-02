// Modified version of https://github.com/kj831ca/KasaSmartPlug

#include "KasaSmartPlug.h"

SemaphoreHandle_t KASASmartPlug::mutex = xSemaphoreCreateMutex();

void KASASmartPlug::SendCommand(const char *cmd)
{
    int err;
    char sendbuf[128];
    xSemaphoreTake(mutex, portMAX_DELAY);
    OpenSock();
    int len = KASAUtil::Encrypt(cmd, strlen(cmd), 1, sendbuf);
    if (sock > 0)
    {
        // DebugPrint(sendbuf,len);
        err = send(sock, sendbuf, len, 0);
        if (err < 0)
        {
            // Serial.printf("\r\n Error while sending data %d", errno);
        }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); // Make sure the data has been send out before close the socket.
    CloseSock();
    xSemaphoreGive(mutex);
}

int KASASmartPlug::Query(const char *cmd, char *buffer, int bufferLength, long timeout)
{
    int sendLen;
    int recvLen;
    int err;
    xSemaphoreTake(mutex, portMAX_DELAY);
    recvLen = 0;
    err = 0;
    OpenSock();
    sendLen = KASAUtil::Encrypt(cmd, strlen(cmd), 1, buffer);
    if (sock > 0)
    {
        err = send(sock, buffer, sendLen, 0);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    else
    {
        CloseSock();
        xSemaphoreGive(mutex);
        return 0;
    }

    struct timeval tv = {
        .tv_sec = 0,
        .tv_usec = timeout,
    };
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);

    int s = select(sock + 1, &rfds, NULL, NULL, &tv);
    // xSemaphoreTake(mutex, portMAX_DELAY);
    if (s < 0)
    {
        // Serial.printf("Select failed: errno %d", errno);
        err = -1;
        CloseSock();
        xSemaphoreGive(mutex);
        return 0;
    }
    else if (s > 0)
    {

        if (FD_ISSET(sock, &rfds))
        {
            // We got response here.
            recvLen = recv(sock, buffer, bufferLength, 0);

            if (recvLen > 0)
            {
                recvLen = KASAUtil::Decrypt(buffer, recvLen, buffer, 4);
            }
        }
    }
    else if (s == 0)
    {
        // Serial.println("Error TCP Read timeout...");
        CloseSock();
        xSemaphoreGive(mutex);
        return 0;
    }

    CloseSock();
    xSemaphoreGive(mutex);
    return recvLen;
}
int KASASmartPlug::QueryInfo(int timeout)
{
    char buffer[1024];
    int recvLen = Query(KASAUtil::get_kasa_info, buffer, 1024, timeout);

    if (recvLen > 500)
    {

        // Serial.println("Parsing info...");
        // Because the StaticJSONDoc uses quite memory block other plug while parsing JSON ...
        xSemaphoreTake(mutex, portMAX_DELAY);
        DeserializationError error = deserializeJson(doc, buffer, recvLen);

        if (error)
        {
            // Serial.print("deserializeJson() failed: ");
            // Serial.println(error.c_str());
            recvLen = -1;
        }
        else
        {
            JsonObject get_sysinfo = doc["system"]["get_sysinfo"];
            strcpy(alias, get_sysinfo["alias"]);
            state = get_sysinfo["relay_state"];
            err_code = get_sysinfo["err_code"];
            brightness = get_sysinfo["brightness"].isNull() ? -1 : get_sysinfo["brightness"];
            init = true;
        }
        xSemaphoreGive(mutex);
    }

    return recvLen;
}

void KASASmartPlug::SetRelayState(uint8_t state)
{
    if (state > 0)
    {
        SendCommand(KASAUtil::relay_on);
    }
    else
        SendCommand(KASAUtil::relay_off);
}

void KASASmartPlug::SetBrightness(uint8_t brightness)
{
    char buffer[128];
    sprintf(buffer, KASAUtil::brightness, brightness);
    SendCommand(buffer);
}
void KASASmartPlug::DebugBufferPrint(char *data, int length)
{
    for (int i = 0; i < length; i++)
    {
        if (i % 8 == 0)
            Serial.print("\r\n");
        else
            Serial.print(" ");

        Serial.printf("%d ", data[i]);
    }
}
