/////////////////////////////////////////////////////////////////////////////
//  Edge AI Voice Assistant - ESP8266 "network brain bridge"
//  Module: Mikrocontrollertechnik (FH Aachen) - wifiTick add-on
//
//  Runs on the ESP8266 (Arduino). It is the bridge to the cloud so that NO PC
//  is needed: the RP2350 sends a question over the UART, this firmware calls
//  the Groq LLM over HTTPS, and returns "mood|reply" back over the UART.
//
//  Protocol on the serial line (115200 8N1, both directions, '\n' terminated):
//      RP2350 -> ESP :  <question text>\n
//      ESP -> RP2350 :  <mood>|<reply text>\n        (mood in
//                       neutral|happy|sad|angry|curious)
//      ESP status    :  #<status text>\n             (lines starting with '#'
//                       are info/log, the RP2350 can show or ignore them)
//
//  Fill firmware/esp8266/groq_bridge/config.h (copy from config.example.h).
/////////////////////////////////////////////////////////////////////////////
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>

#include "config.h"

// The assistant is asked to answer as "mood|reply" so parsing stays trivial on
// both the ESP and the RP2350 (no nested JSON to decode on a microcontroller).
static const char *SYSTEM_PROMPT =
    "You are the Edge AI Voice Assistant on an RP2350 microcontroller. "
    "Answer concisely (1-2 sentences), suitable to be spoken aloud. "
    "You MUST start your answer with one mood word, then a single '|', then "
    "your answer, like: happy|Bonjour, je vais tres bien. "
    "The mood is exactly one of: neutral, happy, sad, angry, curious.";

String msg;  // accumulates the incoming question

// Minimal JSON string-escape for the user/system text we put in the request.
static String jsonEscape(const String &in) {
    String out;
    out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); ++i) {
        char c = in[i];
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') { /* skip */ }
        else out += c;
    }
    return out;
}

// Pull the first  "content":"...."  string out of the Groq JSON response,
// unescaping the basic sequences. Good enough for our single-field reply.
static String extractContent(const String &body) {
    int k = body.indexOf("\"content\"");
    if (k < 0) return "";
    int q = body.indexOf('"', k + 9);          // opening quote of the value
    if (q < 0) return "";
    String out;
    for (size_t i = q + 1; i < body.length(); ++i) {
        char c = body[i];
        if (c == '\\') {
            char n = body[++i];
            if (n == 'n') out += '\n';
            else if (n == 't') out += ' ';
            else out += n;                       // \" \\ / etc.
        } else if (c == '"') {
            break;                               // end of the content string
        } else {
            out += c;
        }
    }
    return out;
}

static String askGroq(const String &question) {
    WiFiClientSecure client;
    client.setInsecure();                        // skip cert check (demo)
    client.setTimeout(15000);

    HTTPClient http;
    if (!http.begin(client, "https://api.groq.com/openai/v1/chat/completions"))
        return "sad|Connexion au cloud impossible.";
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + GROQ_API_KEY);

    String payload = String("{\"model\":\"") + GROQ_MODEL +
        "\",\"temperature\":0.6,\"messages\":[" +
        "{\"role\":\"system\",\"content\":\"" + jsonEscape(SYSTEM_PROMPT) + "\"}," +
        "{\"role\":\"user\",\"content\":\"" + jsonEscape(question) + "\"}]}";

    int code = http.POST(payload);
    String reply;
    if (code == 200) {
        String content = extractContent(http.getString());
        reply = content.length() ? content : "neutral|(reponse vide)";
        if (reply.indexOf('|') < 0) reply = String("neutral|") + reply;
    } else {
        reply = String("sad|Erreur cloud (") + code + ")";
    }
    http.end();
    return reply;
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("#booting");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("#wifi connecting");
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
        delay(300);
        Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED)
        Serial.println(String("#wifi ok ") + WiFi.localIP().toString());
    else
        Serial.println("#wifi FAILED");
}

void loop() {
    if (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            msg.trim();
            if (msg.length()) {
                if (WiFi.status() != WL_CONNECTED)
                    Serial.println("sad|Pas de WiFi.");
                else
                    Serial.println(askGroq(msg));
            }
            msg = "";
        } else if (c != '\r') {
            msg += c;
            if (msg.length() > 400) msg = msg.substring(msg.length() - 400);
        }
    }
}
