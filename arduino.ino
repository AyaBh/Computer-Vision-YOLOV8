#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include <ArduinoJson.h>

// Paramètres de connexion WiFi
//const char* ssid = "Livebox-5AC0";
//const char* password = "G9jHZ56Gu3RDhuTpDM";

const char* ssid = "ESGI";
const char* password = "Reseau-GES";

//const char* ssid = "Livebox-5AC0";
//const char* password = "G9jHZ56Gu3RDhuTpDM";

// Définition des GPIO pour la caméra ESP32-CAM AI Thinker
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Définition du pin pour le flash LED et le buzzer
#define FLASH_LED_PIN 4
#define BUZZER_PIN 2

bool LED_Flash_ON = true;

// URL de base de l'API Gateway AWS pour uploader l'image vers S3 et obtenir la prédiction
String awsS3BaseUrl = "https://i3ox9apa02.execute-api.eu-west-3.amazonaws.com/dev/letsfindsolution-fileupload/";
String predictionUrl = "http://10.33.5.217:5000/predict";  // URL de l'API pour obtenir la prédiction

// Fonction pour capturer une photo
camera_fb_t* CapturePhoto() {
  // Activer le flash LED si nécessaire
  if (LED_Flash_ON) {
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(10);  // Attendre un court instant pour laisser le flash s'activer
  }

  // Définition de la taille d'image à utiliser
  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_UXGA);  // Réglage de la résolution à UXGA
  s->set_quality(s, 10); // Réglage de la qualité JPEG à 10 (meilleure qualité)

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Échec de la capture de la photo");
    if (LED_Flash_ON) {
      digitalWrite(FLASH_LED_PIN, LOW);
    }
    return nullptr;
  }

  // Désactiver le flash LED après la capture de la photo
  if (LED_Flash_ON) {
    digitalWrite(FLASH_LED_PIN, LOW);
  }

  return fb;
}

// Fonction pour envoyer la photo vers S3 AWS avec une requête PUT
bool sendPhotoToAWS(camera_fb_t *fb) {
  HTTPClient http;

  // Vérifier la connexion WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi non connecté");
    return false;
  }

  // Génération d'un nom de fichier unique avec timestamp
  String fileName = "file_" + String(millis()) + ".jpg";
  String awsS3Url = awsS3BaseUrl + fileName;

  http.begin(awsS3Url);  // Début de la connexion HTTP avec l'URL AWS

  // En-têtes requis pour AWS
  http.addHeader("Content-Type", "image/jpeg");

  // Envoi des données de l'image
  int retries = 3;  // Nombre de tentatives de retraitement
  int httpResponseCode = -1;
  
  while (retries > 0) {
    httpResponseCode = http.PUT(fb->buf, fb->len);

    if (httpResponseCode > 0) {
      Serial.print("Photo envoyée avec succès vers AWS S3, code de réponse : ");
      Serial.println(httpResponseCode);
      break;
    } else {
      Serial.print("Échec de l'envoi de la photo vers AWS S3, code d'erreur : ");
      Serial.println(httpResponseCode);
      retries--;
      delay(2000);  // Attente avant de réessayer
    }
  }

  http.end();  // Fin de la connexion HTTP

  return httpResponseCode > 0;
}

// Fonction pour obtenir la prédiction de l'API
int getPrediction() {
  HTTPClient http;

  // Vérifier la connexion WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi non connecté");
    return -1;
  }

  http.begin(predictionUrl);  // Début de la connexion HTTP avec l'URL de prédiction

  int httpResponseCode = http.GET();  // Envoi de la requête GET
  if (httpResponseCode > 0) {
    String response = http.getString();  // Récupération de la réponse
    Serial.print("Réponse de l'API de prédiction : ");
    Serial.println(response);

    // Analyse de la réponse JSON pour obtenir la prédiction
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    int prediction = doc["prediction"];
    return prediction;
  } else {
    Serial.print("Erreur lors de l'appel de l'API de prédiction, code : ");
    Serial.println(httpResponseCode);
    return -1;
  }

  http.end();  // Fin de la connexion HTTP
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  delay(100);

  pinMode(FLASH_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  WiFi.mode(WIFI_STA);

  Serial.print("Connexion à : ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int timeout = 20 * 4; // Timeout de connexion en secondes
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(500);
    Serial.print(".");
    timeout--;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Échec de connexion au WiFi");
    ESP.restart();
  }

  Serial.println();
  Serial.print("Connecté à ");
  Serial.println(ssid);
  Serial.print("Adresse IP : ");
  Serial.println(WiFi.localIP());

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // pour le streaming
  //config.pixel_format = PIXFORMAT_RGB565; // pour la détection/reconnaissance de visage
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Échec d'initialisation de la caméra avec erreur 0x%x", err);
    Serial.println();
    ESP.restart();
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // augmenter légèrement la luminosité
    s->set_saturation(s, -2);  // diminuer la saturation
  }

  // Réglages supplémentaires pour améliorer la qualité de l'image
  s->set_contrast(s, 2);       // Augmenter le contraste
  s->set_sharpness(s, 2);      // Augmenter la netteté
  s->set_denoise(s, 1);        // Activer la réduction du bruit

  Serial.println("Configuration de la caméra terminée.");
}

void loop() {
  camera_fb_t* fb = CapturePhoto();
  if (fb) {
    Serial.println("Photo capturée avec succès.");
    if (sendPhotoToAWS(fb)) {  // Envoi de la photo capturée vers AWS S3
      Serial.println("Photo envoyée avec succès.");

      // Obtention de la prédiction
      int prediction = getPrediction();
      if (prediction == 0) {
        Serial.println("Prédiction négative, aucun son de buzzer.");  // Tonalité de 1000 Hz
      } else {
        Serial.println("Prédiction positive, aucun son de buzzer.");
      }
    } else {
      Serial.println("Échec de l'envoi de la photo.");
    }
    esp_camera_fb_return(fb);  // Libération de la mémoire utilisée par la photo
  } else {
    Serial.println("Échec de la capture de la photo.");
  }

  // Redémarrage de l'Arduino après chaque envoi
  Serial.println("Redémarrage de l'Arduino...");
  ESP.restart();
}