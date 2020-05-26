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

#ifndef  ADT7410_INCLUDED
#define  ADT7410_INCLUDED

#if defined __cplusplus
extern "C" {
#endif

/****************************************************************************/
/***        Include Files                                                 ***/
/****************************************************************************/
#include "sensor_driver.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
typedef struct {
	// protected
	bool_t bBusy;          // should block going into sleep

	// public
	int16 i16Result;

	// private
	uint8 u8TickCount, u8TickWait;
} tsObjData_ADT7410;

/****************************************************************************/
/***        Exported Functions (state machine)                            ***/
/****************************************************************************/
void vADT7410_Init(tsObjData_ADT7410 *pData, tsSnsObj *pSnsObj);
void vADT7410_Final(tsObjData_ADT7410 *pData, tsSnsObj *pSnsObj);

/****************************************************************************/
/***        Exported Functions (primitive funcs)                          ***/
/****************************************************************************/
PUBLIC bool_t bADT7410reset(bool_t);
PUBLIC bool_t bADT7410startRead();
PUBLIC int16 i16ADT7410readResult(bool_t);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

#if defined __cplusplus
}
#endif

#endif  /* ADT7410_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

