#include <ESP8266WiFi.h> //biblioteka do podlaczenia ESP z WIFI
#include <PubSubClient.h> //biblioteka do komunikacji przez MQTT

const char* ssid = "JaSu-Linksys"; //Nazwa WIFI
const char* password = "$JaSu-Linksys"; //Haslo WIFI
const char* mqttServer = "192.168.0.97"; //Adres IP brokera MQTT
const int mqttPort = 1883; //Port na jaki sie laczymy w brokerze
const char* mqttUser = "ESP_przyciski"; //Nazwa uzytkownika MQTT  
const char* mqttPassword = "pracainzynierska"; //Haslo do brokera MQTT
const char* mqttsTopic_button = "homeassistant/button/state"; // state Topic przycisku
const char* mqttsTopic_czujnik = "homeassistant/binary_sensor/serwo/state"; //state Topic czujnika otwarcia/zamkniecia

int ledPin4 = 4; //Pin na jaki jest podpieta zielona dioda
int ledPin5 = 5; //Pin na jaki jest podpieta czerwona dioda

const int przycisk_ON = 12; //definicja przycisku otwierania okna
const int przycisk_OFF = 16; //definicja przycisku zamykania okna
int stan = 0; //definicja zmiennej uzytej jako wiadomosc wysylana przez MQTT
unsigned long lastDebounceTime_ON = 0; //definicja zmiennej zastosowanej do zniwelowania zjawiska debouncingu przycisku odpowiedzialnego za otwieranie okna
int previousState_ON = HIGH; //ustawienie defaultowego stanu zmiennej do monitorowania poprzedniego stanu przycisku ON 
unsigned long lastDebounceTime_OFF = 0; //definicja zmiennej zastosowanej do zniwelowania zjawiska debouncingu przycisku odpowiedzialnego za zamykanie okna
int previousState_OFF = HIGH; //ustawienie defaultowego stanu zmiennej do monitorowania poprzedniego stanu przycisku OFF
unsigned long debounceDelay = 200;

WiFiClient espClient_przycisk; //utworzenie klienta wifi
PubSubClient client(espClient_przycisk); //utworzenie klienta do komunikacji protokołem MQTT

void setup() {
  pinMode(przycisk_ON, INPUT_PULLUP); //zdefiniowanie przycisku ON jako wejscie typu PULLUP 
  pinMode(przycisk_OFF, INPUT_PULLUP);//zdefiniowanie przycisku OFF jako wejscie typu PULLUP 
  pinMode(ledPin4, OUTPUT); //zdefiniowanie diody zielonej jako wyjście
  digitalWrite(ledPin4, LOW); //ustawienie stanu poczatkowego diody zielonej
  pinMode(ledPin5, OUTPUT); //zdefiniowanie diody czerwonej jako wyjście
  digitalWrite(ledPin5, LOW); //ustawienie stanu poczatkowego diody czerwonej

  Serial.begin(115200); //Ustawienie predkosci komunikacji portu szeregowego(monitorowanie w Serial Monitor)
  setupWiFi(); //Wywołanie funkcji odpowiedzialnej za połączenie WiFi
  client.setServer(mqttServer, mqttPort); //Ustawienie danych serwera brokera MQTT(adres IP i port)
  client.setCallback(callback); //Ustawienie funkcji callback jako tej odpowiedzialnej za odbior wiadomosci z serwera
}

void setupWiFi() { //Funkcja odpowiedzialna za zestawienie połączenia WiFi
  Serial.println();
  Serial.print("Łączenie z siecią: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi połączono.");
  Serial.println("Adres IP: " + WiFi.localIP().toString());
  Serial.println();
  Serial.println("Device ID: " + WiFi.macAddress());
}

void callback(char* topic, byte* payload, unsigned int length) {//Funkcja odpowiedzialna za odbieranie wiadomosci z danego tematu MQTT oraz przetworzenie jej w odpowiedni sposob
  if (String(topic) == mqttsTopic_czujnik) {
    String message;
    for (int i = 0; i < length; i++) {
      message += (char)payload[i];
    }
    Serial.print("Otrzymano wiadomość z czujnika: ");
    Serial.println(message);
    if(message == "OPEN"){
      digitalWrite(ledPin4, HIGH);
      digitalWrite(ledPin5, LOW);
    }
    if(message == "CLOSE"){
      digitalWrite(ledPin4, LOW);
      digitalWrite(ledPin5, HIGH);
    }
  }
}

void reconnect() {//Funkcja do podlaczenia sie z serwerem mqtt
  while (!client.connected()) {
    Serial.print("Łączenie z MQTT...");
    if (client.connect("przyciski", mqttUser, mqttPassword)) {
      Serial.println("Połączono");

       // Publikuj informacje konfiguracyjne dla encji przy użyciu Autodiscovery
       client.publish("homeassistant/button/przyciski/config", 
                     "{\"name\":\"Przyciski\",\"state_topic\":\"homeassistant/button/state\",\"command_topic\":\"homeassistant/button/set\",\"device_class\":\"identify\",\"unique_id\":\"przyciski\"}");
                     

      client.subscribe(mqttsTopic_czujnik);
    } else {
      Serial.print("Nie udało się połączyć z MQTT, spróbuj ponownie za 5 sekund...");
      delay(5000);
    }
  }
}

void ReadPublishButton(){ //Funkcja do odczytywania stanu przycisku, niwelowania debouncingu oraz publikacji wiadomosci MQTT
  int buttonState_ON = digitalRead(przycisk_ON);
  int buttonState_OFF = digitalRead(przycisk_OFF);
  
  if (buttonState_ON == LOW && previousState_ON == HIGH && buttonState_OFF == HIGH && millis() - lastDebounceTime_ON > debounceDelay ) {
    stan = 1;
    client.publish(mqttsTopic_button, String(stan).c_str());
    delay(10);
    lastDebounceTime_ON = millis();
  }
  previousState_ON = buttonState_ON;
  
  if (buttonState_OFF == LOW && previousState_OFF == HIGH && buttonState_ON == HIGH && millis() - lastDebounceTime_OFF > debounceDelay ) {
    stan = 0;
    client.publish(mqttsTopic_button, String(stan).c_str());
    delay(10);
    lastDebounceTime_OFF = millis();
  }
  previousState_OFF = buttonState_OFF;
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  ReadPublishButton();
}
