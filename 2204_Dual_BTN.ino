//ArduinoJsonのバージョンに注意！

//#include <IOXhop_FirebaseStream.h>
//#include <IOXhop_FirebaseESP32.h>

//デュアルボタンで自己保持で回転灯を点灯させるプログラム

//https://hooks.slack.com/services/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

//テスト用
//const String webHook_URL = "/services/THP92F74L/B03C4SJPR8S/Eav0RRsIn5N7u4S6GKAo5Rkr";

//機械場用（m2-pr）
const String webHook_URL = "/services/THP92F74L/B03T8583Z50/yWxjMz5hNbGOcAUz26FGaH07";

//HPライン
//const String webHook_URL = "/services/THP92F74L/B03FN9NM56K/XCIuJbt9zXp3rlkWwXdRdxn2"; 

//HFラインエラー
//const String webHook_URL = "/services/THP92F74L/B03DBEKB3NY/W4N1EmcYWsPwY83Kz3guxKkF";

//仕上ハンドライン
//const String webHook_URL = "/services/THP92F74L/B03FNCS80VD/dNt13ENC60LnaJzzpiGG1YuD";

//仕上混流ライン
//const String webHook_URL = "/services/THP92F74L/B03FNAP0J9M/Wd98uBQwFmI1UlvbOxihe8qR";


//画面に表示されるエリア記号
//const String ThisAndon = "HP";
//const String ThisAndon = "SHF";
const String ThisAndon = "M2";


//2022/5/17
//Firebaseへの送信失敗時(failed)のリトライを実装するために、ライブラリを従来のものに変更した。
//slack送信を、３分後にもう一度実行するように変更（１回目はTL。２回目はGM、３回目は生技TL/GM又は工場長が対応する）


//2022/4/20
//A fatal error occurred: Failed to connect to ESP32: Timed out waiting for packet header
//上記エラー発生にて、
//1、PROT　HATを外したー＞改善されず
//2,USB-Cを上下逆にして指し直した->うまくいった！
//以下を参考とした
//https://www.mgo-tec.com/blog-entry-trouble-shooting-esp32-wroom.html/3

#include <M5StickCPlus.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include <WiFiMulti.h>
#include <HTTPClient.h>


//https://github.com/mobizt/Firebase-ESP32
//#include <FirebaseESP32.h>



//ArduinoJson のバージョンは、5.13.5であること。レイテスト6.19.4が必要なプログラムもある。
//バージョンが６の場合、エラーとなる。2021/10/25
#include "ArduinoJson.h"

#include <time.h>
#define JST 3600*9

WiFiClientSecure httpsClient;

//const char* WIFI_SSID = "B_IoT_5G";
const char* WIFI_SSID = "B_IoT";
const char* WIFI_PASSWORD = "wF7y82Az";

/// Firebaseに関するもの---------------------------------------------------------
//当初、Getで採用し始めたこのライブラリを使っていたが、エラー処理がイマイチ理解できなかったので、
//従来どおり、下記を使用するように改めた。2022/5/17

//FirebaseData firebaseData;
//const char* const firebase_host = "https://akilog-default-rtdb.firebaseio.com/";
//const char* const firebase_auth = "wrtVZ4r8zqLIAD930oAnb3m10clHKZmOAVrhwboX";

//const char* const is_power_on_key = "isPowerOn";
//const char* const is_sent_IR_key = "isSentIR";



//===Firebase==================================================================
//なぜか突然送信が出来なくなってしまった。
//ライブラリーが壊れているとか、プログラムにエラーがあるとか、色々と考えたのだが、
//RealTimeDatabase のルールで、日付を超えていたから、だった
//完全ルール無しの、ay-603に変更してみたことで気がついた。／／／情けない 2022/6/10(Fri)
//2999年6月9日までと改めた。その頃の世の中はいったいどんなだろうか。

//ライブラリ https://github.com/ioxhop/IOXhop_FirebaseESP32　からzipを再入手して、IDEからインクルードし直したら治った。　2021/10/25
#include <IOXhop_FirebaseESP32.h>  

//#define FIREBASE_DB_URL "https://n-iot-a25db.firebaseio.com/" // 
#define FIREBASE_DB_URL "https://ay-vue.firebaseio.com/" // 
//#define FIREBASE_DB_URL "https://iot-sandbox-ea132.firebaseio.com/" // 
//#define FIREBASE_DB_URL "https://bishamonn1-46718.firebaseio.com/" // 
//#define FIREBASE_DB_URL "https://ay-603.firebaseio.com/" // 
//#define FIREBASE_DB_URL "https://akilog-default-rtdb.firebaseio.com/" //

String user_path = "SP_Status";
String user_path2 = "NishioMachineCT";

unsigned nextFirebaseTry = 0; //1分ごとにセンサーデータを送るためのタイマー




const String JustNowAndonStatus = "JustNowStatus";

//------------------------------------------------------------------------------

//===機械の設定====================================================  

//String MachineNo = "GT999";  //
//String MachineNo = "FSLS1";  //
String MachineNo = "SHF";  //




//===IMU==============================================

float accX = 0.0f;
float accY = 0.0f;
float accZ = 0.0f;
float x = 80.0f;
float v = 0.0f;

float x2 = 80.0f;
float v2 = 0.0f;

//====================================================

// 時計の設定
//https://qiita.com/tranquility/items/5d0b1a259a0570be35ec
const char* ntpServer = "ntp.jst.mfeed.ad.jp"; // NTPサーバー
const long  gmtOffset_sec = 9 * 3600;          // 時差９時間
const int   daylightOffset_sec = 0;            // サマータイム設定なし

RTC_TimeTypeDef RTC_TimeStruct; // 時刻
RTC_DateTypeDef RTC_DateStruct; // 日付

unsigned long setuptime; // スリープ開始判定用（ミリ秒）
//unsigned long setuptime2nd; // wifi再接続不調時のリブート用（ミリ秒）

int sMin = 0; // 画面書き換え判定用（分）
int sHor = 0; // 画面書き換え判定用（時）
int sDat = 0; //
//===Firebase==============================================
//#define FIREBASE_DB_URL "https://ay-vue.firebaseio.com/" // 
//
//String user_path = "SP_Status";
//String user_path2 = "NishioMachineCT";
//
//unsigned nextFirebaseTry = 0; //1分ごとにセンサーデータを送るためのタイマー
//


//===プログラムで使う変数==================================================================

time_t nowTime;
String startTime = "999";
long beforeMiriSec = 0 ;

long swStartMills=0; //前回実行の時間を格納する。
long nowMillis;

//WiFiMulti WiFiMulti;
int count = 1;  // ③

//boolean ErrBool = false ;  //ERR1でtrurとする　○秒以下では処理を行わない、の為の実装
//boolean RunBool = false ;  //RUN1でtrueとする　○秒以下では処理を行わない、の為の実装

boolean SetKKT = false ;  //BtnA １秒以上の長押しで!(否定演算子)で反転させる。

long MyCount = 0;

bool BoolNowStatus = false ; //起動中はtrue

bool BoolError = false;

int intNowMillis = 0;
int incrementSeconds = 0;

boolean RunBool1 = true ;  //RUN1でtrueとする　○秒以下では処理を行わない、の為の実装
boolean RunBool2 = false ;  //RUN1でtrueとする　○秒以下では処理を行わない、の為の実装

int last_value1 = 0,last_value2 = 0;
int cur_value1 = 0,cur_value2 = 0;

long MyMillis = millis();

//// 接続するpin番号の定義をしています。
int R_switch=32;
int B_switch=33;
int LED_pin=10;
int LED_pin2=26;



//// ここでメインのカウンターです。
boolean counter = true;

int MySet = 1;

int intZangyou = 0;  //残業時間、０時間、１時間、２時間　Bボタンで変化させ、モニター中断右端に表示


//int last_value1 = 0,last_value2 = 0;
//int cur_value1 = 0,cur_value2 = 0;

//long MyMillis = millis();






//=======================================================================================
#define TFT_BLACK       0x0000      /*   0,   0,   0 */
#define TFT_NAVY        0x000F      /*   0,   0, 128 */
#define TFT_DARKGREEN   0x03E0      /*   0, 128,   0 */
#define TFT_DARKCYAN    0x03EF      /*   0, 128, 128 */
#define TFT_MAROON      0x7800      /* 128,   0,   0 */
#define TFT_PURPLE      0x780F      /* 128,   0, 128 */
#define TFT_OLIVE       0x7BE0      /* 128, 128,   0 */
#define TFT_LIGHTGREY   0xC618      /* 192, 192, 192 */
#define TFT_DARKGREY    0x7BEF      /* 128, 128, 128 */
#define TFT_BLUE        0x001F      /*   0,   0, 255 */
#define TFT_GREEN       0x07E0      /*   0, 255,   0 */
#define TFT_CYAN        0x07FF      /*   0, 255, 255 */
#define TFT_RED         0xF800      /* 255,   0,   0 */
#define TFT_MAGENTA     0xF81F      /* 255,   0, 255 */
#define TFT_YELLOW      0xFFE0      /* 255, 255,   0 */
#define TFT_WHITE       0xFFFF      /* 255, 255, 255 */
#define TFT_ORANGE      0xFDA0      /* 255, 180,   0 */
#define TFT_GREENYELLOW 0xB7E0      /* 180, 255,   0 */
#define TFT_PINK        0xFC9F
//=======================================================================================


//○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○
const char *server = "hooks.slack.com";

const String message = "slack emoji test";

//String NowAreaName = "仕上ハンドポンプ〜";
String NowAreaName = "機械場〜";

//const char *json4 ="{\"text\": \"" + message + "\", \"icon_emoji\": \":space_invader:\"}";
//const char *json4 ="{\"text\": \"テストです\", \"icon_emoji\": \":space_invader:\"}";
//const char *json4 ="{\"text\": \"テストです:space_invader:\"}"; //一応は成功か？文章内の絵文字としてスペースインベーダーが現れた！
const char *json ="{\"text\": \":pig:あんどん作動！TLは現場へ\"}"; //やっぱり成功といえる。本当に実装したいのは、アイコンの方！

//const char *json4 ="{\"icon_emoji\":\":tiger:\",\"text\": \"HPライン送信テストです:snail:\"}"; //やっぱり成功といえる。本当に実装したいのは、アイコンの方！

//const char *json = "{\"text\":\"あんどん作動:ghost！TLは現場へ\",\"icon_emoji\":\":ghost:\",\"username\":\"m5stackpost\"}";
const char *json2 = "{\"text\":\":frog:【３分超！】GMは至急現場へ\",\"icon_emoji\": \":snail:\"}";
const char *json3 = "{\"text\":\":panda_face:【６分超！】あら！？生技TL頼みます！\",\"icon_emoji\":\":tiger:\",\"username\":\"m5stackpost\"}";



bool json2Status = false;
bool json3Status = false;

//以下はsllackの証明で、全て同じ。
const char* slack_root_ca= \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n" \
"QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n" \
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n" \
"CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n" \
"nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n" \
"43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n" \
"T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n" \
"gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\n" \
"BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\n" \
"TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\n" \
"DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\n" \
"hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\n" \
"06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\n" \
"PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\n" \
"YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\n" \
"CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\n" \
"-----END CERTIFICATE-----\n" ;
//○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○○

HTTPClient http;

void slack_connect(int SendCount){
  M5.Beep.tone(7000);
  delay(50);
  M5.Beep.mute();
  // post slack
//  M5.Lcd.println("connect slack.com");




//  webhook送信
  http.begin( server, 443,webHook_URL, slack_root_ca );
//  http.begin( server, 443, "/services/THP92F74L/B03C4SJPR8S/Eav0RRsIn5N7u4S6GKAo5Rkr", slack_root_ca );

//  HFライン
//  http.begin( server, 443, "/services/THP92F74L/B03DBEKB3NY/W4N1EmcYWsPwY83Kz3guxKkF", slack_root_ca );



  String ReturnMSG = "";

  while(ReturnMSG !="200") { //リターン値が、200（正常終了）ではない場合...初期値には""が代入されているので、必ず１回はループする

    //WiFi接続状況を確認する
    if( WiFi.status() != WL_CONNECTED) {
      M5.Lcd.fillRect(5,85,150,60,YELLOW);
      M5.Lcd.setTextSize(2);
      M5.Lcd.setCursor(6, 86, 1);
      M5.Lcd.setTextColor(BLACK); 
      M5.Lcd.printf("Connecting to %s\n", WIFI_SSID);
      int cnt2 = 0;
      while(WiFi.status() != WL_CONNECTED) {
        cnt2++;
        delay(500);
        M5.Lcd.print(".");
        if(cnt2%10==0) {
          WiFi.disconnect();
          WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
          M5.Lcd.println("");
        }
        if(cnt2>=30) {
          ESP.restart();
        }
      }
    }



    //slackへのpost実行
    http.addHeader("Content-Type", "application/json" );
    switch(SendCount){
      case 0:
            ReturnMSG = http.POST((uint8_t*)json, strlen(json));  
            break;
      case 1:
            ReturnMSG = http.POST((uint8_t*)json2, strlen(json2));  
            break;
      case 2:
            ReturnMSG = http.POST((uint8_t*)json3, strlen(json3));  
            break;
    }
    
  }
  
  
  Serial.println(ReturnMSG);

  if(ReturnMSG=="200"){
    Serial.println("slack送信成功！");
  }
  
//  M5.Lcd.println("post hooks.slack.com");

}

void printLocalTime() {
  
  nowMillis = millis()-swStartMills;

  if(RunBool2==true){
    incrementSeconds ++;
    
    if(incrementSeconds>180 && json2Status==false){
      slack_connect(1);
      json2Status=true;
    }
    
    if(incrementSeconds>360 && json3Status==false){
      slack_connect(2);
      json3Status=true;
    }
    
//    Serial.println(incrementSeconds);  
    digitalWrite(LED_pin,LOW);        //内蔵LEDは、LOWで点灯
    delay(50);
    digitalWrite(LED_pin,HIGH);        //内蔵LEDは、LOWで点灯
  }
  
  
  static const char *wd[7] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};
  M5.Rtc.GetTime(&RTC_TimeStruct); // 時刻の取り出し
  M5.Rtc.GetData(&RTC_DateStruct); // 日付の取り出し



  M5.Lcd.setTextSize(1);
  
  if (sMin == RTC_TimeStruct.Minutes) {
    // 秒の表示エリアだけ書き換え
    if(RunBool2){
       M5.Lcd.fillRect(140,5,150,70,BLUE);
    }else{
       M5.Lcd.fillRect(140,5,150,70,BLACK);
    }
   
  } else {
    if(RunBool2){
          M5.Lcd.fillScreen(BLUE);
//       M5.Lcd.fillRect(140,5,150,70,BLUE);
    }else{
       M5.Lcd.fillScreen(BLACK);
    }
      


  }
//
  
  M5.Lcd.setTextColor(GREEN);
  M5.Lcd.setCursor(0, 10, 7);  //x,y,font 7:48ピクセル7セグ風フォント

  M5.Lcd.printf("%02d:%02d:%02d", RTC_TimeStruct.Hours, RTC_TimeStruct.Minutes,RTC_TimeStruct.Seconds); // 時分を表示

  M5.Lcd.setTextColor(PINK);
  M5.Lcd.fillRect(5,85,150,60,BLACK);
  M5.Lcd.setCursor(5,85,7);
  M5.Lcd.print(incrementSeconds);

  M5.Lcd.setCursor(150, 100, 1);  //x,y,font 1:Adafruit 8ピクセルASCIIフォント
  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextColor(CYAN);
  M5.Lcd.print(ThisAndon);
  //日付の表示^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  M5.Lcd.setCursor(30, 67, 1);  //x,y,font 1:Adafruit 8ピクセルASCIIフォント
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(WHITE);
//  M5.Lcd.printf("D:%04d.%02d.%02d %s\n", RTC_DateStruct.Year, RTC_DateStruct.Month, RTC_DateStruct.Date, wd[RTC_DateStruct.WeekDay]);
  M5.Lcd.printf("%04d.%02d.%02d %s\n", RTC_DateStruct.Year, RTC_DateStruct.Month, RTC_DateStruct.Date, wd[RTC_DateStruct.WeekDay]);
  //^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  
  sMin = RTC_TimeStruct.Minutes; // 「分」を保存
  sHor = RTC_TimeStruct.Hours; // 「時」を保存
  sDat = RTC_DateStruct.Date ; //日付を保存


  
  



}


WiFiClientSecure client;

void wifiConnect(){
  M5.Lcd.setRotation(3);
  M5.Lcd.setCursor(0, 0, 1);
  M5.Lcd.fillScreen(BLACK);
  
  int cnt=0;
  M5.Lcd.printf("Connecting to %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while(WiFi.status() != WL_CONNECTED) {
    cnt++;
    delay(500);
    M5.Lcd.print(".");
    if(cnt%10==0) {
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      M5.Lcd.println("");
    }
    if(cnt>=60) {
      ESP.restart();
    }
  }
//  
//  int MyRand = 16581265 - random(23);
//  lineNotify("WiFiに再接続しました。",stickerPackage3,MyRand);
  M5.Lcd.printf("\nWiFi connected\n");
  delay(1000);
  M5.Lcd.fillScreen(BLACK);
}
//◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆
void FirebaseAndonSend(bool NowStatus){
  M5.Lcd.setCursor(0,0);
  M5.Lcd.setTextSize(2);
  M5.Lcd.printf("Send to Firebase");
  int FirebaseAndonCount =0;
  Firebase.set("/AndonStatus/"+JustNowAndonStatus,NowStatus);
  while(Firebase.failed()){
    FirebaseAndonCount ++;
    Serial.print("★★★Firebase 送信失敗★★★リトライ");
    Serial.print(FirebaseAndonCount);
    Serial.println(Firebase.error());  
//      Serial.println(String(t) + "000");
    
    delay(500);
    M5.Lcd.print(".");
    if(FirebaseAndonCount%10==0){       //500msecX１０回で５秒経過
      M5.Lcd.println("");
      M5.Lcd.println("Send to Firebase"+FirebaseAndonCount);
      Serial.println("Firebase 再送信実行！");
      //WiFiを検査し、切断されている場合は再接続を試みる””””””””””””””””””””””””””””””””””””””
      while(WiFi.status() != WL_CONNECTED) {
        Serial.println("wifi再接続実行！"); 
        wifiConnect();
      }
      Firebase.set("/AndonStatus/"+JustNowAndonStatus,NowStatus);
    }

    

    if(FirebaseAndonCount >100){
      Serial.print("リトライが100回を超過しました。再起動します");
      M5.Lcd.fillScreen(RED);
      M5.Lcd.printf("Restart later 5sec");
      delay(5000);
      ESP.restart();
      break;
    }
  }


}

//◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆◆

void setup() {

  Serial.begin(9600);
  client.setInsecure();
  
  M5.begin();
  M5.IMU.Init();
  Serial.println("ここまでオッケー");
  wifiConnect();
  Serial.println("どうなんでしょうか");
//  Firebase.begin(FIREBASE_DB_URL);   // ④
  
  configTime( JST, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");

// Get local time
    struct tm timeInfo;
    if (getLocalTime(&timeInfo)) {
      M5.Lcd.print("NTP : ");
      M5.Lcd.println(ntpServer);

      // Set RTC time
      RTC_TimeTypeDef TimeStruct;
      TimeStruct.Hours   = timeInfo.tm_hour;
      TimeStruct.Minutes = timeInfo.tm_min;
      TimeStruct.Seconds = timeInfo.tm_sec;
      M5.Rtc.SetTime(&TimeStruct);

      RTC_DateTypeDef DateStruct;
      DateStruct.WeekDay = timeInfo.tm_wday;
      DateStruct.Month = timeInfo.tm_mon + 1;
      DateStruct.Date = timeInfo.tm_mday;
      DateStruct.Year = timeInfo.tm_year + 1900;
      M5.Rtc.SetData(&DateStruct);
    }

  time_t t;
  t = time(NULL);
  client.setInsecure();
  //▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲
//    Wire.begin();               // I2Cを初期化する
//    while (!bme.begin(0x76)) {  // BMP280を初期化する
//        M5.Lcd.println("BMP280 init fail");
//    }
  //▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲
  

//  // M5.begin();
// pinMode(R_switch,INPUT);
 pinMode(B_switch,INPUT);
 pinMode(LED_pin,OUTPUT);
 pinMode(LED_pin2,OUTPUT);

 digitalWrite(LED_pin,HIGH);

 printLocalTime();

// 
// M5.Lcd.fillCircle(225, 45, 12, BLUE);   // 残業時間の初期表示
// M5.Lcd.setTextColor(WHITE);
// M5.Lcd.setTextSize(2);
// M5.Lcd.setCursor(220, 38,1);
// M5.Lcd.print(intZangyou);
//
//


  M5.begin();
//
//  // M5.begin();
 pinMode(R_switch,INPUT);
 pinMode(B_switch,INPUT);
 pinMode(LED_pin,OUTPUT);
 pinMode(LED_pin2,OUTPUT);

 digitalWrite(LED_pin,HIGH);

// Firebase.begin(firebase_host, firebase_auth);
Firebase.begin(FIREBASE_DB_URL);   // ④
// Firebase.reconnectWiFi(true);

 
}

void loop() {

 M5.update();
  
   //WiFiを検査し、切断されている場合は再接続を試みる””””””””””””””””””””””””””””””””””””””
  while(WiFi.status() != WL_CONNECTED) { 
    wifiConnect();
  }

    if(M5.BtnA.wasPressed()){
      slack_connect(0);
//      String message = "A button";
//      int MyRand = 16581265 - random(23);
//      lineNotify(message, stickerPackage3, MyRand);
    }
  


    if(M5.BtnB.wasPressed()){
//      intZangyouPrint(true);
//      delay(10);
        ESP.restart();
    }

  int btnA = M5.BtnA.pressedFor(1000); // ホームボタン
  if(btnA==1){
    M5.Beep.tone(7000);
    digitalWrite(LED_pin2,HIGH);
    delay(50);
    M5.Beep.mute();
    
    if(SetKKT){
      
      SetKKT = false;
      M5.Lcd.fillRect(155, 80, 85, 65, BLACK);
      M5.Lcd.drawRect(155, 80, 85, 65, WHITE);
//      sendToFirebase(MachineNo,"KKT2");
      delay(10); 
      
    }else{

      SetKKT = true;
      M5.Lcd.fillRect(155, 80, 85, 65, PINK);
      M5.Lcd.drawRect(155, 80, 85, 65, BLUE);
//      sendToFirebase(MachineNo,"KKT1");
      delay(10);
    }

    delay(1000);
    digitalWrite(LED_pin2,LOW);
   
  }


  if(millis()>beforeMiriSec+1000){
      //一秒ごとに実行される、時刻更新処理
      printLocalTime();
      beforeMiriSec = millis();
  }

//
   delay(10);


  
  cur_value1 = 1;
  cur_value2 = 1;
//  cur_value1 = digitalRead(B_switch); // read the value of BUTTON. 
//  cur_value2 = digitalRead(R_switch);
//
  delay(10);

    M5.update();

 if(digitalRead(B_switch)==LOW){
    cur_value1 = digitalRead(B_switch);
    delay(300);
    cur_value2 = digitalRead(B_switch);
    if(cur_value1==cur_value1){
      if(RunBool1){

        M5.Lcd.fillScreen(BLUE);
        digitalWrite(LED_pin2,HIGH);      //外付けパトライトは、HIGH で通電
//        Firebase.set("/AndonStatus/"+ThisAndon,true);
        FirebaseAndonSend(true);

        Serial.println("❏❏❏❏❏❏❏点灯！");
         
        delay(100);
        incrementSeconds = 0;
        RunBool2=true;  //起動後の秒数を、 incrementSecondsに足し込むためのトリガー
        json2Status=false; //３分超メッセージのステータス
        json3Status=false; //６分超メッセージのステータス
        slack_connect(0); //スラック送信        
        
//        Firebase.setBool(firebaseData, is_power_on_key,true);
//        M5.Lcd.fillCircle(200, 110, 20, CYAN);
//        M5.Lcd.drawCircle(225, 45, 14, BLUE);
        
//        M5.Lcd.fillRect(30,90,200,40,BLUE);
        delay(10);
        if(digitalRead(R_switch)==HIGH){
          RunBool1=true;
        }
      }else{
        RunBool1=true;
      }
    }


    
  }else if(!RunBool1){
    M5.Lcd.fillScreen(RED);
    digitalWrite(LED_pin2,LOW);
    delay(100);
//    Firebase.set("/AndonStatus/"+ThisAndon,false);
    FirebaseAndonSend(false);

    Serial.println("RunBoolの値その２");
    Serial.print(RunBool1);

  
    Serial.print("消灯ーーーーーーー");
    
    delay(500);
    M5.Lcd.fillScreen(BLACK);
    
//    Firebase.setBool(firebaseData, is_power_on_key,false);  //Firebaseに、falseを代入
    RunBool1 = true;                    //これを入れることで、繰り返しが無くなる
  }

  delay(10);

  if(digitalRead(R_switch)==LOW){
    cur_value1 = digitalRead(B_switch);
    delay(300);
    cur_value2 = digitalRead(B_switch);

    RunBool2=false;
    incrementSeconds = 0;
    
    M5.Lcd.fillRect(20,90,200,40,RED);
    delay(300);
    M5.Lcd.fillRect(20,90,200,40,BLACK);
    if(cur_value1==cur_value1){
      RunBool1=false;
    }
  }
  delay(10);
 

}
