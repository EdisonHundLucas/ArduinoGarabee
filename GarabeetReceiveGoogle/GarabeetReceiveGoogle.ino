#include <SoftwareSerial.h>
#include <SPI.h>
#include <Ethernet.h>

SoftwareSerial serialGarabee(8,9);// blue 9 - yellow 8

String BMP180temp, DHT11humi, BMP180pressao, LDRvalue;
String Winddir, Windspeed, Rainin, BMP180altitude, DewPoint;
String Batt_lvl, str;

byte mac[] = { 0xf0, 0x4d, 0xa2, 0xd6, 0xe1, 0x06 }; //Setting MAC Address
char server[] = "api.pushingbox.com"; //pushingbox API server
IPAddress ip(192,168,1,7); //Arduino IP address. Only used when DHCP is turned off.
EthernetClient client; //define 'client' as object
String data, stringVal; //GET query with data
boolean connectedTest = false;

void setup(){

  Serial.begin(19200);
  
  while (!Serial) { }
   
  serialGarabee.begin(19200);

  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    Ethernet.begin(mac, ip);
  } 
  
  delay(1000);
}

void loop(){ 
  
  serialGarabee.listen();

  formatData(); //packing GET query with data

 // delay(10000);
}

String getValue(String data, char separator, int index){
    int maxIndex = data.length()-1;
    int j=0;
    String chunkVal = "";

    for(int i=0; i<=maxIndex && j<=index; i++)    {
      chunkVal.concat(data[i]);
      if(data[i]==separator)      {
        j++;
        if(j>index)        {
          chunkVal.trim();
          return chunkVal;
        }    
        chunkVal = "";    
      }  
    }  
}

void sendData(){

  Serial.println("connected");
  client.println(data);
  client.println("Host: api.pushingbox.com");
  client.println("Connection: close");
  client.println();

}

void formatData(){
   serialGarabee.listen();
 
   while(serialGarabee.available() > 0){
      char value = serialGarabee.read();
      str = str+value;
      if(value == '\n'){
         data+="";
         data+="GET /pushingbox?devid=v17489D60C6CE924"; //GET request query to pushingbox API
               
         BMP180temp = getValue(str, ' ', 0);
         DHT11humi = getValue(str, ' ', 1);
         BMP180pressao = getValue(str, ' ', 2);
         LDRvalue = getValue(str, ' ', 3);
         Winddir = getValue(str, ' ', 4);
         Windspeed = getValue(str, ' ', 5);
         Rainin = getValue(str, ' ', 6);
         BMP180altitude = getValue(str, ' ', 7);
         DewPoint = getValue(str, ' ', 8);
         Batt_lvl = getValue(str, ' ', 9);
  
         data+="&BMP180temp=";
         data+=BMP180temp;  
         data+="&DHT11humi=";
         data+=DHT11humi;
         data+="&BMP180pressao="; 
         data+=BMP180pressao;       
         data+="&LDRvalue="; 
         data+=LDRvalue; 
         data+="&Winddir="; 
         data+=Winddir; 
         data+="&Windspeed="; 
         data+=Windspeed; 
         data+="&Rainin="; 
         data+=Rainin; 
         data+="&BMP180altitude="; 
         data+=BMP180altitude; 
         data+="&DewPoint="; 
         data+=DewPoint; 
         data+="&Batt_lvl="; 
         data+=Batt_lvl;                     
         data+=" HTTP/1.1";
         str="";
         
         Serial.println("connecting...");
        
         if(client.connect(server, 80)){
           sendData();  
           Serial.println(data);
         }else{
           Serial.println("connection failed");
         }
         if(client.connected()) {	  
           Serial.println("disconnecting.");
           Serial.println();
           client.stop(); 
           data = ""; //data reset
         }       
      }
   }
}

void formatData2(){

   data+="";
   data+="GET /pushingbox?devid=v17489D60C6CE924"; //GET request query to pushingbox API

   data+="&BMP180temp=";
   data+="1026.50";  
   data+="&DHT11humi=";
   data+="1026.50";
   data+="&BMP180pressao="; 
   data+="333";       
   data+="&LDRvalue="; 
   data+="1026.5"; 
   data+="&Winddir="; 
   data+="10.25"; 
   data+="&Windspeed="; 
   data+="102.2"; 
   data+="&Rainin="; 
   data+="101.2"; 
   data+="&BMP180altitude="; 
   data+="106.25"; 
   data+="&DewPoint="; 
   data+="105.2"; 
   data+="&Batt_lvl="; 
   data+="10.62";
  
   data+=" HTTP/1.1";

   Serial.println("connecting...");

   if(client.connect(server, 80)){
     sendData();  
     Serial.println(data);
   }else{
     Serial.println("connection failed");
   }
   if(client.connected()) {	  
     Serial.println("disconnecting.");
     Serial.println();
     client.stop(); 
     data = ""; //data reset
   }
}


