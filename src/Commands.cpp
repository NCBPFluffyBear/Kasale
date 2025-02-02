//
// Created by ianzh on 1/29/2025.
//

#include "Commands.h"

#include <GroupUtils.h>
#include <Arduino.h>
#include <Preferences.h>
#include <Kasale.h>


// Forward declarations because files are not linking properly
// These are defined in Kasale.h
Preferences preferences;
GroupUtils groupUtils;
KASAUtil kasaUtil;

int selection;
bool hasSerial;
int brightnessStep;
int defaultBrightness;
int idleTimeout;
bool dimDisplay;
// End forward declarations

void saveAlert(Terminal* terminal) {
    terminal->println(INFO, "DON'T FORGET TO RUN \"save\"!");
}


void prefCommand(Terminal* terminal) {
    String p1 = terminal->readParameter();

    if (p1 == nullptr) {
        terminal->println(INFO, "invertScreen: true/false");
        terminal->println(INFO, "brightnessStep: 1-10");
        terminal->println(INFO, "defaultBrightness: 1-100");
        terminal->println(INFO, "idleTimeout: 0-60");
        terminal->println(INFO, "dimDisplay: true/false");
        terminal->prompt();
        return;
    }

    String p2 = terminal->readParameter();
    if (p2 == nullptr) {
        terminal->println(ERROR, "Missing param 2");
        terminal->prompt();
        return;
    }

    if (p1 == "invertScreen") {
        preferences.putBool("invertScreen", p2 == "true");
    } else if (p1 == "brightnessStep") {
        int step = p2.toInt();
        if (step < 1 || step > 10) {
            terminal->println(ERROR, "Brightness step must be between 1 and 10");
            terminal->prompt();
            return;
        }
        preferences.putInt("brightnessStep", step);
        brightnessStep = step;
    } else if (p1 == "defaultBrightness") {
        int brightness = p2.toInt();
        if (brightness < 1 || brightness > 100) {
            terminal->println(ERROR, "Brightness must be between 1 and 100");
            terminal->prompt();
            return;
        }
        preferences.putInt("defaultBrightness", brightness);
        defaultBrightness = brightness;
    } else if (p1 == "idleTimeout") {
        int timeout = p2.toInt();
        if (timeout < 0 || timeout > 60) {
            terminal->println(ERROR, "Idle timeout must be between 0 and 60");
            terminal->prompt();
            return;
        }
        preferences.putInt("idleTimeout", timeout);
        idleTimeout = timeout * 1000000;
    } else if (p1 == "dimDisplay") {
        bool dim = p2 == "true";
        preferences.putBool("dimDisplay", dim);
        dimDisplay = dim;
    } else {
        terminal->println(ERROR, "Unknown parameter");
        terminal->prompt();
        return;
    }

    terminal->println(INFO, "Preference set");
    saveAlert(terminal);
    terminal->prompt();
}

void syntaxError(Terminal* terminal) {
    terminal->println(ERROR, "Syntax error");
    terminal->prompt();
}

void connectWifi(Terminal* terminal) {
    startWifi();
    terminal->prompt();
}

void setSsid(Terminal* terminal) {
    String ssid = terminal->readParameter();
    if (ssid == nullptr) {
        syntaxError(terminal);
        return;
    }

    preferences.putString("ssid", ssid);
    saveAlert(terminal);
    terminal->prompt();
}

void setPassword(Terminal* terminal) {
    String password = terminal->readParameter();
    if (password == nullptr) {
        syntaxError(terminal);
        return;
    }

    preferences.putString("password", password);
    saveAlert(terminal);
    terminal->prompt();
}

void createAllGroup(Terminal* terminal) {
    terminal->println(INFO, "Scanning devices and creating new group called \"all\"");
    kasaUtil.ScanDevices();

    ControlGroup* group = groupUtils.getGroup("all"); // Try to locate existing all group so it can be updated

    if (group != nullptr) { // Group exists already
        terminal->println(INFO, "Group \"all\" already exists. Adding any new devices.");
    } else {
        group = groupUtils.newGroup("all");
    }

    if (group == nullptr) {
        terminal->println(ERROR, "Too many groups.");
        terminal->prompt();
        return;
    }

    for (int i = 0; i < kasaUtil.deviceCount; i++) {
        KASASmartPlug* scannedPlug = kasaUtil.GetSmartPlugByIndex(i);
        if (group->getDeviceByIp(scannedPlug->ip_address) != nullptr) {
            continue; // Device already exists in group
        }

        KASASmartPlug* plug = group->addDevice(scannedPlug->ip_address); // ik we remake the obj but in case info diff
        if (plug == nullptr) {
            terminal->println(ERROR, "Group \"all\" has reached the device limit.");
            terminal->prompt();
            return;
        }
    }

    terminal->println(INFO, String("Group \"all\" has ") + group->groupSize + " devices.");
    saveAlert(terminal);
    terminal->prompt();
}

void cloneGroup(Terminal* terminal) {
    String oldName = terminal->readParameter();
    String newName = terminal->readParameter();

    if (oldName == nullptr || newName == nullptr) {
        syntaxError(terminal);
        return;
    }

    if (groupUtils.getGroup(newName.c_str()) != nullptr) {
        terminal->println(ERROR, "A group with this name already exists.");
        terminal->prompt();
        return;
    }


    ControlGroup* oldGroup = groupUtils.getGroup(oldName.c_str());
    if (oldGroup == nullptr) {
        terminal->println(ERROR, "Old group not found.");
        terminal->prompt();
        return;
    }

    ControlGroup* newGroup = groupUtils.newGroup(newName.c_str());
    if (newGroup == nullptr) {
        terminal->println(ERROR, "New group could not be created.");
        terminal->prompt();
        return;
    }

    for (int i = 0; i < oldGroup->groupSize; i++) {
        KASASmartPlug* plug = newGroup->addDevice(oldGroup->plugs[i]->ip_address);
        if (plug == nullptr) {
            terminal->println(ERROR, "New group is full.");
            terminal->prompt();
            return;
        }
        plug->QueryInfo();
    }

    terminal->println(INFO, "Group cloned.");
    saveAlert(terminal);
    terminal->prompt();
}

void createGroup(Terminal* terminal) {
    String name = terminal->readParameter();
    if (name == nullptr) {
        syntaxError(terminal);
        return;
    }

    if (name == "all") {
        terminal->println(ERROR, "\"all\" is a reserved group name for the all command.");
        terminal->prompt();
        return;
    }

    ControlGroup* group = groupUtils.newGroup(name.c_str());
    if (group == nullptr) {
        terminal->println(ERROR, "Name too long or too many groups");
        terminal->prompt();
        return;
    }

    terminal->println(INFO, "Created group.");
    saveAlert(terminal);
    terminal->prompt();
}

void deleteGroup(Terminal* terminal) {
    String name = terminal->readParameter();
    if (name == nullptr) {
        syntaxError(terminal);
        return;
    }

    bool deleted = groupUtils.deleteGroup(name.c_str());
    if (!deleted) {
        terminal->println(ERROR, "Group not found.");
        terminal->prompt();
        return;
    }

    if (selection >= groupUtils.numGroups) { // In case selection count was too high
        selection = groupUtils.numGroups - 1;
    }

    terminal->println(INFO, "Group deleted.");
    saveAlert(terminal);
    terminal->prompt();
}

void listGroups(Terminal* terminal) {

    if (groupUtils.numGroups == 0) {
        terminal->println(INFO, "   No groups");
    } else {
        terminal->println(INFO, "Control Groups:");

        for (int i = 0; i < groupUtils.numGroups; i++) {
            char output[64];
            sprintf(output, "   %s", groupUtils.groups[i]->name);
            terminal->println(INFO, output);
        }
    }

    terminal->prompt();
}

void scanDevices(Terminal* terminal) {
    if (!WiFi.isConnected()) {
        terminal->println(ERROR, "Not connected to WiFi");
        terminal->prompt();
        return;
    }

    kasaUtil.ScanDevices();
    if (kasaUtil.deviceCount == 0) {
        terminal->println(INFO, "No devices found");
        terminal->prompt();
        return;
    }

    terminal->println(INFO, "Devices found:");
    for (int i = 0; i < kasaUtil.deviceCount; i++) {
        KASASmartPlug* p = kasaUtil.GetSmartPlugByIndex(i);
        char buffer[64];
        sprintf(buffer, "   %d. %s (%s): On/Off: %d Brightness: %d\0", i, p->alias, p->ip_address, p->state, p->brightness);
        terminal->println(INFO, buffer);
    }
    terminal->prompt();
}

void addDeviceToGroup(Terminal* terminal) {
    String groupName = terminal->readParameter();
    String deviceIp = terminal->readParameter();

    if (groupName == nullptr || deviceIp == nullptr) {
        syntaxError(terminal);
        return;
    }

    ControlGroup* group = groupUtils.getGroup(groupName.c_str());
    if (group == nullptr) {
        terminal->println(ERROR, "This group could not be found.");
        terminal->prompt();
        return;
    }

    KASASmartPlug* plug = group->addDevice(deviceIp.c_str());
    if (plug == nullptr) {
        terminal->println(ERROR, "This group is full.");
        terminal->prompt();
        return;
    }

    int recvLen = plug->QueryInfo(5000); // Update new device info
    if (recvLen == -1) terminal->print(ERROR, "Could not find device on network. Use the - command if added on accident.");
    terminal->println(INFO, "Added device.");
    saveAlert(terminal);
    terminal->prompt();
}

void removeDeviceFromGroup(Terminal* terminal) {
    String groupName = terminal->readParameter();
    String deviceIp = terminal->readParameter();

    if (groupName == nullptr || deviceIp == nullptr) {
        syntaxError(terminal);
        return;
    }

    ControlGroup* group = groupUtils.getGroup(groupName.c_str());
    if (group == nullptr) {
        terminal->println(ERROR, "This group could not be found.");
        terminal->prompt();
        return;
    }

    bool removed = group->removeDevice(deviceIp.c_str());
    if (!removed) {
        terminal->println(ERROR, "This device was not found in the group.");
        terminal->prompt();
        return;
    }

    terminal->println(INFO, "Removed device.");
    saveAlert(terminal);
    terminal->prompt();
}

void listAllGroupDevices(Terminal* terminal) {
    for (int groupNum = 0; groupNum < groupUtils.numGroups; groupNum++) {
        ControlGroup* group = groupUtils.groups[groupNum];
        terminal->println(INFO, String(group->name) + ":");
        for (int i = 0; i < group->groupSize; i++) {
            KASASmartPlug* p = group->plugs[i];
            terminal->println(INFO, String("    ") + p->alias + " (" + p->ip_address + ") -- On/Off: " + p->state + " Brightness: " + p->brightness);
        }
    }

    terminal->prompt();
}

void listGroupDevices(Terminal* terminal) {
    String groupName = terminal->readParameter();

    if (groupName == nullptr) {
        syntaxError(terminal);
        return;
    }

    ControlGroup* group = groupUtils.getGroup(groupName.c_str());
    if (group == nullptr) {
        terminal->println(ERROR, "This group could not be found.");
        terminal->prompt();
        return;
    }

    terminal->println(INFO, "Devices in group:");
    for (int i = 0; i < group->groupSize; i++) {
        KASASmartPlug* p = group->plugs[i];
        char buffer[64];
        sprintf(buffer, "   %s (%s): On/Off: %d Brightness: %d", p->alias, p->ip_address, p->state, p->brightness);
        terminal->println(INFO, buffer);
    }
}

void setRelayMode(Terminal* terminal) {
    String groupName = terminal->readParameter();
    String mode = terminal->readParameter();

    if (groupName == nullptr || mode == nullptr) {
        syntaxError(terminal);
        return;
    }

    ControlGroup* group = groupUtils.getGroup(groupName.c_str());
    if (group == nullptr) {
        terminal->println(ERROR, "This group could not be found.");
        terminal->prompt();
        return;
    }

    int state = mode == "on" ? 1 : 0;
    for (int i = 0; i < group->groupSize; i++) {
        KASASmartPlug* p = group->plugs[i];
        p->SetRelayState(state);
    }

    terminal->println(INFO, "Modes set");
    terminal->prompt();
}

void savePreferences(Terminal* terminal) {
    preferences.putInt("numGroups", groupUtils.numGroups);
    for (int groupNum = 0; groupNum < groupUtils.numGroups; groupNum++) {
        ControlGroup* group = groupUtils.groups[groupNum];
        // Build key names as <field><groupNum>
        char groupName[12] = {}; // "groupName", 1-2 digit, null terminator
        char groupSize[12] = {}; // "groupSize", 1-2 digit, null terminator
        char groupIps[11] = {}; // "groupSize", 1-2 digit, null terminator
        sprintf(groupName, "groupName%d", groupNum);
        sprintf(groupSize, "groupSize%d", groupNum);
        // Assign pref values
        preferences.putString(groupName, group->name);
        preferences.putInt(groupSize, group->groupSize);
        // Build ip list with format <ip1>,<ip2>,<ip3>...
        char ipList[10 + MAX_IP_LEN * MAX_GROUPS] = {}; // "groupIps", 1 digit, null terminator, 10 ips of 15 char + splitter
        for (int deviceNum = 0; deviceNum < group->groupSize; deviceNum++) {
            KASASmartPlug* plug = group->plugs[deviceNum];
            strcat(ipList, plug->ip_address);
            if (deviceNum != group->groupSize - 1) {
                strcat(ipList, ",");
            }
        }
        preferences.putString(groupIps, ipList);
    }

    terminal->println(INFO, "Save done");
    terminal->prompt();
}

void loadPreferences(Terminal* terminal) {
    groupUtils.clearGroups();
    int loadedNumGroups = preferences.getInt("numGroups");

    if (loadedNumGroups == 0) {
        if (hasSerial) {
            terminal->println(INFO, "No saved data found");
            terminal->prompt();
        }
        return;
    }

    for (int groupNum = 0; groupNum < loadedNumGroups; groupNum++) {
        // Build key names as <field><groupNum>
        char groupName[12] = {}; // "groupName", 1 digit, null terminator
        char groupSize[12] = {}; // "groupSize", 1 digit, null terminator
        char groupIps[11] = {}; // "groupSize", 1 digit, null terminator
        sprintf(groupName, "groupName%d", groupNum);
        sprintf(groupSize, "groupSize%d", groupNum);
        // Assign pref values
        String loadedName = preferences.getString(groupName, groupName); // If the name somehow gets corrupted, use tag name
        int loadedGroupSize = preferences.getInt(groupSize);
        // Build ip list with format <ip1>,<ip2>,<ip3>...
        char loadedIpList[10 + MAX_IP_LEN * MAX_GROUP_SIZE] = {}; // "groupIps", 1 digit, null terminator, 15 ips of 15 char + splitter
        String loadedIps = preferences.getString(groupIps);
        if (loadedIps.isEmpty()) {
            if (hasSerial) {
                terminal->println(ERROR, "Something broke... A group is missing its IP list");
                terminal->prompt();
            }
            return;
        }

        strcpy(loadedIpList, loadedIps.c_str()); // To tokebize

        ControlGroup* group = groupUtils.newGroup(loadedName.c_str());
        group->groupSize = loadedGroupSize;
        char* token = strtok(loadedIpList, ",");
        while (token != nullptr) {
            KASASmartPlug* plug = group->addDevice(token);
            plug->QueryInfo();
            token = strtok(nullptr, ",");
        }
    }

    if (hasSerial) {
        terminal->println(INFO, "Load done");
        terminal->prompt();
    }
}