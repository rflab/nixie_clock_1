#include <3694.h>
#include <stdlib.h>

// iicタイムアウト
#define IDLE_LOOP     (1000)
#define TX_END_LOOP   (1000)
#define RX_END_LOOP   (1000)
#define BUSBUSY_LOOP  (1000)

// 表示桁数
#define DIGITS (8)
#define DIGITS_OFF_ALL (DIGITS)

// timer_v
#define MSEC_TO_TIMERV (2)
#define MAX_TRANSITION (100)

// 表示フラグ
// デバイスにあわせる。
unsigned char NUM2REG_TABLE[] = 
{
	0, //数字
	9,
	8,
	7,
	6,
	5,
	4,
	3,
	2,
	1,
};

// ボタンコールバック
typedef void (*callback)(void);
callback g_timer_v_callback = NULL;
callback g_timer_a_callback = NULL;

// ステート
int prev_button0_pressed = 0;
int prev_button1_pressed = 0;
int prev_button2_pressed = 0;

typedef struct {
	callback on_button0;
	callback on_button1;
	callback on_button2;
}STATE;
STATE g_state_clock         = {NULL, NULL, NULL};
STATE g_state_calendar      = {NULL, NULL, NULL};
STATE g_state_hour_h_set      = {NULL, NULL, NULL};
STATE g_state_hour_l_set      = {NULL, NULL, NULL};
STATE g_state_minute_h_set    = {NULL, NULL, NULL};
STATE g_state_minute_l_set    = {NULL, NULL, NULL};
STATE g_state_second_h_set    = {NULL, NULL, NULL};
STATE g_state_second_l_set    = {NULL, NULL, NULL};
STATE g_state_year_h_set      = {NULL, NULL, NULL};
STATE g_state_year_l_set      = {NULL, NULL, NULL};
STATE g_state_month_h_set     = {NULL, NULL, NULL};
STATE g_state_month_l_set     = {NULL, NULL, NULL};
STATE g_state_date_h_set      = {NULL, NULL, NULL};
STATE g_state_date_l_set      = {NULL, NULL, NULL};
//STATE g_state_alarm_set     = {NULL, NULL, NULL};
//STATE g_state_stopwatch_set = {NULL, NULL, NULL};
//STATE g_state_timer_set     = {NULL, NULL, NULL};
STATE* g_state_current;
unsigned char g_setting_digit = DIGITS;
unsigned short g_blink_timer = 0;
static const unsigned short BLINK_INTERVAL = 500*MSEC_TO_TIMERV;




// DOT、非表示用の値ダミー
#define DISP_DOT (sizeof(NUM2REG_TABLE))
#define DISP_OFF (DISP_DOT+1)

// static assert
#define STATIC_ASSERT(exp) {char is_size[(exp)?1:0]; (void)is_size;}

// 表示データ
static unsigned char disp[DIGITS] = {0,0,0,0,0,0,0,0};
static unsigned char disp_prev[DIGITS] = {0,0,DISP_OFF,0,0,DISP_OFF,0,0};
static unsigned short transition[DIGITS] = {0,0,0,0,0,0,0,0};
static unsigned char hour_h = 0;
static unsigned char hour_l = 0;
static unsigned char minute_h = 0;
static unsigned char minute_l = 0;
static unsigned char second_h = 0;
static unsigned char second_l = 0;
static unsigned char year_h = 0;
static unsigned char year_l = 0;
static unsigned char month_h = 0;
static unsigned char month_l = 0;
static unsigned char date_h = 0;
static unsigned char date_l = 0;
static unsigned long g_timer_v_callback_time_left = 0;


// とりあえずプロトタイプ
void ChangeDiap(unsigned char ch_, unsigned char num_);

//-----------------------------------------------
// 基本関数
//-----------------------------------------------

// msec単位でwait
void wait_msec(int msec_)
{
	volatile int i,j;
    for (i=0;i<msec_;i++)
	{
    	for (j=0;j<1588;j++); /*1588は約1ms*/
    }
}

short iic_init(void)
{
	// 転送レート
	IIC2.ICCR1.BIT.CKS = 0x0f;
	IIC2.ICCR1.BIT.ICE = 1;
	
	return 1;
}

// busy end wait
short iic_busbusy(void)
{
	volatile short  i = 0;

	while (1)
	{
		if (i++ > BUSBUSY_LOOP)
			return 0;
    	if (IIC2.ICCR2.BIT.BBSY == 0)
			break;
	}

	return (IIC2.ICCR2.BIT.BBSY == 0);
}

// start condition
short iic_start(void)
{
	volatile short  i = 0;

	// ビジー状態解除待ち
	iic_busbusy();
	
	// マスタ送信モードを指定
	IIC2.ICCR1.BYTE = (IIC2.ICCR1.BYTE & 0xcf) | 0x30; 
	
	// 開始
	//BBSY=1,SCP=0 / スタートコンディション、発行
	IIC2.ICCR2.BYTE = (IIC2.ICCR2.BYTE & 0xbe) | 0x80;
	//do 
	//{
	//	IIC2.ICCR2.BYTE = (IIC2.ICCR2.BYTE & 0xbe) | 0x80;
	//	IIC2.ICDRT = SLA;
	//	
	//}while(IIC2.ICIER.BIT.ACKBR)

	// DRT/DRRが空になるまで待ち
	while (IIC2.ICSR.BIT.TDRE == 0);
	{
		if (i++ > TX_END_LOOP)
		{
			return 0;
		}
	}

	return 1;
}

// stop condition
short iic_stop(void)
{
	volatile short  i = 0;
	_BYTE           result;

	// ダミーリード
	result = IIC2.ICSR.BYTE;
	
	// TENDフラグクリア
	IIC2.ICSR.BIT.TEND = 0;

	// マスタ送信モードを指定
	IIC2.ICCR1.BYTE = (IIC2.ICCR1.BYTE & 0xcf) | 0x30;
	
	// 停止条件生成(BBSY=0, SCP=0/ストップコンディション、発行)
	IIC2.ICCR2.BYTE &= 0x3f;

	// 停止条件生成待ち
	while (IIC2.ICSR.BIT.STOP == 0)
	{
		if (i++ > TX_END_LOOP)
			return 0;
	}
	return 1;
}


short iic_put(_BYTE data)
{
	volatile short  i = 0;

	// マスタ送信モードを指定
	IIC2.ICCR1.BYTE = (IIC2.ICCR1.BYTE & 0xcf) | 0x30; 

	// データ書き込み
	IIC2.ICDRT = data;

	// 送信待ち
	while (IIC2.ICSR.BIT.TEND == 0)
	{
		// タイムアウト
		if (i++ > TX_END_LOOP)
			return 0;
	}
	
	// ACK?
	if (IIC2.ICIER.BIT.ACKBR != 0)
	{
		// 停止条件発行
		iic_stop();
		return 0;
	}
	return 1;
}

char iic_get(int ack_)
{
	volatile short  i = 0;
	_BYTE data = 0;
	
	// マスタ送信モードを指定
	IIC2.ICCR1.BYTE = (IIC2.ICCR1.BYTE & 0xcf) | 0x20; 
	
	// ダミーリードすると受信を開始
	data = IIC2.ICDRR;

	// レシーブデータフル待ち
	while (IIC2.ICSR.BIT.RDRF == 0)
	{
		// タイムアウト
		if (i++ > RX_END_LOOP)
			return 0;
	}

	// 読み込み
	data = IIC2.ICDRR;

	// ACK?
	if (IIC2.ICIER.BIT.ACKBR != 0)
	{
		// 停止条件発行
		iic_stop();
		return data;
	}
	
	return data;
}

// H8初期化
void InitH8(void)
{
	// タイマV設定
	TV.TCRV0.BIT.CCLR = 1;	// コンペアマッチAでTCNVクリア
	TV.TCRV0.BIT.CKS = 0;	// 停止
	TV.TCRV1.BIT.ICKS = 1;	// 上記CKS=3と併用で /128=156.25kHφ z
	TV.TCSRV.BIT.CMFA = 0;	// フラグクリア
	TV.TCNTV = 0;			// タイマカウンタクリア
	TV.TCORA = 80;			// 約500us
	TV.TCRV0.BIT.CMIEA = 1; // コンペアマッチで割り込み
	TV.TCRV0.BIT.CKS = 3; 	// タイマVスタート

	// IIC
	iic_init();

	// タイマA設定
	TA.TMA.BIT.CKSI = 8; 	// プリスケーラW/時計用選択 φ=1Hz
	IENR1.BIT.IENTA = 1; 	// タイマA割り込み要求許可

	// IOポートPCR8 設定
	IO.PCR8 = 0xFF; 		// PCR8 = 出力
	
	// IOポートPCR5 設定
	IO.PCR5 = 0xFF; 		// PCR5 = 出力
	
	// IOポートPCR1 設定
	IO.PCR1 = 0xe9; 		// PIO10 = 出力、PIO11,PIO12,PIO14 = 入力
}

void RtcInit()
{     
	wait_msec(1700);
	iic_start();
	iic_put(0xa2);  // 書き込みモード
	iic_put(0x00);  // control0のアドレス
	iic_put(0x0);   // test=0	
	iic_put(0x0);   //AIE=TIE=0
	iic_stop(); 
}

// 二進化十進法で日付を設定
void RtcDateSetBcdInDirect(short year_, char month_, char week_, char date_, char hour_, char minute_, char second_)
{
	iic_start();
	iic_put(0xa2);     // 書き込みモード
	iic_put(0x02);     // 秒のアドレス
	iic_put(second_);  // 秒の値 0-59
	iic_put(minute_);  // 分の値 0-59
	iic_put(hour_);    // 時の値 0-23
	iic_put(date_);    // 日の値 1-31
	iic_put(week_);    // 曜の値 日月火水木金土 0123456
	iic_put(year_>0x2000 ? 0x80|month_ : month_); // 月の値 (C:MSB)1-12   Cは1のとき21世紀
	iic_put((char)(year_&0x00ff)); // 年の値 00-99
	iic_stop();
}

// グローバルの値を使ってRTCの時間を設定
void RtcDateSetBcd()
{
	const char WEEK = 0;

	RtcDateSetBcdInDirect(
		0x2000 | ((year_h<<4)&0xf0 ) | year_l,
		(month_h << 4) | month_l,
		WEEK,
		(date_h << 4) | date_l,
		(hour_h   << 4) | hour_l,
		(minute_h << 4) | minute_l,
		(second_h << 4) | second_l);
}

// RTCのクロック値を読み出す
void RtcDateGetBcd()
{
	char sec   = 0; // 秒の値
	char min   = 0; // 分の値
	char hour  = 0; // 時の値
	char day   = 0; // 日の値
	char week  = 0; // 曜の値
	char month = 0; // 月の値
	char year  = 0; // 年の値
	
	// 通信
    iic_start();
    iic_put(0xa2);   // 書き込みモード
    iic_put(0x02);   // 秒のアドレス
    iic_start();
    iic_put(0xa3);   // 読み込みモード
    sec   = iic_get(1); // 秒の値
    min   = iic_get(1); // 分の値
    hour  = iic_get(1); // 時の値
    day   = iic_get(1); // 日の値
    week  = iic_get(1); // 曜の値
    month = iic_get(1); // 月の値
    year  = iic_get(0); // 年の値
    iic_stop();
	
	// 二進化十進→十進
    sec   &= 0x7f;
    min   &= 0x7f;
    hour  &= 0x3f;
    day   &= 0x3f;
    week  &= 0x07;
    month &= 0x1f;
    year  &= 0x7f;
    
	// 表示値に変換
	hour_h   = (hour >> 4)& 0xf;
	hour_l   = (hour) & 0xf;
	minute_h = (min >> 4) & 0xf;
	minute_l = (min) & 0xf;
	second_h = (sec >> 4) & 0xf;
	second_l = (sec) & 0xf;
	year_h   = (year >> 4)& 0xf;
	year_l   = (year) & 0xf;
	month_h  = (month >> 4) & 0xf;
	month_l  = (month) & 0xf;
	date_h   = (day >> 4) & 0xf;
	date_l   = (day) & 0xf;
}

// 表示設定
void ChangeDigit(unsigned char ch_, unsigned char num_)
{
	// shortは2バイト?
	STATIC_ASSERT(sizeof(short) != 2)
	
	if ((ch_ >= DIGITS)
	||  (num_ == DISP_OFF))
	{
		IO.PDR8.BYTE = 0xFF; // num = All Hiは非表示となる
		IO.PDR1.BYTE = 0x01; // ドットなし
	}

	// 表示チャンネル
	if (num_ < sizeof(NUM2REG_TABLE))
	{
		IO.PDR8.BYTE = (ch_<<4) & 0xf0;
	
		//数字
		IO.PDR8.BYTE |= NUM2REG_TABLE[num_] & 0x0f;

		//ドットなし
		IO.PDR1.BYTE = 0x01;
	}
	else if (num_ == DISP_DOT)
	{
		// 触らなければ数字なし
		// num = All Hiは非表示となる
		// 数字部は非表示
		IO.PDR8.BYTE = (ch_<<4) & 0xf0; 
		IO.PDR8.BYTE |= 0x0f;
		
		//ドットあり
		IO.PDR1.BYTE = 0x00;
	}
	else
	{
		//error
	}
}

void DynamicDispCallbackNormal()
{
	static unsigned char digits= 0;
	static unsigned char flag = 0;
	
	// 点滅カウントアップ
	++g_blink_timer;
	if (g_blink_timer >= BLINK_INTERVAL)
	{
		g_blink_timer = 0;
	}
	
	// 消灯区間
	// 点灯区間
	if( flag == 0)
	{
		ChangeDigit(DIGITS_OFF_ALL, DISP_OFF);
	}
	else
	{
        if (transition[digits] == MAX_TRANSITION)
        {
			if (digits == g_setting_digit)
			{
				if (g_blink_timer > BLINK_INTERVAL/2)
				{
	            	ChangeDigit(digits, disp[digits]);
				}
				else
				{
					ChangeDigit(DIGITS_OFF_ALL, DISP_OFF);
				}
			}
			else
			{
            	ChangeDigit(digits, disp[digits]);
			}
        }
        else
        {
            if (transition[digits]%(transition[digits]/(MAX_TRANSITION/2)+1) == 0)
            {
                ChangeDigit(digits, disp_prev[digits]);
            }
            else
            {
                ChangeDigit(digits, disp[digits]);
            }
            
            ++transition[digits];
        }
    }
	
	++flag;
	if( flag > 3 )
	{
		++digits;
		if( digits >= DIGITS )
		{
			digits = 0;
		}
		flag = 0;
	}
}


void DynamicDispCallbackRandom()
{
	static unsigned char digits= 0;
	static unsigned char flag = 0;
	
	// 消灯区間
	// 点灯区間
	if( flag == 0)
	{
		ChangeDigit(DIGITS_OFF_ALL, DISP_OFF);
	}
	else
	{
		ChangeDigit(digits, rand()%10);
	}
	
	++flag;
	if( flag > 3 )
	{
		++digits;
		if( digits >= DIGITS )
		{
			digits = 0;
		}
		flag = 0;
	}
}

// クロックモード
void ModeCallbackClock()
{
    disp_prev[0] = second_l;
    ++second_l;
    transition[0] = 0;
    transition[2] = 0; //dot

    if( second_l >= 10)
    {
        second_l = 0;
        disp_prev[1] = second_h;
        ++second_h;
        transition[1] = 0;

        if (second_h >= 6)
        {
            second_h = 0;
            disp_prev[3] = minute_l;
            ++minute_l;
            transition[3] = 0;

            if (minute_l >= 10)
            {
                minute_l = 0;
                disp_prev[4] = minute_h;
                minute_h++;
                transition[4] = 0;

                if (minute_h  >= 6)
                {
                    minute_h = 0;
                    disp_prev[6] = hour_l;
                    hour_l++;
                    transition[6] = 0;
                    
                    if (hour_h >= 2 && hour_l >= 4)
                    {
                        disp_prev[7] = hour_h;
                        hour_h = 0;
                        hour_l = 0;
                        transition[7] = 0;
                    }
                    else if(hour_l >= 10)
                    {
                        disp_prev[7] = hour_h;
                        hour_h++;
                        transition[7] = 0;
                    }
                }
            }
        }
    }
	
	// 表示データセット
	disp[0] = second_l;
	disp[1] = second_h;
	disp[2] = DISP_DOT;
	disp[3] = minute_l;
	disp[4] = minute_h;
	disp[5] = DISP_DOT;
	disp[6] = hour_l;
	disp[7] = hour_h;
}

// カレンダーモード
void ModeCallbackCalendar()
{
	disp[0] = date_l;
	disp[1] = date_h;
	disp[2] = month_l;
	disp[3] = month_h;
	disp[4] = year_l;
	disp[5] = year_h;
	disp[6] = 0;
	disp[7] = 2;
}


// タイマV割り込み
void int_timerv(void)
{
 	// 停止
	TV.TCRV0.BIT.CKS = 0;

	// フラグクリア
	TV.TCSRV.BIT.CMFA = 0;
	
	// 表示変化終了
	if (g_timer_v_callback_time_left == 0)
	{
		g_timer_v_callback = DynamicDispCallbackNormal;
	}
	else
	{
		--g_timer_v_callback_time_left;
	}
	
	// コールバック
	if (g_timer_v_callback != NULL)
	{
		(*g_timer_v_callback)();
	}

	// タイマVスタート
	TV.TCRV0.BIT.CKS = 3; 
}

// タイマA割り込み
void int_timera(void)
{
	// 表示
	if (g_timer_a_callback != NULL)
	{
		(*g_timer_a_callback)();
	}

	// タイマA割り込み要求フラグクリア
	IRR1.BIT.IRRTA = 0;
}

//--------------------------------------------------
// アプリ
//--------------------------------------------------

void StateChangeClock()
{
	int i = 0;
	for (i=0; i<DIGITS; ++i)
	{
		transition[i]=MAX_TRANSITION;
	}
	RtcDateGetBcd();
	g_timer_a_callback = ModeCallbackClock;
	g_timer_v_callback = DynamicDispCallbackRandom;
	g_timer_v_callback_time_left = MSEC_TO_TIMERV * 300L;
	g_state_current = &g_state_clock;
	g_setting_digit = 8;
	disp[0] = second_l;
	disp[1] = second_h;
	disp[2] = DISP_DOT;
	disp[3] = minute_l;
	disp[4] = minute_h;
	disp[5] = DISP_DOT;
	disp[6] = hour_l;
	disp[7] = hour_h;
}
void StateChangeCalendar()
{
	int i = 0;
	for (i=0; i<DIGITS; ++i)
	{
		transition[i]=MAX_TRANSITION;
	}
	RtcDateGetBcd();
	g_timer_a_callback = ModeCallbackCalendar;
	g_timer_v_callback = DynamicDispCallbackRandom;
	g_timer_v_callback_time_left = MSEC_TO_TIMERV * 300L;
	g_state_current = &g_state_calendar;
	g_setting_digit = 8;
	disp[0] = date_l;
	disp[1] = date_h;
	disp[2] = month_l;
	disp[3] = month_h;
	disp[4] = year_l;
	disp[5] = year_h;
	disp[6] = 0;
	disp[7] = 2;
}
void StateChangeHourHSet()
{
	RtcDateGetBcd();
	g_timer_a_callback = ModeCallbackClock;
	g_timer_v_callback = DynamicDispCallbackNormal;
	//g_timer_v_callback_time_left = MSEC_TO_TIMERV * 300L;
	g_state_current = &g_state_hour_h_set;
	g_setting_digit = 7;
	g_blink_timer = 0;
}
void StateChangeHourLSet()
{
	RtcDateGetBcd();
	g_timer_a_callback = ModeCallbackClock;
	g_timer_v_callback = DynamicDispCallbackNormal;
	g_state_current = &g_state_hour_l_set;
	g_setting_digit = 6;
	g_blink_timer = 0;
}
void StateChangeMinuteHSet()
{
	RtcDateGetBcd();
	g_timer_a_callback = ModeCallbackClock;
	g_timer_v_callback = DynamicDispCallbackNormal;
	g_state_current = &g_state_minute_h_set;
	g_setting_digit = 4;
	g_blink_timer = 0;
}
void StateChangeMinuteLSet()
{
	RtcDateGetBcd();
	g_timer_a_callback = ModeCallbackClock;
	g_timer_v_callback = DynamicDispCallbackNormal;
	g_state_current = &g_state_minute_l_set;
	g_setting_digit = 3;
	g_blink_timer = 0;
}
void StateChangeSecondHSet()
{
	RtcDateGetBcd();
	g_timer_a_callback = ModeCallbackClock;
	g_timer_v_callback = DynamicDispCallbackNormal;
	g_state_current = &g_state_second_h_set;
	g_setting_digit = 1;
	g_blink_timer = 0;
}
void StateChangeSecondLSet()
{
	RtcDateGetBcd();
	g_timer_a_callback = ModeCallbackClock;
	g_timer_v_callback = DynamicDispCallbackNormal;
	g_state_current = &g_state_second_l_set;
	g_setting_digit = 0;
	g_blink_timer = 0;
}
void StateChangeYearHSet()
{
	RtcDateGetBcd();
	g_timer_a_callback = ModeCallbackCalendar;
	g_timer_v_callback = DynamicDispCallbackNormal;
	g_state_current = &g_state_year_h_set;
	g_setting_digit = 5;
	g_blink_timer = 0;
}
void StateChangeYearLSet()
{
	RtcDateGetBcd();
	g_timer_a_callback = ModeCallbackCalendar;
	g_timer_v_callback = DynamicDispCallbackNormal;
	g_state_current = &g_state_year_l_set;
	g_setting_digit = 4;
	g_blink_timer = 0;
}
void StateChangeMonthHSet()
{
	RtcDateGetBcd();
	g_timer_a_callback = ModeCallbackCalendar;
	g_timer_v_callback = DynamicDispCallbackNormal;
	g_state_current = &g_state_month_h_set;
	g_setting_digit = 3;
	g_blink_timer = 0;
}
void StateChangeMonthLSet()
{
	RtcDateGetBcd();
	g_timer_a_callback = ModeCallbackCalendar;
	g_timer_v_callback = DynamicDispCallbackNormal;
	g_state_current = &g_state_month_l_set;
	g_setting_digit = 2;
	g_blink_timer = 0;
}
void StateChangeDateHSet()
{
	RtcDateGetBcd();
	g_timer_a_callback = ModeCallbackCalendar;
	g_timer_v_callback = DynamicDispCallbackNormal;
	g_state_current = &g_state_date_h_set;
	g_setting_digit = 1;
	g_blink_timer = 0;
}
void StateChangeDateLSet()
{
	RtcDateGetBcd();
	g_timer_a_callback = ModeCallbackCalendar;
	g_timer_v_callback = DynamicDispCallbackNormal;
	g_state_current = &g_state_date_l_set;
	g_setting_digit = 0;
	g_blink_timer = 0;
}
void HourHDown()
{
	RtcDateGetBcd();
	hour_h == 0 ? hour_h = 2 : --hour_h;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void HourHUp()
{
	RtcDateGetBcd();
	hour_h >= 2 ? hour_h = 0 : ++hour_h;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void HourLDown()
{
	RtcDateGetBcd();
	hour_l == 0 ? hour_l = 9 : --hour_l;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void HourLUp()
{
	RtcDateGetBcd();
	hour_l == 9 ? hour_l = 0 : ++hour_l;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void MinuteHDown()
{
	RtcDateGetBcd();
	minute_h == 0 ? minute_h = 5 : --minute_h;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void MinuteHUp()
{
	RtcDateGetBcd();
	minute_h == 5 ? minute_h = 0 : ++minute_h;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void MinuteLDown()
{
	RtcDateGetBcd();
	minute_l == 0 ? minute_l = 9 : --minute_l;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void MinuteLUp()
{
	RtcDateGetBcd();
	minute_l == 9 ? minute_l = 0 : ++minute_l;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void SecondHDown()
{
	RtcDateGetBcd();
	second_h == 0 ? second_h = 5 : --second_h;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void SecondHUp()
{
	RtcDateGetBcd();
	second_h == 5 ? second_h = 0 : ++second_h;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void SecondLDown()
{
	RtcDateGetBcd();
	second_l == 0 ? second_l = 9 : --second_l;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void SecondLUp()
{
	RtcDateGetBcd();
	second_l == 9 ? second_l = 0 : ++second_l;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void YearHDown()
{
	RtcDateGetBcd();
	year_h == 0 ? year_h = 9 : --year_h;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void YearHUp()
{
	RtcDateGetBcd();
	year_h == 9 ? year_h = 0 : ++year_h;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void YearLDown()
{
	RtcDateGetBcd();
	year_l == 0 ? year_l = 9 : --year_l;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void YearLUp()
{
	RtcDateGetBcd();
	year_l == 9 ? year_l = 0 : ++year_l;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void MonthHDown()
{
	RtcDateGetBcd();
	month_h == 0 ? month_h = 1 : --month_h;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void MonthHUp()
{
	RtcDateGetBcd();
	month_h == 1 ? month_h = 0 : ++month_h;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void MonthLDown()
{
	RtcDateGetBcd();
	month_l == 0 ? month_l = 9 : --month_l;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void MonthLUp()
{
	RtcDateGetBcd();
	month_l == 9 ? month_l = 0 : ++month_l;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void DateHDown()
{
	RtcDateGetBcd();
	date_h == 0 ? date_h = 3 : --date_h;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void DateHUp()
{
	RtcDateGetBcd();
	date_h == 3 ? date_h = 0 : ++date_h;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void DateLDown()
{
	RtcDateGetBcd();
	date_l == 0 ? date_l = 9 : --date_l;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}
void DateLUp()
{
	RtcDateGetBcd();
	date_l == 9 ? date_l = 0 : ++date_l;
	RtcDateSetBcd();
	g_blink_timer = BLINK_INTERVAL/2;
}




// メイン
void main(void)
{
	// 表示コールバックを設定
	g_timer_v_callback = DynamicDispCallbackRandom;
	g_timer_a_callback = ModeCallbackClock;
	g_timer_v_callback_time_left = MSEC_TO_TIMERV * 2000L;

	// マイコンを初期化
	InitH8();
	ChangeDigit(DIGITS_OFF_ALL, DISP_OFF);

	RtcInit();
	//RtcDateSetBcd();
	RtcDateGetBcd();

	// ステートマシンを初期化
	//最初は時計ステート
    g_state_clock.on_button0        = StateChangeCalendar;
    g_state_clock.on_button1        = StateChangeCalendar;
    g_state_clock.on_button2        = StateChangeHourHSet;
    g_state_hour_h_set.on_button0   = HourHDown;
    g_state_hour_h_set.on_button1   = HourHUp;
    g_state_hour_h_set.on_button2   = StateChangeHourLSet;
    g_state_hour_l_set.on_button0   = HourLDown;
    g_state_hour_l_set.on_button1   = HourLUp;
    g_state_hour_l_set.on_button2   = StateChangeMinuteHSet;
    g_state_minute_h_set.on_button0 = MinuteHDown;
    g_state_minute_h_set.on_button1 = MinuteHUp;
    g_state_minute_h_set.on_button2 = StateChangeMinuteLSet;
    g_state_minute_l_set.on_button0 = MinuteLDown;
    g_state_minute_l_set.on_button1 = MinuteLUp;
    g_state_minute_l_set.on_button2 = StateChangeSecondHSet;
    g_state_second_h_set.on_button0 = SecondHDown;
    g_state_second_h_set.on_button1 = SecondHUp;
    g_state_second_h_set.on_button2 = StateChangeSecondLSet;
    g_state_second_l_set.on_button0 = SecondLDown;
    g_state_second_l_set.on_button1 = SecondLUp;
    g_state_second_l_set.on_button2 = StateChangeClock;
    g_state_calendar.on_button0     = StateChangeClock;
    g_state_calendar.on_button1     = StateChangeClock;
    g_state_calendar.on_button2     = StateChangeYearHSet;
    g_state_year_h_set.on_button0   = YearHDown;
    g_state_year_h_set.on_button1   = YearHUp;
    g_state_year_h_set.on_button2   = StateChangeYearLSet;
    g_state_year_l_set.on_button0   = YearLDown;
    g_state_year_l_set.on_button1   = YearLUp;
    g_state_year_l_set.on_button2   = StateChangeMonthHSet;
    g_state_month_h_set.on_button0  = MonthHDown;
    g_state_month_h_set.on_button1  = MonthHUp;
    g_state_month_h_set.on_button2  = StateChangeMonthLSet;
    g_state_month_l_set.on_button0  = MonthLDown;
    g_state_month_l_set.on_button1  = MonthLUp;
    g_state_month_l_set.on_button2  = StateChangeDateHSet;
    g_state_date_h_set.on_button0   = DateHDown;
    g_state_date_h_set.on_button1   = DateHUp;
    g_state_date_h_set.on_button2   = StateChangeDateLSet;
    g_state_date_l_set.on_button0   = DateLDown;
    g_state_date_l_set.on_button1   = DateLUp;
    g_state_date_l_set.on_button2   = StateChangeCalendar;
	StateChangeClock();
	
	EI; //?
	while (1)
	{
	   	if (IO.PDR1.BIT.B1 == 0)
	   	{	
			if ((g_state_current->on_button0 != NULL)
			&&  (prev_button0_pressed == 0))
			{
				(*(g_state_current->on_button0))();
    		}
			
			prev_button0_pressed = 1;
    	}
		else
		{
			prev_button0_pressed = 0;
		}
		
	   	if (IO.PDR1.BIT.B2 == 0)
	   	{	
			if ((g_state_current->on_button1 != NULL)
			&&  (prev_button1_pressed == 0))
			{
				(*(g_state_current->on_button1))();
    		}
			
			prev_button1_pressed = 1;
    	}
		else
		{
			prev_button1_pressed = 0;
		}
				
	   	if (IO.PDR1.BIT.B4 == 0)
	   	{	
			if ((g_state_current->on_button2 != NULL)
			&&  (prev_button2_pressed == 0))
			{
				(*(g_state_current->on_button2))();
    		}
			
			prev_button2_pressed = 1;
    	}
		else
		{
			prev_button2_pressed = 0;
		}

		//チャタリング防止
		wait_msec(10);
		
		
	}
}

