#include "BlueMagicCameraConnection.h"


#define USE_NOTIFICATION_STATUS 0
#define USE_NOTIFICATION_TIMECODE 0
#define LIST_WRONG_DEVICES 0


BLERemoteCharacteristic* BlueMagicCameraConnection::_cameraStatus = nullptr;
BLERemoteCharacteristic* BlueMagicCameraConnection::_deviceName = nullptr;
BLERemoteCharacteristic* BlueMagicCameraConnection::_timecode = nullptr;
BLERemoteCharacteristic* BlueMagicCameraConnection::_outgoingCameraControl = nullptr;
BLERemoteCharacteristic* BlueMagicCameraConnection::_incomingCameraControl = nullptr;

static BLEUUID OutgoingCameraControl( "5DD3465F-1AEE-4299-8493-D2ECA2F8E1BB" );
static BLEUUID IncomingCameraControl( "B864E140-76A0-416A-BF30-5876504537D9" );
static BLEUUID Timecode( "6D8F2110-86F1-41BF-9AFB-451D87E976C8" );
static BLEUUID CameraStatus( "7FE8691D-95DC-4FC5-8ABD-CA74339B51B9" );
static BLEUUID DeviceName( "FFAC0C52-C9FB-41A0-B063-CC76282EB89C" );
static BLEUUID ServiceId( "00001800-0000-1000-8000-00805f9b34fb" );
static BLEUUID BmdCameraService( "291D567A-6D75-11E6-8B77-86F30CA893D3" );


//void printHex( uint8_t num ) { char hexCar[2]; sprintf(hexCar, "%02X", num); Serial.print(hexCar); }


static void cameraStatusNotify( BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify )
{
  BlueMagicState *blu = BlueMagicState::getInstance();

  blu->statusNotify( true, pData );
  blu->setCameraStatus( pData[0] );
}

static void timeCodeNotify( BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify )
{
  BlueMagicState* blu = BlueMagicState::getInstance();

  blu->timecodeNotify( true, pData );
  
  // timecode
  uint8_t H, M, S, f;

  H = ( pData[11] / 16 * 10 ) + ( pData[11] % 16 );
  M = ( pData[10] / 16 * 10 ) + ( pData[10] % 16 );
  S = ( pData[9]  / 16 * 10 ) + ( pData[9]  % 16 );
  f = ( pData[8]  / 16 * 10 ) + ( pData[8]  % 16 );
  
  blu->setTimecode( H, M, S, f );
}

static void controlNotify( BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify )
{
  Serial.println( "DEBUG: In controlNotify()" );

  BlueMagicState *blu = BlueMagicState::getInstance();

  blu->settingsNotify( true, pData );
  
  bool changed = false;

  //Serial.println(""); Serial.print("Packet: "); for( int IdxByte = 0; IdxByte < length; IdxByte++) { printHex( pData[IdxByte] ); } Serial.println( "" );


  // recording
  if (length == 13 && pData[0] == 255 && pData[1] == 9 && pData[4] == 10 && pData[5] == 1)
  {
    changed = true;
    int8_t transportMode = pData[8];
    blu->setTransportMode(transportMode);
  }

  //codec
  if (pData[0] == 255 && pData[4] == 10 && pData[5] == 0)
  {
    changed = true;
    int8_t codec = pData[8];
    int8_t quality = pData[9];
    blu->setCodec(codec);
    blu->setQuality(quality);
  }

  //resolution + framerate
  if (pData[0] == 255 && pData[4] == 1 && pData[5] == 9)
  {

    changed = true;
    int16_t frL = pData[8];
    int16_t frH = pData[9] << 8;
    int16_t frameRate = frL + frH;

    int16_t sfrL = pData[10];
    int16_t sfrH = pData[11] << 8;
    int16_t sensorRate = sfrL + sfrH;

    int16_t wL = pData[12];
    int16_t wH = pData[13] << 8;
    int16_t width = wL + wH;

    int16_t hL = pData[14];
    int16_t hH = pData[15] << 8;
    int16_t height = hL + hH;

    int8_t flags = pData[16];

    blu->setFrameRate(frameRate);
    blu->setSensorFrameRate(sensorRate);
    blu->setFrameWidth(width);
    blu->setFrameHeight(height);
    blu->setFormatFlags(flags);
  }

  // white balance
  if (pData[0] == 255 && pData[4] == 1 && pData[5] == 2)
  {
    changed = true;
    int16_t wbL = pData[8];
    int16_t wbH = pData[9] << 8;
    int16_t whiteBalance = wbL + wbH;

    int16_t tintL = pData[10];
    int16_t tintH = pData[11] << 8;
    int16_t tint = tintL + tintH;

    blu->setWhiteBalance(whiteBalance);
    blu->setTint(tint);
  }

  // zoom
  if (pData[0] == 255 && pData[4] == 0 && pData[5] == 7)
  {
    changed = true;

    int16_t zL = pData[8];
    int16_t zH = pData[9] << 8;
    int16_t zoom = zL + zH;

    blu->setZoom(zoom);
  }

  // focus
  if (pData[0] == 255 && pData[4] == 0 && pData[5] == 0)
  {
    changed = true;

    int16_t zL = pData[8];
    int16_t zH = pData[9] << 8;

    int16_t focus = zL + zH;

    blu->setFocus(focus);
  }
  
  // aperture
  if (pData[0] == 255 && pData[4] == 0 && pData[5] == 2)
  {
    changed = true;
    uint16_t low = pData[8];
    uint16_t high = pData[9] << 8;
    float aperture = sqrt(pow(2, (float(low + high) / 2048.0)));
    blu->setAperture(aperture);
  }

  // iso
  if (pData[0] == 255 && pData[4] == 1 && pData[5] == 14)
  {
    changed = true;
    uint16_t low = pData[8];
    uint16_t high = pData[9] << 8;
    int32_t iso = low + high;
    blu->setIso(iso);
  }

  // shutter
  if (pData[0] == 255 && pData[4] == 1 && pData[5] == 11)
  {
    changed = true;
    uint16_t low = pData[8];
    uint16_t high = pData[9] << 8;
    int32_t shutter = low + high;
    blu->setShutter(shutter);
  }

  blu->setChanged(changed);
}


class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    Serial.print("BLE Advertised Device found: ");  Serial.print( advertisedDevice.getAddress().toString().c_str() );

    if( advertisedDevice.haveName() )
    {
      Serial.print(": "); Serial.print( advertisedDevice.getName().c_str() );
    }


    if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(ServiceId))
    {
      Serial.print(": "); Serial.println( advertisedDevice.getServiceUUID().toString().c_str() );      
      Serial.println();

      Serial.println("DEBUG: Stopping bluetooth scan.");

      advertisedDevice.getScan()->stop();
    } 
#if LIST_WRONG_DEVICES    
    else
    {
      Serial.print(" - WRONG SERVICE UUID: ");
      Serial.println( advertisedDevice.getServiceUUID().toString().c_str() );
    }
#endif    
  }
};


class MySecurity : public BLESecurityCallbacks
{
  uint32_t onPassKeyRequest()
  {
    // code snippet from jeppo7745 https://www.instructables.com/id/Magic-Button-4k-the-20USD-BMPCC4k-Remote/
    Serial.println("---> PLEASE ENTER 6 DIGIT PIN (end with ENTER) : ");
    int pinCode = 0;
    char ch;
    do
    {
      while (!Serial.available())
      {
        vTaskDelay(1);
      }
      ch = Serial.read();
      if (ch >= '0' && ch <= '9')
      {
        pinCode = pinCode * 10 + (ch - '0');
        Serial.print(ch);
      }
    } while ((ch != '\n'));
    return pinCode;
  }

  void onPassKeyNotify(uint32_t pass_key)
  {
    pass_key + 1;
  }

  bool onConfirmPIN(uint32_t pin)
  {
    return true;
  }

  bool onSecurityRequest()
  {
    return true;
  }

  void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl)
  {
    int index = 1;
  }
};

BlueMagicCameraConnection::BlueMagicCameraConnection()
{
}

BlueMagicCameraConnection::~BlueMagicCameraConnection()
{
  delete _cameraStatus, _deviceName, _timecode, _outgoingCameraControl, _incomingCameraControl, _device;
  delete _client;
  delete _cameraControl;

  _init = false;

  _device.deinit(true);
}

void BlueMagicCameraConnection::begin()
{
  begin("BlueMagic32");
}

void BlueMagicCameraConnection::begin(String name)
{
  if (_init)
  {
    return;
  }


  _name = name;

  setState(CAMERA_DISCONNECTED);


#if USE_PREFERENCES  
  if (!PREF_INCLUDED)
  {
    _pref = new Preferences();
  }


  _pref->begin(_name.c_str(), false);

  setAuthentication(_pref->getBool("authenticated", false));

  Serial.print( "DEBUG: Preferences bIsAuth: " );
  Serial.println( _pref->getBool("authenticated", false) ? 1 : 0 );

  String addr = _pref->getString( "cameraAddress", "" );

  Serial.print( "DEBUG: Preferences stored addess: " );
  Serial.println( addr.c_str() );

  if (addr.length() > 0)
  {
    setCameraAddress(BLEAddress(addr.c_str()));
  }


  _pref->end();
#endif

  _device.init(_name.c_str());
  _device.setPower(ESP_PWR_LVL_P9);
  _device.setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
  _device.setSecurityCallbacks(new MySecurity());

  BLESecurity* pSecurity = new BLESecurity();

  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
  pSecurity->setCapability(ESP_IO_CAP_IN);
  pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  _init = true;
}

bool BlueMagicCameraConnection::scan(bool active, int duration)
{
  if (getAuthentication() && getCameraAddress() != nullptr)
  {
    Serial.print( "DEBUG: BlueMagicCameraConnection::scan; Already authenticated with Address: " );
    Serial.println( getCameraAddress()->toString().c_str() );

    return false;
  }
  else
  {
    _bleScan = _device.getScan();

    _bleScan->clearResults();
    //_bleScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    _bleScan->setActiveScan(active);
    
    Serial.println( "DEBUG: BlueMagicCameraConnection::scan; Starting scan." );
    
    _bleScan->start(duration);

    Serial.println( "DEBUG: BlueMagicCameraConnection::scan; Scan ended." );
  }

  
  return true;
}

int BlueMagicCameraConnection::connected()
{
  return _connected;
}

bool BlueMagicCameraConnection::available()
{
  return connected() && (_cameraControl != nullptr);
}

bool BlueMagicCameraConnection::connectToServer( BLEAddress address )
{
  Serial.println( "DEBUG: Creating client for device: " );

  _client = _device.createClient();

  setState( CAMERA_CONNECTING );
  
  Serial.print( "DEBUG: Connecting to device: " ); Serial.println( address.toString().c_str() );

  _client->connect( address );

  Serial.println( "DEBUG: Getting Remote Service." );
  
  BLERemoteService *pRemoteService = _client->getService( BmdCameraService );
  
  if (pRemoteService == nullptr)
  {
    Serial.print("Failed to find our service UUID: "); Serial.println(BmdCameraService.toString().c_str());
    return false;
  }


  _deviceName = pRemoteService->getCharacteristic( DeviceName );

  if (_deviceName != nullptr)
  {
    _deviceName->writeValue(_name.c_str(), _name.length());
  }


  Serial.println( "DEBUG: Getting Outgoing Characteristic." );

  _outgoingCameraControl = pRemoteService->getCharacteristic( OutgoingCameraControl );

  if( _outgoingCameraControl == nullptr )
  {
    Serial.print("Failed to find our characteristic UUID: "); Serial.println(OutgoingCameraControl.toString().c_str());
    return false;
  }


  Serial.println( "DEBUG: Getting Incoming Characteristic." );

  _incomingCameraControl = pRemoteService->getCharacteristic( IncomingCameraControl );
  
  if( _incomingCameraControl == nullptr )
  {
    Serial.print("Failed to find our characteristic UUID: "); Serial.println(IncomingCameraControl.toString().c_str());
    return false;
  }


  _incomingCameraControl->registerForNotify( controlNotify, true );


#if USE_NOTIFICATION_TIMECODE
  Serial.println( "DEBUG: Getting Timecode Characteristic." );

  _timecode = pRemoteService->getCharacteristic(Timecode);
  
  if (_timecode == nullptr)
  {
    Serial.print("Failed to find our characteristic UUID: "); Serial.println(Timecode.toString().c_str());
    return false;
  }


  _timecode->registerForNotify( timeCodeNotify );
#endif

#if USE_NOTIFICATION_STATUS
  Serial.println( "DEBUG: Getting Status Characteristic." );

  _cameraStatus = pRemoteService->getCharacteristic( CameraStatus );
  
  if (_cameraStatus == nullptr)
  {
    Serial.print("Failed to find our characteristic UUID: "); Serial.println(CameraStatus.toString().c_str());
    return false;
  }


  _cameraStatus->registerForNotify( cameraStatusNotify );
#endif


  setState( CAMERA_CONNECTED );

  setController();

  Serial.println( "DEBUG: Connected to device!" );

  return true;
}

void BlueMagicCameraConnection::setController()
{
  _cameraControl = new BlueMagicCameraController( _outgoingCameraControl );
}

void BlueMagicCameraConnection::setState(CONNECTION_STATE state)
{
  _connected = state;
}

void BlueMagicCameraConnection::setAuthentication(bool authenticated)
{
  _authenticated = authenticated;
}

bool BlueMagicCameraConnection::getAuthentication()
{
  return _authenticated;
}

void BlueMagicCameraConnection::setCameraAddress(BLEAddress address)
{
  _cameraAddress = &address;
}

BLEAddress *BlueMagicCameraConnection::getCameraAddress()
{
  return _cameraAddress;
}

BlueMagicCameraController *BlueMagicCameraConnection::connect()
{
  return connect(0);
}

BlueMagicCameraController *BlueMagicCameraConnection::connect(uint8_t index)
{
  Serial.println( "DEBUG: BlueMagicCameraConnection::connect()" );
  

  if (_cameraControl != nullptr)
  {
    Serial.println( "DEBUG: Already had a CameraControl. Returning it!" );

    return _cameraControl;
  }


  bool ok = false;
  
  Serial.println( "DEBUG: Starting scanning for devices." );
  
  bool scanned = scan( false, 5 ); //ORIGIN: scan( false, 5 )

  Serial.println( "DEBUG: Scan complete." );

  BLEAddress address = BLEAddress( "FF:FF:FF:FF:FF" );

  if( scanned )
  {
    Serial.println( "DEBUG: Checking scan results." );

    int count = _bleScan->getResults().getCount();

    if( count > 0 )
    {
      Serial.print( "DEBUG: Found " ); Serial.print( count ); Serial.println( " devices." );

      for( int IdxDevice = count - 1; IdxDevice >= 0; --IdxDevice )
      {
        auto CurDevice = _bleScan->getResults().getDevice( IdxDevice );

        address = CurDevice.getAddress();

        if( CurDevice.haveServiceUUID() && CurDevice.getServiceUUID().equals( ServiceId ) )
        {
          Serial.print("DEBUG: Device #" ); Serial.print( IdxDevice );
          Serial.print(": BlackMagic Device: "); Serial.print( address.toString().c_str() );

          if( CurDevice.haveName() )
          {
            Serial.print(": "); Serial.print( CurDevice.getName().c_str() );
          }


          Serial.print(": "); Serial.println( CurDevice.getServiceUUID().toString().c_str() );
          
          ok = connectToServer(address);

          break;
        }
#if LIST_WRONG_DEVICES        
        else
        {
          Serial.print("DEBUG: Other Device found: "); Serial.print( address.toString().c_str() );

          if( CurDevice.haveName() )
          {
            Serial.print(": "); Serial.print( CurDevice.getName().c_str() );
          }


          Serial.print(": "); Serial.println( CurDevice.getServiceUUID().toString().c_str() );          
        }
#endif
      }
    } else
    {
      Serial.println( "DEBUG: Found 0 devices!" );
    }
  }
  else
  {
    address = *getCameraAddress();

    ok = connectToServer( address );
  }


  if( ok )
  {
    setAuthentication( true );

#if USE_PREFERENCES
    _pref->begin(_name.c_str(), false);
    _pref->putString("cameraAddress", address.toString().c_str());
    _pref->putBool("authenticated", getAuthentication());
    _pref->end();
#endif  
    
    setCameraAddress( address );
  
    Serial.println( "DEBUG: Connected to Blackmagic camera." );

    return _cameraControl;
  }


  Serial.println( "DEBUG: Failed to connect to Blackmagic camera, terminating!" );
  
  return nullptr;
}

void BlueMagicCameraConnection::disconnect()
{
  _client->disconnect();

  delete _cameraControl;
  _cameraControl = nullptr;
  
  setState(CAMERA_DISCONNECTED);
}

void BlueMagicCameraConnection::clearPairing()
{
  if (connected() != CAMERA_DISCONNECTED)
  {
    disconnect();
  }


  if (*getCameraAddress()->getNative() != nullptr)
  {
    esp_ble_remove_bond_device(*getCameraAddress()->getNative());
  }


  int dev_num = esp_ble_get_bond_device_num();

  esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
  esp_ble_get_bond_device_list(&dev_num, dev_list);
  for (int i = 0; i < dev_num; i++)
  {
    BLEAddress baddr = BLEAddress(dev_list[i].bd_addr);
    Serial.println(baddr.toString().c_str());
    esp_ble_remove_bond_device(dev_list[i].bd_addr);
  }

  free(dev_list);
  setAuthentication(false);
  // setCameraAddress(nullptr);

#if USE_PREFERENCES
  _pref->begin(_name.c_str(), false);
  _pref->putString("cameraAddress", "");
  _pref->putBool("authenticated", getAuthentication());
  _pref->end();
#endif  
}
