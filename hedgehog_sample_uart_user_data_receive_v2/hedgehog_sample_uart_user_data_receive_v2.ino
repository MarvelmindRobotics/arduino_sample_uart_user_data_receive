/*
 *   This example reads generic user data from mobile beacon (hedgehog)
 */

/*
  The circuit:
 * Serial data from hedgehog : digital pin 0 (RXD)
 * Serial data to hedgehog : digital pin 1 (TXD)
 * LCD RS pin : digital pin 8
 * LCD Enable pin : digital pin 9
 * LCD D4 pin : digital pin 4
 * LCD D5 pin : digital pin 5
 * LCD D6 pin : digital pin 6
 * LCD D7 pin : digital pin 7
 * LCD BL pin : digital pin 10
 *Vcc pin :  +5
 */

#include <LiquidCrystal.h>

#define SERIAL_MONITOR_PRINT

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//  MARVELMIND HEDGEHOG RELATED PART

#define PACKET_TYPE_WRITE_TO_DEVICE 0x4a

#define GENERIC_PAYLOAD_PACKET_ID 0x0280

#define DATA_OFS 5

uint8_t user_payload_buf[256];
uint8_t user_payload_size;
int64_t user_payload_timestamp;
int user_payload_updated;

///

#define HEDGEHOG_BUF_SIZE 250 
byte hedgehog_serial_buf[HEDGEHOG_BUF_SIZE];
byte hedgehog_serial_buf_ofs;
byte hedgehog_packet_size;

byte hedgehog_packet_type;
int hedgehog_packet_id;

typedef union {byte b[2]; unsigned int w;} uni_8x2_16;
typedef union {byte b[8];int64_t vi64;} uni_8x8_64;

////////////////////////////////////////////////////////////////////////////

//    Marvelmind hedgehog support initialize
void setup_hedgehog() 
{int i;
  Serial.begin(500000); // hedgehog transmits data on 500 kbps  

  hedgehog_serial_buf_ofs= 0;
  user_payload_updated= 0;

  hedgehog_packet_id= 0;
}

////////////////////////////////////////

void restart_packet_receive()
{
   hedgehog_serial_buf_ofs= 0;// restart bufer fill
   hedgehog_packet_id= 0;    
}

void process_write_packet()
{
  if (hedgehog_packet_id == GENERIC_PAYLOAD_PACKET_ID)
     {// movement path packet
        int i;
        uni_8x8_64 un64;
        byte recv_size;

        recv_size= hedgehog_serial_buf[4];
        if (recv_size<8) return;
        user_payload_size= recv_size - 8;

        un64.b[0]= hedgehog_serial_buf[5];
        un64.b[1]= hedgehog_serial_buf[6];
        un64.b[2]= hedgehog_serial_buf[7];
        un64.b[3]= hedgehog_serial_buf[8];
        un64.b[4]= hedgehog_serial_buf[9];
        un64.b[5]= hedgehog_serial_buf[10];
        un64.b[6]= hedgehog_serial_buf[11];
        un64.b[7]= hedgehog_serial_buf[12];
        user_payload_timestamp= un64.vi64;

        for(i=0;i<user_payload_size;i++)
          user_payload_buf[i]= hedgehog_serial_buf[5+8+i];

        user_payload_updated= 1;
     }
}

// Marvelmind hedgehog service loop
void loop_hedgehog()
{int incoming_byte;
 int total_received_in_loop;
 int packet_received;
 int packet_id;
 int i,n,ofs;

  total_received_in_loop= 0;
  packet_received= 0;
  while(Serial.available() > 0)
    {
      if (hedgehog_serial_buf_ofs>=HEDGEHOG_BUF_SIZE) 
        {
          hedgehog_serial_buf_ofs= 0;
          break;// buffer overflow
        }
      total_received_in_loop++;
      if (total_received_in_loop>100) break;// too much data without required header
      
      incoming_byte= Serial.read();
  
      if (hedgehog_serial_buf_ofs==0)
        {// check first bytes for constant value
          if (incoming_byte != 0xff) 
            {
              hedgehog_serial_buf_ofs= 0;// restart bufer fill
              hedgehog_packet_id= 0;
              continue;
            }
        } 
      else if (hedgehog_serial_buf_ofs==1)
        {// check packet type
          if ( (incoming_byte == PACKET_TYPE_WRITE_TO_DEVICE) 
             )
            {// correct packet type - save  
              hedgehog_packet_type= incoming_byte; 
            }
           else
            {// incorrect packet type - skip packet
              restart_packet_receive();
              continue;            
            }
        }
      else if (hedgehog_serial_buf_ofs==3) 
        {// Check two-bytes packet ID
          hedgehog_packet_id= hedgehog_serial_buf[2] + incoming_byte*256;
          switch(hedgehog_packet_type)
          {
              case PACKET_TYPE_WRITE_TO_DEVICE:
              {
                switch(hedgehog_packet_id)
                  {
                    case GENERIC_PAYLOAD_PACKET_ID:
                      {                     
                        break;
                      }

                    default:
                      {// incorrect packet ID - skip packet
                        restart_packet_receive();
                        continue;                        
                      }
                  }
                break;
              }
          }        
        }
    else if (hedgehog_serial_buf_ofs==4) 
      {// data field size
        //if (check_packet_data_size(PACKET_TYPE_WRITE_TO_DEVICE, GENERIC_PAYLOAD_PACKET_ID, incoming_byte, 0x0c) == 0)
        //   continue;// incorrect movement path packet data size
           
         // save required packet size   
         hedgehog_packet_size= incoming_byte + 7;
      }
        
      hedgehog_serial_buf[hedgehog_serial_buf_ofs++]= incoming_byte; 
      if (hedgehog_serial_buf_ofs>5) 
       if (hedgehog_serial_buf_ofs == hedgehog_packet_size)
        {// received packet with required header
          packet_received= 1;
          hedgehog_serial_buf_ofs= 0;// restart bufer fill
          break; 
        }
    }
    

  if (packet_received)  
    {    
      hedgehog_set_crc16(&hedgehog_serial_buf[0], hedgehog_packet_size);// calculate CRC checksum of packet
      if ((hedgehog_serial_buf[hedgehog_packet_size] == 0)&&(hedgehog_serial_buf[hedgehog_packet_size+1] == 0))
        {// checksum success       
          switch(hedgehog_packet_type)
          {        
                      
            case PACKET_TYPE_WRITE_TO_DEVICE:
             {            
               process_write_packet();
               break;
             }
          }//switch  
        }
    }// if (packet_received)  
}// loop_hedgehog

// Calculate CRC-16 of hedgehog packet
void hedgehog_set_crc16(byte *buf, byte size)
{uni_8x2_16 sum;
 byte shift_cnt;
 byte byte_cnt;

  sum.w=0xffffU;

  for(byte_cnt=size; byte_cnt>0; byte_cnt--)
   {
   sum.w=(unsigned int) ((sum.w/256U)*256U + ((sum.w%256U)^(buf[size-byte_cnt])));

     for(shift_cnt=0; shift_cnt<8; shift_cnt++)
       {
         if((sum.w&0x1)==1) sum.w=(unsigned int)((sum.w>>1)^0xa001U);
                       else sum.w>>=1;
       }
   }

  buf[size]=sum.b[0];
  buf[size+1]=sum.b[1];// little endian
}// hedgehog_set_crc16

//  END OF MARVELMIND HEDGEHOG RELATED PART
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

LiquidCrystal lcd(8, 13, 9, 4, 5, 6, 7);

void print_out(const char *buf) 
{
  lcd.print(buf);
  #ifdef SERIAL_MONITOR_PRINT
  Serial.write(buf);
  #endif
}

void print_out(int v) 
{
  lcd.print(v);
  #ifdef SERIAL_MONITOR_PRINT
  Serial.print(v);
  #endif
}

void setup()
{
  lcd.clear(); 
  lcd.begin(16, 2);
  lcd.setCursor(0,0); 

  setup_hedgehog();//    Marvelmind hedgehog support initialize
}

void loop()
{int i;

   //delayMicroseconds(500);
   
   loop_hedgehog();// Marvelmind hedgehog service loop

   if (user_payload_updated)
     {// new data from hedgehog available
       user_payload_updated= 0;// clear new data flag 

       for(i=0; i<user_payload_size;i++) {
         lcd.setCursor(0,0);
         print_out(user_payload_buf[i]); 
         print_out("  ");
         if ((i%10)==9) {
            #ifdef SERIAL_MONITOR_PRINT
            Serial.print("\r\n");
            #endif
         }
       } 
       #ifdef SERIAL_MONITOR_PRINT
       Serial.print("\r\n\r\n");
       #endif
     }
}
