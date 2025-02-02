#include <Adafruit_SSD1306.h>
#include <Arduino.h>

#include <Kasale.h>
#include <WiFi.h>
#include <KasaSmartPlug.h>
#include <Preferences.h>
#include <Terminal.h>
#include <Encoder.h>
#include <Commands.h>

Terminal terminal = nullptr;

Encoder brightness_enc;
Encoder selection_enc;
Adafruit_SSD1306 screen;

uint64_t buildWakeupMask() { // Build mask since encoder A/B is ambiguous.
    int pins[] = {BRI_A, BRI_B, SEL_A, SEL_B};
    uint64_t mask = (uint64_t) 0x1 << BRI_SW | (uint64_t) 0x1 << SEL_SW; // Start with switch mask
    for (int pin : pins) {
        if (digitalRead(pin) == LOW) {
            mask = mask | (uint64_t) 0x1 << pin;
        }
    }
    return mask;
}

// For shutdown after delay
void sleep_callback(void*) {
    delay(500);
    digitalWrite(LED, LOW);
    if (dimDisplay) {
        screen.dim(true);
    } else {
        screen.clearDisplay();
        screen.display();
    }
    // Set up touch wake
    esp_sleep_enable_ext1_wakeup(buildWakeupMask(), ESP_EXT1_WAKEUP_ANY_HIGH);
    esp_deep_sleep_start();
}

const esp_timer_create_args_t sleep_timer_args = {
    .callback = &sleep_callback
};

esp_timer_handle_t sleep_timer;

void start_timer() {
    if (idleTimeout == 0) return;
    int err = esp_timer_start_once(sleep_timer, idleTimeout);
    if (err != ESP_OK) {
        // Serial.printf("Failed to initialize timer: %d\n", err);
    }
}

void extend_timer() {
    if (hasSerial || idleTimeout == 0) { // Do not sleep when serial on
        return;
    }
    esp_timer_stop(sleep_timer);
    int err = esp_timer_start_once(sleep_timer, idleTimeout);
    if (err != ESP_OK) {
        // Serial.printf("Failed to restart timer: %d\n", err);
    }
}

// Brightness interrupt
bool brightness_flag = false;
int brightness;
void brightness_isr() {
    int dir = brightness_enc.readDir();
    if (!dir) return;

    if (dir > 0) {
        if (brightness + brightnessStep > 100) {
            brightness = 100;
        } else {
            brightness += brightnessStep;
        }
    } else {
        if (brightness - brightnessStep < 0) {
            brightness = 0;
        } else {
            brightness -= brightnessStep;
        }
    }

    brightness_flag = true;
}

// Group selection interrupt
bool selection_flag = false;
void selection_isr() {
    int dir = selection_enc.readDir();
    if (dir == 0) return;

    selection = groupUtils.nextGroupIndex(selection, dir);
    selection_flag = true;
}

int relay_update = -1; // -1 for no update, 0/1 for off/on
void relay_isr() {
    if (digitalRead(SEL_SW) == HIGH) {
        relay_update = 0;
    } else if (digitalRead(BRI_SW) == HIGH) {
        relay_update = 1;
    }
}

void startWifi() {
    // Load SSID and password from preferences
    bool hasCredentials = true;
    if (!preferences.isKey("ssid")) {
        if (hasSerial) terminal.println(ERROR, "No SSID stored!");
        hasCredentials = false;
    }

    if (!preferences.isKey("password")) {
        if (hasSerial) terminal.println(ERROR, "No Password stored");
        hasCredentials = false;
    }

    if (!hasCredentials) {
        if (hasSerial) terminal.println(ERROR, "Fix missing credentials then run 'connect'");
        return;
    }

    String ssid = preferences.getString("ssid").c_str();
    String password = preferences.getString("password").c_str();

    // connect to WiFi
    if (hasSerial) Serial.printf("Connecting to %s ", ssid);
    WiFi.begin(ssid, password);
}

bool checkWifi() { // To be run only after initial connection
    for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
        delay(250);
        if (hasSerial) Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        if (hasSerial) terminal.println(ERROR, "Failed to connect to WiFi");
        return false;
    }

    if (hasSerial) terminal.println(INFO, " CONNECTED");
    if (hasSerial) terminal.println(INFO, WiFi.localIP().toString());
    return true;
}

void do_wakeup(){
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT1) return;

    uint64_t mask = esp_sleep_get_ext1_wakeup_status();
    int cause = 0;
    if (mask & (uint64_t) 0x1 << BRI_SW) {
        cause = BRI_SW;
    } else if (mask & (uint64_t) 0x1 << SEL_SW) {
        cause = SEL_SW;
    }

    switch (cause) {
        case SEL_SW:
            relay_update = 0;
            break;
        case BRI_SW:
            relay_update = 1;
            break;
        default: // Knob turned, but we don't care
            break;
    }
}

void setup() {
    pinMode(BRI_SW, INPUT);
    pinMode(SEL_SW, INPUT);
    pinMode(LED, OUTPUT);
    pinMode(SERIAL_TOGGLE, INPUT);
    pinMode(PANIC, INPUT);

    hasSerial = digitalRead(SERIAL_TOGGLE) == HIGH;

    esp_timer_create(&sleep_timer_args, &sleep_timer);

    preferences.begin(DEVICE_NAME, false);

    // Load prefs
    brightnessStep = preferences.getInt("brightnessStep", 5);
    int invertScreen = preferences.getBool("invertScreen", false);
    defaultBrightness = preferences.getInt("defaultBrightness", 50);
    idleTimeout = preferences.getInt("idleTimeout", 30) * 1000000;
    dimDisplay = preferences.getBool("dimDisplay", false);

    brightness_enc.setup(BRI_A, BRI_B);

    if (digitalRead(PANIC) == HIGH) {
        preferences.putInt("numGroups", 0);
        preferences.end();
        digitalWrite(LED, HIGH);
        delay(100000);
    }

    // Attempt wifi start, nonblocking
    startWifi();

    if (hasSerial) {
        Serial.begin(115200);

        terminal = Terminal(&Serial);

        // Terminal setup
        terminal.setup();
        terminal.setPrompt(CONSOLE_PROMPT);
        addStandardTerminalCommands();
        TERM_CMD->addCmd("connect", "", "Connect to wifi", connectWifi);
        TERM_CMD->addCmd("ssid", "<ssid>", "Set network SSID", setSsid);
        TERM_CMD->addCmd("password", "<password>", "Set network password", setPassword);
        TERM_CMD->addCmd("create", "<group_name>", "Create new group", createGroup);
        TERM_CMD->addCmd("all", "", "Create new group for all devices", createAllGroup);
        TERM_CMD->addCmd("clone", "<existing_group> <new_group_name>", "Copy a group with a new name", cloneGroup);
        TERM_CMD->addCmd("delete", "<group_name>", "Delete an existing group", deleteGroup);
        TERM_CMD->addCmd("+", "<group_name> <device_ip>", "Add a device to a group", addDeviceToGroup);
        TERM_CMD->addCmd("-", "<group_name> <device_ip>", "Remove a device from a group", removeDeviceFromGroup);
        TERM_CMD->addCmd("!", "<group_name>", "List devices in a group", listGroupDevices);
        TERM_CMD->addCmd("!!", "", "List all devices in all groups", listAllGroupDevices);
        TERM_CMD->addCmd("list", "", "List groups", listGroups);
        TERM_CMD->addCmd("mode", "<group_name> <on/off>", "Set a group of lights on or off", setRelayMode);
        TERM_CMD->addCmd("scan", "", "List all visible devices on the network", scanDevices);
        TERM_CMD->addCmd("save", "", "Saves all groups and their settings", savePreferences);
        TERM_CMD->addCmd("load", "", "Load saved groups", loadPreferences);
        TERM_CMD->addCmd("pref", "Run this cmd to see options", "Set preferences", prefCommand);

        terminal.prompt();
    } else {
        start_timer(); // Only timeout if no terminal
    }

    screen = Adafruit_SSD1306(128, 32,
                               &Wire, -1);

    if(!screen.begin(SSD1306_SWITCHCAPVCC)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Don't proceed, loop forever
    }

    if (dimDisplay) { // Undim on start
        screen.dim(false);
    }
    screen.setRotation(invertScreen ? 2 : 0);
    screen.display();
    screen.setTextColor(WHITE);
    screen.setTextSize(1);
    screen.setTextWrap(false);
    screen.clearDisplay();

    if (!checkWifi()) {
        screen.write("No Wifi");
        screen.display();
        while (true) { // User needs to set ssid and password correctly?
            if (hasSerial) terminal.loop();
            delay(100);
        }
    }

    loadPreferences(&terminal);

    selection_enc.setup(SEL_A, SEL_B);
    selection_flag = true; // Trigger a screen update

    attachInterrupt(BRI_SW, relay_isr, RISING);
    attachInterrupt(SEL_SW, relay_isr, RISING);
    attachInterrupt(BRI_B, brightness_isr, CHANGE);
    attachInterrupt(SEL_B, selection_isr, CHANGE);

    digitalWrite(LED, HIGH);

    do_wakeup();
}

int update_brightness(int value) {
     if (selGroup == nullptr) {
        return 0;
    }

    int numUpdates = 0;

    for (int i = 0; i < selGroup->groupSize; i++) {
        KASASmartPlug* p = selGroup->plugs[i];
        if (p->brightness != -1 && p->state) { // Is brightness controllable and is on
            p->SetBrightness(value);
            p->brightness = value;
            delay(100);
            numUpdates++;
        }
    }

    return numUpdates;
}

void loop() {

    if (hasSerial) terminal.loop();

    if (selection_flag) {
        if (selection >= groupUtils.numGroups) { // Group could get deleted
            selection = groupUtils.numGroups - 1;
        }

        brightness = defaultBrightness; // Reset brightness setting
        screen.clearDisplay();
        screen.setCursor(0, 0);
        screen.write((String(brightness) + "% (X)").c_str());
        screen.setCursor(0, CHAR_HEIGHT);
        screen.write((String("  ") + groupUtils.groups[groupUtils.nextGroupIndex(selection, -1)]->name).c_str());
        screen.setCursor(0, CHAR_HEIGHT * 2);
        screen.write((String("> ") + selGroup->name).c_str());
        screen.setCursor(0, CHAR_HEIGHT * 3);
        screen.write((String("  ") + groupUtils.groups[groupUtils.nextGroupIndex(selection, 1)]->name).c_str());
        screen.display();
        selection_flag = false;
        extend_timer();
    }

    if (brightness_flag) {
        int numUpdates = update_brightness(brightness);
        screen.fillRect(0, 0, 128, CHAR_HEIGHT, BLACK);
        screen.setCursor(0, 0);
        char buffer[16];
        sprintf(buffer, "%3d%% (%d)  \0", brightness, numUpdates);
        screen.write(buffer);
        screen.display();
        brightness_flag = false;
        extend_timer();
    }

    if (relay_update != -1) {
        extend_timer();

        if (selGroup == nullptr) {
            return;
        }

        for (int i = 0; i < selGroup->groupSize; i++) {
            KASASmartPlug* p = selGroup->plugs[i];
            p->SetRelayState(relay_update);
            p->state = relay_update;
            delay(100); // having problems with network so slow down
        }
        extend_timer();
        relay_update = -1;
    }

    delay(100);
}