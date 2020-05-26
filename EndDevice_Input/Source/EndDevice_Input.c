/****************************************************************************
 * (C) Tokyo Cosmos Electric, Inc. (TOCOS) - all rights reserved.
 *
 * Condition to use: (refer to detailed conditions in Japanese)
 *   - The full or part of source code is limited to use for TWE (TOCOS
 *     Wireless Engine) as compiled and flash programmed.
 *   - The full or part of source code is prohibited to distribute without
 *     permission from TOCOS.
 *
 * 利用条件:
 *   - 本ソースコードは、別途ソースコードライセンス記述が無い限り東京コスモス電機が著作権を
 *     保有しています。
 *   - 本ソースコードは、無保証・無サポートです。本ソースコードや生成物を用いたいかなる損害
 *     についても東京コスモス電機は保証致しません。不具合等の報告は歓迎いたします。
 *   - 本ソースコードは、東京コスモス電機が販売する TWE シリーズ上で実行する前提で公開
 *     しています。他のマイコン等への移植・流用は一部であっても出来ません。
 *
 ****************************************************************************/

/**
 * 概要
 *
 * 本パートは、IO状態を取得し、ブロードキャストにて、これをネットワーク層に
 * 送付する。
 *
 */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <string.h>

#include <jendefs.h>
#include <AppHardwareApi.h>

#include "AppQueueApi_ToCoNet.h"

#include "config.h"
#include "ccitt8.h"
#include "Interrupt.h"

#include "EndDevice_Input.h"
#include "Version.h"

#include "utils.h"

#include "config.h"
#include "common.h"

#include "adc.h"

// Serial options
#include "serial.h"
#include "fprintf.h"
#include "sprintf.h"

#include "input_string.h"
#include "Interactive.h"
#include "flash.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/**
 * DI1 の割り込み（立ち上がり）で起床後
 *   - PWM1 に DUTY50% で 100ms のブザー制御
 *
 * 以降１秒置きに起床して、DI1 が Hi (スイッチ開) かどうかチェックし、
 * Lo になったら、割り込みスリープに遷移、Hi が維持されていた場合は、
 * 一定期間 .u16Slp 経過後にブザー制御を 100ms 実施する。
 */
tsTimerContext sTimerPWM[1]; //!< タイマー管理構造体  @ingroup MASTER

/**
 * アプリケーションごとの振る舞いを記述するための関数テーブル
 */
tsCbHandler *psCbHandler = NULL;

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
static void vInitHardware(int f_warm_start);
static void vInitPulseCounter();
static void vInitADC();

static void vSerialInit();
void vSerInitMessage();
void vProcessSerialCmd(tsSerCmd_Context *pCmd);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
// Local data used by the tag during operation
tsAppData_Ed sAppData;

tsFILE sSerStream;
tsSerialPortSetup sSerPort;

/****************************************************************************/
/***        Functions                                                     ***/
/****************************************************************************/

/**
 * 始動時の処理
 */
void cbAppColdStart(bool_t bAfterAhiInit) {
	if (!bAfterAhiInit) {
		// before AHI initialization (very first of code)

		// check DIO source
		sAppData.bWakeupByButton = FALSE;
		if(u8AHI_WakeTimerFiredStatus()) {
		} else
    	if(u32AHI_DioWakeStatus() & u32DioPortWakeUp) {
			// woke up from DIO events
    		sAppData.bWakeupByButton = TRUE;
		}

		// Module Registration
		ToCoNet_REG_MOD_ALL();
	} else {
		// リセットICの無効化
		vPortSetLo(DIO_VOLTAGE_CHECKER);
		vPortAsOutput(DIO_VOLTAGE_CHECKER);
		vPortDisablePullup(DIO_VOLTAGE_CHECKER);

		// センサー用の制御 (Lo:Active)
		vPortSetHi(DIO_SNS_POWER);
		vPortAsOutput(DIO_SNS_POWER);

		// １次キャパシタ(e.g. 220uF)とスーパーキャパシタ (1F) の直結制御用
		vPortSetHi(DIO_SUPERCAP_CONTROL);
		vPortAsOutput(DIO_SUPERCAP_CONTROL);
		vPortDisablePullup(DIO_SUPERCAP_CONTROL);

		if (IS_APPCONF_OPT_DOOR_TIMER()) {
			vPortDisablePullup(DIO_BUTTON); // 外部プルアップのため
		}

		// アプリケーション保持構造体の初期化
		memset(&sAppData, 0x00, sizeof(sAppData));

		// SPRINTFの初期化(128バイトのバッファを確保する)
		SPRINTF_vInit128();

		// フラッシュメモリからの読み出し
		//   フラッシュからの読み込みが失敗した場合、ID=15 で設定する
		sAppData.bFlashLoaded = Config_bLoad(&sAppData.sFlash);

		// configure network
		sToCoNet_AppContext.u32AppId = sAppData.sFlash.sData.u32appid;
		sToCoNet_AppContext.u8Channel = sAppData.sFlash.sData.u8ch;

		//sToCoNet_AppContext.u8TxMacRetry = 1;
		sToCoNet_AppContext.bRxOnIdle = FALSE;

		//sToCoNet_AppContext.u8CCA_Level = 1;
		//sToCoNet_AppContext.u8CCA_Retry = 0;

		sToCoNet_AppContext.u8TxPower = sAppData.sFlash.sData.u8pow;

		// version info
		sAppData.u32ToCoNetVersion = ToCoNet_u32GetVersion();

		// M2がLoなら、設定モードとして動作する
		vPortAsInput(PORT_CONF2);
		if (bPortRead(PORT_CONF2)) {
			sAppData.bConfigMode = TRUE;
		}

		if (sAppData.bConfigMode) {
			// 設定モードで起動

			// Other Hardware
			vInitHardware(FALSE);

			// イベント処理の初期化
			vInitAppConfig();
			ToCoNet_Event_Register_State_Machine(vProcessEvCoreConfig); // デバッグ用の動作マシン

			// インタラクティブモードの初期化
			Interactive_vInit();
		} else
		if (IS_APPCONF_OPT_DOOR_TIMER()) {
			// ドアタイマーで起動
			// sToCoNet_AppContext.u8CPUClk = 1; // runs at 8MHz (Doze を利用するのであまり効果が無いかもしれない)
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)

			// Other Hardware
			vInitHardware(FALSE);

			// イベント処理の初期化
			vInitAppDoorTimer();
			ToCoNet_Event_Register_State_Machine(vProcessEvCore_Door_Timer); // main state machine
			ToCoNet_Event_Register_State_Machine(vProcessEvCore_Door_Timer_Nwk); // main state machine
		} else {
			// 通常アプリで起動
			sToCoNet_AppContext.u8CPUClk = 3; // runs at 32Mhz
			sToCoNet_AppContext.bSkipBootCalib = TRUE; // 起動時のキャリブレーションを省略する(保存した値を確認)

			// ADC の初期化
			vInitADC();

			// Other Hardware
			vInitHardware(FALSE);
			vInitPulseCounter();

			// イベント処理の初期化
			vInitAppStandard();
			ToCoNet_Event_Register_State_Machine(vProcessEvCore); // main state machine
		}

		// ToCoNet DEBUG
		ToCoNet_vDebugInit(&sSerStream);
		ToCoNet_vDebugLevel(TOCONET_DEBUG_LEVEL);
	}
}

/**
 * スリープ復帰時の処理
 * @param bAfterAhiInit
 */
void cbAppWarmStart(bool_t bAfterAhiInit) {
	if (!bAfterAhiInit) {
		// before AHI init, very first of code.
		//  to check interrupt source, etc.

		sAppData.bWakeupByButton = FALSE;
		if(u8AHI_WakeTimerFiredStatus()) {
		} else
		if(u32AHI_DioWakeStatus() & u32DioPortWakeUp) {
			// woke up from DIO events
			sAppData.bWakeupByButton = TRUE;
		}
	} else {
		// Other Hardware
		vSerialInit();
		ToCoNet_vDebugInit(&sSerStream);
		ToCoNet_vDebugLevel(TOCONET_DEBUG_LEVEL);

		if (!IS_APPCONF_OPT_DOOR_TIMER()) {
			// ADC の初期化
			vInitADC();
		}

		// 他のハードの待ち
		vInitHardware(FALSE);

		if (!sAppData.bWakeupByButton) {
			// タイマーで起きた
		} else {
			// ボタンで起床した
		}
	}
}

/**
 * メイン処理
 */
void cbToCoNet_vMain(void) {
	if (psCbHandler && psCbHandler->pf_cbToCoNet_vMain) {
		(*psCbHandler->pf_cbToCoNet_vMain)();
	}
}

/**
 * 受信処理
 */
void cbToCoNet_vRxEvent(tsRxDataApp *pRx) {
	if (psCbHandler && psCbHandler->pf_cbToCoNet_vRxEvent) {
		(*psCbHandler->pf_cbToCoNet_vRxEvent)(pRx);
	}
}

/**
 * 送信完了イベント
 */
void cbToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	V_PRINTF(LB ">>> TxCmp %s(tick=%d,req=#%d) <<<",
			bStatus ? "Ok" : "Ng",
			u32TickCount_ms & 0xFFFF,
			u8CbId
			);

	if (psCbHandler && psCbHandler->pf_cbToCoNet_vTxEvent) {
		(*psCbHandler->pf_cbToCoNet_vTxEvent)(u8CbId, bStatus);
	}

	return;
}

/**
 * ネットワークイベント
 * @param eEvent
 * @param u32arg
 */
void cbToCoNet_vNwkEvent(teEvent eEvent, uint32 u32arg) {
	if (psCbHandler && psCbHandler->pf_cbToCoNet_vNwkEvent) {
		(*psCbHandler->pf_cbToCoNet_vNwkEvent)(eEvent, u32arg);
	}
}

/**
 * ハードウェアイベント処理（割り込み遅延実行）
 */
void cbToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	if (psCbHandler && psCbHandler->pf_cbToCoNet_vHwEvent) {
		(*psCbHandler->pf_cbToCoNet_vHwEvent)(u32DeviceId, u32ItemBitmap);
	}
}

/**
 * ハードウェア割り込みハンドラ
 */
uint8 cbToCoNet_u8HwInt(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	bool_t bRet = FALSE;
	if (psCbHandler && psCbHandler->pf_cbToCoNet_u8HwInt) {
		bRet = (*psCbHandler->pf_cbToCoNet_u8HwInt)(u32DeviceId, u32ItemBitmap);
	}
	return bRet;
}

/**
 * ADCの初期化
 */
static void vInitADC() {
	// ADC
	vADC_Init(&sAppData.sObjADC, &sAppData.sADC, TRUE);
	sAppData.u8AdcState = 0xFF; // 初期化中
	sAppData.sObjADC.u8SourceMask =
			TEH_ADC_SRC_VOLT | TEH_ADC_SRC_ADC_1 | TEH_ADC_SRC_ADC_2;
}

/**
 * パルスカウンタの初期化
 * - cold boot 時に1回だけ初期化する
 */
static void vInitPulseCounter() {
	// カウンタの設定
	bAHI_PulseCounterConfigure(
		E_AHI_PC_0,
		1,      // 0:RISE, 1:FALL EDGE
		0,      // Debounce 0:off, 1:2samples, 2:4samples, 3:8samples
		FALSE,   // Combined Counter (32bitカウンタ)
		FALSE);  // Interrupt (割り込み)

	// カウンタのセット
	bAHI_SetPulseCounterRef(
		E_AHI_PC_0,
		0x0); // 何か事前に値を入れておく

	// カウンタのスタート
	bAHI_StartPulseCounter(E_AHI_PC_0); // start it

	// カウンタの設定
	bAHI_PulseCounterConfigure(
		E_AHI_PC_1,
		1,      // 0:RISE, 1:FALL EDGE
		0,      // Debounce 0:off, 1:2samples, 2:4samples, 3:8samples
		FALSE,   // Combined Counter (32bitカウンタ)
		FALSE);  // Interrupt (割り込み)

	// カウンタのセット
	bAHI_SetPulseCounterRef(
		E_AHI_PC_1,
		0x0); // 何か事前に値を入れておく

	// カウンタのスタート
	bAHI_StartPulseCounter(E_AHI_PC_1); // start it
}

/**
 * ハードウェアの初期化を行う
 * @param f_warm_start TRUE:スリープ起床時
 */
static void vInitHardware(int f_warm_start) {
	// センサー用の電源制御回路を LO に設定する
	vPortSetLo(DIO_SNS_POWER);

	// 入力ポートを明示的に指定する
	vPortAsInput(DIO_BUTTON);

	// Serial Port の初期化
	vSerialInit();

	// PWM の初期化
	if (IS_APPCONF_OPT_DOOR_TIMER()) {
# ifndef JN516x
#  warning "IO_TIMER is not implemented on JN514x"
# endif
		memset(&sTimerPWM[0], 0, sizeof(tsTimerContext));
# ifdef JN516x
		vAHI_TimerFineGrainDIOControl(0x7); // bit 0,1,2 をセット (TIMER0 の各ピンを解放する, PWM1..4 は使用する)
		vAHI_TimerSetLocation(E_AHI_TIMER_1, TRUE, TRUE); // IOの割り当てを設定
# endif

		// PWM
		int i;
		for (i = 0; i < 1; i++) {
			const uint8 au8TimTbl[] = {
				E_AHI_DEVICE_TIMER1,
				//E_AHI_DEVICE_TIMER2
				//E_AHI_DEVICE_TIMER3,
				//E_AHI_DEVICE_TIMER4
			};
			sTimerPWM[i].u16Hz = 1000;
			sTimerPWM[i].u8PreScale = 0;
			sTimerPWM[i].u16duty = 1024; // 1024=Hi, 0:Lo
			sTimerPWM[i].bPWMout = TRUE;
			sTimerPWM[i].bDisableInt = TRUE; // 割り込みを禁止する指定
			sTimerPWM[i].u8Device = au8TimTbl[i];
			vTimerConfig(&sTimerPWM[i]);
			vTimerStart(&sTimerPWM[i]);
		}
	}
}

/**
 * シリアルポートの初期化
 */
void vSerialInit(void) {
	/* Create the debug port transmit and receive queues */
	static uint8 au8SerialTxBuffer[1024];
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
	SERIAL_vInit(&sSerPort);

	sSerStream.bPutChar = SERIAL_bTxChar;
	sSerStream.u8Device = UART_PORT;
}

/**
 * 初期化メッセージ
 */
void vSerInitMessage() {
	V_PRINTF(LB LB "*** " APP_NAME " (ED_Inp) %d.%02d-%d ***", VERSION_MAIN, VERSION_SUB, VERSION_VAR);
	V_PRINTF(LB "* App ID:%08x Long Addr:%08x Short Addr %04x LID %02d Calib=%d",
			sToCoNet_AppContext.u32AppId, ToCoNet_u32GetSerial(), sToCoNet_AppContext.u16ShortAddress,
			sAppData.sFlash.sData.u8id,
			sAppData.sFlash.sData.u16RcClock);
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

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
