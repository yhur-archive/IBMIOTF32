# IBM IOT Foundation for ESP32
IBM IOT Device Helper for ESP32 

With this library, the developer can create an ESP32 IBM IOT device which
1. helps configure with the Captive Portal if not configured as in the following picture,
2. or boots with the stored configuration if configured already,
3. connects to the WiFi/IBM IOT Foundation and run the loop function

![IOT Device Setup Captive Portal](https://user-images.githubusercontent.com/13171662/150662713-58af1cfc-be48-457b-828a-d9c1afe0c561.jpg)

# How to use the IBMIOTF32
You can create a PlatformIO project with the example directory and modify the src/main.cpp for your purpose and build it.

## src/main.cpp 
The following code is the example to use the library. 
```c
#include <IBMIOTF32.h>

String user_html = "";

char*               ssid_pfix = (char*)"YourShortNameHere";
unsigned long       lastPublishMillis = 0;

void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");

    // YOUR CODE for status reporting

    serializeJson(root, msgBuffer);
    client.publish(publishTopic, msgBuffer);
}

void handleUserCommand(JsonDocument* root) {
    JsonObject d = (*root)["d"];

    // YOUR CODE for command handling

}

void message(char* topic, byte* payload, unsigned int payloadLength) {
    byte2buff(msgBuffer, payload, payloadLength);
    StaticJsonDocument<512> root;
    DeserializationError error = deserializeJson(root, String(msgBuffer));

    if (error) {
        Serial.println("handleCommand: payload parse FAILED");
        return;
    }

    handleIOTCommand(topic, &root);
    if (strstr(topic, "/device/update")) {
        JsonObject meta = cfg["meta"];

    // YOUR CODE for meta data synchronization

    } else if (strstr(topic, "/cmd/")) {
        handleUserCommand(&root);
    }
}

void setup() {
    Serial.begin(115200);
    initDevice();

    JsonObject meta = cfg["meta"];
    pubInterval = meta.containsKey("pubInterval") ? atoi((const char*)meta["pubInterval"]) : 0;
    lastPublishMillis = - pubInterval;
    startIOTWatchDog((void*)&lastPublishMillis, (int)(pubInterval * 5));
    // YOUR CODE for initialization of device/environment

    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if(i++ > 10) reboot();
    }
    Serial.printf("\nIP address : "); Serial.println(WiFi.localIP());

    client.setCallback(message);
    set_iot_server();
}

void loop() {
    if (!client.connected()) {
        iot_connect();
    }
    client.loop();
    // YOUR CODE for routine operation in loop

    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();
        lastPublishMillis = millis();
    }
}
```

## Customization
If additional configuration parameter is needed, you can modify the `user_html` for your variable, and handle the variable as shown below. The custom varialbe here is yourVar in the `user_html` and handling in setup code as below

In the global section.
```c
int     yourVar;

String user_html = ""
    "<p><input type='text' name='meta.yourVar' placeholder='Your Config Variable'>";
```

In the functions.
```c
    JsonObject meta = cfg["meta"];
    yourVar = meta.containsKey("yourVar") ? atoi((const char*)meta["yourVar"]) : 0;
```


## dependancy and tips
This library uses SPIFFS, and needs PubSubClient, ArduinoJson to name a few of important ones.

And if you need to send a long MQTT message, then you can increase the default size of 256 byte by setting the build time variable MQTT_MAX_PACKET_SIZE as below. You may need this, if you want to implement the IR remote, since some appliance would need a long sequence of control signal.

The following is a snnipet of a tested platformio.ini.

```
build_flags = 
	-D MQTT_MAX_PACKET_SIZE=512
lib_deps = 
	knolleary/PubSubClient@^2.8
	https://github.com/yhur/IBMIOT32.git
	bblanchon/ArduinoJson@^6.18.5

```

### More Info.
It handles

1. IBM IOT Device Management like remote boot, remote factory reset
2. IBM IOT meta data update
3. Over the Air Firmware Update
4. Configuration reporting
