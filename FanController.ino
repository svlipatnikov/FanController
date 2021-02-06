/*
 * Прошивка для контроллера управления вытяжкой в ванной и санузле
 * Версия 05
 * Дата модификации 22.10.19
 * 
 * Подключение:
 * GPIO0 - выход на реле управления вытяжкой в санузле       
 *       - при низком уровне переводит модуль режим программирования (не использовать для GPIO)
 * GPIO1 - TXD0 
 *       - светодиод (горит при низком уровне)
 * GPIO2 - выход на реле управления вытяжкой в ванной
 * GPIO3 - RXD0 
 * VCC - пиание +3.3V
 * CH_PD - питание +3.3V через резистор 10к (без него модуль не работает)
 * GND - питание GND
 * 
 * v.03 (13.08.19) Добавлено упровелние по UDP 
 * "t1", "t0" - включение и выключение вытяжки в туалете
 * "b1", "b0" - включение и выключение вытяжки в ванной
 * 
 * v.04 (06/10/19) Добавлены функции  ОТА 
 * 
 * v.05 (22/10/19) Пересобран с библиотекой blynk без обязательного подключения к wi-fi
 * 
 * v06 (02/11/19) изменен алгоритм подключения к wi-fi в функции Connect
 * 
 * v07 (09/11/19) доработан алгоритм подключения к wi-fi в функции Connect
 *
 * v08 (22/12/19) Изменены функции переподключения к wi-fi и blynk
 * 
 * v09 (23/12/19) 
 * Функция управления реле вынесены из условий подключения к wi-fi
 * удалена инициализация и печать в серийный порт
 * 
 * v10 (30/12/19)
 * функции подключения к wi-fi и blynk вынесены в отдельный файл.
 * OTA-апдейт заменен на WEB-апдейт
 * 
 * v11 (10/01/2020)
 * 
 * v12 (04/02/2020)
 * увеличено время работы вентилятора до 20 минут
 * 
 * v13 - 10/03/2020
 * переработан под connect_func_v06
 * увеличено время работы вентилятора до 30 минут
 * 
 * v20 - 11/03/2020
 * скорректирована функция fan_timer
 * 
 * v30 - 27/03/2020
 * отказ от Blynk, переход на mqtt
 * 
 * v31 - 10/04/2020
 * NEED_STATIC_IP = true
 * добавлена синхронизация по топику sync
 * 
 * v33 - 06/05/2020
 * измена функция таймера вентиляторов для разделения работы через UDP и вручную 
 * 
 * v34 - 22/05/2020
 * топики mqtt разделены на топики управления ctrl и топики статуса state
 * добавлены флаги retain вместо топиков синхронизации (connect_mqtt_v03)
 * 
 * v35 - 27/05/2020
 * изменил FAN_ON_UDP_TIME и FAN_ON_MANUAL_TIME
 */
 

#define PIN_LED          1  // встроенный светодиод
#define PIN_FAN_bathroom 2  //GPIO2
#define PIN_FAN_toilet   0  //GPIO0


#include <ESP8266WiFi.h>
const char ssid[] = "welcome's wi-fi";
const char pass[] = "27101988";
const bool NEED_STATIC_IP = true;
IPAddress IP_Node_MCU          (192, 168, 1, 71);
IPAddress IP_Fan_controller    (192, 168, 1, 41);
IPAddress IP_Water_sensor_bath (192, 168, 1, 135); 
IPAddress IP_Toilet_controller (192, 168, 1, 54);


#include <WiFiUdp.h>
WiFiUDP Udp;
unsigned int localPort = 8888;      
char Buffer[UDP_TX_PACKET_MAX_SIZE];  


#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;


#include <PubSubClient.h>
WiFiClient Bath_esp8266;
PubSubClient client(Bath_esp8266);
const char* mqtt_client_name = "Fan_esp8266";    // Имя клиента


const int CHECK_PERIOD = 2*60*1000;        // периодичность проверки на подключение к сервисам
const int RESTART_PERIOD = 30*60*1000;     // время до ребута, если не удается подключиться к wi-fi
const int FAN_ON_UDP_TIME = 5*60*1000;     // время удерживания включенного состояния после включения через UDP
const int FAN_ON_MANUAL_TIME = 15*60*1000; // время удерживания включенного состояния после включения через MQTT (вручную)

unsigned long Last_online_time;            // время когда модуль был онлайн
unsigned long Last_check_time;             // время проверки подключения к wi-fi и сервисам

unsigned long FAN_bathroom_UDP_time;       // время включения вентилятора в ванной через UDP
unsigned long FAN_bathroom_manual_time;    // время включения вентилятора в ванной через MQTT
unsigned long FAN_toilet_UDP_time;         // время включения вентилятора в туалете через UDP
unsigned long FAN_toilet_manual_time;      // время включения вентилятора в туалете через MQTT

bool FAN_bathroom_UDP_flag = false;        // флаг включения вентилятора в ванной через UDP
bool FAN_bathroom_manual_flag = false;     // флаг включения вентилятора в ванной через MQTT 
bool FAN_toilet_UDP_flag = false;          // флаг включения вентилятора в туалете через UDP
bool FAN_toilet_manual_flag = false;       // флаг включения вентилятора в туалете через MQTT
 
bool FAN_bathroom_ON = false;              // состояние вытяжки ванной
bool FAN_toilet_ON = false;                // состояние вытяжки туалета
bool last_state_FAN_bathroom = false;      // предыдущее состояние вытяжки ванной
bool last_state_FAN_toilet = false;        // предыдущее состояние вытяжки туалета

//==================================================================================
void setup() {
  pinMode(PIN_FAN_bathroom, OUTPUT); digitalWrite (PIN_FAN_bathroom , HIGH);
  pinMode(PIN_FAN_toilet,   OUTPUT); digitalWrite (PIN_FAN_toilet ,   HIGH);
 
  Connect_WiFi(IP_Fan_controller, NEED_STATIC_IP);
  Connect_mqtt(mqtt_client_name);
  MQTT_subscribe();
}


//==================================================================================
void loop() {

  // сетевые функции
  httpServer.handleClient();            // для обновления по воздуху   
  client.loop();                        // для функций MQTT 
  Receive_UDP();                        // функция управления вентиляторами по команде udp

  Fan_timer();                          // управление вентиляторами во времени
  Fan_control();                        // включение/выключение реле
   
  // проверка подключений к wifi и серверам
  if ((long)millis() - Last_check_time > CHECK_PERIOD) {
    Last_check_time = millis(); 
    
    // wi-fi  
    if (WiFi.status() != WL_CONNECTED) { 
      Connect_WiFi(IP_Toilet_controller, NEED_STATIC_IP);
      Restart(Last_online_time, RESTART_PERIOD);
    }
    else
      Last_online_time = millis();     
    
    // mqtt
    if (!client.connected()) {
      Connect_mqtt(mqtt_client_name);
      MQTT_subscribe();
    }      
  }
}



//==================================================================================
//функция отключения вентиляторв по таймеру
void Fan_timer (void) {  

  // после включения вручную прошло время FAN_ON_MANUAL_TIME 
  if ((((long)millis() - FAN_bathroom_manual_time) >= FAN_ON_MANUAL_TIME) && FAN_bathroom_manual_flag) {
    FAN_bathroom_ON = false; 
    FAN_bathroom_manual_flag = false;
  }
  
  // после включения через UDP прошло время FAN_ON_UDP_TIME 
  if ((((long)millis() - FAN_bathroom_UDP_time) >= FAN_ON_UDP_TIME) && FAN_bathroom_UDP_flag) {
    FAN_bathroom_ON = false;
    FAN_bathroom_UDP_flag = false; 
  }
  
  // после включения вручную прошло время FAN_ON_MANUAL_TIME 
  if ((((long)millis() - FAN_toilet_manual_time) >= FAN_ON_MANUAL_TIME) && FAN_toilet_manual_flag) {
    FAN_toilet_ON = false; 
    FAN_toilet_manual_flag = false;
  }

  // после включения через UDP прошло время FAN_ON_UDP_TIME 
  if ((((long)millis() - FAN_toilet_UDP_time) >= FAN_ON_UDP_TIME) && FAN_toilet_UDP_flag) {
    FAN_toilet_ON = false;
    FAN_toilet_UDP_flag = false;
  }
}

//==================================================================================
// функция приема пакетов по UDP
void Receive_UDP(void) {
  int packetSize = Udp.parsePacket();
  if (packetSize)  {
    int len = Udp.read(Buffer, UDP_TX_PACKET_MAX_SIZE); 
    if ((len == 2) && (Buffer[0] == 't') && Buffer[1] == '1') {
      FAN_toilet_ON = true;
      FAN_toilet_UDP_flag = true;
      FAN_toilet_UDP_time = millis();      
    }
    if ((len == 2) && (Buffer[0] == 't') && Buffer[1] == '0') {
      FAN_toilet_ON = false;
      FAN_toilet_UDP_flag = false;
    }
    if ((len == 2) && (Buffer[0] == 'b') && Buffer[1] == '1') {
      FAN_bathroom_ON = true;
      FAN_bathroom_UDP_flag = true;
      FAN_bathroom_UDP_time = millis();      
    }
    if ((len == 2) && (Buffer[0] == 'b') && Buffer[1] == '0')  {
      FAN_bathroom_ON = false;
      FAN_bathroom_UDP_flag = false;
    }
  }
}
