//
// Created by ianzh on 1/21/2025.
//

#ifndef KASAUTIL_H
#define KASAUTIL_H
#include "KasaSmartPlug.h"

#define MAX_PLUG_ALLOW 10

class KASASmartPlug;

class KASAUtil {
private:
    KASASmartPlug *ptr_plugs[MAX_PLUG_ALLOW];

    void closeSock(int sock);

    int IsContainPlug(const char *name);

    int IsStartWith(const char *prefix, const char *model) {
        return strncmp(prefix, model, strlen(prefix)) == 0;
    }


public:
    static const char *get_kasa_info;
    static const char *relay_on;
    static const char *relay_off;
    static const char *brightness;

    int deviceCount;

    int ScanDevices(int timeoutMs = 3000); // Wait at least xxx ms after received UDP packages..
    static uint16_t Encrypt(const char *data, int length, uint8_t addLengthByte, char *encryped_data);

    static uint16_t Decrypt(char *data, int length, char *decryped_data, int startIndex);

    KASASmartPlug *GetSmartPlug(const char *alias_name);
    KASASmartPlug *GetSmartPlugByIp(const char *ip);

    KASASmartPlug *GetSmartPlugByIndex(int index);

    KASAUtil();
};


#endif //KASAUTIL_H
