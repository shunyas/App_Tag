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

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include "jendefs.h"
#include "AppHardwareApi.h"
#include "string.h"
#include "fprintf.h"

#include "sensor_driver.h"
#include "ADXL345.h"
#include "SMBus.h"

#include "ccitt8.h"

#include "utils.h"

#include "Interactive.h"

#include "EndDevice_Input.h"

# include <serial.h>
# include <fprintf.h>
#undef SERIAL_DEBUG
#ifdef SERIAL_DEBUG
extern tsFILE sDebugStream;
#endif

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
PRIVATE void vProcessSnsObj_ADXL345_FIFO(void *pvObj, teEvent eEvent);
PRIVATE bool_t bSetFIFO( void );

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
void vADXL345_FIFO_Init(tsObjData_ADXL345 *pData, tsSnsObj *pSnsObj) {
	vSnsObj_Init(pSnsObj);

	pSnsObj->pvData = (void*)pData;
	pSnsObj->pvProcessSnsObj = (void*)vProcessSnsObj_ADXL345_FIFO;

	memset((void*)pData, 0, sizeof(tsObjData_ADXL345));
}
//	センサの設定を記述する関数
bool_t bADXL345_FIFO_Setting()
{
	uint8 com;
//	com = 0x09;		//	50Hz Sampling frequency
//	com = 0x0A;		//	100Hz Sampling frequency
//	com = 0x0B;		//	200Hz Sampling frequency
	com = 0x0C;		//	400Hz Sampling frequency
//	com = 0x0D;		//	800Hz Sampling frequency
	bool_t bOk = bSMBusWrite(ADXL345_ADDRESS, ADXL345_BW_RATE, 1, &com );

	com = 0x0B;		//	Full Resolution Mode, +-16g
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_DATA_FORMAT, 1, &com );

	com = 0x00;
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_INT_MAP, 1, &com );

	//	有効にする割り込みの設定
	com = 0x02;
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_INT_ENABLE, 1, &com );

	com = 0xC0 | 0x20 | READ_FIFO;
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_FIFO_CTL, 1, &com );

	com = 0x08;		//	Start Measuring
	bOk = bSMBusWrite(ADXL345_ADDRESS, ADXL345_POWER_CTL, 1, &com );

	return bOk;
}

/****************************************************************************
 *
 * NAME: u16ADXL345readResult
 *
 * DESCRIPTION:
 * Wrapper to read a measurement, followed by a conversion function to work
 * out the value in degrees Celcius.
 *
 * RETURNS:
 * int16: 0~10000 [1 := 5Lux], 100 means 500 Lux.
 *        0x8000, error
 *
 * NOTES:
 * the data conversion fomula is :
 *      ReadValue / 1.2 [LUX]
 *
 ****************************************************************************/
PUBLIC bool_t bADXL345FIFOreadResult( int16* ai16accelx, int16* ai16accely, int16* ai16accelz )
{
	bool_t	bOk = TRUE;
	uint8	au8data[6];
	uint8	num;
	uint8	i=0;

	//	FIFOでたまった個数を読み込む
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_FIFO_STATUS, 0, NULL );
	bOk &= bSMBusSequentialRead( ADXL345_ADDRESS, 1, &num );

	//	FIFOの中身を全部読む
	num = (num&0x7f);
	if( num >= 14 ){
		//	各軸の読み込み
		for( i=0; i<14; i++ ){
			//	加速度を読み込む
			//bOk &= bGetAxis( ADXL345_IDX_X, au8data );
			GetAxis(bOk, au8data);
			ai16accelx[i] = ((au8data[1] << 8) | au8data[0])<<2;
			ai16accely[i] = ((au8data[3] << 8) | au8data[2])<<2;
			ai16accelz[i] = ((au8data[5] << 8) | au8data[4])<<2;
#if 0
			vfPrintf( &sSerStream, "\n\r%2d:%d,%d,%d", i, ai16accelx[i], ai16accely[i], ai16accelz[i] );
			SERIAL_vFlush(E_AHI_UART_0);
		}
	}
	vfPrintf( &sSerStream, "\n\r%2d", num );
	SERIAL_vFlush(E_AHI_UART_0);
	vfPrintf( &sSerStream, "\n\r" );
#else
		}
	}
#endif

	//	FIFOの設定をもう一度
	bOk &= bSetFIFO();

	//	終わり

    return bOk;
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/
PRIVATE bool_t bSetFIFO( void )
{
	//	FIFOの設定をもう一度
	uint8 com = 0x00 | 0x20 | READ_FIFO;
	bool_t bOk = bSMBusWrite(ADXL345_ADDRESS, ADXL345_FIFO_CTL, 1, &com );
	com = 0xC0 | 0x20 | READ_FIFO;
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_FIFO_CTL, 1, &com );
	//	終わり

    return bOk;
}

// the Main loop
PRIVATE void vProcessSnsObj_ADXL345_FIFO(void *pvObj, teEvent eEvent) {
	tsSnsObj *pSnsObj = (tsSnsObj *)pvObj;
	tsObjData_ADXL345 *pObj = (tsObjData_ADXL345 *)pSnsObj->pvData;

	// general process (independent from each state)
	switch (eEvent) {
		case E_EVENT_TICK_TIMER:
			if (pObj->u8TickCount < 100) {
				pObj->u8TickCount += pSnsObj->u8TickDelta;
#ifdef SERIAL_DEBUG
vfPrintf(&sDebugStream, "+");
#endif
			}
			break;
		case E_EVENT_START_UP:
			pObj->u8TickCount = 100; // expire immediately
#ifdef SERIAL_DEBUG
vfPrintf(&sDebugStream, "\n\rADXL345 WAKEUP");
#endif
			break;
		default:
			break;
	}

	// state machine
	switch(pSnsObj->u8State)
	{
	case E_SNSOBJ_STATE_INACTIVE:
		// do nothing until E_ORDER_INITIALIZE event
		break;

	case E_SNSOBJ_STATE_IDLE:
		switch (eEvent) {
		case E_EVENT_NEW_STATE:
			break;

		case E_ORDER_KICK:
			vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_MEASURING);

			#ifdef SERIAL_DEBUG
			vfPrintf(&sDebugStream, "\n\rADXL345 KICKED");
			#endif

			break;

		default:
			break;
		}
		break;

	case E_SNSOBJ_STATE_MEASURING:
		switch (eEvent) {
		case E_EVENT_NEW_STATE:
			pObj->u8Interrupt = u8Interrupt;
			pObj->ai16ResultX[0] = ADXL345_DATA_NOTYET;
			pObj->ai16ResultY[0] = ADXL345_DATA_NOTYET;
			pObj->ai16ResultZ[0] = ADXL345_DATA_NOTYET;
			pObj->u8TickWait = ADXL345_CONVTIME;

			pObj->bBusy = TRUE;
#ifdef ADXL345_ALWAYS_RESET
			u8reset_flag = TRUE;
			if (!bADXL345reset()) {
				vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_COMPLETE);
			}
#else
			//if (!bADXL345startRead()) { // kick I2C communication
			//	vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_COMPLETE);
			//}
#endif
			pObj->u8TickCount = 0;
			break;

		default:
			break;
		}

		// wait until completion
		if (pObj->u8TickCount > pObj->u8TickWait) {
#ifdef ADXL345_ALWAYS_RESET
			if (u8reset_flag) {
				u8reset_flag = 0;
				if (!bADXL345startRead()) {
					vADXL345_new_state(pObj, E_SNSOBJ_STATE_COMPLETE);
				}

				pObj->u8TickCount = 0;
				pObj->u8TickWait = ADXL345_CONVTIME;
				break;
			}
#endif

			bADXL345FIFOreadResult( pObj->ai16ResultX, pObj->ai16ResultY, pObj->ai16ResultZ);

			// data arrival
			pObj->bBusy = FALSE;
			vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_COMPLETE);
		}
		break;

	case E_SNSOBJ_STATE_COMPLETE:
		switch (eEvent) {
		case E_EVENT_NEW_STATE:
			#ifdef SERIAL_DEBUG
			vfPrintf(&sDebugStream, "\n\rADXL345_CP: %d", pObj->i16Result);
			#endif

			break;

		case E_ORDER_KICK:
			// back to IDLE state
			vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_IDLE);
			break;

		default:
			break;
		}
		break;

	default:
		break;
	}
}
/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
