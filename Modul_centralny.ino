#include <LiquidCrystal.h> //Dołączenie bilbioteki
#include <ESP8266WiFi.h> //biblioteka do podlaczenia ESP z WIFI
#include <PubSubClient.h> //biblioteka do komunikacji przez MQTT



const char* ssid = "JaSu-Linksys"; //Nazwa WIFI
const char* password = "$JaSu-Linksys"; //Haslo WIFI
const char* mqttServer = "192.168.0.97"; //Adres IP brokera MQTT
const int mqttPort = 1883; //Port na jaki sie laczymy w brokerze
const char* mqttUser = "ESP_lcd"; //Nazwa uzytkownika MQTT  
const char* mqttPassword = "pracainzynierska"; //Haslo do brokera MQTT

const char* mqttsTopic_button = "homeassistant/button/state"; // state Topic przycisku
const char* mqttsTopic_czujnik_OC = "homeassistant/binary_sensor/serwo/state"; //state Topic czujnika otwarcia/zamkniecia
const char* mqttsTopic_TempSensor = "homeassistant/sensor/TempSensor/state"; //state Topic czujnika temperatury
int stan = 0; //definicja zmiennej uzytej jako wiadomosc wysylana przez MQTT
unsigned long lastDebounceTime_ON = 0; //definicja zmiennej zastosowanej do zniwelowania zjawiska debouncingu przycisku odpowiedzialnego za otwieranie okna
int previousState_ON = HIGH; //ustawienie defaultowego stanu zmiennej do monitorowania poprzedniego stanu przycisku ON 
unsigned long lastDebounceTime_OFF = 0; //definicja zmiennej zastosowanej do zniwelowania zjawiska debouncingu przycisku odpowiedzialnego za zamykanie okna
int previousState_OFF = HIGH; //ustawienie defaultowego stanu zmiennej do monitorowania poprzedniego stanu przycisku OFF
unsigned long debounceDelay = 200;


const int przycisk_ON = 5; //definicja przycisku otwierania okna
const int przycisk_OFF = 4; //definicja przycisku zamykania okna

LiquidCrystal lcd(2, 0, 16, 14, 13, 12); //Informacja o podłączeniu nowego wyświetlacza

WiFiClient espClient_lcd; //utworzenie klienta wifi
PubSubClient client(espClient_lcd); //utworzenie klienta do komunikacji protokołem MQTT

void setup() {
  Serial.begin(115200); //Ustawienie predkosci komunikacji portu szeregowego(monitorowanie w Serial Monitor)
  pinMode(przycisk_ON, INPUT_PULLUP); //zdefiniowanie przycisku ON jako wejscie typu PULLUP 
  pinMode(przycisk_OFF, INPUT_PULLUP); //zdefiniowanie przycisku OFF jako wejscie typu PULLUP 

  //Poczatkowe ustawienia wyswietlacza LCD
  lcd.clear(); //Wyczyszczenie poprzedniej zawartosci wyswietlacza LCD
  lcd.begin(16, 2); //Deklaracja typu wyswietlacza
  lcd.setCursor(0, 0); //Ustawienie kursora
  lcd.print("OKNO-->"); //Wyświetlenie tekstu
  lcd.setCursor(0, 1); //Ustawienie kursora
  lcd.print("TEMPERATURA:"); //Wyświetlenie tekstu
  lcd.setCursor(14, 1); //Ustawienie kursora
  lcd.print("*C"); //Wyświetlenie tekstu
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
  if (String(topic) == mqttsTopic_czujnik_OC) {
    String message_OC;
    for (int i = 0; i < length; i++) {
      message_OC += (char)payload[i];
    }
    Serial.print("Otrzymano wiadomość na temat MQTT: ");
    Serial.println(message_OC);

    if (message_OC == "OPEN") {
      const char* OC = "OTWARTE";
      printLCD_OC(OC);
    }
    if (message_OC == "CLOSE") {
      const char* OC = "ZAMKNIETE";
      printLCD_OC(OC);
    }
  }
  if (String(topic) == mqttsTopic_TempSensor) {
    String message_temp; // {"temperatura":25} tak wyglada cala wiadomosc
    for (int i = 15; i < length-1; i++) {
      message_temp += (char)payload[i];
    }

    Serial.print("Otrzymano wiadomość na temat MQTT: ");
    Serial.println(message_temp);
    lcd.setCursor(12, 1); 
    lcd.print("  "); //Czyszczenie wyswietlacza w 2 wierszu 13 i 14 kolumne
    lcd.setCursor(12, 1); 
    lcd.print(message_temp);
  }
}

void printLCD_OC(const char* OC){//Funkcja odpowiedzialna za czyszenie i wyświetlanie wiadomości na LCD
    lcd.setCursor(7, 0); 
    lcd.print("         "); //Czyszczenie wyswietlacza w 1 wierszu od 8 do 16 kolumny
    lcd.setCursor(7, 0);
    lcd.print(OC);
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


void reconnect() {//Funkcja do podlaczenia sie z serwerem mqtt
  while (!client.connected()) {
    Serial.print("Łączenie z MQTT...");
    if (client.connect("LCD", mqttUser, mqttPassword)) {
      Serial.println("Połączono");
      client.subscribe(mqttsTopic_czujnik_OC);
      client.subscribe(mqttsTopic_TempSensor);
    } else {
      Serial.print("Nie udało się połączyć z MQTT, spróbuj ponownie za 5 sekund...");
      delay(5000);
    }
  }
}
 
void loop() {
 if (!client.connected()) {
    reconnect();
  }
  client.loop();
  ReadPublishButton();
}
