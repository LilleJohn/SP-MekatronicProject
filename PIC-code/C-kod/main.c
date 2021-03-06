/* PWM-generering */
//Headerfiler----------------------------------------------------------------
#include <xc.h> //Definition av register och registerbitar.
#include "lcd.h"
#include <stdbool.h>
//Se pic18f25k22.h i kompilatorns include catalog
//C:\Program Files\Microchip\xc8\Vx.xx\include
//Configuration Bits---------------------------------------------------------
#pragma config FOSC=XT, FCMEN=OFF //Extern Oscillator, FailSafe disabled
#pragma config HFOFST=OFF //Delay until fosc stable
#pragma config PLLCFG=OFF, IESO=OFF //PLL Off, Osc switchover disabled
#pragma config PRICLKEN=ON //Primary clock enabled
#pragma config PWRTEN=ON, BOREN=OFF //Power-Up timer enabled, BOR off
#pragma config MCLRE=EXTMCLR, XINST=OFF//MCLR extern, I-set extension disabled
#pragma config WDTEN=OFF, LVP=OFF //Watchdog timer & Low Volt prog disabled
#pragma config DEBUG=OFF, STVREN=ON //Stack Full/Underflow will cause Reset

#define CLK_knapp0 PORTCbits.RC5
#define CLK_knapp1 PORTBbits.RB6
#define CLK_knapp2 PORTBbits.RB7
#define Ramp_knapp PORTCbits.RC1

#define _XTAL_FREQ 4000000 //Klockoscillatorfrekvens Fosc=4 MHz
//Kr�vs f�r macro ?__delay_us(x)?
//respektive ?__delay_ms(x)?


//Funktionsprototyper---------------------------------------------------------
void init(void);
int AD_omv(char ADkanal);
void interrupt isr(void);
void klock_config();
void print_ramp_symb();
void pump_signal();

//Globala Variabler
    unsigned int time_scaler=0;
    unsigned int timmar=0;
    unsigned int minuter=0;
    unsigned int sekunder=0;
    char blink_battery = 0;
    unsigned int styrsignal = 0;
    double rampsignal = 0;
    unsigned char set_Val = 0; //B�rv�rdet ("set") i procent, visas p� LCDn och beseras p� potentiometer
    unsigned char act_Val = 0; //�rv�rdet ("actual") i procent, visas p� LCDn och baseras p� niv�givare
    bool rampknapp_flag = 0; //Flagga f�r att toggla v�rdet p� rampknappen
    unsigned int sample_flag = 0;   //Flagga som s�tts d� sample interval r�knat till 0.8 sek
    unsigned char sample_interval = 0; //R�knar upp intervall till 0.8 sek.
    unsigned int pot_Val = 0; //B�rv�rde direkt fr�n potentiometer
    unsigned int sensor_Val; //�rv�rdet direkt fr�n niv�givare
    unsigned char ramptime_scaler = 0;
    unsigned int tid = 0; //Tid som g�tt i mikrosekunder;
    unsigned int tid_old = 0; //Tid vid senaste registrerade aktivering av rampknappen (F�rhindrar fladder)
    unsigned char duty_MSB, duty_LSB;
    bool battery_status = 1; //1: Batteri V >= 12V. 0: Batteri V < 12V
    double res_error1 = 0; //Felet mellan �r och b�rv�rde
    double res_error2 = 0; //Tidigare felet mellan �r och b�rv�rde
    double pump_signal1 = 0; //
    double pump_signal2 = 0;


    
//Huvudprogram----------------------------------------------------------------
void main()
{

    init();         //initierar Register
    lcd_init();     //Initierar LCD
    create_char();  //initiering av att skapa nya tecken

  
    while(1) //Huvudloop
    {
        //PWM funktion
        pump_signal();

        //Uppdatering av tidsvariabel som �r till f�r att f�rhindra fladder med knappar
        __delay_us(0.5);
        tid += 1;
        



        //Skriver ut �nskade v�rden/symboler p� displayen
        print_ramp_val(rampknapp_flag, rampsignal, pot_Val);
        print_ramp_symb();
        print_bor_ar(pot_Val, sensor_Val);
        print_battery(CM1CON0bits.C1OUT, blink_battery);

        //St�ller klockan
        klock_config();
    }
}

void pump_signal(){
        if(pump_signal1 <= 0){  //Begr�nsar styrsignalen >=0
            pump_signal1 = 0;
        }
        if(pump_signal1 >= 1023){   //Begr�nsar styrsignalen <= 1023
            pump_signal1 = 1023;
        }
        styrsignal = (int)pump_signal1; //uppdaterar styrv�rdet

        pot_Val = AD_omv(10); //pot_Value 0-1023
        duty_MSB = styrsignal>>2; //8 MSB till duty_MSB
        duty_LSB = styrsignal & 0x0003; //2 LSB till duty_LSB
        CCPR5L = duty_MSB; //8 MSB till CCPR5L
        CCP5CON = (CCP5CON & 0b11001111)|(duty_LSB <<4); //Maska in 2 LSB
        sensor_Val = AD_omv(12); //sensor_Val 0-1023

        //Regulator
        if(sample_flag == 1){
            pump_signal2 = pump_signal1;
            res_error2 = res_error1;
            res_error1 = rampsignal - (double)sensor_Val; //R�knar ut kvarst�ende fel
            pump_signal1=2.298*res_error1 - 2.266*res_error2 + pump_signal2;
            sample_flag = 0;
        }
}

void print_ramp_symb(){
    if(Ramp_knapp && tid >= (tid_old+50)) {

            //Skriver ut rampsymbol till Display
            rampknapp_flag = ~rampknapp_flag;

            tid_old = tid;
        }
        if(rampknapp_flag == 0xFF){
            lcd_goto(0x4E);
            lcd_write(0x03);
        }
        if(rampknapp_flag == 0){
            lcd_goto(0x4E);
            lcd_write(0x20);
        }
}

void klock_config(){
            //Konfig av Klocka------------------------------------------------------
        if(CLK_knapp0 && tid >= (tid_old+50)) {//S�tter timmar
            timmar += 1;
            tid_old = tid;
        }
        if(CLK_knapp1 && tid >= (tid_old+50)) {//S�tter Minuter
            minuter += 1;
            tid_old = tid;
        }
        if(CLK_knapp2 && tid >= (tid_old+50)) {//Nollst�ller Sekunder
            sekunder = 0;
            tid_old = tid;
        }

        if(CLK_knapp1 && CLK_knapp2 == 1){//Nollst�ller sek, min och tim.
            sekunder = 0;
            minuter = 0;
            timmar = 0;
        }

        printTime(sekunder,minuter,timmar); //Skriver ut tiden p� Displayen
}

//Initieringar----------------------------------------------------------------
void init()
{
    ANSELA=0b00000000; //PortA alla digitala
    ANSELB=0b00000011; //RB0 RB1 analoga. Resten digitala
    ANSELC=0b00000000; //PortC alla digitala
    TRISA=0b00000000; //PORTA utg�ngar
    TRISB=0b11000011; //RB0 RB1 RB6 RB7 ing�ng. Resten utg�ngar
    TRISC=0b00100010; //RC5 RC1 ing�ng. Resten utg�ngar
    TRISAbits.TRISA4=0; //CCP5pin = PWM-utg�ng (RA4)
    CCP5CON=0b00001100; //PWM-mode, 2LSbs f�r PWM=00
    CCPTMRS1=0b00000001; //CCP5 anv�nder Timer2
    PR2=0xFF; //PWM-period=256us om Fosc=4MHz och prescaler=1
    T2CON=0x04; //Timer2=on, prescaler=1
    T1CON=0x1; //Timer1=on, prescaler=1
    TMR1H=0x3D; //St�ller timer sp att delay blir 0.05sec
    TMR1L=0x1D; //...
    PIE1=0x1;   //TMR1 interupt enabled
    IPR1=0x1;   //TMR1 interup High priority
    INTCON=0xC0;//Global/Peripheral interupt enable
    ADCON0=0b0101001; //GO/DONE = 0, ADON = 1
    ADCON1=0b00000000; //TRIGSEL: CCP5, Vref+: AVdd, Vref-: AVss
    ADCON2=0b10100100; //H�gerjusterad, ACQT: 8 Tad, ADCS: Fosc/4
    CM1CON0=0b10001100; //C1ON=Enabled, C1OUT=internal
                        //Speed=Normal, C1R=C1Cin+, C1CH=C12IN0-
    CM2CON1=0b00100000; //C1RSEL=FVR BUF1
    VREFCON0=0xF0; //Enabled, Fixed Reference = 4.096V, Flag = 1 (ready to use)
    TXSTA1 = 0b00100100; //Asynkron, High Speed, Transmit Enabled
    RCSTA1 = 0b10000000; //Serial Port Enabled
    BAUDCON1 = 0b00000000; //BRG16 = 8-bit
    SPBRG1 = 12; //19200 (Baudrate)
}


//AD-omvandlings funktion ------------------------------------------------------
int AD_omv(char ADkanal)
{
    unsigned int AD_value = 0;
    ADCON0=(ADCON0 & 0b10000011)|(ADkanal<<2); //Val av AD-kanal
    __delay_us(5); //Delay 5us. Macro i MPLAB XC8.
    ADCON0bits.GO=1; //AD-omvandling startar
    while(ADCON0bits.GO); //V�nta p� att AD-omvandling �r klar
    AD_value = ADRESL + (ADRESH<<8);
    return AD_value; //Returnera 8 MSB av AD-omvandling
}

//Interupt-Klocka -------------------------------------------------------------
void interrupt isr(void){
    if(PIR1bits.TMR1IF && PIE1bits.TMR1IE){
        TMR1H=0x3C; //�terst�ller timer
        TMR1L=0xD6;
        if(++sample_interval >=16){ //Sampletid = 0,8 sec
            sample_flag = 1; // Updatera sampletid f�r regulatorn
            sample_interval = 0;
        
            if(rampknapp_flag == 0xFF && (rampsignal< (double)pot_Val)){
                rampsignal += 7.0; //Rampsignal �kar per 0.8 sek
            }
            //rampsignalen �r samma som b�rv�rdet d� rampflaggan �r sl�kt.
            if((rampknapp_flag == 0) || (rampsignal>= (double)pot_Val)){
                rampsignal = (double)pot_Val;
            }
            //Skriva 8-MSB till TXREG1 (Simulink)
            TXREG1 = sensor_Val>>2;

        }
        if(++time_scaler>=20){ //Prescalear om tid s� delay blir 1 sec
            time_scaler = 0;
            blink_battery = blink_battery + 1; //Blinktimer f�r "low battery"
            
            if(++sekunder>=60){ //R�kna sekunder
                sekunder = 0;
                if(++minuter>=60){//R�kna minuter
                    minuter=0;
                    if(++timmar>=24)//R�kna timmar
                        timmar=0;
                }
            }
        }
        PIR1bits.TMR1IF = 0;    //Nollst�ller Interruptflagga
    }
}