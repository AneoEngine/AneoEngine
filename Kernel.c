#include <stdint.h>

volatile uint16_t* VGA = (uint16_t*)0xB8000;
volatile uint32_t timer_ticks = 0;

int cursor_x = 0;
int cursor_y = 5;
int shift = 0;

uint8_t TCOLOR = 0x0F;
uint8_t BCOLOR = 0x1F;
uint8_t BLACK = 0x0F;
uint8_t ICOLOR = 0x1A;
uint8_t RED = 0x1C;
uint8_t BRED = 0x0C;

#define MAX_FILES 16
#define MAX_NAME 32
#define MAX_DATA 512

typedef struct
{
	char name[MAX_NAME];
	char data[MAX_DATA];
	int size;
	int used;
	int is_dir;
	int parent;
} File;

File fs[MAX_FILES];

int cwd = 0;

char *line1 = "=====AneoEngine V0.2 x86 Operating System========================Shell();=======";
char *empt = "                                                                                ";
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
	if(cursor_y < 49)
		return;

	for(int y = 1; y < 49; y++)
	{
		for(int x = 0; x < 80; x++)
		{
			VGA[(y-1)*80 + x] = VGA[y*80 + x];
		}
	}

	for(int x = 0; x < 80; x++)
	{
		VGA[(48)*80 + x] = (TCOLOR << 8) | ' ';
	}

	cursor_y = 48;
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

	VGA[cursor_y * 80 + cursor_x] = (TCOLOR << 8) | c;

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


void PrintHex(uint32_t n)
{
        char hex[] = "0123456789ABCDEF";

        Print("0x");

        for(int i = 28; i >= 0; i -= 4)
        {
                uint8_t digit = (n >> i) & 0xF;
                PutChar(hex[digit]);
        }
}


void init_timer()
{
        Print("0x00000036 -> 0x00000043\n");
        Print("0x000000A9 -> 0x00000040\n");
        Print("0x00000004 -> 0x00000040\n");

        uint32_t freq = 1000;
        uint32_t divisor = 1193180 / freq;

        outb(0x43,0x36);
        outb(0x40,divisor & 0xFF);
        outb(0x40,(divisor >> 8) & 0xFF);
	Print("Timer loaded   OK  IOP  0x00000040,0x00000043\n");
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

uint8_t parse_hex(char* str)
{
        uint8_t val = 0;
        for (int i = 0; i < 2; i++) {
                char c = str[i];
                uint8_t nibble = 0;

                if (c >= '0' && c <= '9') nibble = c - '0';
                else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
                else break;

                val = (val << 4) | nibble;
        }
        return val;
}

void fs_init()
{
	fs[0].used = 1;
	fs[0].is_dir = 1;
	fs[0].parent = 0;
	fs[0].name[0] = '/';
	fs[0].name[1] = 0;
}

int fs_find(char *name)
{
	for(int i=0;i<MAX_FILES;i++)
	{
		if(fs[i].used && fs[i].parent == cwd && strcmp(fs[i].name,name))
			return i;
	}
	return -1;
}

void fs_touch(char *name)
{
	for(int i=0;i<MAX_FILES;i++)
	{
		if(!fs[i].used)
		{
			fs[i].used = 1;
			fs[i].is_dir = 0;
			fs[i].parent = cwd;

			int j=0;
			while(name[j])
			{
				fs[i].name[j] = name[j];
				j++;
			}
			fs[i].name[j] = 0;

			fs[i].size = 0;
			return;
		}
	}

	Print("FS full\n");
}

void fs_mkdir(char *name)
{
	for(int i=0;i<MAX_FILES;i++)
	{
		if(!fs[i].used)
		{
			fs[i].used = 1;
			fs[i].is_dir = 1;
			fs[i].parent = cwd;

			int j=0;
			while(name[j])
			{
				fs[i].name[j] = name[j];
				j++;
			}

			fs[i].name[j]=0;
			return;
		}
	}
}

void fs_ls()
{
	for(int i=0;i<MAX_FILES;i++)
	{
		if(fs[i].used && fs[i].parent == cwd && i != cwd)
		{
			if(fs[i].is_dir)
				Print("[DIR] ");
			else
				Print("[FILE] ");

			Print(fs[i].name);
			Print("\n");
		}
	}
}

void fs_cat(char *name)
{
	int id = fs_find(name);

	if(id < 0)
	{
		Print("File not found\n");
		return;
	}

	Print(fs[id].data);
	Print("\n");
}

void fs_echo(char *text, char *file)
{
	int id = fs_find(file);

	if(id < 0)
	{
		Print("File not found\n");
		return;
	}

	int i=0;

	while(text[i] && i < MAX_DATA-1)
	{
		fs[id].data[i] = text[i];
		i++;
	}

	fs[id].data[i]=0;
	fs[id].size = i;
}

void fs_cd(char *name)
{
	int id = fs_find(name);

	if(id < 0 || !fs[id].is_dir)
	{
		Print("Directory not found\n");
		return;
	}

	cwd = id;
}

void fs_rm(char *name)
{
	int id = fs_find(name);

	if(id < 0)
	{
		Print("File not found\n");
		return;
	}

	fs[id].used = 0;
}

void help()
{
	Print("AneoEngine commands:\n");
	Print("help........display this message\n");
	Print("info........display AneoEngine info\n");
	Print("cls.........clears the screen\n");
	Print("entropy.....print a random number\n");
	Print("calc........simple calculator: (<X><operator><Y>)\n");
	Print("vmoff.......shut down your virtual machine\n");
	Print("poweroff....shut down your machine\n");
	Print("addresses...display a list of used memory addresses\n");
	Print("banner......print an ascii banner of AneoEngine\n");
	Print("panic.......crash the system\n");
}

void info()
{
	Print("AneoEngine V0.2 x86 Operating System\n");
	Print("Creator/Owner: Rocco Himel\n");
}

void cls()
{
	Print("0x00000F20 -> 0x000B8000\n");
	Sleep(10);

	uint8_t OCOLOR = TCOLOR;
	TCOLOR = RED;
	Print("Failed to write value 0x00000F20 to 0x000B800");

	TCOLOR = OCOLOR;

	volatile unsigned short *vga_mem = (volatile unsigned short *)0xB8000;
	unsigned short blank = (TCOLOR << 8) | ' ';

	for(int i = 0; i < 80 * 50; i++)
	{
		vga_mem[i] = blank;
	}

	cursor_x = 0;
	cursor_y = 0;

	UpdateCursor();
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

void addresses()
{
        Print(" =====ADDRESS======USAGE=======================SIZE====\n");
        Print(" | 0x00000000      Interupt Vector Table       1024   |\n");
        Print(" | 0x00000400      BIOS Data Arena             30720  |\n");
        Print(" | 0x00007C00      Bootloader start            512    |\n");
        Print(" | 0x00007DFF      Bootloader end              512    |\n");
        Print(" | 0x00001000      Kernel start                10240  |\n");
        Print(" | 0x00001000      Kernel entry start          10240  |\n");
        Print(" | 0x000037FF      End of kernel               10240  |\n");
        Print(" | 0x00090000      Stack; protected mode       716800 |\n");
        Print(" | 0x000B8000      VGA text buffer             4000   |\n");
        Print(" | 0x00001919      AneoEngine shell            7910   |\n");
        Print(" | 0x00100000      Unused memory               N/A    |\n");
        Print(" ======================================================\n");
}


void panic(const char* msg)
{
        Sleep(100);

	TCOLOR = BLACK;

	cursor_x = 50;

	while(cursor_x != 0)
	{
		Print(empt);
		cursor_x--;
		Sleep(1);
	}

	__asm__ volatile("cli");

	cls();

	info();

	addresses();

	Print("0x000000F4 -> 0x00000000\n");
        Print("========================\n");
        Print("System halted due to \n");
        Print("REASON: ");
        Print(msg);

        while(1)
        {
                __asm__ volatile("hlt");
        }
}


uint32_t rand()
{
	Print("0x00000012A4F3918C -> 0xA4F3918C\n");
	Print("0x00000012A4F3918C -> 0x00000012\n");
	Print("0xA4F3918C -> 0xA4F3918C\n");
	Print("0x00000012 -> 0x00000012\n");
	Print("0xA4F3918C ^ 0x00000012 -> 0xA4F3919E\n");
	uint32_t a,d;

	__asm__ volatile("rdtsc":"=a"(a),"=d"(d));

	return a ^ d;
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
	TCOLOR = ICOLOR;
	PrintNumber(r);
	TCOLOR = BCOLOR;
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

	Print("0x00002000 -> 0x00000604\n");
	Sleep(10);
	outw(0x604,0x2000);
	TCOLOR = RED;
	Print("Failed to write value 0x00002000 to 0x00000604\n");
	TCOLOR = BCOLOR;

	Print("0x00002000 -> 0x0000B004\n");
	Sleep(10);
	outw(0xB004,0x2000);
	TCOLOR = RED;
	Print("Failed to write value 0x00002000 to 0x0000B004\n");
	TCOLOR = BCOLOR;

	Print("0x00002000 -> 0x00004004\n");
	Sleep(10);
	outw(0x4004,0x2000);
	TCOLOR = RED;
	Print("Failed to write value 0x00002000 to 0x00004004\n");
	Print("ERROR: unable to shutdown virtual machine\n");
	TCOLOR = BCOLOR;
	Sleep(100);
	panic("Unable to shutdown virtual machine\n");
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

		TCOLOR = RED;
		Print("Failed to execute 0x00000015 BIOS interrupt");
		TCOLOR = BCOLOR;
}


void banner()
{
	Print("     ___                     ______            _            \n");
	Print("    /   |  ____  ___  ____  / ____/___  ____ _(_)___  ___   \n");
	Print("   / /| | / __ \\/ _ \\/ __ \\/ __/ / __ \\/ __ `/ / __ \\/ _ \\  \n");
	Print("  / ___ |/ / / /  __/ /_/ / /___/ / / / /_/ / / / / /  __/  \n");
	Print(" /_/  |_/_/ /_/\\___/\\____/_____/_/ /_/\\__, /_/_/ /_/\\___/   \n");
	Print("                                     /____/                 \n");
}

void startup()
{
	help();
	Print("\n");
}
void Shell()
{
	TCOLOR = BCOLOR;
	cls();
	cursor_y = 0;
	Print(line1);
	Print("Shell loaded   OK       0x00001919\n");
	Print("\n");
	startup();
	char buffer[64];
	int pos;

	while(1)
	{
		uint8_t oldcursor_y = cursor_y;
		cursor_y = 0;
		Print(line1);
		cursor_y = oldcursor_y;

		Print("> ");

		TCOLOR = ICOLOR;

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

					VGA[cursor_y * 80 + cursor_x] = (TCOLOR << 8) | ' ';
					UpdateCursor();
				}

				continue;
			}

		buffer[pos++] = c;
		PutChar(c);

		}

		buffer[pos] = 0;

		Print("\n");

		TCOLOR = BCOLOR;
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
		else if(strcmp(buffer,"panic"))
			panic("User input");
		else if(strcmp(buffer,"ls"))
			fs_ls();

		else if(buffer[0]=='t' && buffer[1]=='o' && buffer[2]=='u' && buffer[3]=='c' && buffer[4]=='h' && buffer[5]==' ')
			fs_touch(buffer+6);

		else if(buffer[0]=='m' && buffer[1]=='k' && buffer[2]=='d' && buffer[3]=='i' && buffer[4]=='r' && buffer[5]==' ')
			fs_mkdir(buffer+6);

		else if(buffer[0]=='c' && buffer[1]=='a' && buffer[2]=='t' && buffer[3]==' ')
			fs_cat(buffer+4);

		else if(buffer[0]=='c' && buffer[1]=='d' && buffer[2]==' ')
			fs_cd(buffer+3);

		else if(buffer[0]=='r' && buffer[1]=='m' && buffer[2]==' ')
			fs_rm(buffer+3);
		else
			TCOLOR = RED;
			Print("ERROR: Unknown command\n");
			TCOLOR = BCOLOR;
	}
}

void kmain()
{
	Print("Kernel loaded  OK  OCU  0x00001000-0x000037FF\n");
	init_timer();
	fs_init();
	Print("\n Loading shell...");
	Sleep(150);

	Shell();

	TCOLOR = BRED;
	Print("\nFailed to execute the shell\n");
	Print("ERROR: unable to execute the shell\n");
	panic("Unable to execute the shell");
}
