/*
 * ProyectoAvance2.c
 *
 * Created: 7/09/2025 9:52:35 a. m.
 *  Author: Usuario
 */ 


#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <math.h>
#include "lcd.h"
volatile uint8_t selector;
uint16_t turbidez;
uint16_t TDS;

float voltajeTurbidez;
float voltajeTDS;

float NTU;
float PPM;
volatile int8_t bandera;

char buffer[16];



int main(void)
{
	
	
	DDRB&=~(1<<PB0);  //SELECTOR (Entrada)
	PORTB|=(1<<PB0) ;
	
	DDRC &= ~((1<<PC1) | (1<<PC0));  //Turbidez(Entrada) ; TDS(Entrada)
	
	DDRB |= (1 << PB5);  //LED
	//LCD:PD
	
	
	
	selector = (PINB & (1 << PB0)) ? 1 : 0;
	
	//Inicializar conversor ADC
	ADMUX = (1<<REFS0);
	ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);
	
	
	
	cli (); //Inicializar interrupciones y temporizador 
	
	PCICR |=(1<<PCIE0);//PCINT0
	PCMSK0 |=(1<<PCINT0);  
	
	TCCR1B |= (1 << WGM12); //Timer 1     
	TCCR1B |= (1 << CS12) | (1 << CS10); 
	OCR1A = 15624;                   
	TIMSK1 |= (1 << OCIE1A);
	sei();
	
	
	//Inicializar pantalla LCD
	
	lcd_init(LCD_DISP_ON); 

	lcd_clrscr();            

	lcd_home();   
	
    while(1)
    { //No
		
		if (bandera){  //ACtivar la bandera de actualización?
	
			bandera=0; //Si
	
			if (selector){   //Turbidez?
				//si
				lcd_clrscr();
				ADMUX = (ADMUX & 0b11110000) | 0; //LEER  SENSOR TURBIDEZ (ADC0)
				ADCSRA |= (1<<ADSC);
				while(ADCSRA & (1<<ADSC));
				turbidez = ADC;
    
				voltajeTurbidez = (turbidez * 5.0) / 1023.0; //Calcular voltaje turbidez
				NTU = (435.53*pow(voltajeTurbidez, 2))-(3517.3*voltajeTurbidez)+7105.4; //Calcula NTU
		
				//Mostrar NTU en pantalla LCD
				lcd_clrscr();
				lcd_gotoxy(0,0);
				lcd_puts("Turbidez:");

				dtostrf(NTU, 6, 2, buffer);

				lcd_gotoxy(0,1);
				lcd_puts(buffer);
				lcd_puts(" NTU");

		} 
			   
		else{ //No
	
				lcd_clrscr();
				ADMUX = (ADMUX & 0b11110000) | 1; //LEER SENSOR TDS (ADC1)
				ADCSRA |= (1<<ADSC);
				while(ADCSRA & (1<<ADSC));
				TDS= ADC;  
				voltajeTDS= (TDS*5.0)/1023.0; //Calcular VOLTAJE DE TDS
				PPM =  (314.99*pow(voltajeTDS, 2))+(226.89*voltajeTDS)+257.56;//Calcular PPM		
		
				//Mostrar PPM en pantalla LCD   
				lcd_clrscr();
				lcd_gotoxy(0,0);        
				lcd_puts("TDS:");

				dtostrf(PPM, 6, 2, buffer);

				lcd_gotoxy(0,1);
				lcd_puts(buffer);
				lcd_puts(" PPM");
		
					 }			 	       
				}
			}

}

//RUTINAS DE INTERRUPCION

ISR (PCINT0_vect){ //Interrupción por cambio PB0
	
  if (PINB & (1 << PB0)) { //Leer estado PB0
	  
//Actualizar Variable del selector
	  selector = 1; 
  } else {
	  selector = 0;
  }
}// Retornar interrrupción

ISR (TIMER1_COMPA_vect){//INterrupción por temporizador
	
	bandera=1; //Actualizar bandera de actualización
	
	PORTB ^= (1 << PB5); //Alternar Led 1s
	
}//Retornar interrupción

