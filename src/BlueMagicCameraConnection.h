#define USE_PREFERENCES 0


#ifndef BLEDevice_h
#include <BLEDevice.h>
#endif

#if USE_PREFERENCES
#ifndef Preferences_h
#include <Preferences.h>
#define PREF_INCLUDED false
#endif
#endif 


#ifndef BlueMagicCameraConnection_h

#define BlueMagicCameraConnection_h

#include "Arduino.h"
#include "BlueMagicState.h"
#include "BlueMagicCameraController.h"

enum CONNECTION_STATE
{
  CAMERA_CONNECTED = 1,
  CAMERA_DISCONNECTED = 2,
  CAMERA_CONNECTING = 3
};


class BlueMagicCameraConnection
{
public:
  BlueMagicCameraConnection();
  ~BlueMagicCameraConnection();

  void begin();
  void begin(String name);
  void clearPairing();
  void disconnect();

#if USE_PREFERENCES
  void begin(String name, Preferences &pref);
#endif  

  bool available();
  bool scan(bool active, int duration);

  BlueMagicCameraController* connect();
  BlueMagicCameraController* connect(uint8_t index);


private:
  void setAuthentication(bool authenticated);
  void setCameraAddress(BLEAddress address);
  void setController();
  void setState(CONNECTION_STATE state);
  
  bool authenticated();
  bool connectToServer(BLEAddress address);
  bool getAuthentication();

  int connected();

  BLEAddress* getCameraAddress();


private:
  static BLERemoteCharacteristic* _outgoingCameraControl;
  static BLERemoteCharacteristic* _incomingCameraControl;
  static BLERemoteCharacteristic* _timecode;
  static BLERemoteCharacteristic* _cameraStatus;
  static BLERemoteCharacteristic* _deviceName;


private:
  bool _init = false;
  bool _authenticated = false;

  int _connected = -1;

  String _name;

  BLEDevice _device;

  BLEScan* _bleScan = nullptr;
  BLEClient* _client = nullptr;
  BLEAddress* _cameraAddress = nullptr;

  BlueMagicCameraController* _cameraControl = nullptr;

#if USE_PREFERENCES
  Preferences *_pref;
#endif
};
#endif
