// Modified version of https://github.com/kj831ca/KasaSmartPlug
#ifndef KASA_SMART_PLUG_DOT_HPP
#define KASA_SMART_PLUG_DOT_HPP

#include <WiFi.h>
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include <ArduinoJson.h>
#include "KASAUtil.h"

#define TAG "KasaSmartPlug"
#define KASA_ENCRYPTED_KEY 171

class KASASmartPlug {
private:
    struct sockaddr_in dest_addr;
    static SemaphoreHandle_t mutex;
    JsonDocument doc;

protected:
    int sock;
    /*
        @brief Open the TCP Client socket
    */
    bool OpenSock() {
        int err;
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

        fd_set fdset;
        struct timeval tv;
        int arg;

        if (sock < 0) {
            // Serial.println("Error unable to open socket...");
            return false;
        }

        // Using non blocking connect
        arg = fcntl(sock, F_GETFL, NULL);
        arg |= O_NONBLOCK;

        fcntl(sock, F_SETFL, O_NONBLOCK);

        err = connect(sock, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
        if (err < 0) {
            do {
                tv.tv_sec = 1;
                tv.tv_usec = 0;
                FD_ZERO(&fdset);
                FD_SET(sock, &fdset);

                // Set connect timeout to 1 sec.
                err = select(sock + 1, NULL, &fdset, NULL, &tv);
                if (err < 0 && errno != EINTR) {
                    Serial.println("Unable to open sock");
                    break;
                }

                if (err == 1) {
                    int so_error = 0;
                    socklen_t len = sizeof so_error;

                    getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
                    if (so_error == 0) {
                        // arg &= (~O_NONBLOCK);
                        fcntl(sock, F_SETFL, arg);
                        return true;
                    } else
                        break;
                }
            } while (1);
        }
        Serial.println("Error can not open sock...");
        CloseSock();

        return false;
    }

    void CloseSock() {
        if (sock != -1) {
            shutdown(sock, 0);
            close(sock);
            sock = -1;
        }
    }

    void DebugBufferPrint(char *data, int length);

    void SendCommand(const char *cmd);

    int Query(const char *cmd, char *buffer, int bufferLength, long timeout);

public:
    char alias[32];
    char ip_address[32]{};
    char model[15];
    int brightness;
    int state;
    int err_code;
    bool init = false;

    int QueryInfo(int timeout = 300000);

    void SetRelayState(uint8_t state);

    void SetBrightness(uint8_t brightness);

    void UpdateIPAddress(const char *ip) {
        strcpy(ip_address, ip);
        sock = -1;
        dest_addr.sin_addr.s_addr = inet_addr(ip_address);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(9999);
    }

    KASASmartPlug(const char* ip) {
        err_code = 0;
        UpdateIPAddress(ip);
    }

    KASASmartPlug(const char *name, const char *ip) {
        strcpy(alias, name);
        UpdateIPAddress(ip);
        err_code = 0;
        xSemaphoreGive(mutex);
    }
};

#endif
