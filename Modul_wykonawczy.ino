#include <Servo.h> //biblioteka do obslugi serwa
#include <DHT11.h> // biblioteka czujnika temp i wilg - dht11
#include <ESP8266WiFi.h> //biblioteka do podlaczenia ESP z WIFI
#include <PubSubClient.h> //biblioteka do komunikacji przez MQTT

const char* ssid = "JaSu-Linksys"; //Nazwa WIFI
const char* password = "$JaSu-Linksys"; //Haslo WIFI
const char* mqttServer = "192.168.0.97"; //Adres IP brokera MQTT
const int mqttPort = 1883; //Port na jaki sie laczymy w brokerze
const char* mqttUser = "ESP_serwo"; //Nazwa uzytkownika MQTT  
const char* mqttPassword = "pracainzynierska"; //Haslo do brokera MQTT
const char* mqttsTopic_button = "homeassistant/button/state"; //state Topic przycisku
const char* mqttcTopic_button = "homeassistant/button/set"; //command Topic przycisku
const char* mqttsTopic_czujnik_OC = "homeassistant/binary_sensor/serwo/state"; //state Topic czujnika otwarcia/zamkniecia
const char* mqttsTopic_TempSensor = "homeassistant/sensor/TempSensor/state"; //state Topic czujnika temperatury
unsigned long previousMillis = 0; //Czas ostatniego wyslania wiadomosci(uzyte podczas implementacji czujnika temp i wilg)
unsigned long updateDelay = 5000; // 5 sekundowa przerwa miedzy publikacja wiadomosci z czujnika do brokera
unsigned long currentMillis = millis(); //Definicja zmiennej ktora bedzie "zegarem" odliczajacym czas, za pomoca funkcji milis

Servo myservo; //Utworzenie objektu klasy Servo -> myservo
DHT11 dht(12); //Utowrzenie objektu klasy DHT11 -> dht ktory jest przypisany na pin 12

int servoPin = 13; //definicja pinu do ktorego bedzie podpiete serwo
int targetAngle = 0; //definicja zmiennej pomocniczej ktora bedzie wykorzystana do sterowania serwem przez wiadomosci z MQTT
int ledPin16 = 16; // Czerwona dioda
int ledPin14 = 14; // Zielona dioda

const int czujnik_otwarcia = 5; //def. krancowki - stan LOW jesli otwarte
const int czujnik_zamkniecia = 4; //def. krancowki - stan LOW jesli zamkniete

WiFiClient espClient_servo; //utworzenie klienta wifi
PubSubClient client(espClient_servo); //utworzenie klienta do komunikacji protokołem MQTT

void setup() {
  myservo.attach(servoPin,200,2100); //myservo.attach(servoPin,200,2300);
  myservo.write(180); //Pozycja startowa dla serwa, zawsze 180stopni po uruchomieniu systemu czyli zamkniete okno
  
  pinMode(czujnik_otwarcia, INPUT_PULLUP); //definicja krancowki jako wejscia PULLUP
  pinMode(czujnik_zamkniecia, INPUT_PULLUP); //definicja krancowki jako wejscia PULLUP

  pinMode(ledPin16, OUTPUT); //zdefiniowanie diody czerwonej jako wyjście
  digitalWrite(ledPin16, LOW); //ustawienie stanu poczatkowego diody czerwonej
  pinMode(ledPin14, OUTPUT); //zdefiniowanie diody zielonej jako wyjście
  digitalWrite(ledPin14, LOW); //ustawienie stanu poczatkowego diody zielonej

  Serial.begin(115200); //Ustawienie predkosci komunikacji portu szeregowego(monitorowanie w Serial Monitor)
  setupWiFi(); //Wywołanie funkcji odpowiedzialnej za połączenie WiFi
  client.setServer(mqttServer, mqttPort); //Ustawwienie danych serwera brokera MQTT(adres IP i port)
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
  if (String(topic) == mqttsTopic_button || String(topic) == mqttcTopic_button) {
    String message;
    for (int i = 0; i < length; i++) {
      message += (char)payload[i];
    }
    Serial.print("Otrzymano wiadomość na temat MQTT: ");
    Serial.println(message);

    if (message == "1") {
      targetAngle = 180;
      moveServo();
    }

    if (message == "0") {
      targetAngle = 0;
      moveServo();
    }
  }
}

void moveServo() { //Funkcja wykorzystywana do sterowania serwem na podstawie wiadomosci MQTT
int currentAngle = myservo.read(); //odczytuje ostatni zadany kat przez funkcje write
while (currentAngle != targetAngle) {
   
    if (targetAngle > currentAngle) {
      currentAngle = currentAngle + 2;
      delay(10);
    } else {
      currentAngle = currentAngle - 2;
      delay(10);
    }
      myservo.write(currentAngle);
      delay(10);

    if (digitalRead(czujnik_otwarcia) == LOW){
      digitalWrite(ledPin14, HIGH); // dioda zielona ON
      digitalWrite(ledPin16, LOW); // dioda czerwona OFF
      client.publish(mqttsTopic_czujnik_OC,"OPEN"); // Okno jest OTWARTE 
    }
    if (digitalRead(czujnik_zamkniecia) == LOW){ 
      digitalWrite(ledPin14, LOW); // dioda zielona OFF
      digitalWrite(ledPin16, HIGH); // dioda czerwona ON
      client.publish(mqttsTopic_czujnik_OC,"CLOSE"); // Okno jest ZAMKNIETE
    }
  }
}

void reconnect() {//Funkcja do podlaczenia sie z serwerem mqtt
  while (!client.connected()) {
    Serial.print("Łączenie z MQTT...");
    if (client.connect("serwo", mqttUser, mqttPassword)) {
      Serial.println("Połączono");
      publishBinarySensorConfig(); //Publikacja encji stanu otwarcia okna -> czy otwarte czy zamkniete
      publishTempSensor(); // Publikacja odczytu czujnika temp
      client.subscribe(mqttcTopic_button);
      client.subscribe(mqttsTopic_button);
      
      // Warunek sprawdzajacy czy podczas braku komunikacji zaszly jakies zmiany
      if (digitalRead(czujnik_otwarcia) == LOW){ 
      digitalWrite(ledPin14, HIGH); // dioda zielona ON
      digitalWrite(ledPin16, LOW); // dioda czerwona OFF
      client.publish(mqttsTopic_czujnik_OC,"OPEN"); // Okno jest OTWARTE 
      delay(50);
      }
      if (digitalRead(czujnik_zamkniecia) == LOW){ 
      digitalWrite(ledPin14, LOW); // dioda zielona OFF
      digitalWrite(ledPin16, HIGH); // dioda czerwona ON
      client.publish(mqttsTopic_czujnik_OC,"CLOSE"); // Okno jest ZAMKNIETE
      delay(50);
      }
    } else {
      Serial.print("Nie udało się połączyć z MQTT, spróbuj ponownie za 5 sekund...");
      delay(updateDelay);
    }
  }
}

//Fragment odpowiedzialny za definicje encji esp jako czujnik binarny -> sprawdza czy krancowki sa nacisniete czy nie i wysyla komunikat
void publishBinarySensorConfig() {
  String configPayload = "{\"name\":\"Czujnik O/C\",\"state_topic\":\"" + String(mqttsTopic_czujnik_OC) + "\",\"payload_on\":\"OPEN\",\"payload_off\":\"CLOSE\",\"unique_id\":\"Czujnik O/C\",\"device_class\":\"opening\"}";
  client.publish("homeassistant/binary_sensor/serwo/config", configPayload.c_str());
}
//Fragment odpowiedzialny za definicje encji esp jako sensor temperatury
void publishTempSensor() {
  String configPayload = "{\"name\":\"TempSensor\",\"state_topic\":\"" + String(mqttsTopic_TempSensor) + "\",\"unit_of_measurement\":\"°C\",\"value_template\":\"{{value_json.temperatura}}\",\"unique_id\":\"TempSensor\",\"device_class\":\"temperature\"}";
  client.publish("homeassistant/sensor/TempSensor/config", configPayload.c_str());
}
//Funkcje do publikowania aktualnej temperatury
void publishTemperature(int temperatura) {
  String payload = String("{\"temperatura\":") + temperatura + String("}");
  client.publish(mqttsTopic_TempSensor, payload.c_str());
}

//Fragment odpowiedzialny za pobieranie informacji z czujnika DHT11 i wyswietlanie temperatury i wilgotnosci
void TempHumSensor() {
  if(currentMillis - previousMillis >= updateDelay){
    previousMillis = currentMillis;

  //Pobranie informacji o wilgotnosci
  int wilgotnosc = dht.readHumidity();
  Serial.print(wilgotnosc);
  Serial.print("%RH | ");
  
  //Pobranie informacji o temperaturze
  int temperatura = dht.readTemperature();
  Serial.print(temperatura);
  Serial.println("*C");
  publishTemperature(temperatura);
  }
  yield();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  currentMillis = millis();
  TempHumSensor();
}