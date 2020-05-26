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

#ifndef  ADXL345_INCLUDED
#define  ADXL345_INCLUDED

#if defined __cplusplus
extern "C" {
#endif

/****************************************************************************/
/***        Include Files                                                 ***/
/****************************************************************************/
#include "appsave.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define ADXL345_IDX_X 0
#define ADXL345_IDX_Y 1
#define ADXL345_IDX_Z 2

#define ADXL345_IDX_BEGIN 0
#define ADXL345_IDX_END (ADXL345_IDX_Z+1) // should be (last idx + 1)


#define NORMAL				0
#define S_TAP				1
#define D_TAP				2
#define FREEFALL			4
#define ACTIVE				8
#define INACTIVE			16
#define NEKOTTER			256
#define DICE				512

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
typedef struct {
	// protected
	bool_t	bBusy;			// should block going into sleep

	// data
	int16	ai16Result[3];
	uint8	u8Interrupt;

	// working
	uint8	u8TickCount, u8TickWait;
} tsObjData_ADXL345;

/****************************************************************************/
/***        Exported Functions (state machine)                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions (primitive funcs)                          ***/
/****************************************************************************/
void vADXL345_Init(tsObjData_ADXL345 *pData, tsSnsObj *pSnsObj );
bool_t bADXL345_Setting( int16 i16mode, tsADXL345Param sParam, bool_t bLink );
void vADXL345_Final(tsObjData_ADXL345 *pData, tsSnsObj *pSnsObj);

PUBLIC bool_t bADXL345reset();
PUBLIC bool_t bADXL345startRead();
PUBLIC int16 i16ADXL345readResult( uint8 u8axis );
PUBLIC bool_t bNekotterreadResult( int16* ai16accel );
uint8 u8Read_Interrupt( void );

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/
extern uint8 u8Interrupt;

#if defined __cplusplus
}
#endif

#endif  /* ADXL345_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

