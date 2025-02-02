//
// Created by ianzh on 1/29/2025.
//

#ifndef COMMANDS_H
#define COMMANDS_H
#include <Terminal.h>

void prefCommand(Terminal *terminal);

void connectWifi(Terminal *terminal);

void setSsid(Terminal *terminal);

void setPassword(Terminal *terminal);

void createAllGroup(Terminal *terminal);

void cloneGroup(Terminal *terminal);

void createGroup(Terminal *terminal);

void deleteGroup(Terminal *terminal);

void listGroups(Terminal *terminal);

void scanDevices(Terminal *terminal);

void addDeviceToGroup(Terminal *terminal);

void removeDeviceFromGroup(Terminal *terminal);

void listAllGroupDevices(Terminal *terminal);

void listGroupDevices(Terminal *terminal);

void setRelayMode(Terminal *terminal);

void savePreferences(Terminal *terminal);

void loadPreferences(Terminal *terminal);

#endif //COMMANDS_H
