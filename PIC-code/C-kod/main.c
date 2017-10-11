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
void printTime(char sekunder, char minuter, char timmar);
void create_char(void);

    unsigned int time_scaler=0;
    unsigned int timmar=0;
    unsigned int minuter=0;
    unsigned int sekunder=0;
    unsigned char blink_battery = 0;
    unsigned int styrsignal = 0;
    unsigned int rampsignal = 0;
    unsigned char set_Val = 0; //B�rv�rdet ("set") i procent, visas p� LCDn och beseras p� potentiometer
    unsigned char act_Val = 0; //�rv�rdet ("actual") i procent, visas p� LCDn och baseras p� niv�givare
    bool rampknapp_flag = 0; //
    unsigned int sample_flag = 0;
    unsigned char sample_interval = 0;
    unsigned int pot_Val = 0; //B�rv�rde direkt fr�n potentiometer
    unsigned char ramptime_scaler = 0;

//Huvudprogram----------------------------------------------------------------
void main()
{
    // LCD
    char ASCII_string[4];
    unsigned char character_code=0;

    init();
    lcd_init();
    create_char();

    unsigned char duty_MSB, duty_LSB;    
    unsigned int sensor_Val; //�rv�rdet direkt fr�n niv�givare
    unsigned int tid = 0; //Tid som g�tt i mikrosekunder;
    unsigned int tid_old = 0; //Tid vid senaste registrerade aktivering av rampknappen (F�rhindrar fladder)
    bool battery_status = 1; //1: Batteri V >= 12V. 0: Batteri V < 12V
    char variabel = 0;
    int res_error1 = 0;
    int res_error2 = 0;
    double pump_signal1 = 0;
    double pump_signal2 = 0;
    unsigned char disp_rampsignal = 0;

 
    while(1) //Huvudloop
    {
        //PWM ----------------------------------------------------------------
        styrsignal = (int)pump_signal1;
        if(styrsignal >=1023){
            styrsignal = 1023;
        }
        if(pump_signal1 <= -1023){
            pump_signal1 = -1023;
        }
        if(pump_signal1 >= 1023){
            pump_signal1 = 1023;
        }
        if(pump_signal1 < 0){
           styrsignal += (int)pump_signal1;
        }
        pot_Val = AD_omv(10); //pot_Value 0-1023
        duty_MSB = styrsignal>>2; //8 MSB till duty_MSB
        duty_LSB = styrsignal & 0x0003; //2 LSB till duty_LSB
        CCPR5L = duty_MSB; //8 MSB till CCPR5L
        CCP5CON = (CCP5CON & 0b11001111)|(duty_LSB <<4); //Maska in 2 LSB
        sensor_Val = AD_omv(12); //sensor_Val 0-1023
        __delay_us(0.5);
        tid += 1;
        
        //Skriva 8-MSB till TXREG1
        variabel = sensor_Val>>2;
        TXREG1 = variabel;


        //Regulator
        if(sample_flag == 1){
            pump_signal2 = pump_signal1;
            res_error2 = res_error1;
            res_error1 = rampsignal - sensor_Val; //R�knar ut kvarst�ende fel
            pump_signal1=2.298*res_error1 - 2.266*res_error2 + pump_signal2;
            sample_flag = 0;
        }



        //Rampfunktion -------------------------------------------------------
        
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
        
        //LCD ----------------------------------------------------------------
        //Skriva b�rv�rde
        set_Val = (char)(pot_Val/10.23);
        if(set_Val < 10) {
            lcd_goto(0x04);
            lcd_writesc(" ");
            lcd_goto(0x05);
            lcd_write(0x30+set_Val);
        }
        else if(set_Val < 100) {
            lcd_goto(0x04);
            lcd_writesc(" ");
            lcd_goto(0x04);
            lcd_write(0x30+(set_Val/10));
            lcd_goto(0x05);
            lcd_write(0x30+(set_Val%10));
        }

        //Skriva �rv�rde
        act_Val = (char)(sensor_Val/10.23);
        if(act_Val < 10) {
            lcd_goto(0x44);
            lcd_writesc(" ");
            lcd_goto(0x45);
            lcd_write(0x30+act_Val);
        }
        else if(act_Val < 100) {
            lcd_goto(0x44);
            lcd_writesc(" ");
            lcd_goto(0x44);
            lcd_write(0x30+(act_Val/10));
            lcd_goto(0x45);
            lcd_write(0x30+(act_Val%10));
        }


        //Skriva egna tecken -------------------------------------------------

        //Battery full
        if(CM1CON0bits.C1OUT == 0) {
            lcd_goto(0x4F);
            lcd_write(0x00);
        }

        //Battery low ***********
        if(CM1CON0bits.C1OUT == 1) {
            if(blink_battery%2 == 1){
                lcd_goto(0x4F);
                lcd_write(0x01);
                if(blink_battery >10)
                    blink_battery = 1;
            }
            if(blink_battery%2 == 0){
                lcd_goto(0x4F);
                lcd_write(0x02);
            }
        }

       //Rampv�rde
        if(rampknapp_flag == 0){
            rampsignal = pot_Val;
        }
        disp_rampsignal = (char)(rampsignal/10.23);
        if(disp_rampsignal < 10) {
            lcd_goto(0x4A);
            lcd_writesc(" ");
            lcd_goto(0x4B);
            lcd_write(0x30+disp_rampsignal);
        }
        else if(disp_rampsignal < 100) {
            lcd_goto(0x4A);
            lcd_writesc(" ");
            lcd_goto(0x4A);
            lcd_write(0x30+(disp_rampsignal/10));
            lcd_goto(0x4B);
            lcd_write(0x30+(disp_rampsignal%10));
        } 



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

//Skapar nya symboler ----------------------------------------------------------
void create_char(void){ 
    //Battery Full
    lcd_char(0x00);
    lcd_write(0b01110);
    for(int i=1; i<=7; i++){
        lcd_char(0x00+i);
        lcd_write(0b11111);
    }//*************

    //Battery Low 1
    lcd_char(0x08);
    lcd_write(0b01110);
    for(int i=1; i<6; i++){
        lcd_char(0x08+i);
        lcd_write(0b10001);
    }
    for(int i=6; i<8; i++){
        lcd_char(0x08+i);
        lcd_write(0b11111);
    }//*************

    //Battery Low 2
    lcd_char(0x10);
    lcd_write(0b01110);
    for(int i=1; i<7; i++){
        lcd_char(0x10+i);
        lcd_write(0b10001);
    }
    lcd_char(0x17);
    lcd_write(0b11111);
    //*************

    //Rampfunktion 
    lcd_char(0x18);
    lcd_write(0x00);
    lcd_char(0x19);
    lcd_write(0x00);
    lcd_char(0x1A);
    lcd_write(0b00011);
    lcd_char(0x1B);
    lcd_write(0b00110);
    lcd_char(0x1C);
    lcd_write(0b01100);
    lcd_char(0x1D);
    lcd_write(0b11000);
    lcd_char(0x1E);
    lcd_write(0x00);
    lcd_char(0x1F);
    lcd_write(0x00);
   
    
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
        }
        if(++ramptime_scaler >=2){
            if(rampknapp_flag == 0xFF && (rampsignal< pot_Val)){
                rampsignal += 1;
            }
            if(rampknapp_flag == 0xFF && (rampsignal> pot_Val)){
                rampsignal -= 1;
            }
            if(rampknapp_flag == 0xFF && (rampsignal == pot_Val)){
                rampsignal = rampsignal;
            }
            ramptime_scaler = 0;
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

//Printar ut tiden p� Displayen ------------------------------------------------
void printTime(char sekunder, char minuter, char timmar)
{
    int i = 0;
    int var = 0;
    int pos1 = 0, pos2 = 0;
    for(i=0; i<3; i++){
        switch(i){
                case 0:
                    var = timmar;
                    pos1 = 0x08;
                    pos2 =0x09;
                    break;
                case 1:
                    var = minuter;
                    pos1 = 0x0B;
                    pos2= 0x0C;
                    break;
                case 2:
                    var = sekunder;
                    pos1 = 0x0E;
                    pos2 = 0x0F;
                    break;
        }
        if(var < 10) { //Skriv ut '0' och sen ental
            lcd_goto(pos1);
            lcd_write('0');
            lcd_goto(pos2);
            lcd_write(0x30+var);
        }
        else { //Skriv ut f�rst tiotal och sedan ental
            lcd_goto(pos1);
            lcd_write(0x30+(var/10));
            lcd_goto(pos2);
            lcd_write(0x30+(var%10));
        }
    }


}