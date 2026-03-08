#include <stdint.h>

volatile uint16_t* VGA = (uint16_t*)0xB8000;

int cursor_x = 0;
int cursor_y = 3;

static inline void outb(uint16_t port, uint8_t val)
{
	__asm__ volatile("outb %0,%1"::"a"(val),"Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
	uint8_t val;
	__asm__ volatile("inb %1,%0":"=a"(val):"Nd"(port));
	return val;
}

void UpdateCursor()
{
	uint16_t pos = cursor_y * 80 + cursor_x;

	outb(0x3D4,0x0F);
	outb(0x3D5,(uint8_t)(pos & 0xFF));

	outb(0x3D4,0x0E);
	outb(0x3D5,(uint8_t)((pos >> 8) & 0xFF));
}

void Scroll()
{
	if(cursor_y < 25)
		return;

	for(int y = 1; y < 25; y++)
	{
		for(int x = 0; x < 80; x++)
		{
			VGA[(y-1)*80 + x] = VGA[y*80 + x];
		}
	}

	for(int x = 0; x < 80; x++)
	{
		VGA[(24)*80 + x] = (0x0F << 8) | ' ';
	}

	cursor_y = 24;
}

void PutChar(char c)
{
	if(c == '\n')
	{
		cursor_x = 0;
		cursor_y++;
		Scroll();
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

	Scroll();

	UpdateCursor();
}

void Print(const char* str)
{
	for(int i = 0; str[i]; i++)
		PutChar(str[i]);
}

int strcmp(const char* a,const char* b)
{
	int i = 0;

	while(a[i] && b[i])
	{
		if(a[i] != b[i])
			return 0;
		i++;
	}

	return a[i] == b[i];
}

char keymap[128] =
{
0,0,'1','2','3','4','5','6','7','8','9','0','-','=',8,9,
'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s',
'd','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v',
'b','n','m',',','.','/',0,'*',0,' ',0
};

char GetChar()
{
	while(1)
	{
		if(!(inb(0x64) & 1))
			continue;

		uint8_t sc = inb(0x60);

		if(sc & 0x80)
			continue;

		char c = keymap[sc];

		if(c)
			return c;
	}
}
void help()
{
	Print("AneoEngine commands:\n");
	Print("help: display this message\n");
	Print("info: display AneoEngine info\n");
	Print("cls: clears the screen\n");
	Print("entropy: print a random number\n");
}

void info()
{
	Print("AneoEngine V0.1\n");
	Print("x86 Operating System\n");
	Print("Creator/Owner: Rocco Himel\n");
}

void cls()
{
	for(int i = 0; i < 80*25; i++)
		VGA[i] = (0x0F << 8) | ' ';

	cursor_x = 0;
	cursor_y = 0;

	UpdateCursor();
}

uint32_t rand()
{
	uint32_t a,d;

	__asm__ volatile("rdtsc":"=a"(a),"=d"(d));

	return a ^ d;
}

void PrintNumber(uint32_t n)
{
	char buf[16];
	int i = 0;

	if(n == 0)
	{
		PutChar('0');
		return;
	}

	while(n > 0)
	{
		buf[i++] = '0' + (n % 10);
		n /= 10;
	}

	for(int j = i-1; j >= 0; j--)
		PutChar(buf[j]);
}

void entropy()
{
	uint32_t r = rand() % 90000 + 10000;

	Print("New entropy value: ");
	PrintNumber(r);
	Print("\n");
}

void Shell()
{
	char buffer[64];
	int pos;

	while(1)
	{
		Print("> ");

		pos = 0;

		while(1)
		{
			char c = GetChar();

			if(c == '\n')
				break;

			if(c == 8)
			{
				if(pos > 0)
				{
					pos--;

					if(cursor_x > 0)
						cursor_x--;
					else
					{
						cursor_x = 79;
						cursor_y--;
					}

					VGA[cursor_y * 80 + cursor_x] = (0x0F << 8) | ' ';
					UpdateCursor();
				}

				continue;
			}

		buffer[pos++] = c;
		PutChar(c);

		}

		buffer[pos] = 0;

		Print("\n");

		if(pos == 0)
			continue;

		if(strcmp(buffer,"help"))
			help();
		else if(strcmp(buffer,"info"))
			info();
		else if(strcmp(buffer,"cls"))
			cls();
		else if(strcmp(buffer,"entropy"))
			entropy();
		else
			Print("Unknown command\n");
	}
}

void kmain()
{
	Print("Kernel loaded  OK\n");
	Print("\nWelcome to AneoEngine!\n");

	Shell();
}
