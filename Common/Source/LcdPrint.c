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
/*
 * LcdPrint.c
 *
 *  Created on: 2012/12/28
 *      Author: seigo13
 */

#include <string.h>

#include <jendefs.h>

#include "fprintf.h"
#include "LcdDriver.h"
#include "LcdDraw.h"

#define Y_MAX 8
#define X_MAX 36

uint8 acLcdShadow[Y_MAX + 1][X_MAX];
uint8 u8LcdX = 0, u8LcdY = 0, u8LcdXBtm = 0;

/****************************************************************************
 *
 * NAME:       vDrawLcdDisplay
 *
 * DESCRIPTION:
 * Writes characters from the LCD scratchpad to the LCD driver
 *
 * PARAMETERS: Name     RW  Usage
 *          u32Xoffset  R   Offset in pixels from the left side of the
 *                          display where content is to be written.
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC void vDrawLcdDisplay(uint32 u32Xoffset, uint8 bClearShadow) {
	int n;

	for (n = 0; n < Y_MAX; n++) {
		vLcdWriteText((char*)&acLcdShadow[n][0], n, u32Xoffset);
	}

	vLcdWriteInvertedText("                                ", Y_MAX - 1, 0); // clean up area
	vLcdWriteInvertedText((char*)&acLcdShadow[Y_MAX][0], Y_MAX - 1, 0);
}

PUBLIC void vDrawLcdInit() {
	u8LcdX = 0;
	u8LcdY = 0;
	int n;
	for (n = 0; n <= Y_MAX; n++) {
		acLcdShadow[n][0] = '\0';
	}

	return;
}

/****************************************************************************
 *
 * NAME:       vPutC_LCD
 *
 * DESCRIPTION:
 * Writes characters to the scratchpad area for constructing
 * the LCD content
 *
 * PARAMETERS: Name     RW  Usage
 *          u8Data      R   Character to write to LCD
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC bool_t LCD_bTxChar(uint8 u8SerialPort, uint8 u8Data) {
	int i;

	switch (u8Data) {
	case '\n':
		u8LcdY++;
		if (u8LcdY >= Y_MAX) {
			// scroll!
			for (i = 1; i < Y_MAX; i++) {
				// TODO: need efficient code!
				memcpy(acLcdShadow[i - 1], acLcdShadow[i], X_MAX);
			}
			u8LcdY = Y_MAX - 1;
		}
		acLcdShadow[u8LcdY][0] = '\0';
		u8LcdX = 0;
		break;

	case '\r':
		u8LcdX = 0;
		break; // ignore

	case '\f':
		// clear screen (form feed)
		u8LcdX = 0;
		u8LcdY = 0;
		for (i = 0; i < Y_MAX; i++) {
			acLcdShadow[i][0] = '\0';
		}
		break;

	default:
		if (u8LcdX < X_MAX - 1) {
			acLcdShadow[u8LcdY][u8LcdX++] = u8Data;
			acLcdShadow[u8LcdY][u8LcdX] = '\0';
		}
		break;
	}

	return TRUE;
}

PUBLIC bool_t LCD_bTxBottom(uint8 u8SerialPort, uint8 u8Data) {
	switch (u8Data) {
	case '\n':
	case '\r':
		u8LcdXBtm = 0;
		break;

	case '\f':
		acLcdShadow[Y_MAX][0] = '\0';
		u8LcdXBtm = 0;
		break;

	default:
		if (u8LcdXBtm < X_MAX - 1) {
			acLcdShadow[Y_MAX][u8LcdXBtm++] = u8Data;
			acLcdShadow[Y_MAX][u8LcdXBtm] = '\0';
		}
		break;
	}

	return TRUE;
}
