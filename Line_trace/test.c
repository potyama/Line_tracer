#include "h8-3052-iodef.h"
#include "h8-3052-int.h"

#include "lcd.h"
#include "ad.h"
#include "loader.h"
#include "sci2.h"
#include "timer.h"
#include "key.h"

/* タイマ割り込みの時間間隔[μs] */
#define TIMER0 1000

/* 割り込み処理で各処理を行う頻度を決める定数 */
#define DISPTIME 100
#define KEYTIME 1
#define ADTIME 2
#define PWMTIME 1
#define CONTROLTIME 10

/* LED関係 */
/* LEDがP9に接続されているビット位置 */
#define REDLEDPOS	 0x10
#define GREENLEDPOS 0x20

/* LCD表示関連 */
/* 1段に表示できる文字数 */
#define LCDDISPSIZE 16

/* PWM制御関連 */
/* 制御周期を決める定数 */
//4-3のみ255にしてる
#define MAXPWMCOUNT 255

/* A/D変換関連 */
/* A/D変換のチャネル数とバッファサイズ */
#define ADCHNUM	 4
#define ADBUFSIZE 8
/*} 平均化するときのデータ個数 */
#define ADAVRNUM 4
/* チャネル指定エラー時に返す値 */
#define ADCHNONE -1


/* 割り込み処理に必要な変数は大域変数にとる */
volatile int disp_time, key_time, ad_time, pwm_time, control_time;
volatile int scan_l,scan_r;
/* LED関係 */
unsigned char redval, greenval;

/* LCD関係 */
volatile int disp_flag;
volatile char lcd_str_upper[LCDDISPSIZE+1];
volatile char lcd_str_lower[LCDDISPSIZE+1];

/* モータ制御関係 */
volatile int pwm_count;

/* A/D変換関係 */
volatile unsigned char adbuf[ADCHNUM][ADBUFSIZE];
volatile int adbufdp;

int main(void);
void int_imia0(void);
void int_adi(void);
int ad_read(int ch);
void pwm_proc(void);
void control_proc(void);

int main(void)
{
	/* 初期化 */
	ROMEMU();           /* ROMエミュレーションをON */

	/* ここでLEDポート(P9)の初期化を行う */
	P9DDR = 0x30;
	/* 割り込みで使用する大域変数の初期化 */
	pwm_time = pwm_count = 0;     /* PWM制御関連 */
	disp_time = 0; disp_flag = 1; /* 表示関連 */
	key_time = 0;                 /* キー入力関連 */
	ad_time = 0;                  /* A/D変換関連 */
	control_time = 0;             /* 制御関連 */
	/* ここまで */
	adbufdp = 0;         /* A/D変換データバッファポインタの初期化 */
	lcd_init();          /* LCD表示器の初期化 */
	key_init();          /* キースキャンの初期化 */
	ad_init();           /* A/Dの初期化 */
	timer_init();        /* タイマの初期化 */
	timer_set(0,TIMER0); /* タイマ0の時間間隔をセット */
	timer_start(0);      /* タイマ0スタート */
	ENINT();             /* 全割り込み受付可 */
	redval = 99;
	greenval = 99; /* 赤・緑LEDの両方を消灯とする */
	/* ここでLCDに表示する文字列を初期化しておく */
	int i = 0;
      lcd_str_upper[1] = "R:";
      lcd_str_lower[1] = "L:";
	while (1){ /* 普段はこのループを実行している */
            
	  /* ここで disp_flag によってLCDの表示を更新する */
	if(disp_flag == 1){
            
}

#pragma interrupt
void int_imia0(void)
	   /* タイマ0(GRA) の割り込みハンドラ　　　　　　　　　　　　　　　 */
	   /* 関数の名前はリンカスクリプトで固定している                   */
	   /* 関数の直前に割り込みハンドラ指定の #pragama interrupt が必要 */
	   /* タイマ割り込みによって各処理の呼出しが行われる               */
	   /*   呼出しの頻度は KEYTIME,ADTIME,PWMTIME,CONTROLTIME で決まる */
	   /* 全ての処理が終わるまで割り込みはマスクされる                 */
	   /* 各処理は基本的に割り込み周期内で終わらなければならない       */
{
	/* LCD表示の処理 */
	/* 他の処理を書くときの参考 */
	disp_time++;
	if (disp_time >= DISPTIME){
		disp_flag = 1;
	  	disp_time = 0;
		}

	/* ここにキー処理に分岐するための処理を書く */
	/* キー処理の中身は全て key.c にある */
	key_time++;
	if(key_time >= KEYTIME){
		key_sense();
		key_time = 0;
	}
	/* ここにPWM処理に分岐するための処理を書く */
	   pwm_time++;
	   if(pwm_time == PWMTIME){ 
	        pwm_proc();
	        pwm_time =0;
	   }
	/* ここにA/D変換開始の処理を直接書く */
	ad_time++;
	if(ad_time >= ADTIME){
		ad_scan(0,1);
		ad_time = 0;
	}
	/* A/D変換の初期化・スタート・ストップの処理関数は ad.c にある */
	
	/* ここに制御処理に分岐するための処理を書く */
	control_time++;
	if(control_time == CONTROLTIME){
		control_proc();
		control_time = 0;
	}
	timer_intflag_reset(0); /* 割り込みフラグをクリア */
	ENINT();                /* CPUを割り込み許可状態に */
}

int ad_read(int ch)
	   /* A/Dチャネル番号を引数で与えると, 指定チャネルの平均化した値を返す関数 */
	   /* チャネル番号は，0〜ADCHNUM の範囲 　　　　　　　　　　　             */
	   /* 戻り値は, 指定チャネルの平均化した値 (チャネル指定エラー時はADCHNONE) */
{
	int i,ad,dp;
	if ((ch > ADCHNUM) || (ch < 0)) ad = ADCHNONE; /* チャネル範囲のチェック */
	else {
	dp = adbufdp-3;
	ad = 0;
	for(i = 0;i < ADAVRNUM;i++){
		if(dp == 0)dp = ADBUFSIZE - 1;
		if(dp < 0)dp = ADBUFSIZE -1;
		ad += adbuf[ch][dp];
		dp--;
	}
	  /* ここで指定チャネルのデータをバッファからADAVRNUM個取り出して平均する */
	  /* データを取り出すときに、バッファの境界に注意すること */
	  /* 平均した値が戻り値となる */
	ad /= ADAVRNUM;
	}
	return ad; /* データの平均値を返す */
}


void control_proc(void){
      scan_l = ad_read(1);
      scan_r = ad_read(2);

}
