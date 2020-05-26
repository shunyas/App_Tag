/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>

#include "utils.h"

#include "ccitt8.h"

#include "Interactive.h"
#include "EndDevice_Input.h"

#include "sensor_driver.h"
#include "ADXL345.h"

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vStoreSensorValue();
static void vProcessADXL345_FIFO(teEvent eEvent);
static uint8 u8sns_cmplt = 0;

static uint8 u8GetCount = 0;

static tsSnsObj sSnsObj;
static tsObjData_ADXL345 sObjADXL345;

enum {
	E_SNS_ADC_CMP_MASK = 1,
	E_SNS_ADXL345_CMP = 2,
	E_SNS_ALL_CMP = 3
};

/*
 * ADC 計測をしてデータ送信するアプリケーション制御
 */
PRSEV_HANDLER_DEF(E_STATE_IDLE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	static bool_t bFirst = TRUE;
	static bool_t bFIFO_Measuring = FALSE;
	if (eEvent == E_EVENT_START_UP) {
		if (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK) {
			// Warm start message
			V_PRINTF(LB "*** Warm starting woke by %s. ***", sAppData.bWakeupByButton ? "DIO" : "WakeTimer");
		} else {
			// 開始する
			// start up message
			vSerInitMessage();

			V_PRINTF(LB "*** Cold starting");
			V_PRINTF(LB "* start end device[%d]", u32TickCount_ms & 0xFFFF);
			// ADXL345 の初期化
		}

		// RC クロックのキャリブレーションを行う
		ToCoNet_u16RcCalib(sAppData.sFlash.sData.u16RcClock);

		// センサーがらみの変数の初期化
		u8sns_cmplt = 0;

		vADXL345_FIFO_Init( &sObjADXL345, &sSnsObj );
		if( bFirst ){
			V_PRINTF(LB "*** ADXL345 FIFO Setting...");
			bADXL345reset();
			bADXL345_FIFO_Setting( (uint16)(sAppData.sFlash.sData.i16param&0xFFFF), sAppData.sFlash.sData.uParam.sADXL345Param );
		}
		vSnsObj_Process(&sSnsObj, E_ORDER_KICK);
		if (bSnsObj_isComplete(&sSnsObj)) {
			// 即座に完了した時はセンサーが接続されていない、通信エラー等
			u8sns_cmplt |= E_SNS_ADXL345_CMP;
			V_PRINTF(LB "*** ADXL345 comm err?");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
			return;
		}

		if( sAppData.bWakeupByButton || bFirst ){
			V_PRINTF(LB"Interrupt:0x%02X", sObjADXL345.u8Interrupt);
			if( (sAppData.sFlash.sData.i16param&0xF) && !bFirst ){
				if( sObjADXL345.u8Interrupt&0x74 ){
					if( bFIFO_Measuring && sAppData.sFlash.sData.u8wait == 0 ){
						bFIFO_Measuring = FALSE;
						V_PRINTF(LB"Disable");
						bADXL345_DisableFIFO( (uint16)(sAppData.sFlash.sData.i16param&0x000F) );
					}else{
						bFIFO_Measuring = TRUE;
						V_PRINTF(LB"Enable");
						bADXL345_EnableFIFO( (uint16)(sAppData.sFlash.sData.i16param&0x000F) );
					}
				}else if( (sObjADXL345.u8Interrupt&0x08) && sAppData.sFlash.sData.u8wait == 0 ){
					bFIFO_Measuring = FALSE;
					V_PRINTF(LB"Disable");
					bADXL345_DisableFIFO( (uint16)(sAppData.sFlash.sData.i16param&0x000F) );
				}
				if(bFIFO_Measuring && (sObjADXL345.u8Interrupt&0x02) ){
					bFirst = FALSE;
					// ADC の取得
					vADC_WaitInit();
					vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);

					// RUNNING 状態
					ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
				}else{
					ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
				}
			}else{
				bFirst = FALSE;
				// ADC の取得
				vADC_WaitInit();
				vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);

				// RUNNING 状態
				ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
			}
		}else{
			bADXL345_StartMeasuring(FALSE);
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
		}

	} else {
		V_PRINTF(LB "*** unexpected state.");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
		// 短期間スリープからの起床をしたので、センサーの値をとる
	if ((eEvent == E_EVENT_START_UP) && (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK)) {
		V_PRINTF("#");
		vProcessADXL345_FIFO(E_EVENT_START_UP);
	}

	// 送信処理に移行
	if (u8sns_cmplt == E_SNS_ALL_CMP) {
		ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 100) {
		V_PRINTF(LB"! TIME OUT (E_STATE_RUNNING)");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_TX, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		// ネットワークの初期化
		if (!sAppData.pContextNwk) {
			// 初回のみ
			sAppData.sNwkLayerTreeConfig.u8Role = TOCONET_NWK_ROLE_ENDDEVICE;
			sAppData.pContextNwk = ToCoNet_NwkLyTr_psConfig_MiniNodes(&sAppData.sNwkLayerTreeConfig);
			if (sAppData.pContextNwk) {
				ToCoNet_Nwk_bInit(sAppData.pContextNwk);
				ToCoNet_Nwk_bStart(sAppData.pContextNwk);
			} else {
				ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
				return;
			}
		} else {
			// 一度初期化したら RESUME
			ToCoNet_Nwk_bResume(sAppData.pContextNwk);
		}

		if( sAppData.bWakeupByButton && (sObjADXL345.u8Interrupt&0x02) ){
			u8GetCount++;		// 送信回数をインクリメント

			uint8	i;
			uint8	au8Data[91];
			uint8*	q = au8Data;
			S_OCTET(sAppData.sSns.u8Batt);
			S_BE_WORD(sAppData.sSns.u16Adc1);
			S_BE_WORD(sAppData.sSns.u16Adc2);
			if(sObjADXL345.u8FIFOSample > 10){
				i = sObjADXL345.u8FIFOSample-10;
			}else{
				i = 0;
			}
			S_BE_WORD(sObjADXL345.ai16ResultX[i]);
			S_BE_WORD(sObjADXL345.ai16ResultY[i]);
			S_BE_WORD(sObjADXL345.ai16ResultZ[i]);

			S_OCTET( 0xFA );
			S_OCTET( sObjADXL345.u8FIFOSample-i );
			i++;
			for( ; i<sObjADXL345.u8FIFOSample; i++ ){
				S_BE_WORD(sObjADXL345.ai16ResultX[i]);
				S_BE_WORD(sObjADXL345.ai16ResultY[i]);
				S_BE_WORD(sObjADXL345.ai16ResultZ[i]);
			}

			sAppData.u16frame_count++;

			if ( bTransmitToParent( sAppData.pContextNwk, au8Data, q-au8Data ) ) {
			} else {
				ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 送信失敗
			}
		}else{
			V_PRINTF(LB"First");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // 送信失敗
		}

		V_PRINTF(" FR=%04X", sAppData.u16frame_count);
	}

	if (eEvent == E_ORDER_KICK) { // 送信完了イベントが来たのでスリープする
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 100) {
		V_PRINTF(LB"! TIME OUT (E_STATE_APP_WAIT_TX)");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_SLEEP, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		// Sleep は必ず E_EVENT_NEW_STATE 内など１回のみ呼び出される場所で呼び出す。

		// Mininode の場合、特別な処理は無いのだが、ポーズ処理を行う
		ToCoNet_Nwk_bPause(sAppData.pContextNwk);

		vAHI_DioWakeEnable(PORT_INPUT_MASK_ADXL345, 0); // ENABLE DIO WAKE SOURCE
		(void)u32AHI_DioInterruptStatus(); // clear interrupt register
		vAHI_DioWakeEdge(PORT_INPUT_MASK_ADXL345, 0); // 割り込みエッジ(立上がりに設定)


		uint16 u16Rate = (1000/u16ADXL345_GetSamplingFrequency())*10;
		uint32 u32Sleep = sAppData.sFlash.sData.u32Slp-(u16Rate*sAppData.sFlash.sData.u8wait);
		if( sAppData.sFlash.sData.u8wait == 0 || u8GetCount < sAppData.sFlash.sData.u8wait  ){
			u32Sleep = 0;
		}else{
			if( (sAppData.sFlash.sData.i16param&0x000F) == 0 ){
				bADXL345_EndMeasuring();

				// 起床中にサンプリングしていても良いように読み込んでFIFOの中身をクリアする
				bADXL345FIFOreadResult( sObjADXL345.ai16ResultX, sObjADXL345.ai16ResultY, sObjADXL345.ai16ResultZ );
				// ダメ押しでレジスタに残っているデータも読み込む
				uint8 au8data[6];
				bool_t bOk;
				GetAxis(bOk, au8data);
			}else{
				bADXL345_DisableFIFO( (uint16)(sAppData.sFlash.sData.i16param&0xFFFF) );
				u32Sleep = 0;
			}
			u8GetCount = 0;
		}

		V_PRINTF(LB"! Sleeping... : %dms, 0x%02X", u32Sleep, sObjADXL345.u8Interrupt );
		V_FLUSH();

		u8Read_Interrupt();
		ToCoNet_vSleep( E_AHI_WAKE_TIMER_0, u32Sleep, u32Sleep>0 ? TRUE:FALSE, FALSE);
	}
}

/**
 * イベント処理関数リスト
 */
static const tsToCoNet_Event_StateHandler asStateFuncTbl[] = {
	PRSEV_HANDLER_TBL_DEF(E_STATE_IDLE),
	PRSEV_HANDLER_TBL_DEF(E_STATE_RUNNING),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_WAIT_TX),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_SLEEP),
	PRSEV_HANDLER_TBL_TRM
};

/**
 * イベント処理関数
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	ToCoNet_Event_StateExec(asStateFuncTbl, pEv, eEvent, u32evarg);
}

#if 0
/**
 * ハードウェア割り込み
 * @param u32DeviceId
 * @param u32ItemBitmap
 * @return
 */
static uint8 cbAppToCoNet_u8HwInt(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	uint8 u8handled = FALSE;

	switch (u32DeviceId) {
	case E_AHI_DEVICE_ANALOGUE:
		break;

	case E_AHI_DEVICE_SYSCTRL:
		break;

	case E_AHI_DEVICE_TIMER0:
		break;

	case E_AHI_DEVICE_TICK_TIMER:
		break;

	default:
		break;
	}

	return u8handled;
}
#endif

/**
 * ハードウェアイベント（遅延実行）
 * @param u32DeviceId
 * @param u32ItemBitmap
 */
static void cbAppToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	switch (u32DeviceId) {
	case E_AHI_DEVICE_TICK_TIMER:
		vProcessADXL345_FIFO(E_EVENT_TICK_TIMER);
		break;

	case E_AHI_DEVICE_ANALOGUE:
		/*
		 * ADC完了割り込み
		 */
		V_PUTCHAR('@');
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
		if (bSnsObj_isComplete(&sAppData.sADC)) {
			u8sns_cmplt |= E_SNS_ADC_CMP_MASK;
			vStoreSensorValue();
		}
		break;

	case E_AHI_DEVICE_SYSCTRL:
		break;

	case E_AHI_DEVICE_TIMER0:
		break;

	default:
		break;
	}
}

#if 0
/**
 * メイン処理
 */
static void cbAppToCoNet_vMain() {
	/* handle serial input */
	vHandleSerialInput();
}
#endif

#if 0
/**
 * ネットワークイベント
 * @param eEvent
 * @param u32arg
 */
static void cbAppToCoNet_vNwkEvent(teEvent eEvent, uint32 u32arg) {
	switch(eEvent) {
	case E_EVENT_TOCONET_NWK_START:
		break;

	default:
		break;
	}
}
#endif


#if 0
/**
 * RXイベント
 * @param pRx
 */
static void cbAppToCoNet_vRxEvent(tsRxDataApp *pRx) {

}
#endif

/**
 * TXイベント
 * @param u8CbId
 * @param bStatus
 */
static void cbAppToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	// 送信完了
	V_PRINTF(LB"! Tx Cmp = %d", bStatus);
	ToCoNet_Event_Process(E_ORDER_KICK, 0, vProcessEvCore);
}
/**
 * アプリケーションハンドラー定義
 *
 */
static tsCbHandler sCbHandler = {
	NULL, // cbAppToCoNet_u8HwInt,
	cbAppToCoNet_vHwEvent,
	NULL, // cbAppToCoNet_vMain,
	NULL, // cbAppToCoNet_vNwkEvent,
	NULL, // cbAppToCoNet_vRxEvent,
	cbAppToCoNet_vTxEvent
};

/**
 * アプリケーション初期化
 */
void vInitAppADXL345_FIFO() {
	psCbHandler = &sCbHandler;
	pvProcessEv1 = vProcessEvCore;
}

static void vProcessADXL345_FIFO(teEvent eEvent) {
	if (bSnsObj_isComplete(&sSnsObj)) {
		 return;
	}

	// イベントの処理
	vSnsObj_Process(&sSnsObj, eEvent); // ポーリングの時間待ち
	if (bSnsObj_isComplete(&sSnsObj)) {
		u8sns_cmplt |= E_SNS_ADXL345_CMP;

		uint8 i;
		V_PRINTF( LB"!NUM = %d", sObjADXL345.u8FIFOSample );

		for(i=0; i<sObjADXL345.u8FIFOSample; i++ ){
			V_PRINTF(LB"!ADXL345_%d: X : %d, Y : %d, Z : %d",
					i,
					sObjADXL345.ai16ResultX[i],
					sObjADXL345.ai16ResultY[i],
					sObjADXL345.ai16ResultZ[i]
			);
		}

		// 完了時の処理
		if (u8sns_cmplt == E_SNS_ALL_CMP) {
			ToCoNet_Event_Process(E_ORDER_KICK, 0, vProcessEvCore);
		}
	}
}

/**
 * センサー値を格納する
 */
static void vStoreSensorValue() {
	// センサー値の保管
	sAppData.sSns.u16Adc1 = sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_1];
	sAppData.sSns.u16Adc2 = sAppData.sObjADC.ai16Result[TEH_ADC_IDX_ADC_2];
	sAppData.sSns.u8Batt = ENCODE_VOLT(sAppData.sObjADC.ai16Result[TEH_ADC_IDX_VOLT]);

	// ADC1 が 1300mV 以上(SuperCAP が 2600mV 以上)である場合は SUPER CAP の直結を有効にする
	if (sAppData.sSns.u16Adc1 >= VOLT_SUPERCAP_CONTROL) {
		vPortSetLo(DIO_SUPERCAP_CONTROL);
	}
}
