/* v01 - 24/03/2020
 * подключения к MQTT выделены отдельно из connect_func 
 * 
 * v02 - 25/03/2020
 * функции передачи MQTT_publish_int и MQTT_publish_float перенесены в connect_mqtt
 * 
 * v03 - 22/05/2020
 * добавлен флаг ratain = true при публикации сообщений на сервер
 */

 
//===================================================================================================
// подключение к MQTT 
/*
const char *mqtt_server = "mqtt.dioty.co";          // Имя сервера MQTT
const int   mqtt_port = 1883;                       // Порт для подключения к серверу MQTT
const char *mqtt_user = "sv.lipatnikov@gmail.com";  // Логин от сервера
const char *mqtt_pass = "83eb5858";                 // Пароль от сервера
*/

const char *mqtt_server = "srv1.clusterfly.ru";  // Имя сервера MQTT
const int   mqtt_port = 9124;                    // Порт для подключения к серверу MQTT
const char *mqtt_user = "user_1502445e";         // Логин от сервера
const char *mqtt_pass = "pass_943f7409";         // Пароль от сервера

void Connect_mqtt(const char* client_name) {
  if (WiFi.status() == WL_CONNECTED) {
    client.setServer(mqtt_server, mqtt_port);            
    if (client.connect(client_name, mqtt_user, mqtt_pass)) 
      client.setCallback(mqtt_get);
  }
}


// Функция отправки int на сревер mqtt
void MQTT_publish_int(const char* topic , int data){
  char msg[5];
  sprintf (msg, "%u", data);    
  client.publish(topic, msg, true);
}


// Функция отправки float на сревер mqtt
//void MQTT_publish_float(const char* topic , float data){
//  char msg[4];
//  sprintf (msg, "%2.1f", data);    
//  client.publish(topic, msg, true);
//}
//



//=========================================================================================
//функции MQTT

// топики управления реле
char topic_fan_bath_ctrl[] = "user_1502445e/fan/bath_ctrl"; 
char topic_fan_toilet_ctrl[] = "user_1502445e/fan/toilet_ctrl";

// топики состояния реле
char topic_fan_bath_state[] = "user_1502445e/fan/bath";
char topic_fan_toilet_state[] = "user_1502445e/fan/toilet";


// функция подписки на топики !!!
void MQTT_subscribe(void) {
  if (client.connected()){
    client.subscribe(topic_fan_bath_ctrl); 
    client.subscribe(topic_fan_toilet_ctrl);   
  }
}


// получение данных от сервера
void mqtt_get(char* topic, byte* payload, unsigned int length) {
  // создаем копию полученных данных
  char localPayload[length + 1];
  for (int i=0;i<length;i++) {
    localPayload[i] = (char)payload[i];
  }
  localPayload[length] = 0;  
  
  // присваиваем переменным значения в зависимости от топика 
  if (strcmp(topic, topic_fan_toilet_ctrl) == 0) {
    int ivalue = 0; sscanf(localPayload, "%d", &ivalue);
    FAN_toilet_ON = (bool)ivalue; 
    if (!last_state_FAN_toilet && FAN_toilet_ON) { // проверка что вентилятор не был включен
      FAN_toilet_manual_time = millis();   
      FAN_toilet_manual_flag = true;
    }    
  }
  else if (strcmp(topic, topic_fan_bath_ctrl) == 0) {
    int ivalue = 0; sscanf(localPayload, "%d", &ivalue);
    FAN_bathroom_ON = (bool)ivalue;
    if (!last_state_FAN_bathroom && FAN_bathroom_ON) { // проверка что вентилятор не был включен 
      FAN_bathroom_manual_time = millis();
      FAN_bathroom_manual_flag = true;
    }    
  }
}


//==================================================================================
// функция управления реле для включения вентиляторов
void Fan_control (void) {
  if (FAN_bathroom_ON != last_state_FAN_bathroom) {
    last_state_FAN_bathroom = FAN_bathroom_ON; 
    digitalWrite (PIN_FAN_bathroom , !FAN_bathroom_ON);       // реле управляется низким уровнем 0=ON
    MQTT_publish_int(topic_fan_bath_state, FAN_bathroom_ON);  // публикация данных в MQTT  
  } 
  if (FAN_toilet_ON != last_state_FAN_toilet) {
    last_state_FAN_toilet = FAN_toilet_ON;
    digitalWrite (PIN_FAN_toilet , !FAN_toilet_ON);          // реле управляется низким уровнем 0=ON
    MQTT_publish_int(topic_fan_toilet_state, FAN_toilet_ON); // публикация данных в MQTT  
  } 
}
