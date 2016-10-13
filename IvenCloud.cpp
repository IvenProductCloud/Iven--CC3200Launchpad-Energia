// Created by Rıza Arda Kırmızıoğlu and Berk Özdilek

#include "IvenCloud.h"

/***********************************************/
/*********     HELPER FUNCTIONS      ***********/
/***********************************************/

bool IvenCloudWiFi::ConnectClient()
{
  _client.stop();
  long startTime = millis();
  while (millis() - startTime < 5000) {
    if (_client.connect(server,port)) 
      return true;
  }
  response.error = IR_CONNECTION_ERROR;
  return false;
}

bool IvenCloudWiFi::handleResponseHeader() 
{
  uint8_t j, i = 0;
  long startTime = millis();
  while (millis() - startTime < 15000) {
    if (_client.available()) {
      buffer[i] = _client.read();
      if (buffer[i - 3] == '1' && buffer[i - 2] == '.' && buffer[i - 1] == '1' && buffer[i] == ' ') {
        i = 0;
        while (i != 3) {
          if (_client.available()) {
            buffer[i] = _client.read();
            i++;
          }
        }
        buffer[3] = '\0';
        response.httpStatus = strtoul(buffer, 0, 10);
        if (response.httpStatus > 500)
          return false;
        i = 0;
        while (!(buffer[i - 4] == '\r' && buffer[i - 3] == '\n' && buffer[i - 2] == '\r' && buffer[i - 1] == '\n')) {
          if (_client.available()) {
            buffer[i] = _client.read();
            i++;
            if (buffer[i - 1] != '\r' && buffer[i - 1] != '\n')
              i %= 64;
          }
        }
        buffer[i] = '\0';
        i = 0;
        buffer[4] = '\0';
        while (i != 4) {
          if (_client.available()) {
            buffer[i] = _client.read();
            i++;
          }
        }
        j = strtoul(buffer, 0, 16);
        i = 0;
        while (i != j) {
          if (_client.available()) {
            buffer[i] = _client.read();
            i++;
          }
        }

        buffer[j] = '\0';
        return true;
      }
      i++;
      if (buffer[i - 1] != '1' && buffer[i - 1] != ' ')
        i %= 64;
    }
  }
  
  response.error = IR_TIMEOUT;
  return false;
}

bool IvenCloudWiFi::parseApiKey() 
{
  //Find API-KEY
  uint8_t i = 0;
  _check = false;
  long startTime = millis();
  while (millis() - startTime < 5000) {
    if (i == 127)
      break;
    if (buffer[i] == 'a' && buffer[i + 1] == 'p' && buffer[i + 2] == 'i' && buffer[i + 3] == '_' && 
           buffer[i + 4] == 'k' && buffer[i + 5] == 'e' && buffer[i + 6] == 'y') {
      _check = true;
      break;
    }
    i++;
  }

  i += 10;

  buffer[i + 40] = '\0';

  // Set API-KEY
  _apiKey.concat((buffer + i));

  return _check;
}

bool IvenCloudWiFi::handleResponseBody() 
{
  uint8_t j, i = 0;
  _check = false;
  long startTime = millis();
  while (millis() - startTime < 5000) {
    if (i == 127)
      break;
    if (buffer[i] == 'i' && buffer[i + 1] == 'v' && buffer[i + 2] == 'e' && buffer[i + 3] == 'n' &&
          buffer[i + 4] == 'C' && buffer[i + 5] == 'o' && buffer[i + 6] == 'd' && buffer[i + 7] == 'e') {

      _check = true;
      i += 10;
      j = i;

      while (buffer[j] != '"')
        j++;

      buffer[j] = '\0';

      // Set Iven Code
      response.ivenCode = strtoul((buffer + i), 0, 10);
    }

    if (buffer[i] == 't' && buffer[i + 1] == 'a' && buffer[i + 2] == 's' && buffer[i + 3] == 'k') {

      i += 7;
      j = i;

      while (buffer[j] != '"')
        j++;

      buffer[j] = '\0';

      response.task.concat((buffer + i));
    }

    i++;
  }

  return _check;
}

void IvenCloudWiFi::createActivationCode(const char* secretKey, const char* deviceId, char* activationCode)
{
  uint8_t* hash;
  ShaClass Sha1;
  Sha1.initHmac((const uint8_t*)secretKey, strlen(secretKey));
  Sha1.write(deviceId);
  hash = Sha1.resultHmac();

  uint8_t i = 0, j = 0;

  for (i = 0; i < 20; i++)
  {
    activationCode[j] = "0123456789abcdef"[hash[i] >> 4];
    activationCode[j + 1] = "0123456789abcdef"[hash[i] & 0xf];
    j += 2;
  }

  activationCode[40] = '\0';
}

void IvenCloudWiFi::sendDataRequest(IvenData* data)
{

  // Connect (make TCP connection)
  if(!ConnectClient())
    return;

  char* jsonData = data->toJson();

  // Make request
  _client.println(F("POST /data HTTP/1.1"));
  _client.print(F("Host: "));
  _client.println(server);
  _client.println(F("Connection: keep-alive"));
  _client.println(F("User-Agent: ArduinoWiFi/1.1"));
  _client.println(F("Accept-Encoding: gzip, deflate"));
  _client.println(F("Accept: */*"));
  _client.println(F("Content-Type: application/json"));
  _client.print(F("API-KEY: "));
  _client.println(_apiKey.c_str());
  _client.print(F("Content-Length: "));
  _client.println(data->length());
  _client.println();
  _client.println(jsonData);

    // Read response
    if (handleResponseHeader()) {

      // Parse ivenCode
      if (!handleResponseBody())
        response.error = IR_IVEN_CODE_MISSING;
    }
}

void IvenCloudWiFi::activationRequest(char* activationCode)
{
  // Connect
  if(!ConnectClient())
    return;

  // Make Request
  _client.println(F("GET /activate/device HTTP/1.1"));
  _client.print(F("Host: "));
  _client.println(server);
  _client.print(F("Activation: "));
  _client.println(activationCode);
  _client.println(F("Connection: close"));
  _client.println();

  // Read Response
  if (handleResponseHeader()) {

    // Parse API-KEY
      if (!parseApiKey()) {
        if (!handleResponseBody()) 
          response.error = IR_IVEN_CODE_MISSING;
        else
          response.error = IR_WRONG_ACTIVATION_CODE;
      }
  }
}

/***********************************************/
/*********     CLASS FUNCTIONS      ************/
/***********************************************/

// Constructor
IvenCloudWiFi::IvenCloudWiFi()
{
  _client = WiFiClient();
  _apiKey = String();
  response = IvenResponse();
}

// Activates device on Iven cloud
IvenResponse IvenCloudWiFi::activateDevice(const char* secretKey, const char* deviceId)
{
  response.clearResponse();
  if (!secretKey || !deviceId)
    response.error = IR_NULL_PARAMETER;
  if (strlen(secretKey) != 40)
    response.error = IR_INVALID_PARAMETER;

  // Creates activation code as hex string
  char activationCode[41];
  createActivationCode(secretKey, deviceId, activationCode);

  activationRequest(activationCode);
  
  return response;
}

// Sends data to Iven cloud
IvenResponse IvenCloudWiFi::sendData(IvenData& sensorData)
{
  response.clearResponse();
  if (_apiKey.length() == 0)
    response.error = IR_WRONG_ACTIVATION_CODE;
  sendDataRequest(&sensorData);

  return response;
}