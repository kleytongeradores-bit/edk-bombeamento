/*
 * EDK ELÉTRICA - Controle de Bombeamento (ESP32) + Supabase
 * 
 * Funcionalidades:
 * - API local (GET /api/status e POST /api/command) — continua funcionando
 * - Sincronização com Supabase:
 *     • Envia status periodicamente para a tabela "status"
 *     • Lê comandos pendentes da tabela "commands" e executa
 *
 * Bibliotecas necessárias:
 *   - ArduinoJson (6.x)
 *   - HTTPClient (já inclusa no ESP32)
 *
 * Configuração:
 *   1. Preencha Wi-Fi, SUPABASE_URL, SUPABASE_KEY e DEVICE_ID
 *   2. USE_AP_MODE = false para conectar na internet (necessário para Supabase)
 */

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ===================== CONFIGURAÇÃO =====================
const bool USE_AP_MODE = false;          // false = conecta na rede (necessário para Supabase)

const char* AP_SSID     = "EDK-Bombeamento";
const char* AP_PASSWORD = "12345678";

const char* STA_SSID     = "SUA_REDE_WIFI";
const char* STA_PASSWORD = "SUA_SENHA";

// Supabase
const char* SUPABASE_URL = "https://SEU_PROJETO.supabase.co";
const char* SUPABASE_KEY = "SUA_ANON_KEY_AQUI";
const char* DEVICE_ID    = "bomba-casa-01";   // mesmo ID usado no frontend

// Intervalos
const unsigned long INTERVALO_STATUS   = 10000;  // envia status a cada 10 s
const unsigned long INTERVALO_COMANDOS = 5000;   // verifica comandos a cada 5 s

// ===================== PINOS =====================
const int PIN_BOMBA      = 26;
const int PIN_LED_STATUS = 2;

const bool BOIA_ATIVA_EM_LOW = true;

const int PIN_B1 = 32;
const int PIN_B2 = 33;
const int PIN_B3 = 25;
const int PIN_B4 = 27;
const int PIN_B5 = 14;

// ===================== VARIÁVEIS =====================
WebServer server(80);

bool bombaLigada      = false;
bool modoAutomatico   = true;
bool alarmeAtivo      = false;
String mensagemAlarme = "";

unsigned long ultimoStatus   = 0;
unsigned long ultimoComandos = 0;
unsigned long ultimoComandoLocal = 0;
const unsigned long TEMPO_MIN_ENTRE_COMANDOS = 2000;

// ===================== FUNÇÕES AUXILIARES =====================
bool lerBoia(int pin) {
  bool estado = digitalRead(pin);
  if (BOIA_ATIVA_EM_LOW) estado = !estado;
  return estado;
}

int calcularNivelPercentual(bool b1, bool b2, bool b3, bool b4) {
  if (b4) return 100;
  if (b3) return 75;
  if (b2) return 50;
  if (b1) return 25;
  return 0;
}

void atualizarLogicaAutomatica() {
  if (!modoAutomatico) return;

  bool b1 = lerBoia(PIN_B1);
  bool b2 = lerBoia(PIN_B2);
  bool b3 = lerBoia(PIN_B3);
  bool b4 = lerBoia(PIN_B4);
  bool b5 = lerBoia(PIN_B5);

  bool cisternaComAgua = b5;
  bool caixaCheia      = b4;
  bool caixaBaixa      = !b1 && !b2 && !b3 && !b4;

  if (!cisternaComAgua) {
    alarmeAtivo = true;
    mensagemAlarme = "Sem água na cisterna";
    if (bombaLigada) {
      digitalWrite(PIN_BOMBA, LOW);
      bombaLigada = false;
    }
    return;
  }

  if (caixaCheia) {
    alarmeAtivo = false;
    mensagemAlarme = "";
    if (bombaLigada) {
      digitalWrite(PIN_BOMBA, LOW);
      bombaLigada = false;
    }
    return;
  }

  if (!caixaCheia && cisternaComAgua && (caixaBaixa || !b3)) {
    if (!bombaLigada) {
      digitalWrite(PIN_BOMBA, HIGH);
      bombaLigada = true;
    }
    alarmeAtivo = false;
    mensagemAlarme = "";
  }
}

// ===================== SUPABASE =====================
void enviarStatusSupabase() {
  if (WiFi.status() != WL_CONNECTED) return;

  bool b1 = lerBoia(PIN_B1);
  bool b2 = lerBoia(PIN_B2);
  bool b3 = lerBoia(PIN_B3);
  bool b4 = lerBoia(PIN_B4);
  bool b5 = lerBoia(PIN_B5);
  int nivel = calcularNivelPercentual(b1, b2, b3, b4);

  StaticJsonDocument<512> doc;
  doc["device_id"]     = DEVICE_ID;
  doc["pump_running"]  = bombaLigada;
  doc["mode"]          = modoAutomatico ? "AUTOMATICO" : "MANUAL";
  doc["level_percent"] = nivel;
  doc["b1"] = b1;
  doc["b2"] = b2;
  doc["b3"] = b3;
  doc["b4"] = b4;
  doc["b5"] = b5;
  doc["alarm_active"]  = alarmeAtivo;
  doc["alarm_message"] = mensagemAlarme;
  doc["updated_at"]    = "now()";   // Supabase aceita now() via Prefer

  String payload;
  serializeJson(doc, payload);

  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/status?on_conflict=device_id";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
  http.addHeader("Prefer", "resolution=merge-duplicates,return=minimal");

  int code = http.POST(payload);
  Serial.printf("[Supabase] Status enviado: %d\n", code);
  http.end();
}

void verificarComandosSupabase() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/commands?device_id=eq." + DEVICE_ID + "&executed=eq.false&order=created_at.asc&limit=5";
  http.begin(url);
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));

  int code = http.GET();
  if (code == 200) {
    String response = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject cmd : arr) {
      String id      = cmd["id"].as<String>();
      String command = cmd["command"].as<String>();
      String mode    = cmd["mode"] | "";

      Serial.println("[Supabase] Comando recebido: " + command);

      bool executado = false;

      if (command == "LIGAR" && !modoAutomatico) {
        if (lerBoia(PIN_B5)) {
          digitalWrite(PIN_BOMBA, HIGH);
          bombaLigada = true;
          executado = true;
        }
      } else if (command == "DESLIGAR") {
        digitalWrite(PIN_BOMBA, LOW);
        bombaLigada = false;
        executado = true;
      } else if (command == "MODO") {
        if (mode == "MANUAL") {
          modoAutomatico = false;
          executado = true;
        } else if (mode == "AUTOMATICO") {
          modoAutomatico = true;
          executado = true;
        }
      }

      // Marca como executado
      if (executado) {
        HTTPClient httpPatch;
        String patchUrl = String(SUPABASE_URL) + "/rest/v1/commands?id=eq." + id;
        httpPatch.begin(patchUrl);
        httpPatch.addHeader("Content-Type", "application/json");
        httpPatch.addHeader("apikey", SUPABASE_KEY);
        httpPatch.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
        httpPatch.addHeader("Prefer", "return=minimal");
        httpPatch.PATCH("{\"executed\":true}");
        httpPatch.end();
      }
    }
  }
  http.end();
}

// ===================== API LOCAL (mantida) =====================
void handleStatus() {
  bool b1 = lerBoia(PIN_B1);
  bool b2 = lerBoia(PIN_B2);
  bool b3 = lerBoia(PIN_B3);
  bool b4 = lerBoia(PIN_B4);
  bool b5 = lerBoia(PIN_B5);
  int nivel = calcularNivelPercentual(b1, b2, b3, b4);

  StaticJsonDocument<512> doc;
  doc["pumpRunning"] = bombaLigada;
  doc["mode"] = modoAutomatico ? "AUTOMATICO" : "MANUAL";
  doc["B1"] = b1; doc["B2"] = b2; doc["B3"] = b3; doc["B4"] = b4; doc["B5"] = b5;
  doc["levelPercent"] = nivel;
  doc["alarmActive"] = alarmeAtivo;
  if (alarmeAtivo) doc["alarmMessage"] = mensagemAlarme;

  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleCommand() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method not allowed");
    return;
  }
  if (millis() - ultimoComandoLocal < TEMPO_MIN_ENTRE_COMANDOS) {
    server.send(429, "text/plain", "Aguarde");
    return;
  }
  ultimoComandoLocal = millis();

  String body = server.arg("plain");
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, body)) {
    server.send(400, "text/plain", "JSON inválido");
    return;
  }

  String comando = doc["command"] | "";
  comando.toUpperCase();

  if (comando == "LIGAR") {
    if (modoAutomatico) { server.send(409, "text/plain", "Modo automático"); return; }
    if (!lerBoia(PIN_B5)) { server.send(409, "text/plain", "Sem água"); return; }
    digitalWrite(PIN_BOMBA, HIGH);
    bombaLigada = true;
  } else if (comando == "DESLIGAR") {
    digitalWrite(PIN_BOMBA, LOW);
    bombaLigada = false;
  } else if (comando == "MODO") {
    String m = doc["mode"] | "";
    m.toUpperCase();
    if (m == "MANUAL") modoAutomatico = false;
    else if (m == "AUTOMATICO") modoAutomatico = true;
  } else {
    server.send(400, "text/plain", "Comando desconhecido");
    return;
  }
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleRoot() {
  server.send(200, "text/html", "<h2>EDK Bombeamento online</h2><p>API local + Supabase ativos.</p>");
}

// ===================== SETUP / LOOP =====================
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_BOMBA, OUTPUT);
  digitalWrite(PIN_BOMBA, LOW);
  pinMode(PIN_LED_STATUS, OUTPUT);

  pinMode(PIN_B1, INPUT_PULLUP);
  pinMode(PIN_B2, INPUT_PULLUP);
  pinMode(PIN_B3, INPUT_PULLUP);
  pinMode(PIN_B4, INPUT_PULLUP);
  pinMode(PIN_B5, INPUT_PULLUP);

  if (USE_AP_MODE) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.println("AP: " + String(AP_SSID));
    Serial.println(WiFi.softAPIP());
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(STA_SSID, STA_PASSWORD);
    Serial.print("Conectando");
    int t = 0;
    while (WiFi.status() != WL_CONNECTED && t < 40) {
      delay(500);
      Serial.print(".");
      t++;
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("Falha Wi-Fi. Continuando sem nuvem.");
    }
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/command", HTTP_POST, handleCommand);
  server.begin();
  Serial.println("Servidor local iniciado");
}

void loop() {
  server.handleClient();
  atualizarLogicaAutomatica();

  unsigned long agora = millis();

  if (agora - ultimoStatus >= INTERVALO_STATUS) {
    ultimoStatus = agora;
    enviarStatusSupabase();
  }

  if (agora - ultimoComandos >= INTERVALO_COMANDOS) {
    ultimoComandos = agora;
    verificarComandosSupabase();
  }

  // LED de status
  static unsigned long tLed = 0;
  if (agora - tLed > 600) {
    tLed = agora;
    digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS));
  }
}
