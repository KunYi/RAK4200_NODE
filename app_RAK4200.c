
#include "rui.h"

#include "board.h"
#include "modbus.h"

static RUI_RETURN_STATUS rui_return_status;
// join cnt
#define JOIN_MAX_CNT 6
static uint8_t JoinCnt = 0;
RUI_LORA_STATUS_T app_lora_status; // record status

/*******************************************************************************************
 * The BSP user functions.
 *
 * *****************************************************************************************/

const uint8_t level[2] = {0, 1};
#define low &level[0]
#define high &level[1]

RUI_I2C_ST I2c_1;
volatile static bool autosend_flag = false; // auto send flag
static uint8_t a[80] = {};                  // Data buffer to be sent by lora
bool IsJoiningflag = false; // flag whether joining or not status
bool sample_status =
    false; // flag sensor sample record for print sensor data by AT command

// read slave address 0x01, from register 0 to 2 */
const uint8_t fixModbusTx[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x03, 0xB0, 0x0B};
static uint8_t modData[20] = {0};
static uint8_t modPtr = 0;
MODBUS_HANDLER dev = {0};

static RUI_TIMER_ST appTimer;
static uint8_t timer_flag = 0;
static void timer_callback(void) { timer_flag = 1; }

static void timer_init() {
  appTimer.timer_mode = RUI_TIMER_MODE_REPEATED;
  rui_timer_init(&appTimer, timer_callback);
  rui_timer_setvalue(&appTimer, 5000); // 5 second
  rui_timer_start(&appTimer);
}

void bspInit(void) {
  /* initial UART2 to 9600 */
  rui_uart_init(RUI_UART2, BAUDRATE_9600);
  /* UART2 to UNVARNISHED */
  rui_uart_mode_config(RUI_UART2, RUI_UART_UNVARNISHED);
  timer_init();
  dev.slave = 0x01;
  dev.func = 0x04;
  dev.offset = 0;
  dev.size = 3;
}

void rui_lora_autosend_callback(void) // auto_send timeout event callback
{
  autosend_flag = true;
  IsJoiningflag = false;
}

void app_loop(void) {
  static uint8_t sensor_data_cnt = 0; // send data counter by LoRa
  static uint16_t timerCnt = 0;
  rui_lora_get_status(false, &app_lora_status);

  if (timer_flag) {
    timerCnt++;
    timer_flag = 0;
  }

  if (app_lora_status.IsJoined) // if LoRaWAN is joined
  {
    if (autosend_flag) {
      autosend_flag = false;
      rui_delay_ms(5);
      /*****************************************************************************
       * user app loop code
       *****************************************************************************/
      if (sensor_data_cnt != 0) {
        sample_status = true;
        RUI_LOG_PRINTF("\r\n");
        rui_return_status = rui_lora_send(8, a, sensor_data_cnt);
        switch (rui_return_status) {
        case RUI_STATUS_OK:
          RUI_LOG_PRINTF("[LoRa]: send out\r\n");
          break;
        default:
          RUI_LOG_PRINTF("[LoRa]: send error %d\r\n", rui_return_status);
          rui_lora_get_status(false, &app_lora_status);
          switch (app_lora_status.autosend_status) {
          case RUI_AUTO_ENABLE_SLEEP:
            rui_lora_set_send_interval(
                RUI_AUTO_ENABLE_SLEEP,
                app_lora_status.lorasend_interval); // start autosend_timer
                                                    // after send success
            break;
          case RUI_AUTO_ENABLE_NORMAL:
            rui_lora_set_send_interval(
                RUI_AUTO_ENABLE_NORMAL,
                app_lora_status.lorasend_interval); // start autosend_timer
                                                    // after send success
            break;
          default:
            break;
          }
          break;
        }
        sensor_data_cnt = 0;
      } else {
        rui_lora_set_send_interval(
            RUI_AUTO_DISABLE, 0); // stop it auto send data if no sensor data.
      }
    } else {
      // 1 minute
      if (timerCnt > (60 / 5)) {
        uint8_t txb[10];
        rui_uart_send(RUI_UART2, fixModbusTx, sizeof(fixModbusTx));
        makeModbusPacket(txb, sizeof(txb), &dev);
        rui_uart_send(RUI_UART2, txb, 8);
      }
    }
  } else if (IsJoiningflag == false) {
    IsJoiningflag = true;
    if (app_lora_status.join_mode == RUI_OTAA) {
      rui_return_status = rui_lora_join();
      switch (rui_return_status) {
      case RUI_STATUS_OK:
        RUI_LOG_PRINTF("OTAA Join Start...\r\n");
        break;
      case RUI_LORA_STATUS_PARAMETER_INVALID:
        RUI_LOG_PRINTF("ERROR: RUI_AT_PARAMETER_INVALID %d\r\n",
                       RUI_AT_PARAMETER_INVALID);
        rui_lora_get_status(
            false, &app_lora_status); // The query gets the current status
        switch (app_lora_status.autosend_status) {
        case RUI_AUTO_ENABLE_SLEEP:
          rui_lora_set_send_interval(
              RUI_AUTO_ENABLE_SLEEP,
              app_lora_status.lorasend_interval); // start autosend_timer after
                                                  // send success
          break;
        case RUI_AUTO_ENABLE_NORMAL:
          rui_lora_set_send_interval(
              RUI_AUTO_ENABLE_NORMAL,
              app_lora_status.lorasend_interval); // start autosend_timer after
                                                  // send success
          break;
        default:
          break;
        }
        break;
      default:
        RUI_LOG_PRINTF("ERROR: LORA_STATUS_ERROR %d\r\n", rui_return_status);
        rui_lora_get_status(false, &app_lora_status);
        switch (app_lora_status.autosend_status) {
        case RUI_AUTO_ENABLE_SLEEP:
          rui_lora_set_send_interval(
              RUI_AUTO_ENABLE_SLEEP,
              app_lora_status.lorasend_interval); // start autosend_timer after
                                                  // send success
          break;
        case RUI_AUTO_ENABLE_NORMAL:
          rui_lora_set_send_interval(
              RUI_AUTO_ENABLE_NORMAL,
              app_lora_status.lorasend_interval); // start autosend_timer after
                                                  // send success
          break;
        default:
          break;
        }
        break;
      }
    }
  }
}

/*******************************************************************************************
 * LoRaMac callback functions
 * * void LoRaReceive_callback(RUI_RECEIVE_T* Receive_datapackage);//LoRaWAN
 * callback if receive data
 * * void LoRaP2PReceive_callback(RUI_LORAP2P_RECEIVE_T
 * *Receive_P2Pdatapackage);//LoRaP2P callback if receive data
 * * void LoRaWANJoined_callback(uint32_t status);//LoRaWAN callback after join
 * server request
 * * void LoRaWANSendsucceed_callback(RUI_MCPS_T status);//LoRaWAN call back
 * after send data complete
 * *****************************************************************************************/
void LoRaReceive_callback(RUI_RECEIVE_T *Receive_datapackage) {
  char hex_str[3] = {0};
  RUI_LOG_PRINTF("at+recv=%d,%d,%d,%d", Receive_datapackage->Port,
                 Receive_datapackage->Rssi, Receive_datapackage->Snr,
                 Receive_datapackage->BufferSize);

  if ((Receive_datapackage->Buffer != NULL) &&
      Receive_datapackage->BufferSize) {
    RUI_LOG_PRINTF(":");
    for (int i = 0; i < Receive_datapackage->BufferSize; i++) {
      sprintf(hex_str, "%02x", Receive_datapackage->Buffer[i]);
      RUI_LOG_PRINTF("%s", hex_str);
    }
  }
  RUI_LOG_PRINTF("\r\n");
}
void LoRaP2PReceive_callback(RUI_LORAP2P_RECEIVE_T *Receive_P2Pdatapackage) {
  char hex_str[3] = {0};
  RUI_LOG_PRINTF("at+recv=%d,%d,%d:", Receive_P2Pdatapackage->Rssi,
                 Receive_P2Pdatapackage->Snr,
                 Receive_P2Pdatapackage->BufferSize);
  for (int i = 0; i < Receive_P2Pdatapackage->BufferSize; i++) {
    sprintf(hex_str, "%02X", Receive_P2Pdatapackage->Buffer[i]);
    RUI_LOG_PRINTF("%s", hex_str);
  }
  RUI_LOG_PRINTF("\r\n");
}
void LoRaWANJoined_callback(uint32_t status) {
  static int8_t dr;
  if (status) // Join Success
  {
    JoinCnt = 0;
    IsJoiningflag = false;
    RUI_LOG_PRINTF("[LoRa]:Join Success\r\nOK\r\n");
    rui_lora_get_status(false, &app_lora_status);
    if (app_lora_status.autosend_status != RUI_AUTO_DISABLE) {
      autosend_flag = true; // set autosend_flag after join LoRaWAN succeeded
    }
  } else {
    if (JoinCnt < JOIN_MAX_CNT) // Join was not successful. Try to join again
    {
      JoinCnt++;
      RUI_LOG_PRINTF("[LoRa]:Join retry Cnt:%d\r\n", JoinCnt);
      rui_lora_get_status(false, &app_lora_status);
      if (app_lora_status.lora_dr > 0) {
        app_lora_status.lora_dr -= 1;
      } else
        app_lora_status.lora_dr = 0;
      rui_lora_set_dr(app_lora_status.lora_dr);
      rui_lora_join();
    } else // Join failed
    {
      RUI_LOG_PRINTF("ERROR: RUI_AT_LORA_INFO_STATUS_JOIN_FAIL %d\r\n",
                     RUI_AT_LORA_INFO_STATUS_JOIN_FAIL);
      rui_lora_get_status(false, &app_lora_status);
      switch (app_lora_status.autosend_status) {
      case RUI_AUTO_ENABLE_SLEEP:
        rui_lora_set_send_interval(
            RUI_AUTO_ENABLE_SLEEP,
            app_lora_status
                .lorasend_interval); // start autosend_timer after send success
        break;
      case RUI_AUTO_ENABLE_NORMAL:
        rui_lora_set_send_interval(
            RUI_AUTO_ENABLE_NORMAL,
            app_lora_status
                .lorasend_interval); // start autosend_timer after send success
        break;
      default:
        break;
      }
      JoinCnt = 0;
    }
  }
}

void LoRaWANSendsucceed_callback(RUI_MCPS_T mcps_type,
                                 RUI_RETURN_STATUS status) {
  if (status == RUI_STATUS_OK) {
    switch (mcps_type) {
    case RUI_MCPS_UNCONFIRMED: {
      RUI_LOG_PRINTF("[LoRa]: RUI_MCPS_UNCONFIRMED send success\r\nOK\r\n");
      break;
    }
    case RUI_MCPS_CONFIRMED: {
      RUI_LOG_PRINTF("[LoRa]: RUI_MCPS_CONFIRMED send success\r\nOK\r\n");
      break;
    }
    case RUI_MCPS_PROPRIETARY: {
      RUI_LOG_PRINTF("[LoRa]: RUI_MCPS_PROPRIETARY send success\r\nOK\r\n");
      break;
    }
    case RUI_MCPS_MULTICAST: {
      RUI_LOG_PRINTF("[LoRa]: RUI_MCPS_MULTICAST send success\r\nOK\r\n");
      break;
    }
    default:
      break;
    }
  } else {
    RUI_LOG_PRINTF("ERROR: RUI_RETURN_STATUS %d\r\n", status);
  }

  rui_lora_get_status(false,
                      &app_lora_status); // The query gets the current status
  switch (app_lora_status.autosend_status) {
  case RUI_AUTO_ENABLE_SLEEP:
    rui_lora_set_send_interval(
        RUI_AUTO_ENABLE_SLEEP,
        app_lora_status
            .lorasend_interval); // start autosend_timer after send success
    rui_delay_ms(5);
    break;
  case RUI_AUTO_ENABLE_NORMAL:
    rui_lora_set_send_interval(
        RUI_AUTO_ENABLE_NORMAL,
        app_lora_status
            .lorasend_interval); // start autosend_timer after send success
    break;
  default:
    break;
  }
}

/*******************************************************************************************
 * The RUI is used to receive data from uart.
 *
 * *****************************************************************************************/
void rui_uart_recv(RUI_UART_DEF uart_def, uint8_t *pdata, uint16_t len) {
  switch (uart_def) {
  case RUI_UART1: // process code if RUI_UART1 work at RUI_UART_UNVARNISHED
    break;
  case RUI_UART2: // process RS485
    modData[modPtr++] = pdata[0];
    if (modPtr >= sizeof(modData))
      modPtr = 0;
    break;
  case RUI_UART3: // process code if RUI_UART3 received data ,the len is always
                  // 1
    /*****************************************************************************
     * user code
     ******************************************************************************/
    break;
  default:
    break;
  }
}

/*******************************************************************************************
 * sleep and wakeup callback
 *
 * *****************************************************************************************/
void bsp_sleep(void) {
  /*****************************************************************************
   * user process code before enter sleep
   ******************************************************************************/
}
void bsp_wakeup(void) {
  /*****************************************************************************
   * user process code after exit sleep
   ******************************************************************************/
}

/*******************************************************************************************
 * the app_main function
 * *****************************************************************************************/
void main(void) {
  static RUI_LORA_AUTO_SEND_MODE
      autosendtemp_status; // Flag whether modify autosend_interval by AT_cmd

  rui_init();

  /*******************************************************************************************
   * Register LoRaMac callback function
   *
   * *****************************************************************************************/
  rui_lora_register_recv_callback(LoRaReceive_callback);
  rui_lorap2p_register_recv_callback(LoRaP2PReceive_callback);
  rui_lorajoin_register_callback(LoRaWANJoined_callback);
  rui_lorasend_complete_register_callback(LoRaWANSendsucceed_callback);

  /*******************************************************************************************
   * Register Sleep and Wakeup callback function
   *
   * *****************************************************************************************/
  rui_sensor_register_callback(bsp_wakeup, bsp_sleep);

  /*******************************************************************************************
   *The query gets the current status
   *
   * *****************************************************************************************/
  rui_lora_get_status(false, &app_lora_status);
  autosendtemp_status = app_lora_status.autosend_status;

  if (app_lora_status.autosend_status)
    RUI_LOG_PRINTF("autosend_interval: %us\r\n",
                   app_lora_status.lorasend_interval);

  /*******************************************************************************************
   *Init OK ,print board status and auto join LoRaWAN
   *
   * *****************************************************************************************/
  switch (app_lora_status.work_mode) {
  case RUI_LORAWAN:
    RUI_LOG_PRINTF("Initialization OK,Current work_mode:LoRaWAN,");
    if (app_lora_status.join_mode == RUI_OTAA) {
      RUI_LOG_PRINTF(" join_mode:OTAA,");
      switch (app_lora_status.class_status) {
      case RUI_CLASS_A:
        RUI_LOG_PRINTF(" Class: A\r\n");
        break;
      case RUI_CLASS_B:
        RUI_LOG_PRINTF(" Class: B\r\n");
        break;
      case RUI_CLASS_C:
        RUI_LOG_PRINTF(" Class: C\r\n");
        break;
      default:
        break;
      }
    } else if (app_lora_status.join_mode == RUI_ABP) {
      RUI_LOG_PRINTF(" join_mode:ABP,\r\n");
      switch (app_lora_status.class_status) {
      case RUI_CLASS_A:
        RUI_LOG_PRINTF(" Class: A\r\n");
        break;
      case RUI_CLASS_B:
        RUI_LOG_PRINTF(" Class: B\r\n");
        break;
      case RUI_CLASS_C:
        RUI_LOG_PRINTF(" Class: C\r\n");
        break;
      default:
        break;
      }
      if (rui_lora_join() == RUI_STATUS_OK) // join LoRaWAN by ABP mode
      {
        LoRaWANJoined_callback(1); // ABP mode join success
      }
    }
    break;
  case RUI_P2P:
    RUI_LOG_PRINTF("Current work_mode:P2P\r\n");
    break;
  default:
    break;
  }

  RUI_LOG_PRINTF("\r\n");

  while (1) {
    rui_lora_get_status(false,
                        &app_lora_status); // The query gets the current status
    rui_running();
    switch (app_lora_status.work_mode) {
    case RUI_LORAWAN:
      if (autosendtemp_status != app_lora_status.autosend_status) {
        autosendtemp_status = app_lora_status.autosend_status;
        if (autosendtemp_status == RUI_AUTO_DISABLE) {
          rui_lora_set_send_interval(RUI_AUTO_DISABLE, 0); // stop auto send
                                                           // data
          autosend_flag = false;
        } else {
          autosend_flag = true;
        }
      }

      app_loop();

      break;
    case RUI_P2P:
      /*************************************************************************************
       * user code at LoRa P2P mode
       *************************************************************************************/
      break;
    default:
      break;
    }
  }
}
