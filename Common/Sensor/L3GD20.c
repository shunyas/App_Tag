/****************************************************************************
 * (C) Tokyo Cosmos Electric, Inc. (TOCOS) - 2012 all rights reserved.
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
#include "jendefs.h"
#include "AppHardwareApi.h"
#include "string.h"
#include "fprintf.h"

#include "sensor_driver.h"
#include "L3GD20.h"
#include "SMBus.h"

#include "ccitt8.h"

#include "utils.h"

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
#define L3GD20_ADDRESS		(0x6B)

#define L3GD20_CTRL_REG1	(0x20)

#define L3GD20_X			(0x28)
#define L3GD20_Y			(0x2A)
#define L3GD20_Z			(0x2C)

#define L3GD20_WHO			(0x0F)
#define L3GD20_NAME			(0xD4)

#define L3GD20_CONVTIME    (24+2) // 24ms MAX

#define L3GD20_DATA_NOTYET  (-32768)
#define L3GD20_DATA_ERROR   (-32767)

const uint8 L3GD20_AXIS[] = {
		L3GD20_X,
		L3GD20_Y,
		L3GD20_Z
};

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
PRIVATE bool_t bGetAxis( uint8 u8axis, uint8* au8data );
PRIVATE void vProcessSnsObj_L3GD20(void *pvObj, teEvent eEvent);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
void vL3GD20_Init(tsObjData_L3GD20 *pData, tsSnsObj *pSnsObj) {
	vSnsObj_Init(pSnsObj);

	pSnsObj->pvData = (void*)pData;
	pSnsObj->pvProcessSnsObj = (void*)vProcessSnsObj_L3GD20;

	memset((void*)pData, 0, sizeof(tsObjData_L3GD20));
}

void vL3GD20_Final(tsObjData_L3GD20 *pData, tsSnsObj *pSnsObj) {
	pSnsObj->u8State = E_SNSOBJ_STATE_INACTIVE;
}



/****************************************************************************
 *
 * NAME: bL3GD20reset
 *
 * DESCRIPTION:
 *   to reset L3GD20 device
 *
 * RETURNS:
 * bool_t	fail or success
 *
 ****************************************************************************/
PUBLIC bool_t bL3GD20reset()
{
	bool_t bOk = TRUE;
//	uint8 command = L3GD20_SOFT_COM;

//	bOk &= bSMBusWrite(L3GD20_ADDRESS, L3GD20_SOFT_RST, 1, &command );
	// then will need to wait at least 15ms

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
PUBLIC bool_t bL3GD20startRead()
{
	bool_t bOk = TRUE;
	uint8 u8com;
	uint8 u8name = 0x00;
#ifdef HR_MODE
	uint8 u8hr = L3GD20_HR;
#endif

	//	Who am I?
	bOk &= bSMBusWrite(L3GD20_ADDRESS, L3GD20_WHO, 0, NULL);
	bOk &= bSMBusSequentialRead(L3GD20_ADDRESS, 1, &u8name);
	if( !bOk && u8name != L3GD20_NAME ){
		return FALSE;
	}

	// Set Option
	u8com = 0x0F;	// x,y,z enable, Power on, ODR:95Hz, Cutoff:12.5Hz
	bOk &= bSMBusWrite( L3GD20_ADDRESS, L3GD20_CTRL_REG1, 1, &u8com );

//	u8com = 0x10;	// +-500dps
//	u8com = 0x20;	// +-2000dps
//	bOk &= bSMBusWrite( L3GD20_ADDRESS, 0x23, 1, &u8com );

#ifdef HR_MODE
	bOk &= bSMBusWrite( L3GD20_ADDRESS, L3GD20_SET_HR, 1, &u8hr );
#endif

	return bOk;
}

/****************************************************************************
 *
 * NAME: u16L3GD20readResult
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
PUBLIC int16 i16L3GD20readResult( uint8 u8axis )
{
	bool_t	bOk = TRUE;
	int16	i16result=0;
	uint8	au8data[2];

	//	各軸の読み込み
	switch( u8axis ){
		case L3GD20_IDX_X:
			bOk &= bGetAxis( L3GD20_IDX_X, au8data );
			break;
		case L3GD20_IDX_Y:
			bOk &= bGetAxis( L3GD20_IDX_Y, au8data );
			break;
		case L3GD20_IDX_Z:
			bOk &= bGetAxis( L3GD20_IDX_Z, au8data );
			break;
		default:
			bOk = FALSE;
	}
	i16result = ((int16)((au8data[1] << 8) | au8data[0]))*0.00875;		//	+-250dps( Degree per Second )
//	i16result = ((int16)((au8data[1] << 8) | au8data[0]))*0.0175;		//	+-500dps
//	i16result = ((int16)((au8data[1] << 8) | au8data[0]))*0.07;			//	+-2000dps

	if (bOk == FALSE) {
		i16result = SENSOR_TAG_DATA_ERROR;
	}
#ifdef SERIAL_DEBUG
vfPrintf(&sDebugStream, "\n\rL3GD20 DATA %x", *((uint16*)au8data) );
#endif

    return i16result;
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/
PRIVATE bool_t bGetAxis( uint8 u8axis, uint8* au8data )
{
	uint8 i;
	bool_t bOk = TRUE;

	for( i=0; i<2; i++ ){
		bOk &= bSMBusWrite( L3GD20_ADDRESS, L3GD20_AXIS[u8axis]+i, 0, NULL );
		bOk &= bSMBusSequentialRead( L3GD20_ADDRESS, 1, &au8data[i] );
	}
//	bOk &= bSMBusWrite( L3GD20_ADDRESS, L3GD20_AXIS[u8axis], 0, NULL );
//	bOk &= bSMBusSequentialRead( L3GD20_ADDRESS, 2, au8data );

	return bOk;
}

// the Main loop
PRIVATE void vProcessSnsObj_L3GD20(void *pvObj, teEvent eEvent) {
	tsSnsObj *pSnsObj = (tsSnsObj *)pvObj;
	tsObjData_L3GD20 *pObj = (tsObjData_L3GD20 *)pSnsObj->pvData;

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
vfPrintf(&sDebugStream, "\n\rL3GD20 WAKEUP");
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
			vfPrintf(&sDebugStream, "\n\rL3GD20 KICKED");
			#endif

			break;

		default:
			break;
		}
		break;

	case E_SNSOBJ_STATE_MEASURING:
		switch (eEvent) {
		case E_EVENT_NEW_STATE:
			pObj->ai16Result[L3GD20_IDX_X] = SENSOR_TAG_DATA_ERROR;
			pObj->ai16Result[L3GD20_IDX_Y] = SENSOR_TAG_DATA_ERROR;
			pObj->ai16Result[L3GD20_IDX_Z] = SENSOR_TAG_DATA_ERROR;
			pObj->u8TickWait = L3GD20_CONVTIME;

			pObj->bBusy = TRUE;
#ifdef L3GD20_ALWAYS_RESET
			u8reset_flag = TRUE;
			if (!bL3GD20reset()) {
				vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_COMPLETE);
			}
#else
			if (!bL3GD20startRead()) { // kick I2C communication
				vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_COMPLETE);
			}
#endif
			pObj->u8TickCount = 0;
			break;

		default:
			break;
		}

		// wait until completion
		if (pObj->u8TickCount > pObj->u8TickWait) {
#ifdef L3GD20_ALWAYS_RESET
			if (u8reset_flag) {
				u8reset_flag = 0;
				if (!bL3GD20startRead()) {
					vL3GD20_new_state(pObj, E_SNSOBJ_STATE_COMPLETE);
				}

				pObj->u8TickCount = 0;
				pObj->u8TickWait = L3GD20_CONVTIME;
				break;
			}
#endif
			pObj->ai16Result[L3GD20_IDX_X] = i16L3GD20readResult(L3GD20_IDX_X);
			pObj->ai16Result[L3GD20_IDX_Y] = i16L3GD20readResult(L3GD20_IDX_Y);
			pObj->ai16Result[L3GD20_IDX_Z] = i16L3GD20readResult(L3GD20_IDX_Z);

			// data arrival
			pObj->bBusy = FALSE;
			vSnsObj_NewState(pSnsObj, E_SNSOBJ_STATE_COMPLETE);
		}
		break;

	case E_SNSOBJ_STATE_COMPLETE:
		switch (eEvent) {
		case E_EVENT_NEW_STATE:
			#ifdef SERIAL_DEBUG
			vfPrintf(&sDebugStream, "\n\rL3GD20_CP: %d", pObj->i16Result);
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
