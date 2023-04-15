#include <ESP8266WiFi.h> // Libreria para la conexion WiFi
#include <PubSubClient.h> //Librería para la conexion MQTT
#include "DHTesp.h" //Librería del sensor DHT para la temperatura y humedadad
#include <ArduinoJson.h> // Librería necesaria para la serialización/deserialización de msgs en JSON
#include <ESP8266httpUpdate.h> // Librería necesaria para realizar las actualizaciones de software de forma inalámbrica
#include "Button2.h"; //Librería necesaria para interaccionar con bottones de manera mas sencilla

// URL del servidor de actualizaciones: http://localhost:1880/esp8266-ota
//------------------------------------------//
//---------------CONSTANTES-----------------//
//------------------------------------------//
// Datos para actualización   >>>> SUSTITUIR IP <<<<<
#define OTA_URL "https://iot.ac.uma.es:1880/esp8266-ota/update"
#define HTTP_OTA_ADDRESS      F("https://iot.ac.uma.es")     // Address of OTA update server
#define HTTP_OTA_PATH         F("/esp8266-ota/update")       // Path to update firmware
#define HTTP_OTA_PORT         1880                           // Port of update server                                                      
#define HTTP_OTA_VERSION      String(__FILE__).substring(String(__FILE__).lastIndexOf('\\')+1) + ".nodemcu" // Name of firmware
String OTAfingerprint("5d 56 09 5c 5f 7b a4 3f 01 b7 22 31 d3 a7 da a3 6e 10 2e 60");

uint8_t macAddr[6];

// Definicion del botton flash (GPIO0)
#define BUTTON_PIN  0 
Button2 button = Button2(BUTTON_PIN); 

// Funciones necesarias para la actualización OTA
void progreso_OTA(int,int);
void final_OTA();
void inicio_OTA();
void error_OTA(int);

// Variable manejadora del sensor DHT
DHTesp dht;

// No podemos leer la tensión de alimentación, porque disponemos de un sensor analogico (nivel de agua)
//ADC_MODE(ADC_VCC);

//------------------------------------------//
//---------------VARIABLES -----------------//
//------------------------------------------//

//-----------------WIFI---------------------//

const char* ssidPACO = "No_robes_vecino";
const char* passwordPACO = "silyconhouse2021";

const char* ssidPABLO = "TP-LINK_8D5E_Repetidor";
const char* passwordPABLO = "88225975";

const char* ssidSERGIO = "Merymm";
const char* passwordSERGIO = "Perit@Minguit@z";

const char* ssidADRI = "MiFibra-CB60";
const char* passwordADRI = "QMzYrvEj";

String ssid;
String password;

const char* mqtt_server = "iot.ac.uma.es";
const char* user="infind";
const char* pass="zancudo";

//Topics necesarios para recibir y enviar los datos 
//No hay que modificarlos, ya se ajustan según el chipID del dispositivo
String topicLW = "infind/GRUPO8/ESP";
String topicDatos="infind/GRUPO8/ESP";
String topicConfig="infind/GRUPO8/ESP";
String topicLedPWM="infind/GRUPO8/ESP";
String topicLedStatus="infind/GRUPO8/ESP";
String topicSwitchCmd="infind/GRUPO8/ESP";
String topicSwitchStatus="infind/GRUPO8/ESP";
String topicFOTA="infind/GRUPO8/ESP";

//--------ENVIAR DATOS AL ENCENDER---------//
bool PrimeraVez=true;

//------------------LED--------------------//
// Variable en la que se almacenará la intensidad deseada del LED (0-100) que recibamos a 
// través del topic infind/GRUPO8/led/cmd. Mientras no recibamos nada valdrá 0 (LED apagado)
int level=0;
//Variables utiles para la configuracion del led
int PWM; //Establece la intensidad del led en el rango [0,1023]
int PWMold=0; //Variable para guardar datos del anterior PWM por si queremos encender el led con la misma intensidad de antes de apagarlo
int frecled=10; //frecuencia con la que se va encendiendo o apagando  el led 10ms por defecto
bool LogicaDigital=false; //false= negativa, true=positiva
bool LogicaPWM=false; //false= negativa, true=positiva
bool LedDigital=false; //Controla el Swicth
bool SalidaSwitch=false; //Controla el Swtich despues de configurar la logica
String origenPWM="PLACA";  //Almacena la procedencia de los cambios por defecto de la placa
String origenDigital="PLACA";//Almacena la procedencia de los cambios por defecto de la placa

//---------------FRECUENCIAS----------------//
//Variable que indica la frecuencia con la que se actualizan los datos
int Temp = 5*60*1000; //por defecto 5min

//Variables para la comprobacion de actualizaciones
unsigned long frecActualizacion=0; //frecuencia con la que comprueba si hay una actualizacion
unsigned long lastActualizacion=0;
bool actualizaMQTT=false; //accion inmediata de actualizar

//-------------DATOS A PUBLICAR-------------//
// Char de hasta 512 caracteres en el que almacenaremos la estructura JSON con los datos
// (sensores, Vcc, uptime)... a publicar
char sdatos[512];
// Char en el que almacenaremos la estructura JSON con el nivel de intensidad leído del
// LED para publicarlo
char sledstatus[256];
// Char en el que almacenaremos la estructura JSON con el estado de la conexión para pub
char sconexion[256];
// Char en el que almacenamos la estructura JSON con el estado del switch
char sswitchStatus[256];

//---------------BOTON FLASH--------------//
//Variables necesarias para controlar el led a traves del boton flash
int boton_flash=0;       // GPIO0 = boton flash
int estado_int=HIGH;     // por defecto HIGH (PULLUP). Cuando se pulsa se pone a LOW

bool led_control=true; //Variable de control de estado para saber que accion se debe de realizar (apagar o encender)
int PWMflash; //Variable para guardar la intensidad del led para encenderlo a la misma intensidad con la que estaba

//--------------MQTT--------------------//
// Creamos una instancia de cliente MQTT de tipo WiFi asociada a client
WiFiClient espClient;
PubSubClient client(espClient);
// Hora (en ms) en la que publicamos el último msg
unsigned long lastMsg = 0;
unsigned long lastPWM = 0;

//******************************************//
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^//
//----------------FUNCIONES-----------------//
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^//
//******************************************//


//------------------------------------------//
//-------------- SETUP WIFI ----------------//
//------------------------------------------//
void setup_wifi() 
{
  delay(10);
  if (String(ESP.getChipId())=="1126067") 
  {
    ssid=ssidPACO;
    password=passwordPACO;
  }
  else if (String(ESP.getChipId())=="833295") 
  {
    ssid=ssidPABLO;
    password=passwordPABLO;
  }
  else if (String(ESP.getChipId())=="5821794") 
  {
    ssid=ssidSERGIO;
    password=passwordSERGIO;
  }
  else if(String(ESP.getChipId())=="1089673") 
  {
    ssid=ssidADRI;
    password=passwordADRI;
  }
  else
  {
    Serial.println("Error. SSID incorrecta"); 
    ssid="nada";
  }
  
  Serial.println();
  Serial.print("Connecting to "); 
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str()); //Nos conectamos al wifi

  while (WiFi.status() != WL_CONNECTED) { //Animacion mientras conecta
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP()); //mensaje de conexion exitosa
}


//------------------------------------------//
//----------------CALLBACK------------------//
//------------------------------------------//
void callback(char* topic, byte* payload, unsigned int length) 
{
  // Reservo un espacio en memoria de longitud length+1 para copiar el msg recibido
  char *mensaje=(char *)malloc(length+1);
  // Copio el msg recibido (payload) en el espacio reservado (mensaje)
  strncpy(mensaje,(char*)payload,length);

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  // Mostramos el payload recibido caracter a caracter
  for (int i = 0; i < length; i++) 
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Comprobamos que el msg se ha recibido por el topic infind/GRUPO8/ESPX/config
    if(strcmp(topic,topicConfig.c_str())==0)
  {
    // Creamos la estructura JSON root en la que deserializaremos el msg
    StaticJsonDocument<512> root; 
    // Deserializamos el mensaje en root. Además, se guarda si ha habido un error
    DeserializationError error = deserializeJson(root, mensaje);

    // Si hubo un error, informo por pantalla de cuál es
    if (error) 
    {
      Serial.print("Error, deserializeJson() failed: ");
      Serial.println(error.c_str());
    }
    // Si no hubo error,
    else{
       //...................................................//
       //.............FRECUENCIA ENVIO DE DATOS.............//
       //...................................................//
       //comprobamos si el objeto son los datos de la frecuencia de envio de datos
       // Compruebo si la estructura JSON root, contiene la clave frecuencia que estamos buscando
       
       if(root.containsKey("frecuencia"))
       {
         // Comprobamos que el valor de frecuencia está dentro del rango para un correcto funcionamiento
         if (root["frecuencia"]>=2)
         {
           // Guardamos en la vble Temp el valor correspondiente a la clave "frecuencia"
           Temp = root["frecuencia"];
           Temp = Temp*1000;
           // Indicamos por pantalla que hemos sido capaces de extraer el valor frecuencia del msg JSON
           Serial.print("Mensaje OK, frecuencia de envio de datos = ");
           Serial.println(Temp);
         }
         else{
          Serial.println("El valor de \"frecuencia de envio de datos\" recibido es un numero negativo");
          }
          
        }
        // Si no existe ninguna clave en root que se llame frecuencia, lo indicamos por consola
        else
        {
          Serial.print("Error : ");
          Serial.println("\"frecuencia de envio de datos\" key not found in JSON");
        } 
       //...................................................//
       //................FRECUENCIA PWM LED.................//
       //...................................................//             
       //comprobamos si el objeto son los datos de la frecuencia del led 
       // Compruebo si la estruct JSON root,  contiene la clave fecled que estamos buscando
       if(root.containsKey("frecled"))
       {
         // Comprobamos que el valor de frecled está dentro del rango para un correcto funcionamiento
         if (root["frecled"]>=0)
         {
           // Guardamos en la vble frecled el valor correspondiente a la clave "frecled"
           frecled = root["frecled"];
           // Indicamos por pantalla que hemos sido capaces de extraer el valor frecled del msg JSON
           Serial.print("Mensaje OK, frecuencia del led = ");
           Serial.println(frecled);
         }
         else{
          Serial.println("El valor de \"frecuencia led\" recibido es un numero negativo");
          }
          
        }
        // Si no existe ninguna clave en root que se llame frecled, lo indicamos por consola
        else
        {
          Serial.print("Error : ");
          Serial.println("\"frecuencia led\" key not found in JSON");
        } 
         
    //...................................................//
    //...........FRECUENCIA ACTUALIZACION................//
    //...................................................//
    // Compruebo si la estruct JSON root, contiene la clave frecActualizacion que estamos buscando
      if(root.containsKey("frecActualizacion"))
      {
        // Comprobamos que el valor de frecActualizacion está dentro del rango para un correcto funcionamiento
        if (root["frecActualizacion"]>=0)
        {
          // Guardamos en la vble frecActualizacion el valor correspondiente a la clave "frecActualizacion"
          frecActualizacion = root["frecActualizacion"];
          frecActualizacion=frecActualizacion*60000;//Para pasarlo a minutos 
          // Indicamos por pantalla que hemos sido capaces de extraer el valor frecActualizacion del msg JSON
          Serial.print("Mensaje OK, frecActualizacion = ");
          Serial.println(frecActualizacion);
        }
        else{
          Serial.println("El valor de \"frecActualizacion\" recibido está fuera del rango");
        }
      }
      // Si no existe ninguna clave en root que se llame frecActualizacion, lo indicamos por consola
      else
      {
        Serial.print("Error : ");
        Serial.println("\"frecActualizacion\" key not found in JSON");
      }
           
      //...................................................//
      //.........CONFIGURACION LOGICA DIGITAL..............//
      //...................................................//
      //Comprobamos topic de logica positiva o negativa
      if(root.containsKey("LogicaDigital"))
       {
           // Guardamos en la vble Logica el valor correspondiente a la clave "LogicaDigital"
          LogicaDigital = root["LogicaDigital"];
           // Indicamos por pantalla que hemos sido capaces de extraer el valor LogicaDigital del msg JSON
           Serial.print("Mensaje OK, Logica Digital= ");
           if(LogicaDigital)Serial.println("Positiva");
           else Serial.println("Negativa");
           ControlLogica(); //Actualizamos la LOGICA
         }
        // Si no existe ninguna clave en root que se llame LogicaLogica, lo indicamos por consola
        else
        {
          Serial.print("Error : ");
          Serial.println("\"LogicaDigital\" key not found in JSON");
        }

      //...................................................//
      //...........CONFIGURACION LOGICA PWM................//
      //...................................................//
      //Comprobamos topic de logica positiva o negativa
      if(root.containsKey("LogicaPWM"))
       {
           // Guardamos en la vble Logica el valor correspondiente a la clave "LogicaPWM"
          LogicaPWM = root["LogicaPWM"];
           // Indicamos por pantalla que hemos sido capaces de extraer el valor LogicaPWM del msg JSON
           Serial.print("Mensaje OK, Logica PWM= ");
           if(LogicaPWM)Serial.println("Positiva");
           else Serial.println("Negativa");
           ControlLogica(); //Actualizamos la Logica
         }
        // Si no existe ninguna clave en root que se llame Logica, lo indicamos por consola
        else
        {
          Serial.print("Error : ");
          Serial.println("\"LogicaPWM\" key not found in JSON");
        }
        
     }   
  }
  if(strcmp(topic,topicFOTA.c_str())==0){
    // Creamos la estructura JSON root en la que deserializaremos el msg
    StaticJsonDocument<512> root; 
    // Deserializamos el mensaje en root. Además, se guarda si ha habido un error
    DeserializationError error = deserializeJson(root, mensaje);

    // Si hubo un error, informo por pantalla de cuál es
    if (error) 
    {
      Serial.print("Error, deserializeJson() failed: ");
      Serial.println(error.c_str());
    }
    // Si no hubo error,
    else{
    //...................................................//
      //................ACTUALIZACION......................//
      //...................................................//
      if(root.containsKey("actualiza"))
       {
           // Guardamos en la vble Actualizacion el valor correspondiente a la clave "actualiza"
           actualizaMQTT = root["actualiza"];
           // Indicamos por pantalla que hemos sido capaces de extraer el valor actualiza del msg JSON
           Serial.print("Mensaje OK, actualiza = ");
           Serial.println(actualizaMQTT);
         }
        // Si no existe ninguna clave en root que se llame actualiza, lo indicamos por consola
        else
        {
          Serial.print("Error : ");
          Serial.println("\"Actualizacion\" key not found in JSON");
        }
    } 
  }

  if(strcmp(topic,topicLedPWM.c_str())==0){
    // Creamos la estructura JSON root en la que deserializaremos el msg
    StaticJsonDocument<512> root; 
    // Deserializamos el mensaje en root. Además, se guarda si ha habido un error
    DeserializationError error = deserializeJson(root, mensaje);

    // Si hubo un error, informo por pantalla de cuál es
    if (error) 
    {
      Serial.print("Error, deserializeJson() failed: ");
      Serial.println(error.c_str());
    }
    // Si no hubo error,
    else{
    //...................................................//        
    //................INTENSIDAD LED.....................//      
    //...................................................//
    //comprobamos si el objeto son los datos del LED
    // Compruebo si la estruct JSON root, contiene la clave level que estamos buscando
    if(root.containsKey("level"))
     {
       // Comprobamos que el valor de level está dentro del rango para un correcto funcionamiento
       if (root["level"]>=0 and root["level"]<=100)
       {
         // Guardamos en la vble level el valor correspondiente a la clave "level"
         level = root["level"];
         // Indicamos por pantalla que hemos sido capaces de extraer el valor level del msg JSON
         Serial.print("Mensaje OK, intensidad del led = ");
         Serial.println(level);
         origenPWM="MQTT"; //Actualizamos el origen
         LED(); //Llamamos a la funcion led para que actualice el PWM
        }
        else{
         Serial.println("El valor de \"intensidad del led\" recibido está fuera del rango 0-100");
        }
      }
      // Si no existe ninguna clave en root que se llame level, lo indicamos por consola
      else
      {
        Serial.print("Error : ");
        Serial.println("\"intensidad del\" key not found in JSON");
      }
    }
  }
   
   if(strcmp(topic,topicSwitchCmd.c_str())==0){
    // Creamos la estructura JSON root en la que deserializaremos el msg
    StaticJsonDocument<512> root; 
    // Deserializamos el mensaje en root. Además, se guarda si ha habido un error
    DeserializationError error = deserializeJson(root, mensaje);

    // Si hubo un error, informo por pantalla de cuál es
    if (error) 
    {
      Serial.print("Error, deserializeJson() failed: ");
      Serial.println(error.c_str());
    }
    // Si no hubo error,
    else{
   //...................................................//
   //..............CONTROL LED DIGITAL..................//
   //...................................................//
   //Comprobamos topic de led Digital
      if(root.containsKey("LedDigital"))
       {
           // Guardamos en la vble LedDigital el valor correspondiente a la clave "LedDigital"
           LedDigital = root["LedDigital"];
           // Indicamos por pantalla que hemos sido capaces de extraer el valor LedDigital del msg JSON
           Serial.print("Mensaje OK, Led digital = ");
           Serial.println(LedDigital);
           origenDigital="MQTT"; //Actualizamos el origen
           switchControl(); //Actuliza los paramentros
         }
        // Si no existe ninguna clave en root que se llame LedDigital, lo indicamos por consola
        else
        {
          Serial.print("Error : ");
          Serial.println("\"LedDigital\" key not found in JSON");
        }
    }
   }
  
}


//------------------------------------------//
//-----------------RECONNECT----------------//
//------------------------------------------//
void reconnect() 
{
  topicLW+=String(ESP.getChipId());
  topicLW+="/conexion"; //Declaramos el topicLW segun nuestro CHIP ID de la placa
  while (!client.connected()) 
  { 
    Serial.print("Attempting MQTT connection...");
    
    // En este caso, el clientId no es aleatorio, pero sigue siendo único porque contiene
    // el ChipId de nuestra placa. GetChipId devuelve un entero -> lo pasamos a string
    String clientId = "ESP8266Client-";
    clientId += String(ESP.getChipId()) ;

    // Formateamos el msg LWT en la estructura JSON conexion, que enviaremos al broker
    // al conectarnos (en la función conect)
    StaticJsonDocument<256> conexion;
    // Creamos la clave "online" y le asociamos el valor false. De esta forma, al ser un msg
    // LWT,cuando nos desconectemos abruptamente, se informará a los clientes que estén 
    // suscritos al topic conexión de que el dispositivo no está conectado
    conexion["online"] = false;
    conexion["CHIPID"]="ESP"+String(ESP.getChipId());
    // Serializamos la estructura JSON conexion en el string sconexion y ya esta listo
    // para ser publicado (se publicará al llamar a connect())
    serializeJson(conexion, sconexion);

    // Intentamos conectarnos. Además del clientId, user y pass, le pasamos: 
    // - el topic LWT: el msg LWT se enviará a los clientes que estén suscritos a este topic
    // - QoS: 2
    // - retain flag: true, para que este msg sustituya al msg retenido cuando se pierda la conex
    // - El msg LWT, que es el JSON {"online": false}
    if (client.connect(clientId.c_str(),user,pass,topicLW.c_str(),2,true,sconexion))
    {
      // Construimos ahora el msg retenido, que es el mismo que el LWT pero con online=true
      conexion["online"] = true;
      conexion["CHIPID"]="ESP"+String(ESP.getChipId());
      // Lo serializamos en sconexion
      serializeJson(conexion, sconexion);   
      // Y lo publicamos en el topic /conexion con el retain flag puesto a true
      client.publish(topicLW.c_str(),sconexion,true);

      // Informamos por consola que nos hemos conectado
      Serial.println("connected");
      //Nos suscribimos al topic para obtener las configuraciones necesarias
      //Para ello los modificamos segun nuestras placas (LO HACE SOLO NO HAY QUE CONFIGURAR NADA)
      topicConfig+=String(ESP.getChipId());
      topicConfig+="/config";
      client.subscribe(topicConfig.c_str());
      topicLedPWM+=String(ESP.getChipId());
      topicLedPWM+="/led/cmd";
      client.subscribe(topicLedPWM.c_str());
      topicSwitchCmd+=String(ESP.getChipId());
      topicSwitchCmd+="/switch/cmd";
      client.subscribe(topicSwitchCmd.c_str());
      topicFOTA+=String(ESP.getChipId());
      topicFOTA+="/FOTA";
      client.subscribe(topicFOTA.c_str());    
    } 
    else 
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

//------------------------------------------//
//-------------ACTUALIZACION----------------//
//------------------------------------------//
void actualizar(){
  // ACTUALIZACIÓN OTA
  Serial.println( "-------------------------" );  
  Serial.println( "Comprobando actualización:" );
  Serial.print(HTTP_OTA_ADDRESS);Serial.print(":");Serial.print(HTTP_OTA_PORT);Serial.println(HTTP_OTA_PATH);
  Serial.println( "-------------------------" );  
  ESPhttpUpdate.setLedPin(16,LOW);
  ESPhttpUpdate.onStart(inicio_OTA);
  ESPhttpUpdate.onError(error_OTA);
  ESPhttpUpdate.onProgress(progreso_OTA);
  ESPhttpUpdate.onEnd(final_OTA);
  WiFi.macAddress(macAddr);

  // La siguiente función comprueba si hay un programa nuevo para nuestro dispositivo, si lo
  // hay se lo descarga y lo sustituye por el actual. Una vez sustituido, se reinicia la placa
  // y comienza a ejecutarse el nuevo programa. Le tenemos que decir la dirección IP, puerto y
  // path donde está el servidor de actualizaciones: lo tenemos implementado en NodeRED
  switch(ESPhttpUpdate.update(OTA_URL, HTTP_OTA_VERSION, OTAfingerprint)) 
  {
    case HTTP_UPDATE_FAILED:
      Serial.printf(" HTTP update failed: Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println(F(" El dispositivo ya está actualizado"));
      Serial.printf("Tu mac es: %02X:%02X:%02X:%02X:%02X:%02X\n", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
      break;
    case HTTP_UPDATE_OK:
      Serial.println(F(" OK"));
      break;
  }  
  
}


//------------------------------------------//
//-------------------SETUP------------------//
//------------------------------------------//
void setup() 
{
  pinMode(16, OUTPUT); //Configuramos el PIN 16 como salida (LED DIGITAL)
  Serial.begin(115200);
  setup_wifi();//Configuramos el wifi
  dht.setup(5, DHTesp::DHT11); //Configuracion para obtener datos de DHT11
  client.setServer(mqtt_server, 1883); //Establecemos el server MQTT
  client.setCallback(callback); 
  client.setBufferSize(512);      // Establecemos el tamaño del buffer para el manejo de msgs MQTT a 512 (msgs largos)
  actualizar();   //Comprobamos actualizacion al arrancar la placa
  Serial.println();
  LED(); //Configuramos los LED
  switchControl(); //Configuramos el Switch

 //Comandos necesarios para leer los botones segun la libreria button2
  button.setClickHandler(handler);
  button.setDoubleClickHandler(handler);
  button.setTripleClickHandler(handler);
  button.setLongClickHandler(longpress);

  //Configuramos los topics de publicacion segun el CHIP ID de la placa
  topicDatos+=String(ESP.getChipId());
  topicDatos+="/datos";
  
  topicLedStatus+=String(ESP.getChipId());
  topicLedStatus+="/led/status";

  topicSwitchStatus+=String(ESP.getChipId());
  topicSwitchStatus+="/switch/status";
}


//------------------------------------------//
//-------------FUNCIONES UTILES-------------//
//------------------------------------------//

//-----------CONTROL LOGICA-----------------//
void ControlLogica(){
  //Controlamos si la logica es positiva o negativa
  if(LogicaPWM){
  PWM =level*1023/100;
  }
  else if(!LogicaPWM){
    PWM =1023-(level*1023/100);
  }
  if(!LogicaDigital){
    SalidaSwitch=!LedDigital;
  }
  else if(LogicaDigital){
    SalidaSwitch=LedDigital;
  }
}

void switchControl(){
  //Controlamos el Switch (Un led digital)
  ControlLogica(); //Comprobamos la logica antes de actualizar
  digitalWrite(16,SalidaSwitch); //Actualizamos
  StaticJsonDocument <256> switchStatus; //Publicamos los cambios
  switchStatus["CHIPID"] = "ESP"+String(ESP.getChipId());
  if(LogicaDigital)switchStatus["Switch"]=SalidaSwitch;
  else switchStatus["Switch"]=!SalidaSwitch;
  switchStatus["origen"] = origenDigital;
  serializeJson(switchStatus,sswitchStatus);
  client.publish(topicSwitchStatus.c_str(),sswitchStatus);
}


//--------------CONTROL LED-----------------//
void LED(){
  // Actualizamos constantentemente el valor PWM que estamos mandando al LED, para que
  // si llega un nuevo valor de level, actualicemos el brillo del LED
  // El analogWrite tenemos que llamarlo con un valor PWM entre 0 y 1023: [0,100]->[0-1023]
  // Además, el LED funciona con lógica negativa: 1023->0 y 0->1023
  ControlLogica(); //Comprobamos la logica
  
  // Formateamos el msg a publicar en la estructura JSON ledstatus
  StaticJsonDocument<256> ledstatus;
  // Creamos la clave "led" y le asociamos el valor de led
  ledstatus["led"]=level;
  ledstatus["origen"]=origenPWM;
  ledstatus["CHIPID"]="ESP"+String(ESP.getChipId());
  // Serializamos la estructura JSON ledstatus en el string sledstatus y lo publicamos  
  serializeJson(ledstatus, sledstatus);
  client.publish(topicLedStatus.c_str(),sledstatus);
}

//------------FUNCIONES BUTTON2-------------//
//...longpress..............................//
// Actualizacion Firmware mediante boton
void longpress(Button2& btn) {
    unsigned int time = btn.wasPressedFor();
    Serial.print("You clicked ");
    if (time >= 4000) {
      actualizar(); //Si pulsamos 4s comprobamos si hay actualizacion nueva para el software de la placa
    }
    Serial.print(" (");        
    Serial.print(time);        
    Serial.println(" ms)");
}
//...click/double click..................//
// Control del LED y del Switch mediante boton
void handler(Button2& btn) {
    switch (btn.getClickType()) {
        case SINGLE_CLICK:
              Serial.print("Se detecto una pulsacion y el led esta a ");
      //Un solo click
      Serial.print(1023-PWM);
      Serial.print(" de intensidad");
      //Actualizamos el origen
      origenPWM="PULSADOR";
      origenDigital="PLACA";
      //Controlamos si queremos apagar o encender
      if(led_control){
         PWMflash=level;
         PWMold=PWM;
         level=0;
         led_control=false;
         Serial.print("\n");
         Serial.print("apagando");
         Serial.print("\n");
         LedDigital=false;
         LED();
         switchControl();
      }
      else{
        PWMold=PWM;
        level=PWMflash;
        led_control=true;
        Serial.print("\n");
        Serial.print("encendiendo");
        Serial.print("\n");
        LedDigital=true;
        LED();
        switchControl();
        }
            break;
        case DOUBLE_CLICK:
        //Doble click
            Serial.print("double ");
            origenPWM="PULSADOR";
            level=100;
            LED();
            break;
        case TRIPLE_CLICK:
            Serial.print("triple ");
            break;
    }
    Serial.print("click");
    Serial.print(" (");
    Serial.print(btn.getNumberOfClicks());    
    Serial.println(")");
}


//------------------------------------------//
//*****************LOOP*********************//
//------------------------------------------//
//*****************MAIN*********************//
//------------------------------------------//

void loop() 
{
  if (!client.connected()) {
    reconnect(); //Conexion MQTT
  }

  client.loop();
  button.loop();
  
  unsigned long now = millis();  
  if (now - lastMsg > Temp || PrimeraVez) //Condicion de tiempo para actualizar los datos cada X tiempo 
  {
    PrimeraVez=false; //Para actualizar los datos cuando se enciende la placa
    lastMsg = now;

    // Leemos los datos del sensor
    delay(dht.getMinimumSamplingPeriod());
    float humidity = dht.getHumidity();
    float temperature = dht.getTemperature();

    // No podemos leer la tensión de alimentación de la placa, porque ya estamos leyendo un 
    // sensor analógico: el del nivel de agua
    //float vcc = ESP.getVcc()/1000.0;

    // Obtenemos el RSSI de la conexión WiFi
    float rssi = WiFi.RSSI();
    // Obtenemos la IP del router al que estamos conectados. Esa función me devuelve un
    // valor de tipo IPAddress -> lo pasamos a string
    String IP = WiFi.localIP().toString();
    int NivelAgua=analogRead(A0); 
    //Nos muestra el nivel del agua del recipiente que almacena el agua que viene del Aire acondicionado
    //Como los datos de este sensor muestran numeros de 0 a 500 hacemos un escalado para obtener un %
    NivelAgua=NivelAgua/5;
    if(NivelAgua>=100)NivelAgua=100;
    if(NivelAgua<=0.5)NivelAgua=0;
    
    // Formateamos los datos a publicar en la estructura JSON datos
    StaticJsonDocument<512> datos;

    datos["CHIPID"]="ESP"+String(ESP.getChipId());
    
    // El campo Uptime es el número de ms desde que se inició la placa
    datos["Uptime"] = now;
    if(LogicaDigital)datos["Switch"]=SalidaSwitch;
    else datos["Switch"]=!SalidaSwitch;
    datos["LED"] = level;
    datos["NivelAgua"]=NivelAgua;

    // Creamos el objeto anidado dht11, perteneciente a la estructura JSON datos
    JsonObject dht11 = datos.createNestedObject("DHT11");
    dht11["Temperatura"] = temperature;
    dht11["Humedad"] = humidity;

    // Creamos el objeto anidado wifi, perteneciente a la estructura JSON datos
    JsonObject wifi = datos.createNestedObject("WiFi");
    wifi["SSId"] = ssid;
    wifi["IP"] = IP;
    wifi["RSSI"] = rssi;

    // Serializamos la estructura JSON datos en el string sdatos y lo publicamos
    serializeJson(datos, sdatos);
    client.publish(topicDatos.c_str(),sdatos);
  }

  //Condiciones que cambian el estado del led (PWM)
  //Segun la frecuencia seleccionada
  if(PWMold<=PWM && now - lastPWM  > frecled){     
    lastPWM=now;
    PWMold++;
    analogWrite(2,PWMold);

  }
  if(PWMold>=PWM && now - lastPWM  > frecled){
    lastPWM=now;
    PWMold--;
    analogWrite(2,PWMold);
  }  
//Coondiciones para actualizar segun variables recibidas por MQTT (tiempo o boton)
    if(actualizaMQTT || now-lastActualizacion>frecActualizacion){
      if(frecActualizacion==0 && actualizaMQTT==false)return; // Condicion que evita la actualizacion si el tiempo de actualizacion es = 0
      lastActualizacion=now;
      actualizaMQTT=false;
      actualizar();
  }
}


//------------------------------------------//
//-------------- Funciones OTA -------------//
//------------------------------------------//

void final_OTA(){
  Serial.println("Fin OTA. Reiniciando...");}

void inicio_OTA(){
  Serial.println("Nuevo Firmware encontrado. Actualizando...");}

void error_OTA(int e){
  char cadena[64];
  snprintf(cadena,64,"ERROR: %d",e);
  Serial.println(cadena);}

void progreso_OTA(int x, int todo){
  char cadena[256];
  int progress=(int)((x*100)/todo);
  if(progress%10==0)
  {
    snprintf(cadena,256,"Progreso: %d%% - %dK de %dK",progress,x/1024,todo/1024);
    Serial.println(cadena);
  }
}
