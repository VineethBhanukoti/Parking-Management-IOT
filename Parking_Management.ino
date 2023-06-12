#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <L298N_MotorDriver.h>
#include <Firebase_ESP_Client.h>
//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"
// Insert your network credentials
#define WIFI_SSID "OnePlus Nord"
#define WIFI_PASSWORD "m2gcdtty"
// #define WIFI_SSID "Samanvitha 4G"
// #define WIFI_PASSWORD "9449171477"
// Insert Firebase project API Key
#define API_KEY "AIzaSyC-u1X5VT2OfmlIT86LUL9qUZxMmM7C7eM"
// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://parking-management-8c625-default-rtdb.asia-southeast1.firebasedatabase.app"
//Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
//Declare and initialze variables that are to be used
unsigned long sendDataPrevMillis = 0;
bool signupOK = false;
String plate;
String status;
int count = 0;
int read_data = 0;
int iter=0;
int freeslot[6];
int occupiedslot[6];
int tower[4][2];
String frees;
String occu;
int freesize = 0;
int occusize = 0;
int botm = -1;
int dist = 0;
const int trigPin = 2;
//Motor A
int ena = 4;
int in1 = 5;
int in2 = 0;
//L298 Motor driver initialization.
L298N_MotorDriver motora(ena, in1, in2);

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback;  //see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  pinMode(ena, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(trigPin, INPUT);
  motora.setSpeed(255);
  // motorb.setSpeed(255);
}
void loop() {
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 500 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
    int iter = getcount();
    int i=1;
    Firebase.RTDB.getInt(&fbdo,"User/Tower/12917597-77510148/Proceed/");
    int proceed = fbdo.intData();
    // if(proceed == 1){
      if(iter>0){
        getbottom();
        getslotstatus();
        Serial.println("Got stuck1");
        for(i=1;i<=iter;i++)
        {   
          if(Firebase.RTDB.getInt(&fbdo, "User/Tower/12917597-77510148/count/"+String(i))){
            plate = fbdo.stringData();
            Serial.println(plate);
            moveslot();
            Firebase.RTDB.setInt(&fbdo,"User/Tower/12917597-77510148/Proceed",0);
          }
          if(i==iter){
            iter = getcount();
            if(i==iter)
              break;
          }
          Serial.println("Got stuck");
          delay(10000);
        }
        // Firebase.RTDB.setString(&fbdo,"User/Tower/12917597-77510148/"+plate,"-1");
        //delete processed nodes
        for(int j=i;j>0;j--){
          Firebase.RTDB.deleteNode(&fbdo,"User/Tower/12917597-77510148/count/"+String(j));
          Serial.println("User/Tower/12917597-77510148/count/"+String(j));
          Firebase.RTDB.setInt(&fbdo,"User/Tower/12917597-77510148/count/items",--iter);
        }
      }
    // }
  }
}
void runmotors(bool direction) {
  motora.setDirection(direction);
  motora.enable();
  disablemotor();
}
void disablemotor() {
  // Prints the distance on the Serial Monitor
  int state = 0;
  bool x=true;
  while (state!=dist+1) {
    int data = digitalRead(trigPin);
    if(data==0&&x){
      x=false;
      state+=1;
    }
    else if(data==1)
      x=true;
    delay(10);
    Serial.println(state);
    Serial.println(x);
    Serial.println("Value"+String(data));
  }
  Serial.println("Motor Stopped");
  motora.disable();
}
void string_to_array(String array, String var) {
  if (int(array.charAt(0)) - 48 == 0 && array.length() > 1)
    array = array.substring(1, 5);
  if (var == "free") {
    for (int i = 0; i < 6; i++)
      freeslot[i] = int(array.charAt(i)) - 48;
    freesize = array.length();
  }
  for (int i = 0; i < 6; i++)
    occupiedslot[i] = int(array.charAt(i)) - 48;
  occusize = array.length();
  Serial.println(array.length());
}
String array_to_string(int array[], int size) {
  if (size == 0) {
    // Firebase.RTDB.setBool(&fbdo, "User/Tower/12917597-77510148/Message", true);
    return "0";
  }
  String ans = "";
  for (int i = 0; i < size; i++)
    ans = ans + char(array[i] + 48);
  return ans;
}
bool findslot(int pos){
  int ans, new_bottom = 0, min = 10;
  bool dir;
  change_slotspos_in_tower(botm);
  if(pos==0){
    if (freeslot[0] != 0) {
      Firebase.RTDB.setBool(&fbdo, "User/Tower/12917597-77510148/Message", true);
      for(int i=0;i<4;i++)
        for(int j=0;j<2;j++)
          if(!((i==0&&j==0)||(i==3&&j==1))){
            Serial.println("Hi");
            if(frees.indexOf((String)(tower[i][j]))!=-1)
            {
              dir=directions(j);
              dist = i;
              botm=tower[i][j];
              Serial.println("free");
              for (int x = 0; x < freesize; x++) {
                if (freeslot[x] == botm) {
                  freeslot[x] = freeslot[freesize - 1];
                  occupiedslot[occusize] = botm;
                  occusize += 1;
                  break;
                }
                Serial.println(freeslot[x]);
                delay(1000);
              }
              freesize = freesize - 1;
              return dir;
            }
          }
    }
    else
      Firebase.RTDB.setBool(&fbdo, "User/Tower/12917597-77510148/Message", false);
  }
  else{
    if (occupiedslot[0] != 0) {
      Firebase.RTDB.setBool(&fbdo, "User/Tower/12917597-77510148/Message", true);
      for(int i=0;i<4;i++)
        for(int j=0;j<2;j++)
          if(!((i==0&&j==0)||(i==3&&j==1))){
            if(tower[i][j]==pos)
            {
              dir=directions(j);
              dist = i;
              botm=tower[i][j];
              for (int x = 0; x < occusize; x++) {
                if (occupiedslot[x] == botm) {
                  occupiedslot[x] = occupiedslot[occusize - 1];
                  freeslot[freesize] = botm;
                  freesize += 1;
                  break;
                }
              }
              occusize = occusize - 1;
              return dir;
            }
         }
    }
    else
      Firebase.RTDB.setBool(&fbdo, "User/Tower/12917597-77510148/Message", false);
  }
  return false;
}
// bool findlot(int pos) {
//   int ans, new_bottom = 0, min = 10;
//   bool dir;
//   if (pos == 0) {  //Code to park vehicle by finding available free slot.
//     if (freeslot[0] != 0) {
//       if (botm == 3 || botm == 4) {
//         for (int i = 0; i < freesize; i++) {
//           ans = botm - freeslot[i];
//           if (abs(ans) < min) {
//             min = abs(ans);
//             dir = directions(ans);  //return anticlockwise for neg and clockwise for pos
//             new_bottom = freeslot[i];
//           }
//         }
//       } else if (botm == 2 || botm == 5) {
//         for (int i = 0; i < freesize; i++) {
//           ans = botm - freeslot[i];
//           (ans<0)?ans = abs(ans) / 2:ans=(-1*(ans/2));
//           if (abs(int(ans)) < min) {
//             min = abs(int(ans));
//             dir = directions(int(ans));
//             new_bottom = freeslot[i];
//           }
//         }
//       } else if (botm == 1 || botm == 6) {
//         for (int i = 0; i < freesize; i++) {
//           ans = botm - freeslot[i];
//           dir = (abs(ans)<3)?false:true;
//           if (abs(ans) == 4)
//             ans = ans / 2;
//           else if (abs(ans) == 5) {
//             if (ans < 0)
//               ans = -1;
//             ans = 1;
//           }
//           if (abs(int(ans)) < min) {
//             min = abs(int(ans));
//             dir = directions(int(ans));
//             new_bottom = freeslot[i];
//           }
//         }
//       }
//       botm = new_bottom;
//       dist = min;
//       for (int i = 0; i < freesize; i++) {
//         if (freeslot[i] == botm) {
//           freeslot[i] = freeslot[freesize - 1];
//           occupiedslot[occusize] = botm;
//           occusize += 1;
//         }
//       }
//       freesize = freesize - 1;
//     } else
//       Firebase.RTDB.setBool(&fbdo, "User/Tower/12917597-77510148/Message", false);
//   } else  //Type the code to retrieve the vehicle here
//   {
//     if (occupiedslot[0] != 0) {
//       Firebase.RTDB.setBool(&fbdo, "User/Tower/12917597-77510148/Message",true);
//       if (botm == 3 || botm == 4) {
//         ans = botm - pos;
//         min = abs(ans);
//         dir = directions(ans);  //return anticlockwise for neg and clockwise for pos
//         new_bottom = pos;
//       } else if (botm == 2 || botm == 5) {
//         ans = botm - pos;
//         if (abs(ans) == 4)
//           ans = ans / 2;
//         min = abs(int(ans));
//         dir = directions(int(ans));
//         new_bottom = pos;
//       } else if (botm == 1 || botm == 6) {
//         ans = botm - pos;
//         dir = (abs(ans)<3)?false:true;
//         if (abs(ans) == 4)
//           ans = ans / 2;
//         else if (abs(ans) == 5) {
//           if (ans < 0)
//             ans = -1;
//           ans = 1;
//         }
//         min = abs(int(ans));
//         new_bottom = pos;
//       }
//       botm = new_bottom;
//       dist = min;
//       for (int i = 0; i < occusize; i++) {
//         if (occupiedslot[i] == new_bottom && new_bottom != 0) {
//           occupiedslot[i] = occupiedslot[occusize - 1];
//           freeslot[freesize] = botm;
//           freesize += 1;
//         }
//       }
//       occusize = occusize - 1;
//     } else
//       Firebase.RTDB.setBool(&fbdo, "User/Tower/12917597-77510148/Message", true);
//   }
//   return dir;
// }
bool directions(int ans) {
  if (ans == 0) //was ans<0
    return false;
  return true;
}
void getbottom(){
    if(Firebase.RTDB.getInt(&fbdo, "User/Tower/12917597-77510148/System/Bottom")){
      if(fbdo.dataType()=="int")
        botm = fbdo.intData();
      Serial.println(botm);
    }
    else {
      Serial.println("FAILED to Access top or Bottom");
      Serial.println("REASON: " + fbdo.errorReason());
    }
}
int getcount(){
    if (Firebase.RTDB.getString(&fbdo, "User/Tower/12917597-77510148/count/items")){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      if (fbdo.dataType() == "int"){
        Serial.println(fbdo.intData());
        return(fbdo.intData());
      }
    }
    else {
      Serial.println("FAILED to Access Count");
      Serial.println("REASON: " + fbdo.errorReason());
    }
    return(0);
}
void getslotstatus(){
 //getting the free slots in the model
    if (Firebase.RTDB.getString(&fbdo, "User/Tower/12917597-77510148/System/free")){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      if (fbdo.dataType() == "string"){
        frees=fbdo.stringData();
        string_to_array(frees, "free");
      }
    }else {
      Serial.println("FAILED to Access System Free");
      Serial.println("REASON: " + fbdo.errorReason());
    }

    //getting the occupied slots in the model
    if (Firebase.RTDB.getString(&fbdo, "User/Tower/12917597-77510148/System/occupied")){
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      if (fbdo.dataType() == "string"){
        occu=fbdo.stringData();
        string_to_array(occu, "occupied");
      }
    }else {
      Serial.println("FAILED to Access System Occupied");
      Serial.println("REASON: " + fbdo.errorReason());
    }
}
void moveslot(){
  if (Firebase.RTDB.getString(&fbdo, "User/Tower/12917597-77510148/"+plate)){
        Serial.println("PASSED");
        Serial.println("PATH: " + fbdo.dataPath());
        if (fbdo.dataType() == "string" ){
          status = fbdo.stringData();
          Serial.print("Data received: ");
          Firebase.RTDB.setString(&fbdo, "User/Tower/12917597-77510148/"+plate,"-1");
          Serial.println(status); //print the data received from the Firebase database
          if(status!="-1")
          {
            if(status=="1"){// means the user has requested to park
              bool val = findslot(0);
              if (Firebase.RTDB.getBool(&fbdo, "User/Tower/12917597-77510148/Message")){
                Serial.println("reached");
                if (fbdo.boolData()){
                  Serial.println("reached 2");
                  if(fbdo.boolData() != false){
                    runmotors(val);
                    Serial.println("Not reached");
                    Firebase.RTDB.setInt(&fbdo, "User/Tower/12917597-77510148/Parked Vehicle/"+plate,botm);
                    Firebase.RTDB.setInt(&fbdo, "User/Tower/12917597-77510148/System/Bottom",botm);
                    Firebase.RTDB.setString(&fbdo, "User/Tower/12917597-77510148/System/free",array_to_string(freeslot, freesize));
                    Firebase.RTDB.setString(&fbdo, "User/Tower/12917597-77510148/System/occupied",array_to_string(occupiedslot, occusize));
                  }
                }
              }
            }
            else{ //means the user has requested to retrieve their vehicle
              if(Firebase.RTDB.getInt(&fbdo, "User/Tower/12917597-77510148/Parked Vehicle/"+plate))
                if(fbdo.dataType()=="int")
                {
                  bool val = findslot(fbdo.intData());
                  runmotors(val);
                  Firebase.RTDB.setInt(&fbdo, "User/Tower/12917597-77510148/Parked Vehicle/"+plate,botm);
                  Firebase.RTDB.setInt(&fbdo, "User/Tower/12917597-77510148/System/Bottom",botm);
                  Firebase.RTDB.setString(&fbdo, "User/Tower/12917597-77510148/System/free",array_to_string(freeslot, freesize));
                  Firebase.RTDB.setString(&fbdo, "User/Tower/12917597-77510148/System/occupied",array_to_string(occupiedslot, occusize));
                  Firebase.RTDB.deleteNode(&fbdo,"User/Tower/12917597-77510148/Parked Vehicle/"+plate);
                  Firebase.RTDB.deleteNode(&fbdo,"User/Tower/12917597-77510148/"+plate);
                  Firebase.RTDB.setBool(&fbdo, "User/Tower/12917597-77510148/Message",true);
                }
                else
                  Firebase.RTDB.setBool(&fbdo, "User/Tower/12917597-77510148/Message",true);
            }
          }
        }
    else {
      Serial.println("FAILED to access plate");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
}
void change_slotspos_in_tower(int bot){
  switch(bot){
    case 1:
      tower[0][1]=1;
      tower[1][0]=6;
      tower[1][1]=2;
      tower[2][0]=5;
      tower[2][1]=3;
      tower[3][0]=4;
      return;
    case 2:
      tower[0][1]=2;
      tower[1][0]=1;
      tower[1][1]=3;
      tower[2][0]=6;
      tower[2][1]=4;
      tower[3][0]=5;
      return;
    case 3:
      tower[0][1]=3;
      tower[1][0]=2;
      tower[1][1]=4;
      tower[2][0]=1;
      tower[2][1]=5;
      tower[3][0]=6;
      return;
    case 4:
      tower[0][1]=4;
      tower[1][0]=3;
      tower[1][1]=5;
      tower[2][0]=2;
      tower[2][1]=6;
      tower[3][0]=1;
      return;
    case 5:
      tower[0][1]=5;
      tower[1][0]=4;
      tower[1][1]=6;
      tower[2][0]=3;
      tower[2][1]=1;
      tower[3][0]=2;
      return;
    case 6:
      tower[0][1]=6;
      tower[1][0]=5;
      tower[1][1]=1;
      tower[2][0]=4;
      tower[2][1]=2;
      tower[3][0]=3;
      return;
  }
}