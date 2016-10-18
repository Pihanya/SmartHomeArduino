#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

const char* ssid = "RUTIWF";
const char* password = "0914196409141";
const char* accessPassword = "........";

ESP8266WebServer server(80);

#define SERIAL_HANDLER_PERIOD 50
#define SERIAL_HANDLER_MAX_ITERATIONS 40

String handleSerial() {
  int iterations = 0;

  while (Serial.available() == 0 && iterations < SERIAL_HANDLER_MAX_ITERATIONS) {
    delay(SERIAL_HANDLER_PERIOD);
    ++iterations;
  }

  String response = Serial.readStringUntil('\n');

  if (Serial.available() > 0) {
    Serial.read();
  }

  if (iterations < SERIAL_HANDLER_MAX_ITERATIONS) {
    return response;
  } else {
    return String("Timeout");
  }
}

void handleRoot() {
  server.send(200, "text/plain", "hello from esp8266!");
}

void handleNotFound() {
  String message = "File Not Found\n\n";

  message += "URI: ";
  message += server.uri();

  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";

  message += "\nArguments: ";
  message += server.args();

  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}

void setup(void) {
  Serial.begin(115200);
  
  // config static IP
  IPAddress ip(192, 168, 1, 31); // where xx is the desired IP Address
  IPAddress gateway(192, 168, 1, 1); // set gateway to match your network
  //  Serial.print(F("Setting static ip to : "));
  //  Serial.println(ip);
  IPAddress subnet(255, 255, 255, 0); // set subnet mask to match your network
  WiFi.config(ip, gateway, subnet);
  
//  Serial.println("");
  
  WiFi.begin(ssid, password);
  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
//    Serial.print(".");
  }

//  Serial.println("");
//  Serial.print("Connected to ");
//  Serial.println(ssid);
//  Serial.print("IP address: ");
//  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
//    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  server.on("/set", []() {
    Serial.print("1 ");
    Serial.print(server.arg("name"));
    Serial.print(' ');
    Serial.print(server.arg("value"));
    Serial.print('\n');

    server.send(200, "text/plain", handleSerial());
  });

  server.on("/get", []() {
    Serial.print("2 ");
    Serial.print(server.arg("pin"));
    Serial.print('\n');

    server.send(200, "text/plain", handleSerial());
  });

  server.on("/feature", []() {
    Serial.print("3 ");
    Serial.print(server.arg("name"));
    Serial.print(' ');
    Serial.print(server.arg("value"));
    Serial.print('\n');

    server.send(200, "text/plain", handleSerial());
  });

  server.on("/getFeature", []() {
    Serial.print("4 ");
    Serial.print(server.arg("name"));
    Serial.print('\n');

    server.send(200, "text/plain", handleSerial());
  });

  server.on("/lightning", []() {
    Serial.print("5 ");
    Serial.print(server.arg("room"));
    Serial.print(' ');
    Serial.print(server.arg("value"));
    Serial.print('\n');

    server.send(200, "text/plain", handleSerial());
  });

  server.on("/getLightning", []() {
    Serial.print("4 ");
    Serial.print(server.arg("name"));
    Serial.print('\n');

    server.send(200, "text/plain", handleSerial());
  });

  server.on("/color", []() {
    Serial.print("7 ");
    Serial.print(server.arg("room"));
    Serial.print(' ');
    Serial.print(server.arg("color"));
    Serial.print('\n');

    server.send(200, "text/plain", handleSerial());
  });

  server.on("/getColor", []() {
    Serial.print("8 ");
    Serial.print(server.arg("room"));
    Serial.print('\n');

    server.send(200, "text/plain", handleSerial());
  });

  server.on("/mode", []() {
    Serial.print("9 ");
    Serial.print(server.arg("room"));
    Serial.print(' ');
    Serial.print(server.arg("mode"));
    Serial.print('\n');

    if (server.hasArg("duration")) {
      Serial.print(' ');
      Serial.print(server.arg("duration"));
    }

    Serial.print('\n');

    server.send(200, "text/plain", handleSerial());
  });

  server.on("/getMode", []() {
    Serial.print("9 ");
    Serial.print(server.arg("room"));

    if (server.hasArg("duration")) {
      Serial.print(' ');
      Serial.print(server.arg("duration"));
    }

    Serial.print('\n');

    server.send(200, "text/plain", handleSerial());
  });

  server.onNotFound(handleNotFound);

  server.begin();
//  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
}


