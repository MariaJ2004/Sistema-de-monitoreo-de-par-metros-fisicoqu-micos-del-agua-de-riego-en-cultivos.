/*
 * ProyectoAvance1.c
 *
 * Created: 19/08/2025 8:38:34 p. m.
 *  Author: Usuario
 */ 

#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>

#define TM1637_DIO PD1
#define TM1637_CLK PD0

uint8_t selector;
uint16_t turbidez;
uint16_t TDS;

float voltajeTurbidez;
float voltajeTDS;

int16_t NTU;
int16_t PPM;

uint8_t selector;

uint8_t seg[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D,	0x7D, 0x07,	0x7F, 0x6F};
uint8_t brightness = 0x07; // Brillo (0-7)


int main(void)
{
	ADMUX = (1<<REFS0); 
	ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);
	
	DDRB&=~(1<<PB0);  //SELECTOR ->PB0
	PORTB|=(1<<PB0) ;    
	DDRC &= ~((1<<PC1) | (1<<PC0));  //TURBIDEZ->PC0 ; TDS->PC1
	
	DDRD |= (1 << TM1637_DIO) | (1 << TM1637_CLK); // TM1637_CLK -> PD0; TM1637_DIO -> PD1
	
	

    while(1)
    {
       selector = 1<<PB0;
	   if (PINB & selector){   //TURBIDEZ?
		   //SI
			ADMUX = (ADMUX & 0b11110000) | 0; //LEER ADC0 SENSOR TURBIDEZ
			ADCSRA |= (1<<ADSC);
			while(ADCSRA & (1<<ADSC));
			turbidez = ADC;   
			
			voltajeTurbidez = (turbidez * 5.0) / 1023.0; //LEER VOLTAJE SENSOR TURBIDEZ
			NTU = (int)((3501.9*pow(voltajeTurbidez, 2))-(25456*voltajeTurbidez)+46289);
			
	        TM1637_displayNumber(NTU);//MOSTRAR  NTU EN DISPLAYS A TRAVÉS DE CLK Y DIO
			
			_delay_ms(1000);
		} 
		   
		   
	  else{ //NO
		   ADMUX = (ADMUX & 0b11110000) | 1; //LEER ADC1 SENSOR TDS
		   ADCSRA |= (1<<ADSC);
			while(ADCSRA & (1<<ADSC));
			   TDS= ADC;  
			   voltajeTDS= (TDS*5.0)/1023.0; //LEER VOLTAJE SENSOR TDS
			   PPM =  (int)((508.1*pow(voltajeTDS, 2))-(649.47*voltajeTDS)+702.29);		   
			   TM1637_displayNumber(PPM); //MOSTRAR PPM EN DISPLAYS A TRAVÉS DE CLK Y DIO 
			 }	
			 _delay_ms(1000);	       
		}
	}

static inline void sendByte(uint8_t data) {
	for (uint8_t i = 0; i < 8; i++) {
		if (data & 0x01)
		PORTD |= (1 << TM1637_DIO);
		else
		PORTD &= ~(1 << TM1637_DIO);
		_delay_us(3);
		PORTD |= (1 << TM1637_CLK);
		_delay_us(3);
		data >>= 1;
		PORTD &= ~(1 << TM1637_CLK);
		_delay_us(3);
	}
	
	DDRD &= ~(1 << TM1637_DIO);
	PORTD |= (1 << TM1637_DIO); 
	PORTD |= (1 << TM1637_CLK);
	_delay_us(3);
	
	volatile uint8_t ack = !(PINB & (1 << TM1637_DIO));
	(void)ack;
	PORTD &= ~(1 << TM1637_CLK);
	DDRD |= (1 << TM1637_DIO);  
}

static inline void startCondition(void) {
	PORTD |= (1 << TM1637_DIO);
	PORTD |= (1 << TM1637_CLK);
	_delay_us(2);
	PORTD &= ~(1 << TM1637_DIO);
	_delay_us(2);
	PORTD &= ~(1 << TM1637_CLK);
}

static inline void stopCondition(void) {
	PORTD &= ~(1 << TM1637_CLK);
	_delay_us(2);
	PORTD &= ~(1 << TM1637_DIO);
	_delay_us(2);
	PORTD |= (1 << TM1637_CLK);
	_delay_us(2);
	PORTD |= (1 << TM1637_DIO);
}

void TM1637_displayNumber(uint16_t numero) { //MOSTRAR VALORES EN DISPLAYS
uint8_t display[4] = {0, 0, 0, 0}; 


display[0] = seg[(numero / 1000) % 10]; 
display[1] = seg[(numero / 100) % 10];  
display[2] = seg[(numero / 10) % 10];   
display[3] = seg[numero % 10];

startCondition();  
sendByte(0x40);
stopCondition();

startCondition();
sendByte(0xC0); 
for (uint8_t i = 0; i < 4; i++) {
	sendByte(display[i]); 
}
stopCondition();

startCondition();
sendByte(0x88 | brightness); 
stopCondition();
}


