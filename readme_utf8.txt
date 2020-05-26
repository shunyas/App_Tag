＝＝＝ モニター監視デモ ＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝

本サンプルでは、省電力動作を行う無線タグからの発報を受信し親機に情報を伝達します。

動作の流れは、以下のようになります。
	T: タグ
	R: 中継機
	P: 親機

	   T
	   ↓1
	   R1---2--->R2---3--->P

	1. T からのパケットをR1が受信する。この際 R1 が受信した電波強度 LQI および
	   R1 のアドレス情報を付加する
	2. R1 からの情報を中継する
	3. R2 からの情報をPが受信し、TのパケットをR1が受信した旨表示する

ここで、T->R1, T->R2 が同時に受信された場合は、R1が受信された情報、R2が受信された
情報の２つが表示されます。また T->P も同様に処理されます。

例えば、T->R1 の LQI=100, T->R2 の LQI=65 であるなら、T は R1 により近い場所にいる
と推定されます。

もう一つの機能は、リモコンによる発報情報の取得です。

   Rc: リモコン

	   T
	   |
	   R1<-------R2<------P
                ↑↓
                 Rc

ここではメッセージプールという機能を使用します。この機能は親機Pから各R1,R2に最大
64バイトまでの情報を展開します。Tから発報された情報はPに記録され、Pより定期的に
メッセージプールでの情報展開が行われます。

Rc は、ボタンを押されると、近隣の中継機 R1 または R2 に接続し、メッセージプール
の情報を所得し、その情報を出力します。このように一旦中継機に情報展開しているのは、
RcからPまでデータ要求を行い、PからRcへのデータが戻ってくるまでの時間が長くなる傾
向にあるためです。

■ 構成
  ＊ Parent ⇒ 親機
  ＊ Router ⇒ 中継機
  ＊ EndDevice_Input ⇒ 子機（ボタンまたは定期的に発報）
  ＊ EndDevice_Remote ⇒ 子機 （近隣の中継機に問い合わせて発報状況を収集）

■ バイナリ
  */Build/*JN5148*.bin ⇒ TWE-Regular, Strong 用
                  Build/Makefile 中で TWE_CHIP_MODEL=JN5148 指定
  */Build/*JN5164*.bin ⇒ TWE-Regular, Strong 用
                  Build/Makefile 中で TWE_CHIP_MODEL=JN5164 指定

■ まず動作させるには
  1) Parent(親機) と EndDevice_Input(子機) のみを書き込みます。
  2) Parent には UART 接続しておきます (115200bsp 8N1)。
  3) 子機をリセットします。以後、５秒置きに送信されます。

■ インタラクティブモード
インタラクティブモードでは幾つかの設定が可能です。構成ごとに設定内容は異なります。

Parent, Router では + + + と + を少し時間を空けて３回入力します。
EndDevice_Remote,_Input では、電源投入時に DIO2=LO(GND) に設定します。

・共通設定
  Application ID  : 32bit アプリケーションID
  Device ID       : EndDevice_Inputのみ簡易アドレスとして利用可能です。
  Channels        : 使用するチャネルです。
  Tx Power        : 送信出力 (3: 通常, 2/1/0 は減衰で 0 が最弱です)
  Enc Key         : オプションビットにより暗号化有効時に使用される暗号化キー生成
                    のための 32bit 値です
  Option Bits     : オプションビット
    0x00010000 EndDevice_Input のメッセージ出力を有効にします
    0x00001000 暗号化通信を有効にします（構成全てが暗号化設定します）
    0x00000001 EndDevice_Input の送信方式です。1 の場合、中継機もパケットを受信し
               中継機受信情報が含まれたパケットを親機に報告します。0 の場合は、
               中継機経由のパケットかどうかは親機では表示しません。

・Parent
 a: set Application ID (0x67726305)
 i: set Device ID (--)
 c: set Channels (15)
 x: set Tx Power (3)
 b: set UART baud (115200) <-- UART 通信 (BPS ピン利用時)
 B: set UART option (8N1)  <-- UART 設定
 k: set Enc Key (0xA5A5A5A5)
 o: set Option Bits (0x00000001)

・Router
 a: set Application ID (0x67726305)
 i: set Device ID (--)
 c: set Channels (15)
 x: set Tx Power (3)
 l: set layer (0)          <-- 中継レイヤ数
 k: set Enc Key (0xA5A5A5A5)
 o: set Option Bits (0x00000001)

・EndDevice_Input
 a: set Application ID (0x67726305)
 i: set Device ID (--)
 c: set Channels (15)
 x: set Tx Power (3)
 p: set Sleep Dur (5000)   <--- スリープ期間
 k: set Enc Key (0xA5A5A5A5)
 o: set Option Bits (0x00000001)

・EndDevice_Remote
 a: set Application ID (0x67726305)
 i: set Device ID (--)
 c: set Channels (15)
 x: set Tx Power (3)
 k: set Enc Key (0xA5A5A5A5)
 o: set Option Bits (0x00000001)


■ Parent 親機
＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
UART0/115200/8N1 でターミナルを開いておきます。

子機からの発報が有れば以下のようなメッセージを発します。

::rv=80000000:lq=132:ct=0004:ed=86300002:id=F:bat=3200:a1=1427:a2=0993:p0=000:p1=000::

rv: 受信機のアドレス(0x80000000=親機) 中継機利用時には中継機のアドレスとなる
lq: 受信機が受信した時の LQI
ct: 続き番号(16進数)
ed: 子機のアドレス
id: 子機のＩＤ (16進数）
ba: 電源電圧 [mV] （精度は 5-10mV 刻み）
a1: ADC1 [mV]
a2: ADC2 [mV]
p0: パルスカウンター0 (DIO1)
p1: パルスカウンター1 (DIO8)

＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
また、１０秒に一度、メッセージプールの仕組みを使い受信した
発報情報を各中継機に戻します。

＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
評価開発キット上の LCD が接続されている場合は、受信ごとにメッセージが表示
されます。

004:80000000:140:0001:1427:0933
*1  *2       *3  *4   *5   *6

*1: 経過秒
*2: 受信機のアドレス(0x80000000=親機) 中継機利用時には中継機のアドレスとなる
*3: 受信機が受信した時の LQI
*4: 続き番号(16進数)
*5: ADC1 [mV]
*6: ADC2 [mV]

■ Router 中継機
＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
中継機は、自身の階層数を指定できます。何も指定しないと１で起動します。
設定はインタラクティブモードで実施します。

■ 子機(Input)
＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
DIO3/7 の割り込みにより起床するか、約５秒間隔(EndDevice_Input/EndDevice_Input.h)に
発報します。

DIO18 は稼働中は LO、スリープ中は HI になります。外部のセンサー回路の電源制御などに
利用します。

■ 子機(Remote)
DIO3/7 のいずれかのボタンを押し離しするとと、以下のようなメッセージが出力されます。
最後から３行目が求めるデータです。

*** ToCoSamp IO Monitor 0.06-0 ***
* App ID:67726305 Long Addr:80a004a1 Short Addr 04a1
* start end device[8]
[E_EVENT_TOCONET_NWK_START:84,Ch:11]
* Info: la=0 ty=2 ro=01 st=81
* Parent: 86300001
* LostParent: 0
* SecRescan: 0, SecRelocate: 0
[NWK STARTED and REQUEST IO STATE:84]
[E_STATE_APP_IO_WAIT_RX:84]
---MSGPOOL sl=0 ln=6 msg=34810000FDFF---
[E_STATE_APP_IO_WAIT_RX:GOTDATA:140]
[E_STATE_APP_SLEEP_LED:140]

ここで、msg=34810000FDFF は以下のように解釈されます。
NNAAAAAAAANN....

NNは受信してからのメッセージプールの送信までの経過秒（１６進）
NNがFFの場合は終端を意味します。
AAAAAAAAは発報した子機のアドレス

複数の子機が発報した場合、NNAAAAAAAAANNAAAAAAAA...FF となります。

起床するたびに違うスロットを呼び出します。

■ リリース
[v1.0.1]
・インタラクティブモードの実装
・ソースコードの整備
・EndDevice_Input の消費電流の最適化
・EndDevice_Input で、ADC 2ch, PC 2ch の計測を追加
・その他ソース整備、動作確認 (ToCoNet v1.0 ベース)

[v0.7.4]
・EndDevice スリープ前に UART Disable のコードを含めた（動作確認、省略可）
・TWX-0003/0009 の始動時のコードを追加
・EndDevice の再送回数が１回のみになっていたのを訂正
・Remote に LCD 対処用のコードを追加
・Remote に メッセージプールテスト用のコードを追加 (MSGPL_SLOT_TEST)
・Parent に LED 点灯用のコードを追加
・Parent に メッセージプールの各スロットの設定テストコードを追加
・Router のパケット送出タイミングをランダム的にずらすコードを追加
  sTx.u16DelayMax = 300; // 送信開始の遅延を大きめに設定する