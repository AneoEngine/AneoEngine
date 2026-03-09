#include <stdint.h>

volatile uint16_t* VGA = (uint16_t*)0xB8000;
volatile uint32_t timer_ticks = 0;

int cursor_x = 0;
int cursor_y = 3;
int shift = 0;
uint8_t tcolor = 0x0F;

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

static inline void outw(uint16_t port, uint16_t val)
{
	__asm__ volatile("outw %0,%1"::"a"(val),"Nd"(port));
}

void timer_handler()
{
	timer_ticks++;
}

void init_timer()
{
	uint32_t freq = 1000;
	uint32_t divisor = 1193180 / freq;

	outb(0x43,0x36);
	outb(0x40,divisor & 0xFF);
	outb(0x40,(divisor >> 8) & 0xFF);
}

void Sleep(uint32_t t)
{
	for(uint32_t i=0;i<t*1000000;i++)
	{
		__asm__ volatile("nop");
	}
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

	VGA[cursor_y * 80 + cursor_x] = (tcolor << 8) | c;

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

char shift_keymap[128] =
{
0,0,'!','@','#','$','%','^','&','*','(',')','_','+',8,9,
'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S',
'D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V',
'B','N','M','<','>','?',0,'*',0,' ',0
};

char GetChar()
{
	while(1)
	{
		if(!(inb(0x64) & 1))
			continue;

		uint8_t sc = inb(0x60);

		if(sc == 42 || sc == 54)
		{
			shift = 1;
			continue;
		}

		if(sc == 170 || sc == 182)
		{
			shift = 0;
			continue;
		}

		if(sc & 0x80)
			continue;

		char c;

		if(shift)
			c = shift_keymap[sc];
		else
			c = keymap[sc];

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
	Print("calc: simple calculator: (<X><operator><Y>)\n");
	Print("vmoff: shut down your virtual machine\n");
	Print("poweroff: shut down your machine\n");
	Print("addresses: display a list of used memory addresses\n");
	Print("banner: print an ascii banner of AneoEngine\n");
}

void info()
{
	Print("AneoEngine V0.2\n");
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

int atoi(const char* s)
{
	int n = 0;
	int i = 0;

	while(s[i])
	{
		n = n * 10 + (s[i] - '0');
		i++;
	}

	return n;
}

void entropy()
{
	uint32_t r = rand() % 90000 + 10000;

	Print("New entropy value: ");
	tcolor = 0x02;
	PrintNumber(r);
	tcolor = 0x0F;
	Print("\n");
}

void calc(const char* expr)
{
	int i = 0;

	int a = 0;
	int b = 0;
	char op = 0;

	while(expr[i])
	{
		if(expr[i] == '+' || expr[i] == '-' || expr[i] == '*' || expr[i] == '/')
		{
			op = expr[i];
			i++;
			break;
		}

		a = a * 10 + (expr[i] - '0');
		i++;
	}

	while(expr[i])
	{
		b = b * 10 + (expr[i] - '0');
		i++;
	}

	uint32_t result = 0;

	if(op == '+') result = a + b;
	if(op == '-') result = a - b;
	if(op == '*') result = a * b;
	if(op == '/' && b != 0) result = a / b;

	PrintNumber(result);
	Print("\n");
}

void calcmsg()
{
	Print("Usage: calc <X><operator><Y>\n");
}

void vmoff()
{
	Print("Attempting to poweroff...\n");
	Sleep(50);
	outw(0x604,0x2000);
	Print("Failed to write value 0x00002000 to 0x00000604\n");
	outw(0xB004,0x2000);
	Print("Failed to write value 0x00002000 to 0x0000B004\n");
	outw(0x4004,0x2000);
	Print("Failed to write value 0x00002000 to 0x00004004\n");
	Print("ERROR: unable to shutdown virtual machine\n");

	while(1);
}

void poweroff()
{
	Print("Attempting to poweroff...\n");
	Sleep(50);

	__asm__ volatile(
		"mov $0x5301, %%ax\n"
		"xor %%bx, %%bx\n"
		"int $0x15\n"

		"mov $0x530e, %%ax\n"
		"mov $0x0001, %%bx\n"
		"mov $0x0001, %%cx\n"
		"int $0x15\n"

		"mov $0x5307, %%ax\n"
		"mov $0x0001, %%bx\n"
		"mov $0x0003, %%cx\n"
		"int $0x15\n"
		:
		:
		:"ax","bx","cx"
	);

		Print("Failed to execute 0x00000015 BIOS interrupt");
}

void addresses()
{
	Print("0x00000000   Interupt Vector Table\n");
	Print("0x00000400   BIOS Data Arena\n");
	Print("0x00007C00   Bootloader start\n");
	Print("0x00007DFF   Bootloader end\n");
	Print("0x00001000   Kernel start\n");
	Print("0x00001000   Kernel entry start lable\n");
	Print("0x000037FF   End of kernel\n");
	Print("0x00090000   Stack; protected mode\n");
	Print("0x000B8000   VGA text buffer\n");
	Print("0x00001919   AneoEngine shell\n");
	Print("0x00100000+  Unused memory\n");
}

void banner()
{
	tcolor = 0x03;
	Print("     ___                     ______            _            \n");
	Print("    /   |  ____  ___  ____  / ____/___  ____ _(_)___  ___   \n");
	Print("   / /| | / __ \\/ _ \\/ __ \\/ __/ / __ \\/ __ `/ / __ \\/ _ \\  \n");
	Print("  / ___ |/ / / /  __/ /_/ / /___/ / / / /_/ / / / / /  __/  \n");
	Print(" /_/  |_/_/ /_/\\___/\\____/_____/_/ /_/\\__, /_/_/ /_/\\___/   \n");
	Print("                                     /____/                 \n");
	tcolor = 0x0F;
}

void Shell()
{
	Print("Shell loaded   OK       0x00001919\n");
        Print("\nAneoEngine V0.2 x86\n");

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
		else if(buffer[0]=='c' && buffer[1]=='a' && buffer[2]=='l' && buffer[3]=='c' && buffer[4]==' ')
			calc(buffer+5);
		else if(strcmp(buffer,"calc"))
			calcmsg();
		else if(strcmp(buffer,"vmoff"))
			vmoff();
		else if(strcmp(buffer,"poweroff"))
			poweroff();
		else if(strcmp(buffer,"addresses"))
			addresses();
		else if(strcmp(buffer,"banner"))
			banner();
		else
			Print("Unknown command\n");
	}
}

void kmain()
{
	Print("Kernel loaded  OK  OCU  0x00001000-0x000037FF\n");
	init_timer();
	Print("Timer loaded   OK  IOP  0x00000040,0x00000043\n");
	Shell();
}
