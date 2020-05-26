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

#ifndef  LIS3DH_INCLUDED
#define  LIS3DH_INCLUDED

#if defined __cplusplus
extern "C" {
#endif

/****************************************************************************/
/***        Include Files                                                 ***/
/****************************************************************************/

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define LIS3DH_IDX_X 0
#define LIS3DH_IDX_Y 1
#define LIS3DH_IDX_Z 2

#define LIS3DH_IDX_BEGIN 0
#define LIS3DH_IDX_END (LIS3DH_IDX_Z+1) // should be (last idx + 1)

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
typedef struct {
	// protected
	bool_t bBusy;			// should block going into sleep

	// data
	int16 ai16Result[3];

	// working
	uint8 u8TickCount, u8TickWait;
} tsObjData_LIS3DH;

/****************************************************************************/
/***        Exported Functions (state machine)                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions (primitive funcs)                          ***/
/****************************************************************************/
void vLIS3DH_Init(tsObjData_LIS3DH *pData, tsSnsObj *pSnsObj );
void vLIS3DH_Final(tsObjData_LIS3DH *pData, tsSnsObj *pSnsObj);

PUBLIC bool_t bLIS3DHreset();
PUBLIC bool_t bLIS3DHstartRead();
PUBLIC int16 i16LIS3DHreadResult( uint8 u8axis );

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

#if defined __cplusplus
}
#endif

#endif  /* LIS3DH_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

