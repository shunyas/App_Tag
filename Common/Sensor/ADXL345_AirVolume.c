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
#include<math.h>

#include "jendefs.h"
#include "AppHardwareApi.h"
#include "string.h"
#include "fprintf.h"

#include "sensor_driver.h"
#include "ADXL345_AirVolume.h"
#include "SMBus.h"

#include "ccitt8.h"

#include "utils.h"

#include "Interactive.h"

#undef SERIAL_DEBUG
#ifdef SERIAL_DEBUG
# include <serial.h>
# include <fprintf.h>
extern tsFILE sDebugStream;
#endif
tsFILE sSerStream;

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#ifdef LITE2525A
#define ADXL345_ADDRESS		(0x1D)
#else
#define ADXL345_ADDRESS		(0x53)
#endif

#define ADXL345_CONVTIME    (0)//(24+2) // 24ms MAX

#define ADXL345_DATA_NOTYET	(-32768)
#define ADXL345_DATA_ERROR	(-32767)

#define ADXL345_THRESH_TAP		0x1D
#define ADXL345_OFSX			0x1E
#define ADXL345_OFSY			0x1F
#define ADXL345_OFSZ			0x20
#define ADXL345_DUR				0x21
#define ADXL345_LATENT			0x22
#define ADXL345_WINDOW			0x23
#define ADXL345_THRESH_ACT		0x24
#define ADXL345_THRESH_INACT	0x25
#define ADXL345_TIME_INACT		0x26
#define ADXL345_ACT_INACT_CTL	0x27
#define ADXL345_THRESH_FF		0x28
#define ADXL345_TIME_FF			0x29
#define ADXL345_TAP_AXES		0x2A
#define ADXL345_ACT_TAP_STATUS	0x2B
#define ADXL345_BW_RATE			0x2C
#define ADXL345_POWER_CTL		0x2D
#define ADXL345_INT_ENABLE		0x2E
#define ADXL345_INT_MAP			0x2F
#define ADXL345_INT_SOURCE		0x30
#define ADXL345_DATA_FORMAT		0x31
#define ADXL345_DATAX0			0x32
#define ADXL345_DATAX1			0x33
#define ADXL345_DATAY0			0x34
#define ADXL345_DATAY1			0x35
#define ADXL345_DATAZ0			0x36
#define ADXL345_DATAZ1			0x37
#define ADXL345_FIFO_CTL		0x38
#define ADXL345_FIFO_STATUS		0x39

#define ADXL345_X	ADXL345_DATAX0
#define ADXL345_Y	ADXL345_DATAY0
#define ADXL345_Z	ADXL345_DATAZ0

#define READ_FIFO 5

const uint8 ADXL345_AirVolume_AXIS[] = {
		ADXL345_X,
		ADXL345_Y,
		ADXL345_Z
};


/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
PRIVATE bool_t bGetAxis( uint8 u8axis, uint8* au8data );
PRIVATE void vProcessSnsObj_ADXL345_AirVolume(void *pvObj, teEvent eEvent);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
void vADXL345_AirVolume_Init(tsObjData_ADXL345 *pData, tsSnsObj *pSnsObj) {
	vSnsObj_Init(pSnsObj);

	pSnsObj->pvData = (void*)pData;
	pSnsObj->pvProcessSnsObj = (void*)vProcessSnsObj_ADXL345_AirVolume;

	memset((void*)pData, 0, sizeof(tsObjData_ADXL345));
}

void vADXL345_AirVolume_Final(tsObjData_ADXL345 *pData, tsSnsObj *pSnsObj) {
	pSnsObj->u8State = E_SNSOBJ_STATE_INACTIVE;
}

//	センサの設定を記述する関数
bool_t bADXL345_AirVolume_Setting()
{
	bool_t bOk = TRUE;

	uint8 com = 0x1A;		//	100Hz Sampling frequency
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_BW_RATE, 1, &com );
	com = 0x0B;		//	Full Resolution Mode, +-16g
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_DATA_FORMAT, 1, &com );
	com = 0x08;		//	Start Measuring
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_POWER_CTL, 1, &com );
	bOk &= bSetActive();

	return bOk;
}

/****************************************************************************
 *
 * NAME: bADXL345reset
 *
 * DESCRIPTION:
 *   to reset ADXL345 device
 *
 * RETURNS:
 * bool_t	fail or success
 *
 ****************************************************************************/
PUBLIC bool_t bADXL345_AirVolumeReset()
{
	bool_t bOk = TRUE;
	return bOk;
}

/****************************************************************************
 *
 * NAME: vHTSstartReadTemp
 *
 * DESCRIPTION:
 * Wrapper to start a read of the temperature sensor.
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC bool_t bADXL345_AirVolumeStartRead()
{
	return TRUE;
}

/****************************************************************************
 *
 * NAME: u16ADXL345_AirVolumereadResult
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
PUBLIC bool_t b16ADXL345_AirVolumeReadResult( int32* ai32accel )
{
	bool_t	bOk = TRUE;
	uint8	au8data[2];
	uint8	num;				//	FIFOのデータ数
	uint8	i;
	int16	temp;
	int32	x[33];
	int32	y[33];
	int32	z[33];
	int32	avex = 0;
	int32	avey = 0;
	int32	avez = 0;

	//	FIFOでたまった個数を読み込む
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_FIFO_STATUS, 0, NULL );
	bOk &= bSMBusSequentialRead( ADXL345_ADDRESS, 1, &num );

	//	FIFOの中身を全部読む
	num = (num&0x7f);
	if( num == READ_FIFO ){
		//	各軸の読み込み
		for( i=0; i<num; i++ ){
			//	X軸
			bOk &= bGetAxis( ADXL345_AirVolume_IDX_X, au8data );
			temp = (((au8data[1] << 8) | au8data[0]));
			x[i] = (int32)temp;
		}
		for( i=0; i<num; i++ ){
			//	Y軸
			bOk &= bGetAxis( ADXL345_AirVolume_IDX_Y, au8data );
			temp = (((au8data[1] << 8) | au8data[0]));
			y[i] = (int32)temp;
		}
		for( i=0; i<num; i++ ){
			//	Z軸
			bOk &= bGetAxis( ADXL345_AirVolume_IDX_Z, au8data );
			temp = (((au8data[1] << 8) | au8data[0]));
			z[i] = (int32)temp;
		}
		bOk &= bSetFIFO_Air();

		for( i=0; i<num; i++ ){
			x[i] = (x[i]<<2);
			avex += x[i];
			y[i] = (y[i]<<2);
			avey += y[i];
			z[i] = (z[i]<<2);
			avez += z[i];
#if 0
			vfPrintf(& sSerStream, "\n\r%2d:%d,%d,%d %d", i, x[i], y[i], z[i], sum[i] );
			SERIAL_vFlush(E_AHI_UART_0);
		}
		vfPrintf( &sSerStream, "\n\r" );
#else
		}
#endif

		ai32accel[0] = avex/num;
		ai32accel[1] = avey/num;
		ai32accel[2] = avez/num;
	}else{
		bOk &= bSetFIFO_Air();
		ai32accel[0] = 0;
		ai32accel[1] = 0;
		ai32accel[2] = 0;
	}

	//	終わり

    return bOk;
}

PUBLIC bool_t b16ADXL345_AirVolumeSingleReadResult( int32* ai32accel )
{
	bool_t	bOk = TRUE;
	uint8	au8data[2];

	//	X軸
	bOk &= bGetAxis( ADXL345_AirVolume_IDX_X, au8data );
	ai32accel[0] = (((au8data[1] << 8) | au8data[0]));
	ai32accel[0] = (ai32accel[0]<<2);
	//	Y軸
	bOk &= bGetAxis( ADXL345_AirVolume_IDX_Y, au8data );
	ai32accel[1] = (((au8data[1] << 8) | au8data[0]));
	ai32accel[1] = (ai32accel[1]<<2);
	//	Z軸
	bOk &= bGetAxis( ADXL345_AirVolume_IDX_Z, au8data );
	ai32accel[2] = (((au8data[1] << 8) | au8data[0]));
	ai32accel[2] = (ai32accel[2]<<2);

    return bOk;
}
/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/
PRIVATE bool_t bGetAxis( uint8 u8axis, uint8* au8data )
{
	bool_t bOk = TRUE;

	bOk &= bSMBusWrite( ADXL345_ADDRESS, ADXL345_AirVolume_AXIS[u8axis], 0, NULL );
	bOk &= bSMBusSequentialRead( ADXL345_ADDRESS, 2, au8data );

	return bOk;
}

bool_t bSetFIFO_Air( void )
{
	//	FIFOの設定をもう一度
	uint8 com = 0x00 | 0x20 | READ_FIFO;
	bool_t bOk = bSMBusWrite(ADXL345_ADDRESS, ADXL345_FIFO_CTL, 1, &com );
	com = 0xC0 | 0x20 | READ_FIFO;
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_FIFO_CTL, 1, &com );
	//	有効にする割り込みの設定
	com = 0x02;
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_INT_ENABLE, 1, &com );
	//	終わり

    return bOk;
}

bool_t bSetActive(void)
{
	uint8 com;
	bool_t bOk = TRUE;
	//	動いていることを判断するための閾値
	com = 0x07;
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_THRESH_ACT, 1, &com );

	com = 0x60;
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_ACT_INACT_CTL, 1, &com );

	//	割り込みピンの設定
	com = 0x10;		//	ACTIVEは別ピン
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_INT_MAP, 1, &com );

	//	有効にする割り込みの設定
	com = 0x10;
	bOk &= bSMBusWrite(ADXL345_ADDRESS, ADXL345_INT_ENABLE, 1, &com );

    return bOk;
}

uint8 u8Read_Interrupt_Air( void )
{
	uint8	u8source;
	bool_t bOk = TRUE;

	bOk &= bSMBusWrite( ADXL345_ADDRESS, 0x30, 0, NULL );
	bOk &= bSMBusSequentialRead( ADXL345_ADDRESS, 1, &u8source );

	if(!bOk){
		u8source = 0xFF;
	}

	return u8source;
}

// the Main loop
PRIVATE void vProcessSnsObj_ADXL345_AirVolume(void *pvObj, teEvent eEvent) {
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
			pObj->ai32Result[ADXL345_AirVolume_IDX_X] = SENSOR_TAG_DATA_ERROR;
			pObj->ai32Result[ADXL345_AirVolume_IDX_Y] = SENSOR_TAG_DATA_ERROR;
			pObj->ai32Result[ADXL345_AirVolume_IDX_Z] = SENSOR_TAG_DATA_ERROR;
			pObj->u8TickWait = ADXL345_CONVTIME;

			pObj->bBusy = TRUE;
#ifdef ADXL345_ALWAYS_RESET
			u8reset_flag = TRUE;
			if (!bADXL345reset()) {
				vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_COMPLETE);
			}
#else
//			if (!bADXL345_AirVolumeStartRead()) { // kick I2C communication
//				vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_COMPLETE);
//			}
#endif
			pObj->u8TickCount = 0;
			break;

		default:
			break;
		}

		// wait until completion
		if (pObj->u8TickCount > pObj->u8TickWait) {
			if( (pObj->u8Interrupt&0x02) != 0 ){
				b16ADXL345_AirVolumeReadResult( pObj->ai32Result );
			}else{
				b16ADXL345_AirVolumeSingleReadResult( pObj->ai32Result );
			}

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
