//
// Created by ianzh on 1/24/2025.
//

#ifndef GROUPUTILS_H
#define GROUPUTILS_H

#define MAX_GROUP_SIZE 15
#define MAX_GROUP_NAME_LENGTH 16
#define MAX_GROUPS 5
#include <KasaSmartPlug.h>

class ControlGroup {
  public:
    char name[MAX_GROUP_NAME_LENGTH];
    KASASmartPlug* plugs[MAX_GROUP_SIZE];
    int groupSize;

    ControlGroup(const char* name) {
        strcpy(this->name, name);
        this->groupSize = 0;
    }

    KASASmartPlug *addDevice(const char *ip);

    bool removeDevice(const char *ip);

    KASASmartPlug *getDeviceByIp(const char *ip);
};

class GroupUtils {
  public:
    ControlGroup *groups[MAX_GROUPS]{};
    int numGroups = 0;

    ControlGroup *newGroup(const char *name);

    bool deleteGroup(const char *name);

    ControlGroup *getGroup(const char *name);

    void clearGroups();

    int nextGroupIndex(int currIndex, int dir);

    GroupUtils() {
        numGroups = 0;
    };
};


#endif //GROUPUTILS_H
