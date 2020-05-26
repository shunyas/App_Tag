/****************************************************************************
 * (C) Tokyo Cosmos Electric, Inc. (TOCOS) - 2013 all rights reserved.
 *
 * Condition to use:
 *   - The full or part of source code is limited to use for TWE (TOCOS
 *     Wireless Engine) as compiled and flash programmed.
 *   - The full or part of source code is prohibited to distribute without
 *     permission from TOCOS.
 *
 ****************************************************************************/

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <string.h>

#include <jendefs.h>
#include <AppHardwareApi.h>

#include "utils.h"

#include "Parent.h"
#include "config.h"
#include "Version.h"

#include "serial.h"
#include "fprintf.h"
#include "sprintf.h"

#include "btnMgr.h"

#include "Interactive.h"
#include "sercmd_gen.h"

#include "common.h"
#include "AddrKeyAry.h"

/****************************************************************************/
/***        ToCoNet Definitions                                           ***/
/****************************************************************************/
#define ToCoNet_USE_MOD_NWK_LAYERTREE // Network definition
#define ToCoNet_USE_MOD_NBSCAN // Neighbour scan module
#define ToCoNet_USE_MOD_NBSCAN_SLAVE // Neighbour scan slave module
//#define ToCoNet_USE_MOD_CHANNEL_MGR
#define ToCoNet_USE_MOD_NWK_MESSAGE_POOL
#define ToCoNet_USE_MOD_DUPCHK

// includes
#include "ToCoNet.h"
#include "ToCoNet_mod_prototype.h"

#include "app_event.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define TOCONET_DEBUG_LEVEL 0
#define USE_LCD

#ifdef USE_LCD
#include "LcdDriver.h"
#include "LcdDraw.h"
#include "LcdPrint.h"
#define V_PRINTF_LCD(...) vfPrintf(&sLcdStream, __VA_ARGS__)
#define V_PRINTF_LCD_BTM(...) vfPrintf(&sLcdStreamBtm, __VA_ARGS__)
#endif

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vInitHardware(int f_warm_start);
static void vSerialInit(uint32 u32Baud, tsUartOpt *pUartOpt);

void vSerOutput_Standard(uint8 u8lqi_1st, uint32 u32addr_1st, uint32 u32addr_rcvr, uint8 *p);
void vSerOutput_SmplTag3(uint8 u8lqi_1st, uint32 u32addr_1st, uint32 u32addr_rcvr, uint8 *p);

void vSerInitMessage();
void vProcessSerialCmd(tsSerCmd_Context *pCmd);

#ifdef USE_LCD
static void vLcdInit(void);
static void vLcdRefresh(void);
#endif
/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
tsAppData_Pa sAppData; // application information
tsFILE sSerStream; // serial output context
tsSerialPortSetup sSerPort; // serial port queue

tsSerCmd_Context sSerCmdOut; //!< シリアル出力用

tsAdrKeyA_Context sEndDevList; // 子機の発報情報を保存するデータベース

#ifdef USE_LCD
static tsFILE sLcdStream, sLcdStreamBtm;
#endif

static uint32 u32sec;

/****************************************************************************/
/***        ToCoNet Callback Functions                                    ***/
/****************************************************************************/
/**
 * アプリケーションの起動時の処理
 * - ネットワークの設定
 * - ハードウェアの初期化
 */
void cbAppColdStart(bool_t bAfterAhiInit) {
	if (!bAfterAhiInit) {
		// before AHI init, very first of code.

		// Register modules
		ToCoNet_REG_MOD_ALL();

	} else {
		// disable brown out detect
		vAHI_BrownOutConfigure(0, //0:2.0V 1:2.3V
				FALSE, FALSE, FALSE, FALSE);

		// clear application context
		memset(&sAppData, 0x00, sizeof(sAppData));
		ADDRKEYA_vInit(&sEndDevList);
		SPRINTF_vInit128();

		// フラッシュメモリからの読み出し
		//   フラッシュからの読み込みが失敗した場合、ID=15 で設定する
		sAppData.bFlashLoaded = Config_bLoad(&sAppData.sFlash);

		// ToCoNet configuration
		sToCoNet_AppContext.u32AppId = sAppData.sFlash.sData.u32appid;
		sToCoNet_AppContext.u8Channel = sAppData.sFlash.sData.u8ch;
		sToCoNet_AppContext.u32ChMask = sAppData.sFlash.sData.u32chmask;

		sToCoNet_AppContext.bRxOnIdle = TRUE;
		sToCoNet_AppContext.u8TxMacRetry = 1;

		// Register
		ToCoNet_Event_Register_State_Machine(vProcessEvCore);

		// Others
		vInitHardware(FALSE);
		Interactive_vInit();

		// シリアルの書式出力のため
		if (IS_APPCONF_OPT_UART_BIN()) {
			SerCmdBinary_vInit(&sSerCmdOut, NULL, 128); // バッファを指定せず初期化
		} else {
			SerCmdAscii_vInit(&sSerCmdOut, NULL, 128); // バッファを指定せず初期化
		}
	}
}

/**
 * スリープ復帰時の処理（本アプリケーションでは処理しない)
 * @param bAfterAhiInit
 */
void cbAppWarmStart(bool_t bAfterAhiInit) {
	cbAppColdStart(bAfterAhiInit);
}

/**
 * メイン処理
 * - シリアルポートの処理
 */
void cbToCoNet_vMain(void) {
	/* handle uart input */
	vHandleSerialInput();
}

/**
 * ネットワークイベント。
 * - E_EVENT_TOCONET_NWK_START\n
 *   ネットワーク開始時のイベントを vProcessEvCore に伝達
 *
 * @param eEvent
 * @param u32arg
 */
void cbToCoNet_vNwkEvent(teEvent eEvent, uint32 u32arg) {
	switch (eEvent) {
	case E_EVENT_TOCONET_NWK_START:
		// send this event to the local event machine.
		ToCoNet_Event_Process(eEvent, u32arg, vProcessEvCore);
		break;
	default:
		break;
	}
}

/**
 * 子機または中継機を経由したデータを受信する。
 *
 * - アドレスを取り出して、内部のデータベースへ登録（メッセージプール送信用）
 * - UART に指定書式で出力する
 *   - 出力書式\n
 *     ::(受信元ルータまたは親機のアドレス):(シーケンス番号):(送信元アドレス):(LQI)<CR><LF>
 *
 * @param pRx 受信データ構造体
 */
void cbToCoNet_vRxEvent(tsRxDataApp *pRx) {
	uint8 *p = pRx->auData;

	// 暗号化対応時に平文パケットは受信しない
	if (IS_APPCONF_OPT_SECURE()) {
		if (!pRx->bSecurePkt) {
			return;
		}
	}

	// パケットの表示
	if (pRx->u8Cmd == TOCONET_PACKET_CMD_APP_DATA) {
		// Turn on LED
		sAppData.u32LedCt = u32TickCount_ms;

		// LED の点灯を行う
		sAppData.u16LedDur_ct = 125;

		// 基本情報
		uint8 u8lqi_1st = pRx->u8Lqi;
		uint32 u32addr_1st = pRx->u32SrcAddr;

		// データの解釈
		uint8 u8b = G_OCTET();

		// 受信機アドレス
		uint32 u32addr_rcvr = TOCONET_NWK_ADDR_PARENT;
		if (u8b == 'R') {
			// ルータからの受信
			u32addr_1st = G_BE_DWORD();
			u8lqi_1st = G_OCTET();

			u32addr_rcvr = pRx->u32SrcAddr;
		}

		// 出力用の関数を呼び出す
		if (IS_APPCONF_OPT_SHT21()) {
			vSerOutput_SmplTag3(u8lqi_1st, u32addr_1st, u32addr_rcvr, p);
		} else {
			vSerOutput_Standard(u8lqi_1st, u32addr_1st, u32addr_rcvr, p);
		}

		// データベースへ登録（線形配列に格納している）
		ADDRKEYA_vAdd(&sEndDevList, u32addr_1st, 0); // アドレスだけ登録。
	}
}

/**
 * 送信完了時のイベント
 * @param u8CbId
 * @param bStatus
 */
void cbToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	return;
}

/**
 * ハードウェア割り込みの遅延実行部
 *
 * - BTM による IO の入力状態をチェック\n
 *   ※ 本サンプルでは特別な使用はしていない
 *
 * @param u32DeviceId
 * @param u32ItemBitmap
 */
void cbToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	//static uint32 u32LastTickPressed = 0;
	//uint32 bmPorts, bmChanged;

	switch (u32DeviceId) {
	case E_AHI_DEVICE_TICK_TIMER:
		// LED の点灯消灯を制御する
		if (sAppData.u16LedDur_ct) {
			sAppData.u16LedDur_ct--;
			if (sAppData.u16LedDur_ct) {
				vPortSet_TrueAsLo(PORT_KIT_LED1, TRUE);
			}
		} else {
			vPortSet_TrueAsLo(PORT_KIT_LED1, FALSE);
		}

		break;

	default:
		break;
	}
}

/**
 * ハードウェア割り込み
 * - 処理なし
 *
 * @param u32DeviceId
 * @param u32ItemBitmap
 * @return
 */
uint8 cbToCoNet_u8HwInt(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	return FALSE;
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/**
 * ハードウェアの初期化
 * @param f_warm_start
 */
static void vInitHardware(int f_warm_start) {
	// BAUD ピンが GND になっている場合、かつフラッシュの設定が有効な場合は、設定値を採用する (v1.0.3)
	tsUartOpt sUartOpt;
	memset(&sUartOpt, 0, sizeof(tsUartOpt));
	uint32 u32baud = UART_BAUD;
	if (sAppData.bFlashLoaded && bPortRead(PORT_BAUD)) {
		u32baud = sAppData.sFlash.sData.u32baud_safe;
		sUartOpt.bHwFlowEnabled = FALSE;
		sUartOpt.bParityEnabled = UART_PARITY_ENABLE;
		sUartOpt.u8ParityType = UART_PARITY_TYPE;
		sUartOpt.u8StopBit = UART_STOPBITS;

		// 設定されている場合は、設定値を採用する (v1.0.3)
		switch(sAppData.sFlash.sData.u8parity & APPCONF_UART_CONF_PARITY_MASK) {
		case 0:
			sUartOpt.bParityEnabled = FALSE;
			break;
		case 1:
			sUartOpt.bParityEnabled = TRUE;
			sUartOpt.u8ParityType = E_AHI_UART_ODD_PARITY;
			break;
		case 2:
			sUartOpt.bParityEnabled = TRUE;
			sUartOpt.u8ParityType = E_AHI_UART_EVEN_PARITY;
			break;
		}

		// ストップビット
		if (sAppData.sFlash.sData.u8parity & APPCONF_UART_CONF_STOPBIT_MASK) {
			sUartOpt.u8StopBit = E_AHI_UART_2_STOP_BITS;
		} else {
			sUartOpt.u8StopBit = E_AHI_UART_1_STOP_BIT;
		}

		// 7bitモード
		if (sAppData.sFlash.sData.u8parity & APPCONF_UART_CONF_WORDLEN_MASK) {
			sUartOpt.u8WordLen = 7;
		} else {
			sUartOpt.u8WordLen = 8;
		}

		vSerialInit(u32baud, &sUartOpt);
	} else {
		vSerialInit(u32baud, NULL);
	}

	ToCoNet_vDebugInit(&sSerStream);
	ToCoNet_vDebugLevel(TOCONET_DEBUG_LEVEL);

	// IO の設定
	vPortAsOutput(PORT_KIT_LED1);
	vPortAsOutput(PORT_KIT_LED2);
	vPortAsOutput(PORT_KIT_LED3);
	vPortAsOutput(PORT_KIT_LED4);
	vPortSetHi(PORT_KIT_LED1);
	vPortSetHi(PORT_KIT_LED2);
	vPortSetHi(PORT_KIT_LED3);
	vPortSetHi(PORT_KIT_LED4);

	// LCD の設定
#ifdef USE_LCD
	vLcdInit();
#endif
}

/**
 * UART の初期化
 */
static void vSerialInit(uint32 u32Baud, tsUartOpt *pUartOpt) {
	/* Create the debug port transmit and receive queues */
	static uint8 au8SerialTxBuffer[1532];
	static uint8 au8SerialRxBuffer[512];

	/* Initialise the serial port to be used for debug output */
	sSerPort.pu8SerialRxQueueBuffer = au8SerialRxBuffer;
	sSerPort.pu8SerialTxQueueBuffer = au8SerialTxBuffer;
	sSerPort.u32BaudRate = UART_BAUD;
	sSerPort.u16AHI_UART_RTS_LOW = 0xffff;
	sSerPort.u16AHI_UART_RTS_HIGH = 0xffff;
	sSerPort.u16SerialRxQueueSize = sizeof(au8SerialRxBuffer);
	sSerPort.u16SerialTxQueueSize = sizeof(au8SerialTxBuffer);
	sSerPort.u8SerialPort = UART_PORT;
	sSerPort.u8RX_FIFO_LEVEL = E_AHI_UART_FIFO_LEVEL_1;
	SERIAL_vInitEx(&sSerPort, pUartOpt);

	sSerStream.bPutChar = SERIAL_bTxChar;
	sSerStream.u8Device = UART_PORT;
}


/**
 * アプリケーション主要処理
 * - E_STATE_IDLE\n
 *   ネットワークの初期化、開始
 *
 * - E_STATE_RUNNING\n
 *   - データベースのタイムアウト処理
 *   - 定期的なメッセージプール送信
 *
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {

	if (eEvent == E_EVENT_TICK_SECOND) {
		u32sec++;
	}

	switch (pEv->eState) {
	case E_STATE_IDLE:
		if (eEvent == E_EVENT_START_UP) {
			vSerInitMessage();

			V_PRINTF(LB"[E_STATE_IDLE]");

			if (IS_APPCONF_OPT_SECURE()) {
				bool_t bRes = bRegAesKey(sAppData.sFlash.sData.u32EncKey);
				V_PRINTF(LB "*** Register AES key (%d) ***", bRes);
			}

			// Configure the Network
			sAppData.sNwkLayerTreeConfig.u8Layer = 0;
			sAppData.sNwkLayerTreeConfig.u8Role = TOCONET_NWK_ROLE_PARENT;
			sAppData.pContextNwk =
					ToCoNet_NwkLyTr_psConfig(&sAppData.sNwkLayerTreeConfig);
			if (sAppData.pContextNwk) {
				ToCoNet_Nwk_bInit(sAppData.pContextNwk);
				ToCoNet_Nwk_bStart(sAppData.pContextNwk);
			}

		} else if (eEvent == E_EVENT_TOCONET_NWK_START) {
			ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
		} else {
			;
		}
		break;

	case E_STATE_RUNNING:
		if (eEvent == E_EVENT_NEW_STATE) {
			V_PRINTF(LB"[E_STATE_RUNNING]");
		} else if (eEvent == E_EVENT_TICK_SECOND) {
			static uint8 u8Ct_s = 0;
			int i;

			// 定期クリーン（タイムアウトしたノードを削除する）
			ADDRKEYA_bFind(&sEndDevList, 0, 0);

			// 共有情報（メッセージプール）の送信
			//   - OCTET: 経過秒 (0xFF は終端)
			//   - BE_DWORD: 発報アドレス
			if (++u8Ct_s > PARENT_MSGPOOL_TX_DUR_s) {
				uint8 au8pl[TOCONET_MOD_MESSAGE_POOL_MAX_MESSAGE];
					// メッセージプールの最大バイト数は 64 なので、これに収まる数とする。
				uint8 *q = au8pl;

				for (i = 0; i < ADDRKEYA_MAX_HISTORY; i++) {
					if (sEndDevList.au32ScanListAddr[i]) {

						uint16 u16Sec = (u32TickCount_ms
								- sEndDevList.au32ScanListTick[i]) / 1000;
						if (u16Sec >= 0xF0)
							continue; // 古すぎるので飛ばす

						S_OCTET(u16Sec & 0xFF);
						S_BE_DWORD(sEndDevList.au32ScanListAddr[i]);
					}
				}
				S_OCTET(0xFF);

				S_OCTET('A'); // ダミーデータ(不要：テスト目的)
				S_OCTET('B');
				S_OCTET('C');
				S_OCTET('D');

				ToCoNet_MsgPl_bSetMessage(0, 0, q - au8pl, au8pl);
				u8Ct_s = 0;
			}
		} else {
			;
		}
		break;

	default:
		break;
	}
}

/**
 * 初期化メッセージ
 */
void vSerInitMessage() {
	A_PRINTF(LB "*** " APP_NAME " (Parent) %d.%02d-%d ***", VERSION_MAIN, VERSION_SUB, VERSION_VAR);
	A_PRINTF(LB "* App ID:%08x Long Addr:%08x Short Addr %04x LID %02d" LB,
			sToCoNet_AppContext.u32AppId, ToCoNet_u32GetSerial(), sToCoNet_AppContext.u16ShortAddress,
			sAppData.sFlash.sData.u8id);

#ifdef USE_LCD
	// 最下行を表示する
	V_PRINTF_LCD_BTM(" ToCoSamp IO Monitor %d.%02d-%d ", VERSION_MAIN, VERSION_SUB, VERSION_VAR);
	vLcdRefresh();
#endif
}

/**
 * 標準の出力
 */
void vSerOutput_Standard(uint8 u8lqi_1st, uint32 u32addr_1st, uint32 u32addr_rcvr, uint8 *p) {
	// ID などの基本情報
	uint8 u8id = G_OCTET();
	uint16 u16fct = G_BE_WORD();

	// 受信機のアドレス
	A_PRINTF("::rc=%08X", u32addr_rcvr);

	// LQI
	A_PRINTF(":lq=%d", u8lqi_1st);

	// フレーム
	A_PRINTF(":ct=%04X", u16fct);

	// 送信元子機アドレス
	A_PRINTF(":ed=%08X:id=%X", u32addr_1st, u8id);

	// パケットの種別により処理を変更
	uint8 u8pkt = G_OCTET();

	switch(u8pkt) {
	case PKT_ID_STANDARD:
		_C {
			uint8 u8batt = G_OCTET();

			uint16 u16adc1 = G_BE_WORD();
			uint16 u16adc2 = G_BE_WORD();
			uint16 u16pc1 = G_BE_WORD();
			uint16 u16pc2 = G_BE_WORD();

			// センサー情報
			A_PRINTF(":ba=%04d:a1=%04d:a2=%04d:p0=%03d:p1=%03d" LB,
					DECODE_VOLT(u8batt), u16adc1, u16adc2, u16pc1, u16pc2);

#ifdef USE_LCD
			// LCD への出力
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:A:%04d:%04d\n",
					u32sec % 1000,
					u32addr_1st,
					u8lqi_1st,
					u16fct & 0xFF,
					u16adc1,
					u16adc2
					);
			vLcdRefresh();
#endif
		}
		break;
	case PKT_ID_SHT21:
		_C {
			uint8 u8batt = G_OCTET();

			uint16 u16adc1 = G_BE_WORD();
			uint16 u16adc2 = G_BE_WORD();
			int16 i16temp = G_BE_WORD();
			int16 i16humd = G_BE_WORD();

			// センサー情報
			A_PRINTF(":ba=%04d:a1=%04d:a2=%04d:te=%04d:hu=%04d" LB,
					DECODE_VOLT(u8batt), u16adc1, u16adc2, i16temp, i16humd);

#ifdef USE_LCD
			// LCD への出力
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:S:%04d:%04d\n",
					u32sec % 1000,
					u32addr_1st,
					u8lqi_1st,
					u16fct & 0xFF,
					i16temp,
					i16humd
					);
			vLcdRefresh();
#endif
		}
		break;
	case PKT_ID_IO_TIMER:
		_C {
			uint8 u8stat = G_OCTET();
			uint32 u32dur = G_BE_DWORD();

			A_PRINTF(":btn=%d:dur=%d::" LB, u8stat, u32dur / 1000);

#ifdef USE_LCD
			// LCD への出力
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:B:%04d:%04d\n",
					u32sec % 1000,
					u32addr_1st,
					u8lqi_1st,
					u16fct & 0xFF,
					u8stat,
					u32dur / 1000
					);
			vLcdRefresh();
#endif
		}
		break;

	case PKT_ID_UART:
		_C {
			uint8 u8len = G_OCTET();

			sSerCmdOut.u16len = u8len;
			sSerCmdOut.au8data = p;

			sSerCmdOut.vOutput(&sSerCmdOut, &sSerStream);

			sSerCmdOut.au8data = NULL; // p は関数を抜けると無効であるため、念のため NULL に戻す
		}
		break;

	default:
		break;
	}

}

/**
 * SimpleTag v3 互換 (SHT21 用) の出力
 */
void vSerOutput_SmplTag3(uint8 u8lqi_1st, uint32 u32addr_1st, uint32 u32addr_rcvr, uint8 *p) {
	// ID などの基本情報
	uint8 u8id = G_OCTET(); (void)u8id;
	uint16 u16fct = G_BE_WORD();

	// パケットの種別により処理を変更
	uint8 u8pkt = G_OCTET();

	if (u8pkt == PKT_ID_SHT21) {
		uint8 u8batt = G_OCTET();
		uint16 u16adc1 = G_BE_WORD();
		uint16 u16adc2 = G_BE_WORD(); (void)u16adc2;
		int16 i16temp = G_BE_WORD();
		int16 i16humd = G_BE_WORD();

		vfPrintf(&sSerStream, ";"
				"%d;"			// TIME STAMP
				"%08X;"			// 受信機のアドレス
				"%03d;"			// LQI  (0-255)
				"%03d;"			// 連番
				"%07x;"			// シリアル番号
				"%04d;"			// 電源電圧 (0-3600, mV)
				"%04d;"			// SHT21 TEMP
				"%04d;"			// SHT21 HUMID
				"%04d;"			// adc1
				"%04d;"			// adc2
				LB,
				u32TickCount_ms / 1000,
				u32addr_rcvr & 0x0FFFFFFF,
				u8lqi_1st,
				u16fct,
				u32addr_1st & 0x0FFFFFFF,
				DECODE_VOLT(u8batt),
				i16temp,
				i16humd,
				u16adc1,
				u16adc2
		);

#ifdef USE_LCD
		// LCD への出力
		V_PRINTF_LCD("%03d:%08X:%03d:%02X:S:%04d:%04d\n",
				u32sec % 1000,
				u32addr_1st,
				u8lqi_1st,
				u16fct & 0xFF,
				i16temp,
				i16humd
				);
		vLcdRefresh();
#endif
	}

	if (u8pkt == PKT_ID_STANDARD) {
		uint8 u8batt = G_OCTET();

		uint16 u16adc1 = G_BE_WORD();
		uint16 u16adc2 = G_BE_WORD();
		uint16 u16pc1 = G_BE_WORD();
		uint16 u16pc2 = G_BE_WORD();

		// LM61用の温度変換
		//   Vo=T[℃]x10[mV]+600[mV]
		//   T     = Vo/10-60
		//   100xT = 10xVo-6000
		int32 iTemp = 10 * (int32)u16adc2 - 6000L;

		// センサー情報
		vfPrintf(&sSerStream, ";"
				"%d;"			// TIME STAMP
				"%08X;"			// 受信機のアドレス
				"%03d;"			// LQI  (0-255)
				"%03d;"			// 連番
				"%07x;"			// シリアル番号
				"%04d;"			// 電源電圧 (0-3600, mV)
				"%04d;"			// LM61温度(100x ℃)
				"%04d;"			// SuperCAP 電圧(mV)
				"%04d;"			// PC1
				"%04d;"			// PC2
				LB,
				u32TickCount_ms / 1000,
				u32addr_rcvr & 0x0FFFFFFF,
				u8lqi_1st,
				u16fct,
				u32addr_1st & 0x0FFFFFFF,
				DECODE_VOLT(u8batt),
				iTemp,
				u16adc1 * 2 * 3, // 3300mV で 99% 相当
				u16pc1,
				u16pc2
		);

#ifdef USE_LCD
		// LCD への出力
		V_PRINTF_LCD("%03d:%08X:%03d:%02X:S:%04d:%04d\n",
				u32sec % 1000,
				u32addr_1st,
				u8lqi_1st,
				u16fct & 0xFF,
				iTemp,
				u16adc1 * 2
				);
		vLcdRefresh();
#endif
	}
}

/**
 * コマンド受け取り時の処理
 * @param pCmd
 */
void vProcessSerialCmd(tsSerCmd_Context *pCmd) {
	V_PRINTF(LB "! cmd len=%d data=", pCmd->u16len);
	int i;
	for (i = 0; i < pCmd->u16len && i < 8; i++) {
		V_PRINTF("%02X", pCmd->au8data[i]);
	}
	if (i < pCmd->u16len) {
		V_PRINTF("...");
	}

	return;
}


#ifdef USE_LCD
/**
 * LCDの初期化
 */
static void vLcdInit(void) {
	/* Initisalise the LCD */
	vLcdReset(3, 0);

	/* register for vfPrintf() */
	sLcdStream.bPutChar = LCD_bTxChar;
	sLcdStream.u8Device = 0xFF;
	sLcdStreamBtm.bPutChar = LCD_bTxBottom;
	sLcdStreamBtm.u8Device = 0xFF;
}

/**
 * LCD を描画する
 */
static void vLcdRefresh(void) {
	vLcdClear();
	vDrawLcdDisplay(0, TRUE); /* write to lcd module */
	vLcdRefreshAll(); /* display new data */
}
#endif
/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
