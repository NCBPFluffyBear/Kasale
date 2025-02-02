//
// Created by ianzh on 1/24/2025.
//

#include "GroupUtils.h"

// Create a new group with the given name
ControlGroup* GroupUtils::newGroup(const char* name) {
    if (numGroups >= MAX_GROUPS || strlen(name) >= MAX_GROUP_NAME_LENGTH - 1) {
        return nullptr;
    }

    ControlGroup* group = new ControlGroup(name);
    groups[numGroups] = group;

    numGroups++;

    return group;
}

// Delete a group by name
bool GroupUtils::deleteGroup(const char* name) {
    for (int i = 0; i < numGroups; i++) {
        if (strcmp(groups[i]->name, name) == 0) {
            delete groups[i];
            for (int j = i; j < numGroups - 1; j++) {
                groups[j] = groups[j + 1];
            }
            numGroups--;
            return true;
        }
    }

    return false;
}

ControlGroup* GroupUtils::getGroup(const char* name) {
    for (int i = 0; i < numGroups; i++) {
        if (strcmp(groups[i]->name, name) == 0) {
            return groups[i];
        }
    }

    return nullptr;
}

void GroupUtils::clearGroups() {
    for (int i = 0; i < numGroups; i++) {
        delete groups[i];
    }

    numGroups = 0;
}

KASASmartPlug* ControlGroup::addDevice(const char* ip) {
    if (groupSize >= MAX_GROUP_SIZE) {
        return nullptr;
    }

    KASASmartPlug* plug = new KASASmartPlug(ip);
    plugs[groupSize] = plug;
    groupSize++;
    return plug;
}

bool ControlGroup::removeDevice(const char* ip) {
    for (int i = 0; i < groupSize; i++) {
        if (strcmp(plugs[i]->ip_address, ip) == 0) {
            for (int j = i; j < groupSize - 1; j++) {
                plugs[j] = plugs[j + 1];
            }
            groupSize--;
            return true;
        }
    }
    return false;
}

KASASmartPlug* ControlGroup::getDeviceByIp(const char* ip) {
    for (int i = 0; i < groupSize; i++) {
        if (strcmp(plugs[i]->ip_address, ip) == 0) {
            return plugs[i];
        }
    }
    return nullptr;
}

int GroupUtils::nextGroupIndex(int currIndex, int dir) {
    if (dir > 0) {
        return currIndex + 1 >= numGroups ? 0 : currIndex + 1;
    }

    return currIndex - 1 < 0 ? numGroups - 1 : currIndex - 1;
}
