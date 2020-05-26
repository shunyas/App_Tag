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
 * LcdPrint.h
 *
 *  Created on: 2012/12/28
 *      Author: seigo13
 */

#ifndef LCDPRINT_H_
#define LCDPRINT_H_

#include <jendefs.h>

PUBLIC bool_t LCD_bTxChar(uint8 u8SerialPort, uint8 u8Data);
PUBLIC bool_t LCD_bTxBottom(uint8 u8SerialPort, uint8 u8Data);
PUBLIC void vDrawLcdDisplay(uint32 u32Xoffset, uint8 bClearShadow);
PUBLIC void vDrawLcdInit();
extern const uint8 au8TocosHan[];
extern const uint8 au8TocosZen[];
extern const uint8 au8Tocos[];

#define CHR_BLK 0x7f
#define CHR_BLK_50 0x80

#endif /* LCDPRINT_H_ */
