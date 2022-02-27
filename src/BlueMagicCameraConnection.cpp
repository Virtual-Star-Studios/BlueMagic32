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


static void printHex( uint8_t num ) 
{ 
  char hexCar[2]; 
  
  sprintf( hexCar, "%02X", num ); 
  
  Serial.print( hexCar ); 
}


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
  else if (pData[0] == 255 && pData[4] == 10 && pData[5] == 0)
  {
    changed = true;
    int8_t codec = pData[8];
    int8_t quality = pData[9];
    blu->setCodec(codec);
    blu->setQuality(quality);
  }

  //resolution + framerate
  else if (pData[0] == 255 && pData[4] == 1 && pData[5] == 9)
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
  else if (pData[0] == 255 && pData[4] == 1 && pData[5] == 2)
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
  else if( pData[0] == 255 && pData[4] == 0 && pData[5] == 7 )
  {
    changed = true;

    int16_t zL = pData[8];
    int16_t zH = pData[9] << 8;

    int16_t zoom = zL + zH;

    blu->setZoom( zoom );
  }

  
  // aperture
  else if (pData[0] == 255 && pData[4] == 0 && pData[5] == 2)
  {
    changed = true;

    uint16_t low = pData[8];
    uint16_t high = pData[9] << 8;
    float aperture = sqrt(pow(2, (float(low + high) / 2048.0)));

    blu->setAperture(aperture);
  }

  // iso
  else if (pData[0] == 255 && pData[4] == 1 && pData[5] == 14)
  {
    changed = true;

    uint16_t low = pData[8];
    uint16_t high = pData[9] << 8;
    int32_t iso = low + high;

    blu->setIso(iso);
  }

  // shutter
  else if (pData[0] == 255 && pData[4] == 1 && pData[5] == 11)
  {
    changed = true;

    uint16_t low = pData[8];
    uint16_t high = pData[9] << 8;
    int32_t shutter = low + high;

    blu->setShutter(shutter);
  }



  
  //---------------------------------------------------------------------------------------------> 0x00  
  //Unknown packet: Length:  4: 0x00 0x00 0x00 0x00
  else if( pData[0] == 0x00 && pData[1] == 0x00 && pData[2] == 0x00 && pData[3] == 0x00 && length == 4 ) { changed = true; }


  //Unknown packet: Length:  8: 0xFF 0x04 0x00 0x00 0x00 0x01 0x00 0x02
  //Instantaneous autofocus?
  else if( pData[0] == 0xFF && pData[4] == 0x00 && pData[5] == 0x01 ) { changed = true; }


  //---------------------------------------------------------------------------------------------> 0x01
  //Unknown packet: Length:  9: 0xFF 0x05 0x00 0x00 0x01 0x07 0x01 0x02 0x01
  //Dynamic Range Mode?
  else if( pData[0] == 0xFF && pData[4] == 0x01 && pData[5] == 0x07 ) { changed = true; }


  //Unknown packet: Length:  9: 0xFF 0x05 0x00 0x00 0x01 0x08 0x01 0x02 0x00
  //Video sharpening level?
  else if( pData[0] == 0xFF && pData[4] == 0x01 && pData[5] == 0x08 ) { changed = true; }


  //Unknown packet: Length:  9: 0xFF 0x05 0x00 0x00 0x01 0x0A 0x01 0x02 0x00
  //Set auto exposure mode?
  else if( pData[0] == 0xFF && pData[4] == 0x01 && pData[5] == 0x0A ) { changed = true; }


  //Unknown packet: Length: 12: 0xFF 0x08 0x00 0x00 0x01 0x0C 0x03 0x02 0x32 0x00 0x00 0x00
  //Shutter speed?
  else if( pData[0] == 0xFF && pData[4] == 0x01 && pData[5] == 0x0C ) { changed = true; }


  //Unknown packet: Length: 10: 0xFF 0x06 0x00 0x00 0x01 0x0F 0x01 0x02 0x03 0x01
  //
  else if( pData[0] == 0xFF && pData[4] == 0x01 && pData[5] == 0x0F ) { changed = true; }



  //---------------------------------------------------------------------------------------------> 0x03
  //0xFF 0x06 0x00 0x00 0x03 0x00 0x02 0x02 0x00 0x00
  //Overlay enables?
  else if( pData[0] == 0xFF && pData[4] == 0x03 && pData[5] == 0x00 ) { changed = true; }


  //Unknown packet: Length: 12: 0xFF 0x08 0x00 0x00 0x03 0x03 0x01 0x02 0x03 0x32 0x5A 0x01
  //Overlays (replaces .1 and .2 above from Cameras 4.0)?
  else if( pData[0] == 0xFF && pData[4] == 0x03 && pData[5] == 0x03 ) { changed = true; }


  //---------------------------------------------------------------------------------------------> 0x09
  //Unknown packet: Length: 14: 0xFF 0x0A 0x00 0x00 0x09 0x00 0x02 0x02 0x00 0x00 0x00 0x00 0x02 0x00
  else if( pData[0] == 0xFF && pData[4] == 0x09 && pData[5] == 0x00 ) { changed = true; }


  //Unknown packet: Length: 10: 0xFF 0x06 0x00 0x00 0x09 0x01 0x01 0x02 0x00 0x00
  else if( pData[0] == 0xFF && pData[4] == 0x09 && pData[5] == 0x01 ) { changed = true; }


  //Unknown packet: Length: 16: 0xFF 0x0C 0x00 0x00 0x09 0x02 0x02 0x02 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
  else if( pData[0] == 0xFF && pData[4] == 0x09 && pData[5] == 0x02 ) { changed = true; }


  //Unknown packet: Length: 12: 0xFF 0x08 0x00 0x00 0x09 0x05 0x01 0x02 0x00 0x0A 0x00 0x03
  else if( pData[0] == 0xFF && pData[4] == 0x09 && pData[5] == 0x05 ) { changed = true; }


  //Unknown packet: Length: 10: 0xFF 0x06 0x00 0x00 0x09 0x06 0x01 0x02 0x00 0x01
  else if( pData[0] == 0xFF && pData[4] == 0x09 && pData[5] == 0x06 ) { changed = true; }


  //Unknown packet: Length: 18: 0xFF 0x0E 0x00 0x00 0x09 0x07 0x02 0x02 0xA5 0x03 0xE8 0x03 0x00 0x00 0x00 0x00 0x10 0x27
  else if( pData[0] == 0xFF && pData[4] == 0x09 && pData[5] == 0x07 ) { changed = true; }



  //---------------------------------------------------------------------------------------------> 0x0C
  //Unknown
  //0xFF 0x06 0x00 0x00 0x0C 0x00 0x02 0x02 0x0F 0x00
  else if( pData[0] == 0xFF && pData[4] == 0x0C && pData[5] == 0x00 ) { changed = true; }


  //Unknown
  //0xFF 0x07 0x00 0x00 0x0C 0x01 0x01 0x02 0xFF 0x01 0x01
  else if( pData[0] == 0xFF && pData[4] == 0x0C && pData[5] == 0x01 ) { changed = true; }


  //Unknown
  //0xFF 0x05 0x00 0x00 0x0C 0x02 0x05 0x02 0x31
  else if( pData[0] == 0xFF && pData[4] == 0x0C && pData[5] == 0x02 ) { changed = true; }
 

  //Unknown
  //0xFF 0x06 0x00 0x00 0x0C 0x03 0x01 0x02 0x63 0xFF
  else if( pData[0] == 0xFF && pData[4] == 0x0C && pData[5] == 0x03 ) { changed = true; }


  //Unknown
  //0xFF 0x05 0x00 0x00 0x0C 0x04 0x00 0x02 0x00
  else if( pData[0] == 0xFF && pData[4] == 0x0C && pData[5] == 0x04 ) { changed = true; }


  //Unknown
  //0xFF 0x05 0x00 0x00 0x0C 0x05 0x05 0x02 0x41
  else if( pData[0] == 0xFF && pData[4] == 0x0C && pData[5] == 0x05 ) { changed = true; }


  //Unknown
  //0xFF 0x04 0x00 0x00 0x0C 0x06 0x05 0x02
  else if( pData[0] == 0xFF && pData[4] == 0x0C && pData[5] == 0x06 ) { changed = true; }


  //Unknown
  //0xFF 0x04 0x00 0x00 0x0C 0x07 0x05 0x02
  else if( pData[0] == 0xFF && pData[4] == 0x0C && pData[5] == 0x07 ) { changed = true; }


  //Unknown packet: Length:  8: /0xFF 0x04 0x00 0x00 0x0C 0x08 0x05 0x02
  else if( pData[0] == 0xFF && pData[4] == 0x0C && pData[5] == 0x08 ) { changed = true; }


  //Unknown packet: Length: 41: 0xFF 0x25 0x00 0x00 0x0C 0x09 0x05 0x02 0x43 0x61 0x6E 0x6F 0x6E 0x20 0x43 0x4E 0x2D 0x45 0x31 0x38 0x2D 0x38 0x30 0x6D 0x6D 0x20 0x54 0x34 0x2E 0x34 0x20 0x4C 0x20 0x49 0x53 0x20 0x4B 0x41 0x53 0x20 0x53
  else if( pData[0] == 0xFF && pData[4] == 0x0C && pData[5] == 0x09 ) { changed = true; }


  //Unknown packet: Length: 11: 0xFF 0x07 0x00 0x00 0x0C 0x0A 0x05 0x02 0x54 0x31 0x37
  //Aperture!
  else if( pData[0] == 0xFF && pData[4] == 0x0C && pData[5] == 0x0A ) { changed = true; }


  //Unknown packet: Length: 12: 0xFF 0x08 0x00 0x00 0x0C 0x0B 0x05 0x02 0x32 0x34 0x6D 0x6D
  //Zoom!
  else if( pData[0] == 0xFF && pData[4] == 0x0C && pData[5] == 0x0B ) 
  {
#if 1      
      String Str;
      
      for( int IdxChr = 8; IdxChr < 24; ++IdxChr )
      {
        if( !isPrintable( pData[IdxChr] ) )
        {
          break;
        }


        Str.concat( (char)pData[IdxChr] );
      }
      

      Serial.print( "DEBUG: Zoom String: " ); Serial.println( Str );
#endif


    changed = true;
  }


  //Unknown packet: Length: 24: 0xFF 0x14 0x00 0x00 0x0C 0x0C 0x05 0x02 0x31 0x31 0x36 0x30 0x6D 0x6D 0x20 0x74 0x6F 0x20 0x31 0x32 0x32 0x30 0x6D 0x6D
  //Focus!
  else if( pData[0] == 0xFF && pData[4] == 0x0C && pData[5] == 0x0C ) 
  {
    int IdxChr = 8;

    String Lo;

    while( pData[IdxChr] >= 48 && pData[IdxChr] <= 57 && IdxChr <= length )
    {
      Lo.concat( (char)pData[IdxChr++] );
    }


    if( pData[IdxChr] == 0x00 )
    { // Closest
      blu->setFocus( (float)Lo.toInt(), 0.F );
      blu->setChanged( true );

      return;
    }

    else if( pData[IdxChr+0] == 73  //I
          && pData[IdxChr+1] == 110 //n
          && pData[IdxChr+2] == 102 //f
    )
    { // Farthest
      blu->setFocus( 99999.F, 0.F );
      blu->setChanged( true );

      return;
    }

    else if( pData[IdxChr+0] != 109 //m
          || pData[IdxChr+1] != 109 //m
          || pData[IdxChr+2] != 32  // 
          || pData[IdxChr+3] != 116 //t 
          || pData[IdxChr+4] != 111 //o 
          || pData[IdxChr+5] != 32  // 
    )
    {
#if 0      
      String Str;
      Str.concat( (char)pData[8] );
      Str.concat( (char)pData[9] );  
      Str.concat( (char)pData[10] );
      Str.concat( (char)pData[11] );
      Str.concat( (char)pData[12] );
      Str.concat( (char)pData[13] );
      Str.concat( (char)pData[14] );
      Str.concat( (char)pData[15] );
      Str.concat( (char)pData[16] );
      Str.concat( (char)pData[17] );
      Str.concat( (char)pData[18] );
      Str.concat( (char)pData[19] );
      Str.concat( (char)pData[20] );
      Str.concat( (char)pData[21] );
      Str.concat( (char)pData[22] );
      Str.concat( (char)pData[23] );

      Serial.print( "DEBUG: Error parsing string ( after first value ): " );
      Serial.println( Str );
#endif

      return;
    }


    IdxChr += 6;
    String Hi;

    while( pData[IdxChr] >= 48 && pData[IdxChr] <= 57 && IdxChr <= length  )
    {
      Hi.concat( (char)pData[IdxChr++] );
    }


    if( IdxChr <= length && ( pData[IdxChr+0] != 109 || pData[IdxChr+1] != 109 ) )
    {
#if 0
      String StrErr;
      StrErr.concat( (char)pData[IdxChr+0] );
      StrErr.concat( (char)pData[IdxChr+1] );  

      String Str;
      Str.concat( (char)pData[8] );
      Str.concat( (char)pData[9] );
      Str.concat( (char)pData[10] );
      Str.concat( (char)pData[11] );
      Str.concat( (char)pData[12] );
      Str.concat( (char)pData[13] );
      Str.concat( (char)pData[14] );
      Str.concat( (char)pData[15] );
      Str.concat( (char)pData[16] );
      Str.concat( (char)pData[17] );
      Str.concat( (char)pData[18] );
      Str.concat( (char)pData[19] );
      Str.concat( (char)pData[20] );
      Str.concat( (char)pData[21] );
      Str.concat( (char)pData[22] );
      Str.concat( (char)pData[23] );

      Serial.print( "DEBUG: Error parsing string ( after second value ): " );
      Serial.print( StrErr );
      Serial.print( " != mm (" );
      Serial.print( Str );
      Serial.print( ") length: " );
      Serial.print( length );
      Serial.print( ", IdxChr: " );
      Serial.println( IdxChr );
#endif

      return;
    }


    int FocusLo = Lo.toInt();
    int FocusHi = Hi.toInt();

    float FocusMid = FocusLo;

    float FocusDiffHalf = 0.F;

    if( FocusHi > FocusLo )
    {
      float FocusDiff = FocusHi - FocusLo;
      
      FocusDiffHalf = FocusDiff / 2.F;

      FocusMid = FocusLo + FocusDiffHalf;
    }


    blu->setFocus( FocusMid, FocusDiffHalf );

    changed = true;


#if 0
    Serial.print( "DEBUG:" );
    Serial.print( " Lo: " ); Serial.print( Lo );
    Serial.print( ",  Hi: " ); Serial.print( Hi );
    Serial.print( ", FocusLo: " ); Serial.print( FocusLo );
    Serial.print( ", FocusHi: " ); Serial.print( FocusHi );
    Serial.print( ", FocusDiff: " ); Serial.print( FocusDiff );
    Serial.print( ", FocusDiffHalf: " ); Serial.print( FocusDiffHalf );
    Serial.print( ", FocusMid: " ); Serial.println( FocusMid );
#endif 


    //                  1   2   3   4   5   6   7   8
    //                 XXX XXX XXX XXX XXX XXX XXX XXX                 XXX                                         XXX
    //FocusPacketData: 255 009 000 000 012 012 005 002 053 048 048 109 109 032 067 078 045 069 049 056 045 056 048 109
    //FocusPacketData: 255 018 000 000 012 012 005 002 053 050 048 109 109 032 116 111 032 053 051 048 109 109 048 109
    //FocusPacketData: 255 018 000 000 012 012 005 002 053 054 048 109 109 032 116 111 032 053 055 048 109 109 048 109
    //FocusPacketData: 255 018 000 000 012 012 005 002 054 049 048 109 109 032 116 111 032 054 050 048 109 109 048 109
    //FocusPacketData: 255 018 000 000 012 012 005 002 054 054 048 109 109 032 116 111 032 054 056 048 109 109 048 109
    //FocusPacketData: 255 018 000 000 012 012 005 002 055 053 048 109 109 032 116 111 032 055 055 048 109 109 048 109
    //FocusPacketData: 255 018 000 000 012 012 005 002 056 055 048 109 109 032 116 111 032 057 048 048 109 109 048 109
    //FocusPacketData: 255 018 000 000 012 012 005 002 057 051 048 109 109 032 116 111 032 057 055 048 109 109 048 109
    //FocusPacketData: 255 019 000 000 012 012 005 002 057 055 048 109 109 032 116 111 032 049 048 049 048 109 109 109
    //FocusPacketData: 255 020 000 000 012 012 005 002 049 049 054 048 109 109 032 116 111 032 049 050 050 048 109 109
    //FocusPacketData: 255 020 000 000 012 012 005 002 049 053 056 048 109 109 032 116 111 032 049 055 049 048 109 109
    //FocusPacketData: 255 020 000 000 012 012 005 002 050 051 049 048 109 109 032 116 111 032 050 054 051 048 109 109
    //FocusPacketData: 255 020 000 000 012 012 005 002 052 054 054 048 109 109 032 116 111 032 054 051 053 048 109 109
    //FocusPacketData: 255 007 000 000 012 012 005 002 073 110 102 048 109 109 032 116 111 032 054 051 053 048 109 109

#if 0
    Serial.print( "FocusPacketData: " ); 
    Serial.print( pData[0] ); Serial.print( " " );
    Serial.print( pData[1] ); Serial.print( " " );
    Serial.print( pData[2] ); Serial.print( " " );
    Serial.print( pData[3] ); Serial.print( " " );
    Serial.print( pData[4] ); Serial.print( " " );
    Serial.print( pData[5] ); Serial.print( " " );
    Serial.print( pData[6] ); Serial.print( " " );
    Serial.print( pData[7] ); Serial.print( " " );
    Serial.print( pData[8] ); Serial.print( " " );
    Serial.print( pData[9] ); Serial.print( " " );

    Serial.print( pData[10] ); Serial.print( " " );
    Serial.print( pData[11] ); Serial.print( " " );
    Serial.print( pData[12] ); Serial.print( " " );
    Serial.print( pData[13] ); Serial.print( " " );
    Serial.print( pData[14] ); Serial.print( " " );
    Serial.print( pData[15] ); Serial.print( " " );
    Serial.print( pData[16] ); Serial.print( " " );
    Serial.print( pData[17] ); Serial.print( " " );
    Serial.print( pData[18] ); Serial.print( " " );
    Serial.print( pData[19] ); Serial.print( " " );

    Serial.print( pData[20] ); Serial.print( " " );
    Serial.print( pData[21] ); Serial.print( " " );
    Serial.print( pData[22] ); Serial.print( " " );
    Serial.println( pData[23] );
#endif
  }


  //Unknown packet: Length:  8: 0xFF 0x04 0x00 0x00 0x0C 0x0D 0x05 0x02
  else if( pData[0] == 0xFF && pData[4] == 0x0C && pData[5] == 0x0D ) { changed = true; }


  //Unknown packet: Length:  9: 0xFF 0x05 0x00 0x00 0x0C 0x0E 0x01 0x02 0x00
  else if( pData[0] == 0xFF && pData[4] == 0x0C && pData[5] == 0x0E ) { changed = true; }


  //Unknown packet: Length: 17: 0xFF 0x0D 0x00 0x00 0x0C 0x0F 0x05 0x02 0x4E 0x65 0x78 0x74 0x20 0x43 0x6C 0x69 0x70
  else if( pData[0] == 0xFF && pData[4] == 0x0C && pData[5] == 0x0F ) { changed = true; }


  //Unknown
  //if( pData[0] == XX && pData[4] == YY && pData[5] == ZZ ) { changed = true; }

  else 
  {
    //Serial.println("");
    Serial.print( "Unknown packet: Length: " );
    Serial.print( length );
    Serial.print( ": " );
    
    for( int IdxByte = 0; IdxByte < length; IdxByte++) 
    { 
      Serial.print( "0x" );
      printHex( pData[IdxByte] );

      if( IdxByte < length - 1 ) 
      {
        Serial.print( " " );
      }
    }
    

    Serial.println();

    delay( 5000 );
  }


#if DEBUG_PRINT_PACKET_CMD
  Serial.print( "Packet Cmd "); Serial.print( pData[4] ); Serial.print( "." ); Serial.print( pData[5] ); Serial.println();
#endif


  blu->setChanged( changed );
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
    Serial.print("Failed to find our characteristic UUID: "); Serial.println( IncomingCameraControl.toString().c_str() );
    return false;
  }


  _incomingCameraControl->registerForNotify( controlNotify, false );


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
      Serial.print( "DEBUG: Found " ); Serial.print( count ); Serial.println( " bluetooth devices." );

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
          Serial.print("DEBUG: Device #" ); Serial.print( IdxDevice );
          Serial.print(": Other Device found: "); Serial.print( address.toString().c_str() );

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
      Serial.println( "DEBUG: Didnt find any Blackmagic devices!" );
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
  
    Serial.println( "DEBUG: Connected to Blackmagic device." );

    return _cameraControl;
  }


  Serial.println( "DEBUG: No Blackmagic devices found!" );
  
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
