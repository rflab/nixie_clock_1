#include <3694.h>
#include <stdlib.h>

// iic�^�C���A�E�g
#define IDLE_LOOP     (1000)
#define TX_END_LOOP   (1000)
#define RX_END_LOOP   (1000)
#define BUSBUSY_LOOP  (1000)

// �\������
#define DIGITS (8)
#define DIGITS_OFF_ALL (DIGITS)

// timer_v
#define MSEC_TO_TIMERV (2)
#define MAX_TRANSITION (100)

// �\���t���O
// �f�o�C�X�ɂ��킹��B
unsigned char NUM2REG_TABLE[] = 
{
	0, //����
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

// �{�^���R�[���o�b�N
typedef void (*callback)(void);
callback g_timer_v_callback = NULL;
callback g_timer_a_callback = NULL;

// �X�e�[�g
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




// DOT�A��\���p�̒l�_�~�[
#define DISP_DOT (sizeof(NUM2REG_TABLE))
#define DISP_OFF (DISP_DOT+1)

// static assert
#define STATIC_ASSERT(exp) {char is_size[(exp)?1:0]; (void)is_size;}

// �\���f�[�^
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


// �Ƃ肠�����v���g�^�C�v
void ChangeDiap(unsigned char ch_, unsigned char num_);

//-----------------------------------------------
// ��{�֐�
//-----------------------------------------------

// msec�P�ʂ�wait
void wait_msec(int msec_)
{
	volatile int i,j;
    for (i=0;i<msec_;i++)
	{
    	for (j=0;j<1588;j++); /*1588�͖�1ms*/
    }
}

short iic_init(void)
{
	// �]�����[�g
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

	// �r�W�[��ԉ����҂�
	iic_busbusy();
	
	// �}�X�^���M���[�h���w��
	IIC2.ICCR1.BYTE = (IIC2.ICCR1.BYTE & 0xcf) | 0x30; 
	
	// �J�n
	//BBSY=1,SCP=0 / �X�^�[�g�R���f�B�V�����A���s
	IIC2.ICCR2.BYTE = (IIC2.ICCR2.BYTE & 0xbe) | 0x80;
	//do 
	//{
	//	IIC2.ICCR2.BYTE = (IIC2.ICCR2.BYTE & 0xbe) | 0x80;
	//	IIC2.ICDRT = SLA;
	//	
	//}while(IIC2.ICIER.BIT.ACKBR)

	// DRT/DRR����ɂȂ�܂ő҂�
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

	// �_�~�[���[�h
	result = IIC2.ICSR.BYTE;
	
	// TEND�t���O�N���A
	IIC2.ICSR.BIT.TEND = 0;

	// �}�X�^���M���[�h���w��
	IIC2.ICCR1.BYTE = (IIC2.ICCR1.BYTE & 0xcf) | 0x30;
	
	// ��~��������(BBSY=0, SCP=0/�X�g�b�v�R���f�B�V�����A���s)
	IIC2.ICCR2.BYTE &= 0x3f;

	// ��~���������҂�
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

	// �}�X�^���M���[�h���w��
	IIC2.ICCR1.BYTE = (IIC2.ICCR1.BYTE & 0xcf) | 0x30; 

	// �f�[�^��������
	IIC2.ICDRT = data;

	// ���M�҂�
	while (IIC2.ICSR.BIT.TEND == 0)
	{
		// �^�C���A�E�g
		if (i++ > TX_END_LOOP)
			return 0;
	}
	
	// ACK?
	if (IIC2.ICIER.BIT.ACKBR != 0)
	{
		// ��~�������s
		iic_stop();
		return 0;
	}
	return 1;
}

char iic_get(int ack_)
{
	volatile short  i = 0;
	_BYTE data = 0;
	
	// �}�X�^���M���[�h���w��
	IIC2.ICCR1.BYTE = (IIC2.ICCR1.BYTE & 0xcf) | 0x20; 
	
	// �_�~�[���[�h����Ǝ�M���J�n
	data = IIC2.ICDRR;

	// ���V�[�u�f�[�^�t���҂�
	while (IIC2.ICSR.BIT.RDRF == 0)
	{
		// �^�C���A�E�g
		if (i++ > RX_END_LOOP)
			return 0;
	}

	// �ǂݍ���
	data = IIC2.ICDRR;

	// ACK?
	if (IIC2.ICIER.BIT.ACKBR != 0)
	{
		// ��~�������s
		iic_stop();
		return data;
	}
	
	return data;
}

// H8������
void InitH8(void)
{
	// �^�C�}V�ݒ�
	TV.TCRV0.BIT.CCLR = 1;	// �R���y�A�}�b�`A��TCNV�N���A
	TV.TCRV0.BIT.CKS = 0;	// ��~
	TV.TCRV1.BIT.ICKS = 1;	// ��LCKS=3�ƕ��p�� /128=156.25kH�� z
	TV.TCSRV.BIT.CMFA = 0;	// �t���O�N���A
	TV.TCNTV = 0;			// �^�C�}�J�E���^�N���A
	TV.TCORA = 80;			// ��500us
	TV.TCRV0.BIT.CMIEA = 1; // �R���y�A�}�b�`�Ŋ��荞��
	TV.TCRV0.BIT.CKS = 3; 	// �^�C�}V�X�^�[�g

	// IIC
	iic_init();

	// �^�C�}A�ݒ�
	TA.TMA.BIT.CKSI = 8; 	// �v���X�P�[��W/���v�p�I�� ��=1Hz
	IENR1.BIT.IENTA = 1; 	// �^�C�}A���荞�ݗv������

	// IO�|�[�gPCR8 �ݒ�
	IO.PCR8 = 0xFF; 		// PCR8 = �o��
	
	// IO�|�[�gPCR5 �ݒ�
	IO.PCR5 = 0xFF; 		// PCR5 = �o��
	
	// IO�|�[�gPCR1 �ݒ�
	IO.PCR1 = 0xe9; 		// PIO10 = �o�́APIO11,PIO12,PIO14 = ����
}

void RtcInit()
{     
	wait_msec(1700);
	iic_start();
	iic_put(0xa2);  // �������݃��[�h
	iic_put(0x00);  // control0�̃A�h���X
	iic_put(0x0);   // test=0	
	iic_put(0x0);   //AIE=TIE=0
	iic_stop(); 
}

// ��i���\�i�@�œ��t��ݒ�
void RtcDateSetBcdInDirect(short year_, char month_, char week_, char date_, char hour_, char minute_, char second_)
{
	iic_start();
	iic_put(0xa2);     // �������݃��[�h
	iic_put(0x02);     // �b�̃A�h���X
	iic_put(second_);  // �b�̒l 0-59
	iic_put(minute_);  // ���̒l 0-59
	iic_put(hour_);    // ���̒l 0-23
	iic_put(date_);    // ���̒l 1-31
	iic_put(week_);    // �j�̒l �����ΐ��؋��y 0123456
	iic_put(year_>0x2000 ? 0x80|month_ : month_); // ���̒l (C:MSB)1-12   C��1�̂Ƃ�21���I
	iic_put((char)(year_&0x00ff)); // �N�̒l 00-99
	iic_stop();
}

// �O���[�o���̒l���g����RTC�̎��Ԃ�ݒ�
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

// RTC�̃N���b�N�l��ǂݏo��
void RtcDateGetBcd()
{
	char sec   = 0; // �b�̒l
	char min   = 0; // ���̒l
	char hour  = 0; // ���̒l
	char day   = 0; // ���̒l
	char week  = 0; // �j�̒l
	char month = 0; // ���̒l
	char year  = 0; // �N�̒l
	
	// �ʐM
    iic_start();
    iic_put(0xa2);   // �������݃��[�h
    iic_put(0x02);   // �b�̃A�h���X
    iic_start();
    iic_put(0xa3);   // �ǂݍ��݃��[�h
    sec   = iic_get(1); // �b�̒l
    min   = iic_get(1); // ���̒l
    hour  = iic_get(1); // ���̒l
    day   = iic_get(1); // ���̒l
    week  = iic_get(1); // �j�̒l
    month = iic_get(1); // ���̒l
    year  = iic_get(0); // �N�̒l
    iic_stop();
	
	// ��i���\�i���\�i
    sec   &= 0x7f;
    min   &= 0x7f;
    hour  &= 0x3f;
    day   &= 0x3f;
    week  &= 0x07;
    month &= 0x1f;
    year  &= 0x7f;
    
	// �\���l�ɕϊ�
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

// �\���ݒ�
void ChangeDigit(unsigned char ch_, unsigned char num_)
{
	// short��2�o�C�g?
	STATIC_ASSERT(sizeof(short) != 2)
	
	if ((ch_ >= DIGITS)
	||  (num_ == DISP_OFF))
	{
		IO.PDR8.BYTE = 0xFF; // num = All Hi�͔�\���ƂȂ�
		IO.PDR1.BYTE = 0x01; // �h�b�g�Ȃ�
	}

	// �\���`�����l��
	if (num_ < sizeof(NUM2REG_TABLE))
	{
		IO.PDR8.BYTE = (ch_<<4) & 0xf0;
	
		//����
		IO.PDR8.BYTE |= NUM2REG_TABLE[num_] & 0x0f;

		//�h�b�g�Ȃ�
		IO.PDR1.BYTE = 0x01;
	}
	else if (num_ == DISP_DOT)
	{
		// �G��Ȃ���ΐ����Ȃ�
		// num = All Hi�͔�\���ƂȂ�
		// �������͔�\��
		IO.PDR8.BYTE = (ch_<<4) & 0xf0; 
		IO.PDR8.BYTE |= 0x0f;
		
		//�h�b�g����
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
	
	// �_�ŃJ�E���g�A�b�v
	++g_blink_timer;
	if (g_blink_timer >= BLINK_INTERVAL)
	{
		g_blink_timer = 0;
	}
	
	// �������
	// �_�����
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
	
	// �������
	// �_�����
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

// �N���b�N���[�h
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
	
	// �\���f�[�^�Z�b�g
	disp[0] = second_l;
	disp[1] = second_h;
	disp[2] = DISP_DOT;
	disp[3] = minute_l;
	disp[4] = minute_h;
	disp[5] = DISP_DOT;
	disp[6] = hour_l;
	disp[7] = hour_h;
}

// �J�����_�[���[�h
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


// �^�C�}V���荞��
void int_timerv(void)
{
 	// ��~
	TV.TCRV0.BIT.CKS = 0;

	// �t���O�N���A
	TV.TCSRV.BIT.CMFA = 0;
	
	// �\���ω��I��
	if (g_timer_v_callback_time_left == 0)
	{
		g_timer_v_callback = DynamicDispCallbackNormal;
	}
	else
	{
		--g_timer_v_callback_time_left;
	}
	
	// �R�[���o�b�N
	if (g_timer_v_callback != NULL)
	{
		(*g_timer_v_callback)();
	}

	// �^�C�}V�X�^�[�g
	TV.TCRV0.BIT.CKS = 3; 
}

// �^�C�}A���荞��
void int_timera(void)
{
	// �\��
	if (g_timer_a_callback != NULL)
	{
		(*g_timer_a_callback)();
	}

	// �^�C�}A���荞�ݗv���t���O�N���A
	IRR1.BIT.IRRTA = 0;
}

//--------------------------------------------------
// �A�v��
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




// ���C��
void main(void)
{
	// �\���R�[���o�b�N��ݒ�
	g_timer_v_callback = DynamicDispCallbackRandom;
	g_timer_a_callback = ModeCallbackClock;
	g_timer_v_callback_time_left = MSEC_TO_TIMERV * 2000L;

	// �}�C�R����������
	InitH8();
	ChangeDigit(DIGITS_OFF_ALL, DISP_OFF);

	RtcInit();
	//RtcDateSetBcd();
	RtcDateGetBcd();

	// �X�e�[�g�}�V����������
	//�ŏ��͎��v�X�e�[�g
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

		//�`���^�����O�h�~
		wait_msec(10);
		
		
	}
}

