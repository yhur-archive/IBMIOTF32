/*
 *  iIOT32G : ESP32 Library for IBMIOTDevice Gateway Connection
 *
 *  Yoonseok Hur
 *
 *  Important C API to be used by the Extension
 *      iotInitDevice();
 *      iotConfigDevice();
 *      webServer.on("/uri", html);    to add the additional custom page
 *      reboot();
 *      reset_config();
 *
 *  Usage Scenario:
 *      After include, customize these variables to set the Access Point prefix 
 *      and the custom field on the first setup page
 *          String user_html;
 *          char   *ssid_pfix;
 *      run iotInitDevice 
 *      if no config, add custom pages and run iotConfigDevice
 *
 */
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <PubSubClient.h>
#include <HTTPUpdate.h>
#include<ESPmDNS.h>

const char          compile_date[] = __DATE__ " " __TIME__;
char                publishTopic[200]   = "iot-2/evt/status/fmt/json";
char                infoTopic[200]      = "iot-2/evt/info/fmt/json";
char                commandTopic[200]   = "iot-2/cmd/+/fmt/+";
char                responseTopic[200]  = "iotdm-1/response";
char                manageTopic[200]    = "iotdevice-1/mgmt/manage";
char                updateTopic[200]    = "iotdm-1/device/update";
char                rebootTopic[200]    = "iotdm-1/mgmt/initiate/device/reboot";
char                resetTopic[200]     = "iotdm-1/mgmt/initiate/device/factory_reset";

#define             JSON_BUFFER_LENGTH 2048
StaticJsonDocument<JSON_BUFFER_LENGTH> cfg;
char cfgBuffer[JSON_BUFFER_LENGTH];

WebServer           webServer(80);
const int           RESET_PIN = 0;

char                cfgFile[] = "/config.json";

void                (*userConfigLoop)() = NULL; 
            // you can run something even during the iotDevceConfig routine

extern              String user_html;
extern char         *ssid_pfix;

char                caFile[] = "/ca.txt";
String              ca = ""
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n"
    "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
    "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n"
    "QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n"
    "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
    "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n"
    "9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n"
    "CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n"
    "nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n"
    "43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n"
    "T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n"
    "gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\n"
    "BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\n"
    "TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\n"
    "DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\n"
    "hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\n"
    "06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\n"
    "PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\n"
    "YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\n"
    "CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\n"
    "-----END CERTIFICATE-----\n";

int                 mqttPort = 0;

String html_begin = ""
    "<html><head><title>IOT Device Setup</title></head>"
    "<body><center><h1>Device Setup Page</h1>"
        "<style>"
            "input {font-size:3em; width:90%; text-align:center;}"
            "button { border:0;border-radius:0.3rem;background-color:#1fa3ec;"
            "color:#fff; line-height:2em;font-size:3em;width:90%;}"
        "</style>"
        "<form action='/save'>"
            "<p><input type='text' name='ssid' placeholder='SSID'>"
            "<p><input type='text' name='w_pw'placeholder='Password'>"
            "<p><input type='text' name='org' placeholder='ORG ID/Edge Address'>"
            "<p><input type='text' name='devType' placeholder='Device Type'>"
            "<p><input type='text' name='devId' placeholder='Device Id'>"
            "<p><input type='text' name='token' placeholder='Device Token'>"
            "<p><input type='text' name='meta.pubInterval' placeholder='Publish Interval'>";

String html_end = ""
            "<p><button type='submit'>Save</button>"
        "</form>"
    "</center></body></html>";

String postSave_html = ""
    "<html><head><title>Reboot Device</title></head>"
    "<body><center><h5>Device Configuration Finished</h5><h5>Click the Reboot Button</h5>"
        "<p><button type='button' onclick=\"location.href='/reboot'\">Reboot</button>"
    "</center></body></html>";


WiFiClient          wifiClient;
WiFiClientSecure    wifiClientSecure;
PubSubClient        client;
char                iot_server[200];
char                msgBuffer[JSON_BUFFER_LENGTH];
unsigned long       pubInterval;

void byte2buff(char* msg, byte* payload, unsigned int len) {
    unsigned int i, j;
    for (i=j=0; i < len ;) {
        msg[j++] = payload[i++];
    }
    msg[j] = '\0';
}

ICACHE_RAM_ATTR void reboot() {
    WiFi.disconnect();
    ESP.restart();
}

unsigned    iotWatchDogLimit = 300000;
void iotWatchDog(void * elapsed) {
    unsigned df;
    while(true) {
        df = millis() - *((unsigned*)elapsed);
        // Serial.printf("\nWatchDog Ticker : %u %u\n", df, iotWatchDogLimit);
        if(df > iotWatchDogLimit) {
            reboot();
        }
        vTaskDelay(10000);
    }
}

void startIOTWatchDog(void* wdTime, unsigned wdlimit = iotWatchDogLimit){
    iotWatchDogLimit = wdlimit;
    xTaskCreate( iotWatchDog, "iotWatchDog", 10000, wdTime, 1, NULL);
} 

void save_config_json(){
    serializeJson(cfg, cfgBuffer);
    File f = SPIFFS.open(cfgFile, "w");
    f.print(cfgBuffer);
    f.close();
}

void toGatewayTopic(char* topic, const char* devType, const char* devId) {
    char buffer[200];
    char devInfo[100];
    sprintf(devInfo, "/type/%s/id/%s", devType, devId);

    char* slash = strchr(topic, '/');
    int len = slash - topic;
    strncpy(buffer, topic, len);
    strcpy(buffer + len, devInfo);
    strcpy(buffer + strlen(buffer), slash);
    strcpy(topic, buffer);
}

void reset_config() {
	deserializeJson(cfg, "{meta:{}}");
    save_config_json();
}

void maskConfig(char* buff) {
    DynamicJsonDocument temp_cfg = cfg;
    temp_cfg["w_pw"] = "********";
    temp_cfg["token"] = "********";
    serializeJson(temp_cfg, buff, JSON_BUFFER_LENGTH);
}

void init_cfg() {
    if (SPIFFS.exists(cfgFile)) {
        File f = SPIFFS.open(cfgFile, "r");
        char buff[512];
        int i = 0;
        while(f.available()) {
            buff[i++] = f.read();
        }
        f.close();

        DeserializationError error = deserializeJson(cfg, String(buff));
	    if (error) {
	        deserializeJson(cfg, "{meta:{}}");
	    } else {
	        Serial.println("CONFIG JSON Successfully loaded");
	        char maskBuffer[JSON_BUFFER_LENGTH];
	        maskConfig(maskBuffer);
	        Serial.println(String(maskBuffer));
	    }
    } else {
	    deserializeJson(cfg, "{meta:{}}");
    }
}

void iotInitDevice() {
    // check Factory Reset Request and reset if requested
    // and initialize

    if(!SPIFFS.begin()) {
        SPIFFS.format();
    }
    pinMode(RESET_PIN, INPUT_PULLUP);
    if( digitalRead(RESET_PIN) == 0 ) {
        unsigned long t1 = millis();
        while(digitalRead(RESET_PIN) == 0) {
            delay(500);
            Serial.print(".");
        }
        if (millis() - t1 > 5000) {
            reset_config();             // Factory Reset
        }
    }
    attachInterrupt(RESET_PIN, reboot, FALLING);
    init_cfg();
}

void saveEnv() {
    int args = webServer.args();
    for (int i = 0; i < args ; i++){
        if (webServer.argName(i).indexOf(String("meta.")) == 0 ) {
            cfg["meta"][webServer.argName(i).substring(5)] = webServer.arg(i);
        } else {
            cfg[webServer.argName(i)] = webServer.arg(i);
        }
    }
    cfg["config"] = "done";
    save_config_json();
    webServer.send(200, "text/html", postSave_html);
}

void iotConfigDevice() {
    DNSServer   dnsServer;
    const byte  DNS_PORT = 53;
    IPAddress   apIP(192, 168, 1, 1);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    char ap_name[100];
    sprintf(ap_name, "%s_%08X", ssid_pfix, (unsigned int)ESP.getEfuseMac());
    WiFi.softAP(ap_name);
    delay(3000);
    dnsServer.start(DNS_PORT, "*", apIP);

    webServer.on("/save", saveEnv);
    webServer.on("/reboot", reboot);

    webServer.onNotFound([]() {
        webServer.send(200, "text/html", html_begin + user_html + html_end);
    });
    webServer.begin();
    Serial.println("starting the config");
    while(1) {
        yield();
        dnsServer.processNextRequest();
        webServer.handleClient();
        if(userConfigLoop != NULL) {
            (*userConfigLoop)();
        }
    }
}

bool subscribeTopic(const char* topic) {
    if (client.subscribe(topic)) {
        Serial.printf("Subscription to %s OK\n", topic);
        return true;
    } else {
        Serial.printf("Subscription to %s Failed\n", topic);
        return false;
    }
}

void initDevice() {
    iotInitDevice();
    if(!cfg.containsKey("config") || strcmp((const char*)cfg["config"], "done") || !cfg.containsKey("org")) {
        reset_config();
        iotConfigDevice();
    }

    String org = cfg["org"];
    if (org.indexOf(".") == -1) {
        if (SPIFFS.exists(caFile)) {
            File f = SPIFFS.open(caFile, "r");
            ca = f.readString();
            ca.trim();
            f.close();
        }
        sprintf(iot_server, "%s.messaging.internetofthings.ibmcloud.com", (const char*)cfg["org"]);
        wifiClientSecure.setCACert(ca.c_str());
        client.setClient(wifiClientSecure);
        mqttPort = 8883;
    } else {
        const char* devType = (const char*)cfg["devType"];
        const char* devId = (const char*)cfg["devId"];
        toGatewayTopic(publishTopic, devType, devId);
        toGatewayTopic(infoTopic, devType, devId);
        toGatewayTopic(commandTopic, devType, devId);
        toGatewayTopic(responseTopic, devType, devId);
        toGatewayTopic(manageTopic, devType, devId);
        toGatewayTopic(updateTopic, devType, devId);
        toGatewayTopic(rebootTopic, devType, devId);
        toGatewayTopic(resetTopic, devType, devId);
        client.setClient(wifiClient);
        mqttPort = 1883;
    }
}

void iot_connect() {
    while (!client.connected()) {
        int mqConnected;
        if (mqttPort == 8883) {
            sprintf(msgBuffer,"d:%s:%s:%s", (const char*)cfg["org"], (const char*)cfg["devType"], (const char*)cfg["devId"]);
            mqConnected = client.connect(msgBuffer,"use-token-auth",cfg["token"]);
        } else {
            sprintf(msgBuffer,"d:%s:%s", (const char*)cfg["devType"], (const char*)cfg["devId"]);
            mqConnected = client.connect(msgBuffer);
        }
        if (mqConnected) {
            Serial.println("MQ connected");
        } else {
            if(WiFi.status() == WL_CONNECTED) {
                if(client.state() == -2) {
                    if (mqttPort == 8883) {
                        wifiClientSecure.connect(iot_server, mqttPort);
                    } else {
                        wifiClient.connect(iot_server, mqttPort);
                    }
                    Serial.println("socket reconnection");
                } else {
                    Serial.printf("MQ Connection fail RC = %d, try again in 5 seconds\n", client.state());
                }
                delay(5000);
            } else {
                Serial.println("Reconnecting to WiFi");
                WiFi.disconnect();
                WiFi.begin();
                while (WiFi.status() != WL_CONNECTED) {
                    Serial.print("*");
                    delay(5000);
                }
            }
        }
    }
    if (!subscribeTopic(responseTopic)) return;
    if (!subscribeTopic(rebootTopic)) return;
    if (!subscribeTopic(resetTopic)) return;
    if (!subscribeTopic(updateTopic)) return;
    if (!subscribeTopic(commandTopic)) return;
    JsonObject meta = cfg["meta"];
    StaticJsonDocument<512> root;
    JsonObject d = root.createNestedObject("d");
    JsonObject metadata = d.createNestedObject("metadata");
    for (JsonObject::iterator it=meta.begin(); it!=meta.end(); ++it) {
        metadata[it->key().c_str()] = it->value();
    }
    JsonObject supports = d.createNestedObject("supports");
    supports["deviceActions"] = true;
    serializeJson(root, msgBuffer);
    Serial.printf("publishing device metadata: %s\n", msgBuffer);
    if (client.publish(manageTopic, msgBuffer)) {
        serializeJson(d, msgBuffer);
        String info = String("{\"info\":") + String(msgBuffer) + String("}");
        client.publish(infoTopic, info.c_str());
    }
}

void publishError(char *msg) {
    String payload = "{\"info\":{\"error\":";
    payload += "\"" + String(msg) + "\"}}";
    client.publish(infoTopic, (char*) payload.c_str());
    Serial.println(payload);
}

String ip_resolve(String name){
    IPAddress ipaddr = IPAddress();
    if(!ipaddr.fromString(name)) {
        mdns_init ();
        name.replace(".local\0", "");
        ipaddr = MDNS.queryHost(name, 5000);      // 5000 ms
    }
    return ipaddr.toString();
}

void handleIOTCommand(char* topic, JsonDocument* root) {
    JsonObject d = (*root)["d"];

    if (strstr(topic, "/response")) {
        return;                                 // just print of response for now
    } else if (strstr(topic, "/device/reboot")) {   // rebooting
        reboot();
    } else if (strstr(topic, "/device/factory_reset")) {    // clear the configuration and reboot
        reset_config();
        ESP.restart();
    } else if (strstr(topic, "/device/update")) {
        JsonArray fields = d["fields"];
        for(JsonArray::iterator it=fields.begin(); it!=fields.end(); ++it) {
            DynamicJsonDocument field = *it;
            const char* fieldName = field["field"];
            if (strstr(fieldName, "metadata")) {
                JsonObject fieldValue = field["value"];
                cfg.remove("meta");
                JsonObject meta = cfg.createNestedObject("meta");
                for (JsonObject::iterator fv=fieldValue.begin(); fv!=fieldValue.end(); ++fv) {
                    meta[(char*)fv->key().c_str()] = fv->value();
                }
                save_config_json();
            }
        }
        pubInterval = cfg["meta"]["pubInterval"];
    } else if (strstr(topic, "/cmd/")) {
        if (d.containsKey("upgrade")) {
            JsonObject upgrade = d["upgrade"];
            String response = "{\"OTA\":{\"status\":";
            if(upgrade.containsKey("server") && 
                        upgrade.containsKey("port") && 
                        upgrade.containsKey("uri")) {
                WiFiClient cli;
                HTTPUpdate httpUpdate;
		        Serial.println("firmware upgrading");

	            String ota_server = upgrade["server"];
	            char fw_server[20];
	            sprintf(fw_server, ip_resolve(ota_server).c_str());
	            int fw_server_port = atoi(upgrade["port"]);
	            const char *fw_uri = upgrade["uri"];
                client.publish(infoTopic,"{\"info\":{\"upgrade\":\"Device will be upgraded.\"}}" );
                t_httpUpdate_return ret = httpUpdate.update(cli, fw_server, fw_server_port, fw_uri);
	            switch(ret) {
		            case HTTP_UPDATE_FAILED:
                        response += "\"[update] Update failed. http://" + String(fw_server);
                        response += ":"+ String(fw_server_port) + String(fw_uri) +"\"}}";
                        client.publish(infoTopic, (char*) response.c_str());
                        Serial.println(response);
		                break;
		            case HTTP_UPDATE_NO_UPDATES:
                        response += "\"[update] Update no Update.\"}}";
                        client.publish(infoTopic, (char*) response.c_str());
                        Serial.println(response);
		                break;
		            case HTTP_UPDATE_OK:
		                Serial.println("[update] Update ok."); // may not called we reboot the ESP
		                break;
	            }
            } else {
                response += "\"OTA Information Error\"}}";
                client.publish(infoTopic, (char*) response.c_str());
                Serial.println(response);
            }
        } else if (d.containsKey("config")) {
            char maskBuffer[JSON_BUFFER_LENGTH];
            cfg["compile_date"] = compile_date;
            maskConfig(maskBuffer);
            cfg.remove("compile_date");
            String info = String("{\"config\":") + String(maskBuffer) + String("}");
            client.publish(infoTopic, info.c_str());
        }
    }
}

void set_iot_server() {

    if(mqttPort == 8883) {
        if (!wifiClientSecure.connect(iot_server, mqttPort)) {
            Serial.println("ssl connection failed");
            return;
        }
    } else {
        String broker = (const char*)cfg["org"];
        String ip = ip_resolve(broker);
        sprintf(iot_server, "%s", ip.c_str());

        if (!wifiClient.connect(iot_server, mqttPort)) {
            Serial.println("connection failed");
            return;
        }
    }

    client.setServer(iot_server, mqttPort);   //IOT Server
    iot_connect();
}
/* FW Upgrade informaiton 
 * var evt1 = { 'd': { 
 *   'upgrade' : {
 *       'server':'192.168.0.9',
 *       'port':'3000',
 *       'uri' : '/file/IOTPurifier4GW.ino.nodemcu.bin'
 *       }
 *   }
 * };
*/