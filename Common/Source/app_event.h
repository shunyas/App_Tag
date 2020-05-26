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
 * event.h
 *
 *  Created on: 2013/01/10
 *      Author: seigo13
 */

#ifndef APP_EVENT_H_
#define APP_EVENT_H_

#include "ToCoNet_event.h"

typedef enum
{
	E_EVENT_APP_BASE = ToCoNet_EVENT_APP_BASE,
	E_EVENT_APP_START_NWK,
	E_EVENT_APP_GET_IC_INFO
} teEventApp;

// STATE MACHINE
typedef enum
{
	E_STATE_APP_BASE = ToCoNet_STATE_APP_BASE,
	E_STATE_APP_WAIT_NW_START,
	E_STATE_APP_WAIT_TX,
	E_STATE_APP_IO_WAIT_RX,
	E_STATE_APP_IO_RECV_ERROR,
	E_STATE_APP_FAILED,
	E_STATE_APP_INTERACTIVE,
	E_STATE_APP_SLEEP,
	E_STATE_APP_CHAT_SLEEP,
	E_STATE_APP_PREUDO_SLEEP,
	E_STATE_APP_RETRY,
	E_STATE_APP_RETRY_TX
} teStateApp;

#endif /* EVENT_H_ */
