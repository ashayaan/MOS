#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

unsigned int mask = '*';

static void putstr(char *s)      { while (*s != '\0') putchar(*s++); }

static int getch(){
	struct termios old, new;
    int ch;
    tcgetattr(STDIN_FILENO, &old);
    new = old;
    new.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    return ch;
}

int main()
{
	char buf[1024], c;
	int i;

	putstr("Namaste!\n");
	putstr("login : ");
	for (i = 0; i < 1024; i++) {
		buf[i] = getchar();
		if (buf[i] == '\n') break;
	}
	buf[i] = '\0';
	putstr("Secret : ");

	while ((c = getch()) != '\r' && c != '\n')
		putchar(mask);

	putstr("\nhello ");
	putstr(buf);
	putstr(", your secret is safe with me!\n\nPress Enter key to exit\n");
	getchar();
}
