// basic
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <ESP8266httpUpdate.h>

#include <ArduinoJson.h>
#include <FS.h>

// firmware version
#define SOFT_NAME "bicoqueRelay"
#define SOFT_VERSION "0.0.01"
#define SOFT_DATE "2020-11-30"

#define DEBUG 1


// Init
bool networkEnable      = 1;
bool internetConnection = 0;
IPAddress ip;
int wifiActivationTempo = 600; // Time to enable wifi if it s define disable

// Relay
String relayStatus       = "";
const int RelayPin       = 0;

// Timer variable
int timerPerHourLast     = 0;

// Wifi AP info for configuration
const char* wifiApSsid = SOFT_NAME;


// Update info
#define BASE_URL "http://esp.bicoque.com/" SOFT_NAME "/"
#define UPDATE_URL BASE_URL "update.php"


// Web server info
ESP8266WebServer server(80);


// config default
typedef struct configWifi 
{
  boolean enable      = 1;
  String ssid         = "";
  String password     = "";
};
typedef struct configRelay
{
  boolean enable     = 1;
  boolean status     = 0;
};
typedef struct configCloud
{
  boolean enable = 1;
  String  url    = "";
  String  apiKey = "";
};
typedef struct config
{
  configWifi wifi;
  configRelay relay;
  configCloud cloud;
  boolean alreadyStart      = 0;
  String softName           = SOFT_NAME;
  String softVersion        = SOFT_VERSION;
  boolean checkUpdateEnable = 1;
};
config softConfig;



//----------------
//
//
// Functions
//
//
//----------------
// ********************************************
// SPIFFFS storage Functions
// ********************************************
String storageDir(String name)
{
    String output;
    Dir dir = SPIFFS.openDir(name);
    while (dir.next()) 
    {
      output += dir.fileName();
      output += " - ";
      File f = dir.openFile("r");
      output += f.size();
      output += "\n";
    }

   return output;
}
String storageRead(String fileName)
{
  String dataText;

  File file = SPIFFS.open(fileName, "r");
  if (!file)
  {
    //logger("FS: opening file error");
    //-- debug
  }
  else
  {
    size_t sizeFile = file.size();
    if (sizeFile > 400 and 1 == 0)
    {
       Serial.println("Size of file is too clarge");
    }
    else
    {
      dataText = file.readString();
    }
  }

  file.close();
  return dataText;
}
void storageDel(String fileName)
{
  SPIFFS.remove(fileName);
}

bool storageWrite(char *fileName, String dataText)
{
  File file = SPIFFS.open(fileName, "w");
  file.println(dataText);

  file.close();

  return true;
}

bool storageAppend(String fileName, String dataText)
{
  File file = SPIFFS.open(fileName, "a");
  file.print(dataText);
  file.close();
  return true;
}

bool storageClear(char *fileName)
{
  SPIFFS.remove(fileName);
  return true;
}



#
#
#  // C O N F I G
#
#


String configSerialize()
{
  String dataJsonConfig;
  DynamicJsonDocument jsonConfig(800);
  JsonObject jsonConfigWifi    = jsonConfig.createNestedObject("wifi");
  JsonObject jsonConfigRelay    = jsonConfig.createNestedObject("relay");
  JsonObject jsonConfigCloud   = jsonConfig.createNestedObject("cloud");

  jsonConfig["alreadyStart"]      = softConfig.alreadyStart;
  jsonConfig["checkUpdateEnable"] = softConfig.checkUpdateEnable;
  jsonConfig["softName"]          = softConfig.softName;
  jsonConfig["softVersion"]       = softConfig.softVersion;
  jsonConfigWifi["ssid"]          = softConfig.wifi.ssid;
  jsonConfigWifi["password"]      = softConfig.wifi.password;
  jsonConfigWifi["enable"]        = softConfig.wifi.enable;
  jsonConfigRelay["enable"]       = softConfig.relay.enable;
  jsonConfigRelay["status"]       = softConfig.relay.status;
  jsonConfigCloud["apiKey"]       = softConfig.cloud.apiKey;
  jsonConfigCloud["url"]          = softConfig.cloud.url;
  jsonConfigCloud["enable"]       = softConfig.cloud.enable;
  serializeJson(jsonConfig, dataJsonConfig);

  return dataJsonConfig;
}


bool configSave()
{
  String dataJsonConfig = configSerialize();
  bool fnret = storageWrite("/config.json", dataJsonConfig);

  if (DEBUG) 
  {
    Serial.print("write config : ");
    Serial.println(dataJsonConfig);
    Serial.print("Return of write : "); Serial.println(fnret);
  }

  return fnret;
}

bool configRead(config &ConfigTemp)
{
  String dataJsonConfig;
  DynamicJsonDocument jsonConfig(800);

  dataJsonConfig = storageRead("/config.json");

  DeserializationError jsonError = deserializeJson(jsonConfig, dataJsonConfig);
  if (jsonError)
  {
    Serial.println("Got Error when deserialization : "); //Serial.println(jsonError);
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(jsonError.c_str());
    return false;
    // Getting error when deserialise... I don know what to do here...
  }

  ConfigTemp.alreadyStart      = jsonConfig["alreadyStart"];
  ConfigTemp.checkUpdateEnable = jsonConfig["checkUpdateEnable"];
  ConfigTemp.softName          = jsonConfig["softName"].as<String>();
  ConfigTemp.softVersion       = jsonConfig["softVersion"].as<String>();
  ConfigTemp.wifi.ssid         = jsonConfig["wifi"]["ssid"].as<String>();
  ConfigTemp.wifi.password     = jsonConfig["wifi"]["password"].as<String>();
  ConfigTemp.wifi.enable       = jsonConfig["wifi"]["enable"];
  ConfigTemp.relay.enable      = jsonConfig["relay"]["enable"];
  ConfigTemp.relay.status      = jsonConfig["relay"]["status"];
  ConfigTemp.cloud.enable      = jsonConfig["cloud"]["enable"];
  ConfigTemp.cloud.apiKey      = jsonConfig["cloud"]["apiKey"].as<String>();
  ConfigTemp.cloud.url         = jsonConfig["cloud"]["url"].as<String>();

  return true;

}

void configDump(config ConfigTemp)
{
  Serial.println("wifi data :");
  Serial.print("  - ssid : "); Serial.println(ConfigTemp.wifi.ssid);
  Serial.print("  - password : "); Serial.println(ConfigTemp.wifi.password);
  Serial.print("  - enable : "); Serial.println(ConfigTemp.wifi.enable);
  Serial.println("Relay data :");
  Serial.print("  - enable : "); Serial.println(ConfigTemp.relay.enable);
  Serial.print("  - status : "); Serial.println(ConfigTemp.relay.status);
  Serial.println("Cloud data :");
  Serial.print("  - enable : "); Serial.println(ConfigTemp.cloud.enable);
  Serial.print("  - apiKey : "); Serial.println(ConfigTemp.cloud.apiKey);
  Serial.print("  - url : "); Serial.println(ConfigTemp.cloud.url);
  Serial.println("General data :");
  Serial.print("  - alreadyStart : "); Serial.println(ConfigTemp.alreadyStart);
  Serial.print("  - checkUpdateEnable : "); Serial.println(ConfigTemp.checkUpdateEnable);
  Serial.print("  - softName : "); Serial.println(ConfigTemp.softName);
  Serial.print("  - softVersion : "); Serial.println(ConfigTemp.softVersion);
}



// ------------------------------
// Wifi
// ------------------------------
bool wifiConnectSsid(const char* ssid, const char* password)
{
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int c = 0;
  Serial.println("Waiting for Wifi to connect");
  while ( c < 80 ) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    delay(500);
    Serial.print(WiFi.status());
    c++;
  }
  Serial.println("");
  Serial.println("Connect timed out, opening AP");
  return false;
}
String wifiScan(void)
{
  String st;
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    st = "<ol>";
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      st += "<li>";
      st += WiFi.SSID(i);
      st += " (";
      st += WiFi.RSSI(i);
      st += ")";
      st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
      st += "</li>";
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
    st += "</ol>";
  }
  Serial.println("");
  delay(100);

  return st;
}
void wifiReset()
{
  softConfig.wifi.ssid     = "";
  softConfig.wifi.password = "";
  configSave();

  WiFi.mode(WIFI_OFF);
  WiFi.disconnect();
}
int wifiPower()
{
  int dBm = WiFi.RSSI();
  int quality;
  // dBm to Quality:
  if (dBm <= -100)     { quality = 0; }
  else if (dBm >= -50) { quality = 100; }
  else                 { quality = 2 * (dBm + 100); }

  return quality;
}

void wifiDisconnect()
{
  WiFi.mode( WIFI_OFF );
}

bool wifiConnect(String ssid, String password)
{
    if ( DEBUG ) { Serial.println("Enter wifi config"); }

    if (DEBUG) { Serial.print("Wifi: Connecting to '"); Serial.print(ssid); Serial.println("'"); }

    // Unconnect from AP
    WiFi.mode(WIFI_AP);
    WiFi.disconnect();

    // Connecting to SSID
    WiFi.mode(WIFI_STA);
    WiFi.hostname(SOFT_NAME);

    bool wifiConnected = wifiConnectSsid(ssid.c_str(), password.c_str());

    if (wifiConnected)
    {
      if (DEBUG) { Serial.println("WiFi connected"); }
      internetConnection = 1;

      ip = WiFi.localIP();

      return true;
    }
    else
    {
      // Display
    }

    // If SSID connection failed. Go into AP mode

    // Disconnecting from Standard Mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Connect to AP mode
    WiFi.mode(WIFI_AP);
    WiFi.hostname(SOFT_NAME);

    if (DEBUG) { Serial.println("Wifi config not present. set AP mode"); }
    WiFi.softAP(wifiApSsid);
    if (DEBUG) {Serial.println("softap"); }

    ip = WiFi.softAPIP();

    return false;
}


// --------------------------------
// Update
// --------------------------------
void updateCheck(bool displayScreen)
{
    if (softConfig.checkUpdateEnable == 0)
    {
      return;
    }


    if (displayScreen)
    {
      // display.println("- check update");
      // display.display();
    }

    String updateUrl = UPDATE_URL;
    Serial.println("Check for new update at : "); Serial.println(updateUrl);
    ESPhttpUpdate.rebootOnUpdate(1);
    t_httpUpdate_return ret = ESPhttpUpdate.update(updateUrl, SOFT_VERSION);
    //t_httpUpdate_return ret = ESPhttpUpdate.update(updateUrl, ESP.getSketchMD5() );

    Serial.print("return : "); Serial.println(ret);
    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;

      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;

      default:
        Serial.print("Undefined HTTP_UPDATE Code: "); Serial.println(ret);
    }

    if (displayScreen)
    {
      // display.println("update done");
      // display.display();
    }
}

void updateWebServerFile(String fileName)
{
    HTTPClient httpClient;
    String urlTemp = BASE_URL;
    urlTemp += fileName;

    if (DEBUG)
    {
      Serial.print("updateWebServerFiles : Url for external use : "); Serial.println(urlTemp);
    }

    httpClient.begin(urlTemp);
    int httpResponseCode = httpClient.GET();
    if (httpResponseCode > 0)
    {
      Serial.print("Size from file : "); Serial.println(httpClient.getSize());

      File file = SPIFFS.open(fileName, "w");
      int bytesWritten = httpClient.writeToStream(&file);
      //String payload = httpClient.getString();
      //httpClient.end();
      //file.println(payload);
      file.close();
 
      Serial.print("BytesWritten : "); Serial.println(bytesWritten);

    }
}





// -------------------------------
// Logger function
// -------------------------------
String urlencode(String str)
{
  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++)
  {
    c = str.charAt(i);
    if (c == ' ')
    {
      encodedString += '+';
    }
    else if (isalnum(c))
    {
      encodedString += c;
    }
    else
    {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9)
      {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9)
      {
        code0 = c - 10 + 'A';
      }
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }

  return encodedString;
}

void logger(String message)
{
  if (internetConnection)
  {
    HTTPClient httpClientPost;
    String urlTemp = BASE_URL;
    urlTemp       += "log.php";
    String data    = "message=";
    data          += urlencode(message);

    httpClientPost.begin(urlTemp);
    httpClientPost.addHeader("Content-Type", "application/x-www-form-urlencoded");
    httpClientPost.POST(data);
    httpClientPost.end();
  }
}



//
//--   R E L A Y 
//
void relayOn()
{

  if (DEBUG) { Serial.println("Set Relay to on"); }
  digitalWrite(RelayPin, HIGH);
  relayStatus = "on";
  softConfig.relay.status = 1;
  configSave();
}

void relayOff()
{

  if (DEBUG) { Serial.println("Set Relay to off"); }
  digitalWrite(RelayPin, LOW);
  relayStatus = "off";
  softConfig.relay.status = 0;
  configSave();
}








// ********************************************
// WebServer
// ********************************************
void webReboot()
{
  String message = "<!DOCTYPE HTML>";
  message += "<html>";
  message += "Rebbot in progress...<b>";
  message += "</html>\n";
  server.send(200, "text/html", message);

  ESP.restart();
}

void webFsDir()
{
  String directory    = server.arg("directory");

  String message = storageDir(directory);
  server.send(200, "text/html", message);
}
void webFsRead()
{
  String file    = server.arg("file");

  String message = storageRead(file);
  server.send(200, "text/html", message);
}
void webFsDel()
{
  String file    = server.arg("file");

  storageDel(file);
  server.send(200, "text/html", "done");
}
void webFsDownload()
{
  String file    = server.arg("file");

  updateWebServerFile(file);
  server.send(200, "text/html", "done");
}



void webApiConfig()
{
  String dataJsonConfig = configSerialize();
  server.send(200, "application/json", dataJsonConfig);
}

void webApiRelayStatus()
{
  server.send(200, "text/html", relayStatus);
}


void webNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " Name: " + server.argName(i) + " - Value: " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void webWrite()
{
  String message;

  String relayEnable     = server.arg("relayEnable");
  String relayStatus     = server.arg("relayStatus");

  String cloudUrl        = server.arg("cloudUrl");
  String cloudApiKey     = server.arg("cloudApiKey");
  String cloudEnable     = server.arg("cloudEnable");

  String wifiEnable      = server.arg("wifiEnable");

  String alreadyBoot     = server.arg("alreadStart");
  String checkUpdate     = server.arg("checkUpdateEnable");


  // Global ----------  
  if (checkUpdate != "")
  {
    softConfig.checkUpdateEnable = false;
    if ( checkUpdate == "1") { softConfig.checkUpdateEnable = true; }
    configSave();
  }
  if ( alreadyBoot != "")
  {
    softConfig.alreadyStart = false;
    if ( alreadyBoot == "1") { softConfig.alreadyStart = true; }
    configSave();
  }

  // Wifi
  if ( wifiEnable != "")
  {
    softConfig.wifi.enable = false;
    if ( wifiEnable == "1") { softConfig.wifi.enable = true; }
    configSave();
  }

  // Relay
  if (relayEnable != "")
  {
    softConfig.relay.enable = false;
    if ( relayEnable == "1") { softConfig.relay.enable = true; }
    configSave();
  }
  if (relayStatus != "")
  {
    if ( relayStatus == "1") 
    { 
      relayOn(); 
    }
    else
    { 
      relayOff(); 
    }
  }

  // Cloud
  if (cloudEnable != "")
  {
    softConfig.cloud.enable = false;
    if ( cloudEnable == "1") { softConfig.cloud.enable = true; }
    configSave();
  }
  if (cloudUrl != "")
  {
    softConfig.cloud.url = cloudUrl;
    configSave();
  }
  if (cloudApiKey != "")
  {
    softConfig.cloud.apiKey = cloudApiKey;
    configSave();
  }


  Serial.println("Write done");
  message += "Write done...\n";
  server.send(200, "text/plain", message);
}

void webWifiSetup()
{
  String wifiList = wifiScan();
  String message = "<!DOCTYPE HTML>";
  message += "<html>";

  message += SOFT_NAME;
  message += "<br><br>\n";

  message += "Please configure your Wifi : <br>\n";
  message += "<p>\n";
  message += wifiList;
  message += "</p>\n<form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input type='submit'></form>\n";
  message += "</html>\n";
  server.send(200, "text/html", message);
}

void webWifiWrite()
{
  String content;
  int statusCode;
  String qsid = server.arg("ssid");
  String qpass = server.arg("pass");
  if (qsid.length() > 0 && qpass.length() > 0)
  {
    Serial.print("Debug : qsid : ");Serial.println(qsid);
    Serial.print("Debug : qpass : ");Serial.println(qpass);
    softConfig.wifi.ssid     = qsid;
    softConfig.wifi.password = qpass;
    softConfig.wifi.enable   = 1;
    configSave();

    content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
    statusCode = 200;
  }
  else
  {
    content = "{\"Error\":\"404 not found\"}";
    statusCode = 404;
    Serial.println("Sending 404");
  }
  server.send(statusCode, "application/json", content);
}



void web_index()
{
  String index_html = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <script src="https://code.highcharts.com/highcharts.js"></script>
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
<div class="container" style="width: 80%; margin: auto;max-width: 564px">

  <h2>Relay</h2>
  <p>
    <i class="fas fa-toggle-on style="color:#059e8a;"></i> 
    <span class="dht-labels">Chauffage</span> 
    <span id="relayStatus">)rawliteral";
index_html += relayStatus;
index_html += R"rawliteral(</span>
  </p>

  <p>
    <div class="pull-right">
                <a href="/config">Configuration</a>
    </div>
  </p>

</div>
</body>



setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("relayStatus").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/status", true);
  xhttp.send();
}, 10000 ) ;

</script>
</html>)rawliteral";

  server.send(200, "text/html", index_html);
}



void web_config()
{
  String index_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.4.1/css/bootstrap.min.css">
  <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js"></script>
  <script src="https://maxcdn.bootstrapcdn.com/bootstrap/3.4.1/js/bootstrap.min.js"></script>

  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">

</head>
<body>

<div class="container" style="width: 80%; margin: auto;max-width: 564px">
  <h2>bicoqueTemperature - configuration</h2>

    <p>
    <ul class="list-group">
      <li class="list-group-item">
        <p>Wifi</p>
        <p>
          <ul class="list-group">
          <li class="list-group-item">Enable<span class="pull-right"><input type="checkbox" data-toggle="toggle" id=wifiEnable onChange="updateBinary('wifiEnable')"></span></li>
          <li class="list-group-item">Ssid<span class="pull-right"><input type="text" value=ssid id=wifiSsid></span></li>
          <li class="list-group-item">Password<span class="pull-right"><input type="text" value=password id=wifiPassword></span></li>
          </ul>
        </p>
        <p align=right>
          <input type="button" value="update" onclick="wifiUpdate()">
        </p>
      </li>
      <li class="list-group-item">
        <p>Relay</p>
        <p>
          <ul class="list-group">
          <li class="list-group-item">Enable<span class="pull-right"><input type="checkbox" data-toggle="toggle" id=relayEnable onChange="updateBinary('relayEnable')"></span></li>
          <li class="list-group-item">Status<span class="pull-right"><input type="checkbox" data-toggle="toggle" id=relayStatus onChange="updateBinary('relayStatus')"></span></li>
          </ul>
        </p>
      </li>
      <li class="list-group-item">
        <p>Cloud Extension</p>
        <p>
          <ul class="list-group">
          <li class="list-group-item">Enable<span class="pull-right"><input type="checkbox" data-toggle="toggle" id=cloudEnable onChange="updateBinary('cloudEnable')"></span></li>
          <li class="list-group-item">URL<span class="pull-right"><input type="text" value=url id=cloudUrl onChange="updateText('cloudUrl')"></span></li>
          <li class="list-group-item">API Key<span class="pull-right"><input type="text" value=apiKey id=cloudApiKey onChange="updateText('cloudApiKey')"></span></li>
          <li class="list-group-item">&nbsp;<span class="pull-right"><a href='https://github.com/maertems/bicoqueTemperature' target="_blank">Docs</a></span></li>
          </ul>
        </p>
      </li>
      <li class="list-group-item">
        <p>Global</p>
        <p>
          <ul class="list-group">
          <li class="list-group-item">alreadyStart<span class="pull-right"><input type="checkbox" data-toggle="toggle" id=alreadyStart onChange="updateBinary('alreadyStart')"></span></li>
          <li class="list-group-item">softName<span class="pull-right" id=softName></span></li>
          <li class="list-group-item">softVersion<span class="pull-right" id=softVersion></span></li>
          <li class="list-group-item">Check New Version<span class="pull-right"><input type="checkbox" data-toggle="toggle" id=checkUpdateEnable onChange="updateBinary('checkUpdateEnable')"></span></li>
          </ul>
        </p>
      </li>
    </ul>
    </p>

</div>
</body>

<script>
function getData()
{
  var xhttp = new XMLHttpRequest();

  xhttp.onreadystatechange = function()
  {
    if (this.readyState == 4 && this.status == 200)
    {
      const obj = JSON.parse(this.responseText);
      console.log(obj.wifi.ssid);
      //document.getElementById("wifi_ssid").innerHTML = obj.wifi.ssid;
      document.getElementById("wifiSsid").value = obj.wifi.ssid;
      document.getElementById("wifiPassword").value = obj.wifi.password;
      document.getElementById("wifiEnable").checked = obj.wifi.enable;

      document.getElementById("relayEnable").checked = obj.relay.enable;
      document.getElementById("relayStatus").checked = obj.relay.status;

      document.getElementById("cloudApiKey").value = obj.cloud.apiKey;
      document.getElementById("cloudUrl").value = obj.cloud.url;
      document.getElementById("cloudEnable").checked = obj.cloud.enable;

      document.getElementById("alreadyStart").checked = obj.alreadyStart;
      document.getElementById("softName").innerHTML = obj.softName;
      document.getElementById("softVersion").innerHTML = obj.softVersion;
      document.getElementById("checkUpdateEnable").checked = obj.checkUpdateEnable;
    }
  };
  xhttp.open("GET", "/api/config", true);
  xhttp.send();
}

function updateBinary(binary)
{
  var xhr = new XMLHttpRequest();
  var url = "/write?" + binary + "=";

  if (document.getElementById(binary).checked == true) { url = url + "1"; }
  else { url = url + "0"; }

  xhr.onreadystatechange = function () {
    if (xhr.readyState === 4 && xhr.status === 200) {
        // console.log(xhr.responseText);
    }
  };

  xhr.open("GET", url, true);
  xhr.send();
}

function updateText(textId)
{
  var xhr = new XMLHttpRequest();
  var url = "/write?" + textId + "=" + document.getElementById(textId).value;

  xhr.onreadystatechange = function () {
    if (xhr.readyState === 4 && xhr.status === 200) {
        // console.log(xhr.responseText);
    }
  };

  xhr.open("GET", url, true);
  xhr.send();
}

function wifiUpdate()
{
  var xhr = new XMLHttpRequest();
  var url = "/setting?ssid=" + document.getElementById("wifi_ssid").value + "&pass=" + document.getElementById("wifi_password").value;

  xhr.onreadystatechange = function () {
    if (xhr.readyState === 4 && xhr.status === 200) {
        // console.log(xhr.responseText);
    }
  };

  xhr.open("GET", url, true);
  xhr.send();
}

getData();
//setInterval(getData, 10000);

</script>
</html>
)rawliteral";

  server.send(200, "text/html", index_html);
}









void setup() 
{
  // Start the Serial Monitor
  Serial.begin(115200);

  Serial.println("");
  Serial.print("Welcome to "); Serial.print(SOFT_NAME); Serial.print(" - "); Serial.println(SOFT_VERSION);

  // FileSystem
  if (SPIFFS.begin())
  {
    boolean configFileToCreate = 0;
    // check if we have et config file
    if (SPIFFS.exists("/config.json"))
    {
      Serial.println("Config.json found. read data");
      configRead(softConfig);

      if (softConfig.softName != SOFT_NAME)
      {
        Serial.println("Not the same softname");
        Serial.print("Name from configFile : "); Serial.println(softConfig.softName);
        Serial.print("Name from code       : "); Serial.println(SOFT_NAME);
        configFileToCreate = 1;
      }
      else
      {
        // For update
        if ( softConfig.softVersion < "0.0.01" or softConfig.softVersion == "null" or softConfig.softVersion == "" )
        {
          softConfig.relay.enable      = 1;
          softConfig.relay.status      = 0;
          softConfig.checkUpdateEnable = 1;
          softConfig.cloud.enable      = 0;
          softConfig.alreadyStart      = 1;
          configSave();
        }

      }

    }
    else
    {
      configFileToCreate = 1;
    }

    if (configFileToCreate == 1)
    {
      // No config found.
      // Start in AP mode to configure
      // debug create object here
      Serial.println("Config.json not found. Create one");
      Serial.println("Clear SPIFF before");
      SPIFFS.format();

      softConfig.wifi.enable       = 1;
      softConfig.wifi.ssid         = "";
      softConfig.wifi.password     = "";
      softConfig.relay.enable      = 0;
      softConfig.relay.status      = 0;
      softConfig.cloud.enable      = 0;
      softConfig.cloud.url         = "";
      softConfig.cloud.apiKey      = "";
      softConfig.alreadyStart      = 0;
      softConfig.softName          = SOFT_NAME;
      softConfig.softVersion       = SOFT_VERSION;
      softConfig.checkUpdateEnable = 1;

      Serial.println("Config.json : load save function");
      configSave();
    }
  }

  if (DEBUG)
  {
    configDump(softConfig);
  }

  // Connect to Wifi
  internetConnection = wifiConnect(softConfig.wifi.ssid, softConfig.wifi.password);

  if (softConfig.wifi.enable)
  {
    wifiActivationTempo = 0;
  }
  else
  {
    if (DEBUG) { Serial.println("Deactive wifi in 5 mins."); }
    wifiActivationTempo = 600;
  }

  Serial.println("End of wifi config");


  // Init relay to the last state Relay
  pinMode(RelayPin, OUTPUT);
  if ( softConfig.relay.status == true)
  {
    relayOn();
  }
  else
  {
    relayOff();
  } 


  // server.serveStatic("/", SPIFFS, "/index.html","Content-Security-Policy script-src;" );
  server.on("/", web_index);
  server.on("/config", web_config);
  server.on("/write", webWrite);
  server.on("/reboot", webReboot);

  server.serveStatic("/fs/config", SPIFFS, "/config.json");

  server.on("/api/config", webApiConfig);
  server.on("/api/status", webApiRelayStatus);

  server.on("/setting", webWifiWrite);
  server.on("/wifi", webWifiSetup);

  //-- debug / help programming
  server.on("/fs/dir", webFsDir);
  server.on("/fs/read", webFsRead);
  server.on("/fs/del", webFsDel);
  server.on("/fs/download", webFsDownload);

  server.onNotFound(webNotFound);
  server.begin();

  if (DEBUG)
  {
    Serial.print("Http: Server started at http://");
    Serial.print(ip);
    Serial.println("/");
    Serial.print("Status : ");
    Serial.println(WiFi.RSSI());
  }

  delay(1000);


  if (internetConnection)
  {
    // check update
    updateCheck(1);

    logger("Starting bicoqueTemp");
    String messageToLog = SOFT_VERSION ; messageToLog += " "; messageToLog += SOFT_DATE;
    logger(messageToLog);
  }



  //-- things to do for upgrade after internet connection TODO: what we do if we have not internet connection... Example for the first use....
  if ( softConfig.softVersion < "0.0.01")
  {
  }


  if (softConfig.softVersion != SOFT_VERSION)
  {
    softConfig.softVersion = SOFT_VERSION;
    configSave();
  }
  
}







void loop() 
{
  // We check httpserver evry 1 sec


  if (networkEnable)
  {
    // check web client connections
    server.handleClient();

   if (wifiActivationTempo > 0 )
   {
      if (wifiActivationTempo > (millis() / 1000) )
      {
        if (softConfig.wifi.enable)
        {
          // try to reconnect evry 5 mins
	  internetConnection = wifiConnect(softConfig.wifi.ssid, softConfig.wifi.password);
 
          if (internetConnection)
          {
            wifiActivationTempo = 0;
          }
          else
          {
            wifiActivationTempo = (millis() / 1000) + 600;
          }
        }
        else
        {
          // Need to de-active wifi
          WiFi.mode(WIFI_OFF);
          WiFi.disconnect();

          wifiActivationTempo = 0;
          networkEnable       = 0;
        }
      }
   }
  }



  if ( (timerPerHourLast + 3600) < (millis() / 1000) )
  {
    updateCheck(0);
    timerPerHourLast = millis() / 1000;

    String messageToLog;
    messageToLog = "Debug: heapSize        : "; messageToLog += ESP.getFreeHeap() ; logger(messageToLog);
    messageToLog = "Debug: sketchFreeSpace : "; messageToLog += ESP.getFreeSketchSpace() ; logger(messageToLog);
  }


  delay(100);
}
