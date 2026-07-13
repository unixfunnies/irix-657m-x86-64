/*
 * irix/kern/ml/X86_64/serial.c
 *
 * Early 16550 UART console (COM1) for the IRIX x86-64 port.
 * This is the first-light output path — the moral equivalent of the
 * IP-board duart early console the MIPS kernels used.
 */

#include "x86_64.h"

#define COM1		0x3f8

#define UART_DATA	(COM1 + 0)	/* data register (DLAB=0)	*/
#define UART_IER	(COM1 + 1)	/* interrupt enable (DLAB=0)	*/
#define UART_DLL	(COM1 + 0)	/* divisor latch low (DLAB=1)	*/
#define UART_DLH	(COM1 + 1)	/* divisor latch high (DLAB=1)	*/
#define UART_FCR	(COM1 + 2)	/* FIFO control			*/
#define UART_LCR	(COM1 + 3)	/* line control			*/
#define UART_MCR	(COM1 + 4)	/* modem control		*/
#define UART_LSR	(COM1 + 5)	/* line status			*/

#define LSR_THRE	0x20		/* transmit holding reg empty	*/

void
serial_early_init(void)
{
	outb(UART_IER, 0x00);		/* no interrupts, we poll	*/
	outb(UART_LCR, 0x80);		/* DLAB on			*/
	outb(UART_DLL, 0x01);		/* 115200 baud			*/
	outb(UART_DLH, 0x00);
	outb(UART_LCR, 0x03);		/* 8n1, DLAB off		*/
	outb(UART_FCR, 0xc7);		/* FIFO on, clear, 14-byte	*/
	outb(UART_MCR, 0x0b);		/* DTR|RTS|OUT2			*/
}

void
serial_putc(char c)
{
	if (c == '\n')
		serial_putc('\r');
	while ((inb(UART_LSR) & LSR_THRE) == 0)
		;
	outb(UART_DATA, (__u8)c);
}

void
serial_puts(const char *s)
{
	while (*s)
		serial_putc(*s++);
}
