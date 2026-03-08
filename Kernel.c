#include <stdint.h>

volatile uint16_t* VGA = (uint16_t*)0xB8000;

int cursor_x = 0;
int cursor_y = 3;

static inline void outb(uint16_t port, uint8_t val)
{
	__asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void UpdateCursor()
{
	uint16_t pos = cursor_y * 80 + cursor_x;

	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t)(pos & 0xFF));

	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void PutChar(char c)
{
	if(c == '\n')
	{
		cursor_x = 0;
		cursor_y++;
		UpdateCursor();
		return;
	}

	VGA[cursor_y * 80 + cursor_x] = (0x0F << 8) | c;

	cursor_x++;

	if(cursor_x >= 80)
	{
		cursor_x = 0;
		cursor_y++;
	}

	UpdateCursor();
}

void Print(const char* str)
{
	for(int i = 0; str[i]; i++)
		PutChar(str[i]);
}

void kmain()
{
	Print("Kernel loaded OK\n");
	Print("AneoEngine V0.1\n");
	Print("\nWelcome to AneoEngine!");

	for(;;);
}
