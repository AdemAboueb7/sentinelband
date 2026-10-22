// =====================================================================
// SENTINELBAND - CODE COMPLET DE TEST
// ESP32 + MPU6050 + GPS reel + SIM800L + buzzer + LED RGB + bouton
// Temperature et gaz constants : aucun LM35 ni MQ-2 utilise
// Version avec DETECTION EXPLICITE DE LA CARTE SIM
// =====================================================================

#include <Arduino.h>
#include <Wire.h>
#include <TinyGPSPlus.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ============================ BROCHES ================================
const int PIN_SDA    = 21;
const int PIN_SCL    = 22;
const int PIN_GPS_RX = 16; // ESP32 RX <- TX du GPS
const int PIN_GPS_TX = 17; // ESP32 TX -> RX du GPS
const int PIN_BUZZER = 4;
const int PIN_LED_R  = 25;
const int PIN_LED_G  = 26;
const int PIN_LED_B  = 27;
const int PIN_BUTTON = 33;

// ========================= SIM800L (GSM / SMS) =======================
#define ENABLE_SIM800L       1
const int   PIN_SIM_RX       = 18; // ESP32 RX1 <- TXD du SIM800L
const int   PIN_SIM_TX       = 19; // ESP32 TX1 -> RXD du SIM800L

// Numero du maitre de travail / responsable de securite (format international)
const char* NUMERO_MAITRE_TRAVAIL = "+21627794412";

// Delai entre deux tentatives automatiques d envoi SMS en alarme rouge
const uint32_t SMS_RETRY_MS = 30000;

// ============================ WIFI/API ===============================
// 1 : transmettre la telemetrie a l'API et a l'application mobile.
// 0 : fonctionner hors-ligne sur le moniteur serie uniquement.
#define ENABLE_UPLINK 1
#define WIFI_SSID     "Adm"
#define WIFI_PASS     "Admm@@@@"
#define API_URL       "http://10.111.172.169:3000/api/telemetry"

// Cle API du boitier (compte demo "Adem" par defaut).
#define DEVICE_KEY    "adem_dev_key_0123456789abcdef01234567"

// Repli GPS : false = le boitier n envoie JAMAIS de fausses coordonnees.
const bool   TEST_INDOOR_GPS_COORDS = false;
const double TEST_LAT               = 0.0;
const double TEST_LON               = 0.0;

// ========================== OPTIONS ==================================
// true  : patte commune de la LED RGB sur GND
// false : patte commune de la LED RGB sur 3V3
const bool LED_RGB_CATHODE_COMMUNE = true;

// AD0 du MPU6050 a GND = 0x68 ; AD0 a 3V3 = 0x69
const uint8_t MPU_ADDR = 0x68;

// ==================== VALEURS CONSTANTES =============================
const float TEMP_CONSTANTE_C = 25.0f;
const int   GAZ_CONSTANT_MV  = 150;

// ===================== SEUILS DE DETECTION ===========================
const float TEMP_JAUNE     = 45.0f;
const float TEMP_ROUGE     = 60.0f;

const int GAZ_BASE_MV       = 150;
const int GAZ_JAUNE_DELTA   = 300;
const int GAZ_ROUGE_DELTA   = 800;

// Delais de demonstration
const uint32_t IMMOBILE_MS          = 20000;
const uint32_t STILL_AFTER_FALL_MS  = 5000;
const uint32_t YELLOW_TIMEOUT_MS    = 15000;
const uint32_t CANCEL_HOLDOFF_MS    = 15000;

// ============= FALL PROFILE CLASSIFIER (FSM) =========================
// Phase 1 - Perte d equilibre : agitation soudaine avant chute
// Phase 2 - Apesanteur (free-fall)
// Phase 3 - Impact
// Phase 4 - Posture post-chute

// Seuils phase 2 - apesanteur
const float    FREEFALL_G_MAX      = 0.55f;
const uint32_t FREEFALL_MIN_MS     = 80;
const uint32_t FREEFALL_MAX_MS     = 1500;

// Seuils phase 3 - impact
const float    IMPACT_G            = 2.3f;
const uint32_t IMPACT_MAX_AFTER_MS = 800;

// Seuils phase 1 - agitation pre-chute
const float    PREFALL_GYRO_DPS    = 80.0f;
const uint32_t PREFALL_WINDOW_MS   = 500;

// Phase 4 - posture post-chute
const float    POSTURE_ANGLE_DEG   = 50.0f;
const uint32_t STILL_G_MS          = STILL_AFTER_FALL_MS;
const float    STILL_GYRO_DPS      = 20.0f;
const float    STILL_G_BAND        = 0.15f;

// Score minimal pour declarer une vraie chute
const int      FALL_MIN_CONFIDENCE = 60;

// ============================ OBJETS =================================
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;
HardwareSerial simSerial(1);
bool simOk = false;
bool smsSentForCurrentAlarm = false;
uint32_t lastSmsTry = 0;

// Etat de la carte SIM (detection explicite)
enum SimCardStatus {
  SIMCARD_UNKNOWN,    // pas encore teste / reponse inconnue
  SIMCARD_NO_MODULE,  // le module SIM800L ne repond pas
  SIMCARD_ABSENT,     // aucune carte SIM detectee
  SIMCARD_PIN,        // SIM presente, code PIN actif
  SIMCARD_PUK,        // SIM presente, bloquee (PUK)
  SIMCARD_READY       // SIM presente et prete
};
SimCardStatus simCardStatus = SIMCARD_UNKNOWN;
String simIccid = "";

enum State { NORMAL, YELLOW, RED };
State state = NORMAL;

// ===================== PROTOTYPES (ordre de definition) ==============
void enterState(State newState, const char* why);
void printStatus();
void resetFallFSM();
bool sendEmergencySMS(const char* customMsg = nullptr);

// =========================== VARIABLES ===============================
float ax = 0, ay = 0, az = 0;
float gx = 0, gy = 0, gz = 0;
float accG = 1.0f;
float gyroDps = 0.0f;
bool imuOk = false;

uint32_t stillSince = 0;
uint32_t yellowAt = 0;
uint32_t cancelUntil = 0;
uint32_t lastImu = 0;
uint32_t lastLog = 0;
uint32_t lastButton = 0;

bool simulatedFall = false;
String reason = "";
volatile bool telemetryNow = false;

// -------- Fall Profile FSM --------
enum FallPhase {
  FP_IDLE,
  FP_PREFALL,
  FP_FREEFALL,
  FP_IMPACT,
  FP_CONFIRMED
};
FallPhase fallPhase = FP_IDLE;

uint32_t preFallAt   = 0;
uint32_t freeFallAt  = 0;
uint32_t freeFallEnd = 0;
uint32_t impactAt    = 0;
float    impactPeak  = 0;
float    freeFallMin = 9;
uint32_t postureStartAt = 0;

int    scoreFF  = 0;
int    scoreImp = 0;
int    scorePos = 0;
int    fallConfidence = 0;
float  postureAngleDeg = 0;
bool   fallArmed = false;

uint32_t gpsBytes = 0;
uint32_t lastGpsByte = 0;

// ============================ OUTILS =================================
const char* stateName(State s) {
  if (s == NORMAL) return "NORMAL";
  if (s == YELLOW) return "JAUNE";
  return "ROUGE";
}

const char* simCardName(SimCardStatus s) {
  switch (s) {
    case SIMCARD_NO_MODULE: return "MODULE_KO";
    case SIMCARD_ABSENT:    return "ABSENTE";
    case SIMCARD_PIN:       return "PIN_ACTIF";
    case SIMCARD_PUK:       return "PUK_BLOQUEE";
    case SIMCARD_READY:     return "PRESENTE_PRETE";
    default:                return "INCONNU";
  }
}

void logLine(const char* message) {
  Serial.print("[T+");
  Serial.print(millis() / 1000);
  Serial.print("s] ");
  Serial.println(message);
}

void logState(const char* message) {
  Serial.print("[T+");
  Serial.print(millis() / 1000);
  Serial.print("s] ETAT -> ");
  Serial.println(message);
}

// ============================ LED RGB ================================
void setLedRaw(bool r, bool g, bool b) {
  if (LED_RGB_CATHODE_COMMUNE) {
    digitalWrite(PIN_LED_R, r ? HIGH : LOW);
    digitalWrite(PIN_LED_G, g ? HIGH : LOW);
    digitalWrite(PIN_LED_B, b ? HIGH : LOW);
  } else {
    digitalWrite(PIN_LED_R, r ? LOW : HIGH);
    digitalWrite(PIN_LED_G, g ? LOW : HIGH);
    digitalWrite(PIN_LED_B, b ? LOW : HIGH);
  }
}

void updateLed() {
  if (state == NORMAL) {
    setLedRaw(false, true, false);       // vert
  } else if (state == YELLOW) {
    setLedRaw(true, true, false);        // jaune
  } else {
    setLedRaw(true, false, false);       // rouge
  }
}

// ============================ BUZZER ================================
void updateBuzzer() {
  uint32_t t = millis();
  bool on = false;

  if (state == YELLOW) on = ((t % 1000) < 150);
  if (state == RED)    on = (((t / 250) % 2) == 0);

  digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
}

// ============================ MPU6050 ================================
bool imuInit() {
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);
  Wire.write(0x10); // +/-8 g
  if (Wire.endTransmission() != 0) return false;

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1B);
  Wire.write(0x00); // +/-250 deg/s
  if (Wire.endTransmission() != 0) return false;

  return true;
}

bool imuRead() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)14) != 14) return false;

  int16_t v[7];
  for (int i = 0; i < 7; i++) {
    uint8_t h = Wire.read();
    uint8_t l = Wire.read();
    v[i] = (int16_t)((h << 8) | l);
  }

  ax = v[0] / 4096.0f;
  ay = v[1] / 4096.0f;
  az = v[2] / 4096.0f;
  gx = v[4] / 131.0f;
  gy = v[5] / 131.0f;
  gz = v[6] / 131.0f;

  accG = sqrtf(ax * ax + ay * ay + az * az);
  gyroDps = sqrtf(gx * gx + gy * gy + gz * gz);
  return true;
}

// ============================== GPS ==================================
void readGps() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
    gpsBytes++;
    lastGpsByte = millis();
  }
}

// ============================ SIM800L ================================
// Envoie une commande AT et s arrete des que "OK" ou "ERROR" est recu.
String simSendCommand(const String& cmd, uint32_t timeoutMs = 1500) {
  while (simSerial.available()) simSerial.read();
  simSerial.println(cmd);
  uint32_t start = millis();
  String resp = "";
  while (millis() - start < timeoutMs) {
    while (simSerial.available()) resp += (char)simSerial.read();
    if (resp.indexOf("OK\r\n") != -1 || resp.indexOf("ERROR") != -1) break;
    readGps(); // ne pas perdre les trames GPS pendant l attente
  }
  return resp;
}

// Nettoie une reponse AT pour l afficher sur une seule ligne
String simClean(String s) {
  s.replace("\r", " ");
  s.replace("\n", " ");
  s.trim();
  return s;
}

// ---------------------------------------------------------------------
// DETECTION EXPLICITE DE LA CARTE SIM
// Interroge AT+CPIN? et classe la reponse. Necessite AT+CMEE=2 pour
// obtenir les messages textuels (ex: "+CME ERROR: SIM not inserted").
// ---------------------------------------------------------------------
SimCardStatus simDetectCard() {
  String r = simSendCommand("AT+CPIN?", 2000);
  String c = simClean(r);

  if (c.length() == 0) return SIMCARD_NO_MODULE;

  if (c.indexOf("READY") != -1)                                   return SIMCARD_READY;
  if (c.indexOf("SIM PUK") != -1)                                 return SIMCARD_PUK;
  if (c.indexOf("SIM PIN") != -1)                                 return SIMCARD_PIN;
  if (c.indexOf("not inserted") != -1 ||
      c.indexOf("CME ERROR: 10") != -1 ||
      c.indexOf("SIM failure") != -1 ||
      c.indexOf("CME ERROR: 13") != -1)                           return SIMCARD_ABSENT;

  return SIMCARD_UNKNOWN;
}

// Lit l ICCID (numero de la puce, ~20 chiffres). Retourne "" si illisible.
String simReadIccid() {
  String r = simSendCommand("AT+CCID", 1500);
  String digits = "";
  for (unsigned int i = 0; i < r.length(); i++) {
    char ch = r[i];
    if ((ch >= '0' && ch <= '9') || ch == 'F' || ch == 'f') digits += ch;
  }
  // Un ICCID valide contient 18 a 22 caracteres
  if (digits.length() >= 18 && digits.length() <= 22) return digits;
  return "";
}

// Affiche l etat de la SIM de facon claire dans le moniteur serie
void simReportCard(SimCardStatus s) {
  switch (s) {
    case SIMCARD_READY:
      logLine("[SIM800L] Carte SIM DETECTEE et prete");
      break;
    case SIMCARD_PIN:
      logLine("[SIM800L] Carte SIM DETECTEE mais code PIN actif (desactivez-le dans un telephone)");
      break;
    case SIMCARD_PUK:
      logLine("[SIM800L] Carte SIM DETECTEE mais BLOQUEE (PUK requis)");
      break;
    case SIMCARD_ABSENT:
      logLine("[SIM800L] AUCUNE CARTE SIM DETECTEE (absente, mal inseree ou contacts sales)");
      break;
    case SIMCARD_NO_MODULE:
      logLine("[SIM800L] Module sans reponse sur CPIN (alimentation ou cablage)");
      break;
    default:
      logLine("[SIM800L] Etat de la carte SIM inconnu");
      break;
  }
}

// Attend l enregistrement reseau Ooredoo : +CREG: 0,1 (local) ou 0,5 (roaming)
bool simWaitNetwork(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    String r = simSendCommand("AT+CREG?", 800);
    if (r.indexOf("+CREG: 0,1") != -1 || r.indexOf("+CREG: 0,5") != -1 ||
        r.indexOf("+CREG: 1,1") != -1 || r.indexOf("+CREG: 1,5") != -1) {
      return true;
    }
    uint32_t t = millis();
    while (millis() - t < 1000) readGps();
  }
  return false;
}

bool simInit() {
  simSerial.begin(9600, SERIAL_8N1, PIN_SIM_RX, PIN_SIM_TX);
  delay(1000);

  // Plusieurs essais AT (le SIM800L synchronise son debit au premier AT)
  bool alive = false;
  for (int i = 0; i < 5; i++) {
    if (simSendCommand("AT", 800).indexOf("OK") != -1) { alive = true; break; }
    delay(300);
  }
  if (!alive) {
    simCardStatus = SIMCARD_NO_MODULE;
    logLine("[SIM800L] Pas de reponse (verifiez batterie 18650, GND commun et cablage)");
    return false;
  }
  logLine("[SIM800L] Module detecte (repond aux commandes AT)");

  simSendCommand("ATE0", 500);              // Echo off
  simSendCommand("AT+CMEE=2", 500);         // Erreurs detaillees (texte)

  // --- Detection de la carte SIM (3 essais : la SIM peut mettre
  //     quelques secondes a etre lue apres la mise sous tension) ---
  simCardStatus = SIMCARD_UNKNOWN;
  for (int i = 0; i < 3; i++) {
    simCardStatus = simDetectCard();
    if (simCardStatus == SIMCARD_READY || simCardStatus == SIMCARD_PIN ||
        simCardStatus == SIMCARD_PUK) break;
    delay(1500);
  }
  simReportCard(simCardStatus);

  if (simCardStatus != SIMCARD_READY) return false;

  // --- Lecture de l ICCID pour confirmer que la puce est lue ---
  simIccid = simReadIccid();
  if (simIccid.length() > 0) {
    Serial.printf("[SIM800L] ICCID de la carte : %s\n", simIccid.c_str());
  } else {
    logLine("[SIM800L] ICCID illisible (carte SIM possiblement defectueuse)");
  }

  simSendCommand("AT+COPS=0", 3000);        // Selection automatique de l operateur

  logLine("[SIM800L] Attente enregistrement reseau Ooredoo (2G)...");
  if (!simWaitNetwork(60000)) {
    logLine("[SIM800L] SIM OK mais pas de reseau (antenne, couverture 2G ou alimentation)");
    return false;
  }

  simSendCommand("AT+CMGF=1", 500);          // Mode texte SMS
  simSendCommand("AT+CSCS=\"GSM\"", 500);    // Jeu caracteres GSM
  simSendCommand("AT+CNMI=0,0,0,0,0", 500);  // Pas de notifications entrantes
  simSendCommand("AT+CSMP=17,167,0,0", 500); // Parametres SMS standards
  logLine("[SIM800L] Reseau OK. Mode SMS d alerte pret.");
  return true;
}

// Une tentative d envoi SMS (retourne true si +CMGS recu)
bool sendSmsOnce(const String& msg) {
  while (simSerial.available()) simSerial.read();
  simSendCommand("AT+CMGF=1", 500);

  simSerial.print("AT+CMGS=\"");
  simSerial.print(NUMERO_MAITRE_TRAVAIL);
  simSerial.println("\"");

  uint32_t start = millis();
  bool promptReady = false;
  String simPromptResp = "";
  while (millis() - start < 5000) {
    if (simSerial.available()) {
      char c = simSerial.read();
      simPromptResp += c;
      if (c == '>') { promptReady = true; break; }
    }
    readGps();
  }

  if (!promptReady) {
    Serial.printf("[SIM800L] Echec prompt '>'. Reponse brute: [%s]\n", simPromptResp.c_str());
    if (simPromptResp.indexOf("CMS ERROR: 302") != -1) {
      Serial.println("[SIM800L] -> Cause : operation non permise (reseau pas pret).");
    } else if (simPromptResp.indexOf("SIM PIN") != -1 || simPromptResp.indexOf("CMS ERROR: 311") != -1) {
      Serial.println("[SIM800L] -> Cause : SIM verrouillee par PIN. Desactivez le PIN.");
    } else if (simPromptResp.indexOf("CMS ERROR: 310") != -1) {
      Serial.println("[SIM800L] -> Cause : SIM non detectee ou mal inseree.");
    } else if (simPromptResp.indexOf("CMS ERROR: 330") != -1) {
      Serial.println("[SIM800L] -> Cause : centre SMS inconnu (AT+CSCA?).");
    }
    simSerial.write(27); // ESC : annule la saisie en cours
    return false;
  }

  simSerial.print(msg);
  simSerial.write(26); // Ctrl+Z

  start = millis();
  String resp = "";
  while (millis() - start < 15000) {   // l envoi peut durer 10-15 s
    while (simSerial.available()) resp += (char)simSerial.read();
    if (resp.indexOf("+CMGS:") != -1 && resp.indexOf("OK") != -1) return true;
    if (resp.indexOf("ERROR") != -1) break;
    readGps();
  }
  Serial.printf("[SIM800L] Echec envoi SMS. Reponse : %s\n", resp.c_str());
  return false;
}

bool sendEmergencySMS(const char* customMsg) {
  if (!ENABLE_SIM800L) return false;

  logLine("[SIM800L] Envoi du SMS d urgence au maitre de travail...");

  // Si l init a echoue au demarrage, on retente
  if (!simOk) simOk = simInit();
  if (!simOk) return false;

  bool fix = gps.location.isValid();
  double lat = fix ? gps.location.lat() : (TEST_INDOOR_GPS_COORDS ? TEST_LAT : 0.0);
  double lon = fix ? gps.location.lng() : (TEST_INDOOR_GPS_COORDS ? TEST_LON : 0.0);
  bool hasCoords = fix || TEST_INDOOR_GPS_COORDS;

  String msg = "";
  if (customMsg != nullptr) {
    msg = customMsg;
  } else {
    msg += "ALERTE SENTINELBAND !\n";
    msg += "Travailleur: Adem\n";
    msg += "Etat: DANGER ROUGE\n";
    msg += "Motif: " + reason + "\n";
    msg += "Acc: " + String(accG, 2) + "g | Chute conf: " + String(fallConfidence) + "%\n";
    if (hasCoords) {
      msg += "Position:\nhttps://maps.google.com/?q=" + String(lat, 6) + "," + String(lon, 6) + "\n";
    } else {
      msg += "Position: GPS en attente de fix satellite\n";
    }
    msg += "Intervention requise !";
  }

  // Verifier le reseau avant l envoi
  if (!simWaitNetwork(10000)) {
    logLine("[SIM800L] Reseau indisponible");
    return false;
  }

  // 3 tentatives
  for (int essai = 1; essai <= 3; essai++) {
    Serial.printf("[SIM800L] Tentative %d/3\n", essai);
    if (sendSmsOnce(msg)) {
      logLine("[SIM800L] SMS D URGENCE ENVOYE AVEC SUCCES AU MAITRE DE TRAVAIL !");
      return true;
    }
    uint32_t t = millis();
    while (millis() - t < 2000) readGps();
  }
  logLine("[SIM800L] ECHEC DEFINITIF apres 3 tentatives");
  return false;
}

void printSimDiagnostic() {
  simSendCommand("AT+CMEE=2", 300);

  Serial.println("----- DIAGNOSTIC SIM800L / GSM -----");
  Serial.printf("1. Communication AT  : %s\n", simClean(simSendCommand("AT", 600)).c_str());
  Serial.printf("2. Lecture Puce ICCID: %s\n", simClean(simSendCommand("AT+CCID", 800)).c_str());
  Serial.printf("3. Etat Carte SIM    : %s\n", simClean(simSendCommand("AT+CPIN?", 1500)).c_str());
  Serial.printf("4. Signal GSM (CSQ)  : %s\n", simClean(simSendCommand("AT+CSQ", 600)).c_str());
  Serial.printf("5. Enregistrement GSM: %s\n", simClean(simSendCommand("AT+CREG?", 600)).c_str());
  Serial.printf("6. Operateur reseau  : %s\n", simClean(simSendCommand("AT+COPS?", 1200)).c_str());
  Serial.printf("7. Centre SMS (CSCA) : %s\n", simClean(simSendCommand("AT+CSCA?", 800)).c_str());

  // Verdict automatique sur la carte SIM
  simCardStatus = simDetectCard();
  Serial.printf("=> VERDICT CARTE SIM : %s\n", simCardName(simCardStatus));
  simReportCard(simCardStatus);
  Serial.println("------------------------------------");
}

// Detection rapide de la carte SIM (commande serie 'd')
void printSimCardDetection() {
  simSendCommand("AT+CMEE=2", 300);
  Serial.println("----- DETECTION CARTE SIM -----");
  bool moduleOk = simSendCommand("AT", 800).indexOf("OK") != -1;
  Serial.printf("Module SIM800L : %s\n", moduleOk ? "REPOND" : "NE REPOND PAS");
  if (!moduleOk) {
    simCardStatus = SIMCARD_NO_MODULE;
    Serial.println("-------------------------------");
    return;
  }
  simCardStatus = simDetectCard();
  Serial.printf("Carte SIM      : %s\n", simCardName(simCardStatus));
  simReportCard(simCardStatus);
  if (simCardStatus == SIMCARD_READY) {
    simIccid = simReadIccid();
    Serial.printf("ICCID          : %s\n", simIccid.length() ? simIccid.c_str() : "illisible");
  }
  Serial.println("-------------------------------");
}

// ============================ BOUTON =================================
void checkButton() {
  if (digitalRead(PIN_BUTTON) == LOW && millis() - lastButton > 300) {
    lastButton = millis();
    if (state != NORMAL) {
      state = NORMAL;
      reason = "";
      fallArmed = false;
      smsSentForCurrentAlarm = false; // Rearmer pour la prochaine alarme
      cancelUntil = millis() + CANCEL_HOLDOFF_MS;
      stillSince = millis();
      telemetryNow = true;
      logLine("ALERTE ANNULEE PAR LE BOUTON");
      updateLed();
    }
  }
}

// ========================= COMMANDES SERIE ===========================
// c/n = annuler alerte, f = impact/chute, j = forcer jaune, r = forcer rouge,
// t = telemetrie immediate, s = statut, g = diag GPS, m = SMS test,
// p = diag SIM complet, d = detection carte SIM
void checkSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'c' || c == 'n') {
      if (state != NORMAL) {
        state = NORMAL;
        reason = "";
        fallArmed = false;
        smsSentForCurrentAlarm = false; // Rearmer pour la prochaine alarme
        stillSince = millis();
        cancelUntil = millis() + CANCEL_HOLDOFF_MS;
        telemetryNow = true;
        logLine("ALERTE ANNULEE (RETOUR NORMAL)");
        updateLed();
      }
    } else if (c == 'f') {
      simulatedFall = true;
      logLine("COMMANDE SERIE : IMPACT SIMULE");
    } else if (c == 'j') {
      enterState(YELLOW, "TEST_MANUEL_JAUNE");
      logLine("COMMANDE SERIE : ETAT JAUNE FORCE");
    } else if (c == 'r') {
      enterState(RED, "TEST_MANUEL_ROUGE");
      logLine("COMMANDE SERIE : ETAT ROUGE FORCE");
    } else if (c == 't') {
      telemetryNow = true;
      logLine("COMMANDE SERIE : ENVOI TELEMETRIE FORCE");
    } else if (c == 's') {
      printStatus();
    } else if (c == 'm') {
      logLine("COMMANDE SERIE : ENVOI SMS TEST");
      sendEmergencySMS("TEST SENTINELBAND: Liaison GSM et reception SMS validee avec succes !");
    } else if (c == 'p') {
      printSimDiagnostic();
    } else if (c == 'd') {
      printSimCardDetection();
    } else if (c == 'A' && Serial.peek() == 'T') {
      String atCmd = "A" + Serial.readStringUntil('\n');
      atCmd.trim();
      Serial.printf(">> %s\n", atCmd.c_str());
      String resp = simSendCommand(atCmd, 1500);
      Serial.println(resp);
    } else if (c == 'g') {
      Serial.println("----- DIAGNOSTIC GPS REEL -----");
      Serial.printf("Satellites detectes : %d\n", (int)gps.satellites.value());
      Serial.printf("Location valide     : %s\n", gps.location.isValid() ? "OUI (FIX)" : "NON (NOFIX)");
      if (gps.location.isValid()) {
        Serial.printf("Latitude            : %.6f\n", gps.location.lat());
        Serial.printf("Longitude           : %.6f\n", gps.location.lng());
        Serial.printf("Age derniere mesure : %lu ms\n", (unsigned long)gps.location.age());
      }
      Serial.printf("Trames valides      : %lu\n", (unsigned long)gps.passedChecksum());
      Serial.printf("Trames corrompues   : %lu\n", (unsigned long)gps.failedChecksum());
      Serial.printf("Trames avec FIX     : %lu\n", (unsigned long)gps.sentencesWithFix());
      Serial.printf("HDOP (precision)    : %.1f\n", gps.hdop.hdop());
      Serial.println("-------------------------------");
    }
  }
}

// ========================== CHANGEMENT ETAT ==========================
void enterState(State newState, const char* why) {
  state = newState;
  reason = why;
  if (state == YELLOW) yellowAt = millis();
  telemetryNow = true;
  updateLed();
  char buffer[100];
  snprintf(buffer, sizeof(buffer), "%s (%s)", stateName(state), why);
  logState(buffer);

  // DECLENCHEMENT AUTOMATIQUE SMS EN ETAT D ALARME ROUGE
  // Si l envoi echoue, le flag reste a false : evaluateState() retentera.
  if (state == RED && !smsSentForCurrentAlarm) {
    lastSmsTry = millis();
    smsSentForCurrentAlarm = sendEmergencySMS();
  }
}

// ============================== LOGIQUE ==============================
void evaluateState() {
  uint32_t now = millis();

  // Relance automatique du SMS si l envoi precedent a echoue
  if (state == RED && !smsSentForCurrentAlarm && now - lastSmsTry > SMS_RETRY_MS) {
    lastSmsTry = now;
    smsSentForCurrentAlarm = sendEmergencySMS();
    now = millis();
  }

  bool fall = fallArmed && (now - stillSince >= STILL_AFTER_FALL_MS);
  bool immobile = (now - stillSince >= IMMOBILE_MS);
  bool tempYellow = TEMP_CONSTANTE_C >= TEMP_JAUNE;
  bool tempRed = TEMP_CONSTANTE_C >= TEMP_ROUGE;
  int gasDelta = GAZ_CONSTANT_MV - GAZ_BASE_MV;
  bool gasYellow = gasDelta >= GAZ_JAUNE_DELTA;
  bool gasRed = gasDelta >= GAZ_ROUGE_DELTA;
  bool anyYellow = fall || immobile || tempYellow || gasYellow;

  const char* why;
  if (fall) {
    char buf[50];
    snprintf(buf, sizeof(buf), "CHUTE+IMMOBILITE(conf=%d%%)", fallConfidence);
    reason = buf;
    why = reason.c_str();
  } else {
    why = immobile ? "IMMOBILITE" : (gasYellow ? "GAZ" : "TEMPERATURE");
  }

  if (state == NORMAL) {
    if (tempRed || gasRed) {
      enterState(RED, tempRed ? "TEMP_CRITIQUE" : "GAZ_CRITIQUE");
    } else if (now >= cancelUntil && anyYellow) {
      enterState(YELLOW, why);
    }
  } else if (state == YELLOW) {
    if (tempRed || gasRed) {
      enterState(RED, tempRed ? "TEMP_CRITIQUE" : "GAZ_CRITIQUE");
    } else if (!anyYellow) {
      state = NORMAL;
      reason = "";
      resetFallFSM();
      telemetryNow = true;
      updateLed();
      logLine("CONDITIONS NORMALES RESTAUREES");
    } else if (now - yellowAt >= YELLOW_TIMEOUT_MS) {
      enterState(RED, "NON_CONFIRME");
    }
  }
}

// ======================== FALL PROFILE FSM ===========================
float computePostureAngle() {
  float mag = accG;
  if (mag < 0.1f) return 90.0f;
  float cosA = az / mag;
  cosA = constrain(cosA, -1.0f, 1.0f);
  return degrees(acosf(cosA));
}

void resetFallFSM() {
  fallPhase    = FP_IDLE;
  fallArmed    = false;
  freeFallMin  = 9.0f;
  impactPeak   = 0.0f;
  scoreFF = scoreImp = scorePos = fallConfidence = 0;
}

void runFallFSM() {
  uint32_t now = millis();

  // --- Simulation serie (commande 'f') ---
  if (simulatedFall) {
    simulatedFall = false;
    fallPhase      = FP_CONFIRMED;
    fallArmed      = true;
    fallConfidence = 95;
    postureAngleDeg = 85.0f;
    stillSince     = now;
    impactAt       = now;
    logLine("[FSM] CHUTE SIMULEE (commande serie 'f') - Confiance 95%");
    return;
  }

  switch (fallPhase) {

    case FP_IDLE:
      if (gyroDps > PREFALL_GYRO_DPS) {
        fallPhase = FP_PREFALL;
        preFallAt = now;
        freeFallMin = accG;
      }
      if (accG < FREEFALL_G_MAX) {
        fallPhase  = FP_FREEFALL;
        freeFallAt = now;
        freeFallMin = accG;
        scoreFF = 0;
      }
      break;

    case FP_PREFALL:
      if (accG < FREEFALL_G_MAX) {
        fallPhase  = FP_FREEFALL;
        freeFallAt = now;
        freeFallMin = accG;
        scoreFF = 0;
      } else if (now - preFallAt > PREFALL_WINDOW_MS) {
        resetFallFSM();
      }
      break;

    case FP_FREEFALL:
      if (accG < freeFallMin) freeFallMin = accG;

      if (accG >= FREEFALL_G_MAX) {
        freeFallEnd = now;
        uint32_t dur = freeFallEnd - freeFallAt;

        if (dur < FREEFALL_MIN_MS) {
          resetFallFSM();
          break;
        }
        if (dur > FREEFALL_MAX_MS) {
          resetFallFSM();
          break;
        }

        int durScore = 0;
        if (dur >= 80  && dur < 200)  durScore = 60;
        if (dur >= 200 && dur < 600)  durScore = 100;
        if (dur >= 600 && dur < 1500) durScore = 70;
        int gScore = (freeFallMin < 0.2f) ? 100 : (freeFallMin < 0.4f) ? 75 : 50;
        scoreFF = (durScore + gScore) / 2;

        fallPhase  = FP_IMPACT;
        impactPeak = 0.0f;
        char buf[80];
        snprintf(buf, sizeof(buf), "[FSM] Apesanteur %.0fms min=%.2fg score=%d",
                 (float)dur, freeFallMin, scoreFF);
        logLine(buf);
      } else if (now - freeFallAt > FREEFALL_MAX_MS) {
        resetFallFSM();
      }
      break;

    case FP_IMPACT:
      if (accG > impactPeak) impactPeak = accG;

      if (accG > IMPACT_G && impactPeak > IMPACT_G) {
        impactAt = now;
        uint32_t delay_ff = now - freeFallEnd;

        if (delay_ff > IMPACT_MAX_AFTER_MS) {
          resetFallFSM();
          break;
        }

        if (impactPeak < 2.3f)       scoreImp = 30;
        else if (impactPeak < 4.0f)  scoreImp = 80;
        else if (impactPeak < 7.0f)  scoreImp = 100;
        else                          scoreImp = 60;

        postureAngleDeg = computePostureAngle();
        postureStartAt = now;
        fallPhase = FP_CONFIRMED;
        stillSince = now;

        fallConfidence = (scoreFF * 4 + scoreImp * 6) / 10;

        char buf[100];
        snprintf(buf, sizeof(buf),
                 "[FSM] IMPACT %.2fg apres %lums apesanteur - conf provisoire %d%%",
                 impactPeak, (unsigned long)delay_ff, fallConfidence);
        logLine(buf);

        if (fallConfidence >= FALL_MIN_CONFIDENCE) {
          fallArmed = true;
          logLine("[FSM] Seuil confiance atteint -> ALERTE ACTIVEE");
        }
      } else if (now - freeFallEnd > IMPACT_MAX_AFTER_MS) {
        resetFallFSM();
      }
      break;

    case FP_CONFIRMED: {
      bool still = gyroDps < STILL_GYRO_DPS && fabsf(accG - 1.0f) < STILL_G_BAND;
      if (!still) {
        postureStartAt = now;
        postureAngleDeg = computePostureAngle();
      } else {
        float angle = computePostureAngle();
        postureAngleDeg = angle;
        if (angle > POSTURE_ANGLE_DEG) {
          scorePos = (int)constrain(((angle - POSTURE_ANGLE_DEG) / 40.0f) * 100, 0, 100);
        } else {
          scorePos = 0;
        }

        fallConfidence = (int)(scoreFF * 0.35f + scoreImp * 0.45f + scorePos * 0.20f);

        char buf[100];
        snprintf(buf, sizeof(buf),
                 "[FSM] Posture %.1fdeg scorePos=%d conf=%d%%",
                 angle, scorePos, fallConfidence);
        logLine(buf);

        if (fallConfidence >= FALL_MIN_CONFIDENCE) {
          fallArmed = true;
        }
      }

      if (postureAngleDeg < 30.0f && !still && now - impactAt > 10000) {
        logLine("[FSM] Personne relevee -> reset FSM");
        resetFallFSM();
        fallArmed = false;
      }
      break;
    }
  }
}

// ========================= LECTURE CAPTEURS ==========================
void readSensors() {
  uint32_t now = millis();

  if (now - lastImu >= 20) {  // 50 Hz
    lastImu = now;
    bool oldOk = imuOk;
    imuOk = imuRead();

    if (imuOk) {
      bool still = gyroDps < STILL_GYRO_DPS && fabsf(accG - 1.0f) < STILL_G_BAND;
      if (!still) stillSince = now;
      runFallFSM();
    } else if (oldOk) {
      logLine("MPU6050 PERDU");
      resetFallFSM();
    }
  }
}

// ============================= STATUS ================================
void printStatus() {
  Serial.print("[");
  Serial.print(millis() / 1000);
  Serial.print("s] ");
  Serial.print(stateName(state));
  Serial.printf(" acc=%.2fg gyro=%.1fdps T=%.1fC gaz=%dmV ",
                accG, gyroDps, TEMP_CONSTANTE_C, GAZ_CONSTANT_MV);

  if (gps.location.isValid()) {
    Serial.printf("GPS=FIX %.6f,%.6f sats=%d ",
                  gps.location.lat(), gps.location.lng(),
                  (int)gps.satellites.value());
  } else {
    Serial.printf("GPS=NOFIX sats=%d ", (int)gps.satellites.value());
  }

  Serial.printf("imu=%s led=%s gps_octets=%lu sim=%s carteSIM=%s\n",
                imuOk ? "OK" : "KO",
                stateName(state),
                (unsigned long)gpsBytes,
                simOk ? "OK" : "KO",
                simCardName(simCardStatus));
}

// ============================ WIFI/API ===============================
#if ENABLE_UPLINK
void wifiBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("[WIFI] Connexion a \"%s\" en cours", WIFI_SSID);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WIFI] Connecte ! IP ESP32 : %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI] Serveur cible API : %s\n", API_URL);
  } else {
    Serial.println("[WIFI] Pas encore connecte (reconnexion automatique en cours)...");
  }
}

void telemetryTick() {
  static uint32_t dernierEnvoi = 0;
  static uint32_t dernierEssaiWiFi = 0;
  uint32_t now = millis();
  uint32_t periode = (state == NORMAL) ? 4000 : 2000;

  if (WiFi.status() != WL_CONNECTED) {
    if (now - dernierEssaiWiFi >= 8000) {
      dernierEssaiWiFi = now;
      WiFi.reconnect();
      logLine("Wi-Fi non connecte : nouvelle tentative");
    }
    return;
  }

  if (!telemetryNow && now - dernierEnvoi < periode) return;
  telemetryNow = false;
  dernierEnvoi = now;

  bool fix = gps.location.isValid();
  double lat = fix ? gps.location.lat() : (TEST_INDOOR_GPS_COORDS ? TEST_LAT : 0.0);
  double lon = fix ? gps.location.lng() : (TEST_INDOOR_GPS_COORDS ? TEST_LON : 0.0);
  bool hasCoords = fix || TEST_INDOOR_GPS_COORDS;

  const char* fallPhaseStr;
  switch (fallPhase) {
    case FP_IDLE:      fallPhaseStr = "IDLE";      break;
    case FP_PREFALL:   fallPhaseStr = "PREFALL";   break;
    case FP_FREEFALL:  fallPhaseStr = "FREEFALL";  break;
    case FP_IMPACT:    fallPhaseStr = "IMPACT";    break;
    case FP_CONFIRMED: fallPhaseStr = "CONFIRMED"; break;
    default:           fallPhaseStr = "UNKNOWN";
  }

  String payload = "{";
  payload += "\"state\":\"" + String(stateName(state)) + "\",";
  payload += "\"reason\":\"" + reason + "\",";
  payload += "\"temp_c\":" + String(TEMP_CONSTANTE_C, 1) + ",";
  payload += "\"gas_mv\":" + String(GAZ_CONSTANT_MV) + ",";
  payload += "\"gas_delta_mv\":0,";
  payload += "\"acc_g\":" + String(accG, 2) + ",";
  payload += "\"gyro_dps\":" + String(gyroDps, 1) + ",";
  payload += "\"gps_fix\":" + String(fix ? "true" : "false") + ",";
  payload += "\"uptime_s\":" + String(millis() / 1000) + ",";
  payload += "\"fall_phase\":\"" + String(fallPhaseStr) + "\",";
  payload += "\"fall_confidence\":" + String(fallConfidence) + ",";
  payload += "\"posture_angle_deg\":" + String(postureAngleDeg, 1) + ",";
  payload += "\"source\":\"REEL\"";
  if (hasCoords) {
    payload += ",\"lat\":" + String(lat, 6);
    payload += ",\"lon\":" + String(lon, 6);
  }
  if (fix) {
    payload += ",\"satellites\":" + String(gps.satellites.value());
  }
  payload += "}";

  HTTPClient http;
  http.setConnectTimeout(2500);
  http.setTimeout(3000);

  if (!http.begin(API_URL)) {
    logLine("API : URL invalide ou connexion impossible");
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", DEVICE_KEY);
  readGps();
  int code = http.POST(payload);
  readGps();

  if (code >= 200 && code < 300) {
    Serial.printf("[API] Envoi OK HTTP %d (Etat: %s)\n", code, stateName(state));
  } else {
    Serial.printf("[API] Echec HTTP %d\n", code);
  }

  http.end();
}
#endif

// ================================ SETUP ===============================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  digitalWrite(PIN_BUZZER, LOW);
  setLedRaw(false, false, false);

  gpsSerial.setRxBufferSize(1024);
  gpsSerial.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  imuOk = imuInit();
  stillSince = millis();

#if ENABLE_SIM800L
  simOk = simInit();
#endif

#if ENABLE_UPLINK
  wifiBegin();
#endif

  Serial.println();
  Serial.println("============================================");
  Serial.println("SENTINELBAND - CONFIGURATION COMPLETE");
  Serial.println("MPU6050 + GPS + SIM800L + BUZZER + LED RGB + BOUTON");
  Serial.println("LM35 et MQ-2 supprimes : valeurs constantes");
  Serial.println("============================================");
  Serial.printf("MPU6050 : %s (adresse 0x%02X)\n", imuOk ? "OK" : "KO", MPU_ADDR);
  Serial.println("GPS : 9600 bauds, TX GPS -> GPIO16, RX GPS -> GPIO17");
  Serial.printf("Temperature constante : %.1f C\n", TEMP_CONSTANTE_C);
  Serial.printf("Gaz constant : %d mV\n", GAZ_CONSTANT_MV);
  Serial.printf("SIM800L GSM : %s (Destinataire: %s)\n", simOk ? "CONNECTE" : "NON PRET", NUMERO_MAITRE_TRAVAIL);
  Serial.printf("Carte SIM   : %s\n", simCardName(simCardStatus));
  if (simIccid.length() > 0) Serial.printf("ICCID       : %s\n", simIccid.c_str());
  Serial.println("Commandes serie : c/n=annuler, f=chute, j=jaune, r=rouge, t=telemetrie, s=status, g=diag GPS, m=SMS test, p=diag SIM/GSM, d=detection carte SIM, ATxxx=commande AT");
  Serial.printf("Fall FSM : FreeFall<%.2fg %d-%dms | Impact>%.1fg | Posture>%.0fdeg | MinConf=%d%%\n",
                FREEFALL_G_MAX, FREEFALL_MIN_MS, FREEFALL_MAX_MS,
                IMPACT_G, POSTURE_ANGLE_DEG, FALL_MIN_CONFIDENCE);
  Serial.printf("API Mobile : %s\n", ENABLE_UPLINK ? API_URL : "DESACTIVEE");
  Serial.println("============================================");

  state = NORMAL;
  updateLed();
  delay(150);
}

// ================================= LOOP ================================
void loop() {
  readGps();
  readSensors();
  checkButton();
  checkSerial();
  evaluateState();
  updateBuzzer();
  updateLed();
#if ENABLE_UPLINK
  telemetryTick();
#endif

  if (millis() - lastLog >= 1000) {
    lastLog = millis();
    printStatus();
  }
}
