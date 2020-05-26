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
#define ADXL345_AirVolume_IDX_X 0
#define ADXL345_AirVolume_IDX_Y 1
#define ADXL345_AirVolume_IDX_Z 2

#define ADXL345_AirVolume_IDX_BEGIN 0
#define ADXL345_AirVolume_IDX_END (ADXL345_AirVolume_IDX_Z+1) // should be (last idx + 1)
/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
typedef struct {
	// protected
	bool_t	bBusy;			// should block going into sleep

	// data
	int32	ai32Result[3];
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
void vADXL345_AirVolume_Init(tsObjData_ADXL345 *pData, tsSnsObj *pSnsObj );
bool_t bADXL345_AirVolume_Setting();
void vADXL345_AirVolume_Final(tsObjData_ADXL345 *pData, tsSnsObj *pSnsObj);

bool_t bSetFIFO_Air();
bool_t bSetActive();
uint8 u8Read_Interrupt_Air();

PUBLIC bool_t bADXL345_AirVolumeReset();
PUBLIC bool_t bADXL345_AirVolumeStartRead();
PUBLIC bool_t b16ADXL345_AirVolumeReadResult( int32* ai32accel );
PUBLIC bool_t b16ADXL345_AirVolumeSingleReadResult( int32* ai32accel );

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

