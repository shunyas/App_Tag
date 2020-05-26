/****************************************************************************
 * (C) Mono Wireless Inc. - 2016 all rights reserved.
 *
 * Condition to use: (refer to detailed conditions in Japanese)
 *   - The full or part of source code is limited to use for TWE (The
 *     Wireless Engine) as compiled and flash programmed.
 *   - The full or part of source code is prohibited to distribute without
 *     permission from Mono Wireless.
 *
 * 利用条件:
 *   - 本ソースコードは、別途ソースコードライセンス記述が無い限りモノワイヤレスが著作権を
 *     保有しています。
 *   - 本ソースコードは、無保証・無サポートです。本ソースコードや生成物を用いたいかなる損害
 *     についてもモノワイヤレスは保証致しません。不具合等の報告は歓迎いたします。
 *   - 本ソースコードは、モノワイヤレスが販売する TWE シリーズ上で実行する前提で公開
 *     しています。他のマイコン等への移植・流用は一部であっても出来ません。
 *
 ****************************************************************************/

#include <jendefs.h>

#include "ccitt8.h"

#include "utils.h"

#include "Interactive.h"
#include "EndDevice_Input.h"

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vStoreSensorValue();


/*
 * 最初に遷移してくる状態
 */
PRSEV_HANDLER_DEF(E_STATE_IDLE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	//	この状態から始まったときモジュールIDなどを表示する
	if (eEvent == E_EVENT_START_UP) {
		// 起床メッセージ
		vSerInitMessage();

		//	初回起動(リセット)かスリープからの復帰かで表示するメッセージを変える
		if (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK) {
			// Warm start message
			V_PRINTF(LB "*** Warm starting woke by %s. ***", sAppData.bWakeupByButton ? "DIO" : "WakeTimer");

			// RESUME
			ToCoNet_Nwk_bResume(sAppData.pContextNwk);

			// RUNNING状態へ遷移
			ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
		} else {
			// 開始する
			// start up message
			V_PRINTF(LB "*** Cold starting");
			V_PRINTF(LB "* start end device[%d]", u32TickCount_ms & 0xFFFF);

			sAppData.sNwkLayerTreeConfig.u8Role = TOCONET_NWK_ROLE_ENDDEVICE;
			// ネットワークの初期化
			sAppData.pContextNwk = ToCoNet_NwkLyTr_psConfig_MiniNodes(&sAppData.sNwkLayerTreeConfig);

			if (sAppData.pContextNwk) {
				ToCoNet_Nwk_bInit(sAppData.pContextNwk);
				ToCoNet_Nwk_bStart(sAppData.pContextNwk);
			}

			// 最初にパケットを送りたくないのでチャタリング対策状態へ遷移後、割り込みがあるまでスリープ
			ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
		}

	}
}

/*	パケットを送信する状態	*/
PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		// ADC の開始
		vADC_WaitInit();
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
	}
	if (eEvent == E_ORDER_KICK) {
		vPortAsInput(0);
		uint8 DI_Bitmap = bPortRead(0) ? 0x01 : 0x00;
		vPortDisablePullup(0);
		vPortSetSns(FALSE);

		uint8*	q;
		bool_t bOk = FALSE;
		if( IS_APPCONF_OPT_APP_TWELITE() ){		//超簡単!TWEアプリあてに送信する場合
			uint8	au8Data[7];
			q = au8Data;

			// DIO の設定
			S_OCTET(DI_Bitmap);
			S_OCTET(0x0F);

			// PWM(AI)の設定
			uint8 u8MSB = (sAppData.sSns.u16Adc1 >> 2) & 0xFF;
			S_OCTET(u8MSB);
			S_OCTET(0x00);
			S_OCTET(0x00);
			S_OCTET(0x00);

			// 下2bitを u8LSBs に詰める
			uint8 u8LSBs = sAppData.sSns.u16Adc1|0x03;
			S_OCTET(u8LSBs);

			bOk = bTransmitToAppTwelite( au8Data, q-au8Data );
		}else{									// 無線タグアプリ宛に送信する場合
			uint8	au8Data[4];
			uint16 u16RandNum = ToCoNet_u16GetRand();
			q = au8Data;
			S_OCTET(sAppData.sSns.u8Batt);
			S_BE_WORD(u16RandNum);

			/*	DIの入力状態を取得	*/
			S_OCTET( DI_Bitmap );

			bOk = bTransmitToParent( sAppData.pContextNwk, au8Data, q-au8Data );
		}

		if (  bOk ) {
			ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
			V_PRINTF(LB"TxOk");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
		} else {
			V_PRINTF(LB"TxFl");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
		}

		V_PRINTF(" FR=%04X", sAppData.u16frame_count);
	}
}

/*	送信完了状態	*/
PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_TX, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_ORDER_KICK) { // 送信完了イベントが来たのでスリープする
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

/*	スリープをする状態	*/
PRSEV_HANDLER_DEF(E_STATE_APP_SLEEP, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		// Sleep は必ず E_EVENT_NEW_STATE 内など１回のみ呼び出される場所で呼び出す。
		V_PRINTF(LB"Complete!!");
		V_FLUSH();
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
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
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
		break;

	case E_AHI_DEVICE_ANALOGUE:
		/*
		 * ADC完了割り込み
		 */
		V_PUTCHAR('@');
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
		if (bSnsObj_isComplete(&sAppData.sADC)) {
			// 全チャネルの処理が終わったら、次の処理を呼び起こす
			vStoreSensorValue();
			ToCoNet_Event_Process(E_ORDER_KICK, 0, vProcessEvCore);
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
void vInitAppSwing() {
	psCbHandler = &sCbHandler;
	pvProcessEv1 = vProcessEvCore;
}

/**
 * センサー値を格納する
 */
static void vStoreSensorValue() {
	// センサー値の保管
	sAppData.sSns.u16Adc1 = 0;
	sAppData.sSns.u8Batt = ENCODE_VOLT(sAppData.sObjADC.ai16Result[TEH_ADC_IDX_VOLT]);
}
