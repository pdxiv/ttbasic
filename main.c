/*
	TOYOSHIKI Tiny BASIC for Linux
	(C)2015 Tetsuya Suzuki
	Build: cc ttbasic.c basic.c -o ttbasic
*/

/*
TO DO:
The following variables use pointer arithmetic, which is probably a bad idea if
you want to convert the code to another language:
  current_icode
  current_line
  ip
  line_pointer
  keyword_pointer
  character_in_line_buffer_pointer
  top_of_command_line
*/

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

void basic(void); // prototype

int main()
{
	srand((unsigned int)time(0)); // for RND function
	basic();					  // call The BASIC
	return 0;
}

// Compiler requires description

// TOYOSHIKI TinyBASIC symbols
// TO-DO Rewrite defined values to fit your machine as needed
#define SIZE_LINE_COMMAND 78  // Command line buffer length + NULL
#define SIZE_IBUFFER 78		  // i-code conversion buffer size
#define SIZE_LIST_BUFFER 1024 // List buffer size
#define SIZE_ARRAY_AREA 64	// Array area size
#define SIZE_GOSUB_STACK 6	// GOSUB stack size(2/nest)
#define SIZE_LSTK 15		  // FOR stack size(5/nest)

// Depending on device functions
// TO-DO Rewrite these functions to fit your machine
#define STR_EDITION "LINUX"

// Terminal control
#define c_putch(c) putchar(c)

char c_getch()
{
	struct termios b;
	struct termios a;
	char c;

	tcgetattr(STDIN_FILENO, &b);
	a = b;
	a.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &a);
	c = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &b);

	return c;
}

char c_kbhit(void)
{
	char c;
	int f;

	f = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, f | O_NONBLOCK);

	c = c_getch();

	fcntl(STDIN_FILENO, F_SETFL, f);

	if (c != EOF)
	{
		ungetc(c, stdin);
		return 1;
	}

	return 0;
}

#define KEY_ENTER 10
void newline(void)
{
	c_putch(KEY_ENTER); // LF
}

// Return random number
short get_random_number(short value)
{
	return (rand() % value) + 1;
}

// Prototypes (necessity minimum)
short i_the_parser(void);

// Keyword table
const char *keyword_table[] = {
	"GOTO", "GOSUB", "RETURN",
	"FOR", "TO", "STEP", "NEXT",
	"IF", "REM", "STOP",
	"INPUT", "PRINT", "LET",
	",", ";",
	"-", "+", "*", "/", "(", ")",
	">=", "#", ">", "=", "<=", "<",
	"@", "RND", "ABS", "SIZE",
	"LIST", "RUN", "NEW", "SYSTEM"};

// i-code(Intermediate code) assignment
enum
{
	I_GOTO,   // 0 GOTO
	I_GOSUB,  // 1
	I_RETURN, // 2
	I_FOR,	// 3
	I_TO,	 // 4
	I_STEP,   // 5
	I_NEXT,   // 6
	I_IF,	 // 7
	I_REM,	// 8
	I_STOP,   // 9
	I_INPUT,  // 10
	I_PRINT,  // 11 PRINT
	I_LET,	// 12
	I_COMMA,  // 13
	I_SEMI,   // 14 Semicolon
	I_MINUS,  // 15
	I_PLUS,   // 16
	I_MUL,	// 17
	I_DIV,	// 18
	I_OPEN,   // 19
	I_CLOSE,  // 20
	I_GTE,	// 21
	I_SHARP,  // 22
	I_GT,	 // 23
	I_EQ,	 // 24
	I_LTE,	// 25
	I_LT,	 // 26
	I_ARRAY,  // 27
	I_RND,	// 28
	I_ABS,	// 29
	I_SIZE,   // 30
	I_LIST,   // 31
	I_RUN,	// 32
	I_NEW,	// 33
	I_SYSTEM, // 34
	I_NUM,	// 35
	I_VAR,	// 36 Variable
	I_STR,	// 37
	I_EOL	 // 38
};

// Keyword count
#define SIZE_KEYWORD_TABLE (sizeof(keyword_table) / sizeof(const char *))

// List formatting condition
// no space after
const unsigned char i_no_space_after[] = {
	I_RETURN, I_STOP, I_COMMA,
	I_MINUS, I_PLUS, I_MUL, I_DIV, I_OPEN, I_CLOSE,
	I_GTE, I_SHARP, I_GT, I_EQ, I_LTE, I_LT,
	I_ARRAY, I_RND, I_ABS, I_SIZE};

// no space before (after numeric or variable only)
const unsigned char i_no_space_before[] = {
	I_MINUS, I_PLUS, I_MUL, I_DIV, I_OPEN, I_CLOSE,
	I_GTE, I_SHARP, I_GT, I_EQ, I_LTE, I_LT,
	I_COMMA, I_SEMI, I_EOL};

// exception search function
char sstyle(unsigned char code, const unsigned char *table, unsigned char count)
{
	while (count--)
		if (code == table[count])
			return 1;
	return 0;
}

// exception search macro
#define nospacea(c) sstyle(c, i_no_space_after, sizeof(i_no_space_after))
#define nospaceb(c) sstyle(c, i_no_space_before, sizeof(i_no_space_before))

// Error messages
unsigned char err; // Error message index
const char *errmsg[] = {
	"OK",
	"Devision by zero",
	"Overflow",
	"Subscript out of range",
	"Icode buffer full",
	"List full",
	"GOSUB too many nested",
	"RETURN stack underflow",
	"FOR too many nested",
	"NEXT without FOR",
	"NEXT without counter",
	"NEXT mismatch FOR",
	"FOR without variable",
	"FOR without TO",
	"LET without variable",
	"IF without condition",
	"Undefined line number",
	"\'(\' or \')\' expected",
	"\'=\' expected",
	"Illegal command",
	"Syntax error",
	"Internal error",
	"Abort by [ESC]"};

// Error code assignment
enum
{
	ERR_OK,
	ERR_DIVBY0,
	ERR_VOF,
	ERR_SOR,
	ERR_IBUFOF,
	ERR_LBUFOF,
	ERR_GSTKOF,
	ERR_GSTKUF,
	ERR_LSTKOF,
	ERR_LSTKUF,
	ERR_NEXTWOV,
	ERR_NEXTUM,
	ERR_FORWOV,
	ERR_FORWOTO,
	ERR_LETWOV,
	ERR_IFWOC,
	ERR_ULN,
	ERR_PAREN,
	ERR_VWOEQ,
	ERR_COM,
	ERR_SYNTAX,
	ERR_SYS,
	ERR_ESC
};

// RAM mapping
char command_line_buffer[SIZE_LINE_COMMAND];		 // Command line buffer
unsigned char icode_conversion_buffer[SIZE_IBUFFER]; // i-code conversion buffer
short variable_area[26];							 // Variable area
short array_area[SIZE_ARRAY_AREA];					 // Array area
unsigned char list_area[SIZE_LIST_BUFFER];			 // List area
unsigned char *current_line;						 // Pointer current line
unsigned char *current_icode;						 // Pointer current Intermediate code
unsigned char *gosub_stack[SIZE_GOSUB_STACK];		 // GOSUB stack
unsigned char gosub_stack_index;					 // GOSUB stack index
unsigned char *for_stack[SIZE_LSTK];				 // FOR stack
unsigned char for_stack_index;						 // FOR stack index

// Standard C libraly (about) same functions
char c_toupper(char c) { return (c <= 'z' && c >= 'a' ? c - 32 : c); }
char c_isprint(char c) { return (c >= 32 && c <= 126); }
char c_isspace(char c) { return (c == ' ' || (c <= 13 && c >= 9)); }
char c_isdigit(char c) { return (c <= '9' && c >= '0'); }
char c_isalpha(char c) { return ((c <= 'z' && c >= 'a') || (c <= 'Z' && c >= 'A')); }
void c_puts(const char *character_in_line_buffer_pointer)
{
	while (*character_in_line_buffer_pointer)
		c_putch(*character_in_line_buffer_pointer++);
}
void c_gets()
{
	char c;
	unsigned char len;

	len = 0;
	while ((c = c_getch()) != KEY_ENTER)
	{
		if (c == 9)
			c = ' '; // TAB exchange Space
		if (((c == 8) || (c == 127)) && (len > 0))
		{ // Backspace manipulation
			len--;
			c_putch(8);
			c_putch(' ');
			c_putch(8);
		}
		else if (c_isprint(c) && (len < (SIZE_LINE_COMMAND - 1)))
		{
			command_line_buffer[len++] = c;
			c_putch(c);
		}
	}
	newline();
	command_line_buffer[len] = 0; // Put NULL

	if (len > 0)
	{
		while (c_isspace(command_line_buffer[--len]))
			;							// Skip space
		command_line_buffer[++len] = 0; // Put NULL
	}
}

// Print numeric specified columns
void print_numeric_specified_columns(short value, short d)
{
	unsigned char i;
	unsigned char sign;

	if (value < 0)
	{
		sign = 1;
		value = -value;
	}
	else
	{
		sign = 0;
	}

	command_line_buffer[6] = 0;
	i = 6;
	do
	{
		command_line_buffer[--i] = (value % 10) + '0';
		value /= 10;
	} while (value > 0);

	if (sign)
		command_line_buffer[--i] = '-';

	// String length = 6 - i
	while (6 - i < d)
	{				  // If short
		c_putch(' '); // Fill space
		d--;
	}
	c_puts(&command_line_buffer[i]);
}

// Input numeric and return value
// Called by only INPUT statement
short input_numeric_and_return_value()
{
	short value, tmp;
	char c;
	unsigned char len;
	unsigned char sign;

	len = 0;
	while ((c = c_getch()) != KEY_ENTER)
	{
		if (((c == 8) || (c == 127)) && (len > 0))
		{ // Backspace manipulation
			len--;
			c_putch(8);
			c_putch(' ');
			c_putch(8);
		}
		else if ((len == 0 && (c == '+' || c == '-')) ||
				 (len < 6 && c_isdigit(c)))
		{ // Numeric or sign only
			command_line_buffer[len++] = c;
			c_putch(c);
		}
	}
	newline();
	command_line_buffer[len] = 0;

	switch (command_line_buffer[0])
	{
	case '-':
		sign = 1;
		len = 1;
		break;
	case '+':
		sign = 0;
		len = 1;
		break;
	default:
		sign = 0;
		len = 0;
		break;
	}

	value = 0; // Initialize value
	tmp = 0;   // Temp value
	while (command_line_buffer[len])
	{
		tmp = 10 * value + command_line_buffer[len++] - '0';
		if (value > tmp)
		{ // It means overflow
			err = ERR_VOF;
		}
		value = tmp;
	}
	if (sign)
		return -value;
	return value;
}

// Convert token to i-code
// Return byte length or 0
unsigned char convert_token_to_icode()
{
	unsigned char i;											  // Loop counter(i-code sometime)
	unsigned char len = 0;										  // byte counter
	char *keyword_pointer = 0;									  // Temporary keyword pointer
	char *top_of_command_line;									  // Temporary token pointer
	char *character_in_line_buffer_pointer = command_line_buffer; // Pointer to character in line buffer
	char c;														  // Surround the string character, " or '
	short value;												  // numeric
	short tmp;													  // numeric for overflow check

	while (*character_in_line_buffer_pointer)
	{
		while (c_isspace(*character_in_line_buffer_pointer))
			character_in_line_buffer_pointer++; // Skip space

		// Try keyword conversion
		for (i = 0; i < SIZE_KEYWORD_TABLE; i++)
		{
			keyword_pointer = (char *)keyword_table[i];				// Point keyword
			top_of_command_line = character_in_line_buffer_pointer; // Point top of command line

			// Compare 1 keyword
			while ((*keyword_pointer != 0) && (*keyword_pointer == c_toupper(*top_of_command_line)))
			{
				keyword_pointer++;
				top_of_command_line++;
			}

			if (*keyword_pointer == 0)
			{ // Case success

				if (len >= SIZE_IBUFFER - 1)
				{ // List area full
					err = ERR_IBUFOF;
					return 0;
				}

				// i have i-code
				icode_conversion_buffer[len++] = i;
				character_in_line_buffer_pointer = top_of_command_line;
				break;
			}
		}

		// Case statement needs an argument except numeric, variable, or strings
		if (i == I_REM)
		{
			while (c_isspace(*character_in_line_buffer_pointer))
				character_in_line_buffer_pointer++; // Skip space
			top_of_command_line = character_in_line_buffer_pointer;
			for (i = 0; *top_of_command_line++; i++)
				; // Get length
			if (len >= SIZE_IBUFFER - 2 - i)
			{
				err = ERR_IBUFOF;
				return 0;
			}
			icode_conversion_buffer[len++] = i; // Put length
			while (i--)
			{ // Copy strings
				icode_conversion_buffer[len++] = *character_in_line_buffer_pointer++;
			}
			break;
		}

		if (*keyword_pointer == 0)
			continue;

		top_of_command_line = character_in_line_buffer_pointer; // Point top of command line

		// Try numeric conversion
		if (c_isdigit(*top_of_command_line))
		{
			value = 0;
			tmp = 0;
			do
			{
				tmp = 10 * value + *top_of_command_line++ - '0';
				if (value > tmp)
				{
					err = ERR_VOF;
					return 0;
				}
				value = tmp;
			} while (c_isdigit(*top_of_command_line));

			if (len >= SIZE_IBUFFER - 3)
			{
				err = ERR_IBUFOF;
				return 0;
			}
			icode_conversion_buffer[len++] = I_NUM;
			icode_conversion_buffer[len++] = value & 255;
			icode_conversion_buffer[len++] = value >> 8;
			character_in_line_buffer_pointer = top_of_command_line;
		}
		else if (*character_in_line_buffer_pointer == '\"' || *character_in_line_buffer_pointer == '\'') // Try string conversion
		{
			// If start of string
			c = *character_in_line_buffer_pointer++;
			top_of_command_line = character_in_line_buffer_pointer;
			for (i = 0; (*top_of_command_line != c) && c_isprint(*top_of_command_line); i++) // Get length
				top_of_command_line++;
			if (len >= SIZE_IBUFFER - 1 - i)
			{ // List area full
				err = ERR_IBUFOF;
				return 0;
			}
			icode_conversion_buffer[len++] = I_STR; // Put i-code
			icode_conversion_buffer[len++] = i;		// Put length
			while (i--)
			{ // Put string
				icode_conversion_buffer[len++] = *character_in_line_buffer_pointer++;
			}
			if (*character_in_line_buffer_pointer == c)
				character_in_line_buffer_pointer++; // Skip " or '
		}
		else if (c_isalpha(*top_of_command_line)) // Try conversion
		{
			if (len >= SIZE_IBUFFER - 2)
			{
				err = ERR_IBUFOF;
				return 0;
			}
			if (len >= 4 && icode_conversion_buffer[len - 2] == I_VAR && icode_conversion_buffer[len - 4] == I_VAR)
			{					  // Case series of variables
				err = ERR_SYNTAX; // Syntax error
				return 0;
			}
			icode_conversion_buffer[len++] = I_VAR;									// Put i-code
			icode_conversion_buffer[len++] = c_toupper(*top_of_command_line) - 'A'; // Put index of variable area
			character_in_line_buffer_pointer++;
		}
		else // Nothing much
		{
			err = ERR_SYNTAX;
			return 0;
		}
	}
	icode_conversion_buffer[len++] = I_EOL; // Put end of line
	return len;								// Return byte length
}

// Get line numbere by line pointer
short get_line_number_by_line_pointer(unsigned char *line_pointer)
{
	if (*line_pointer == 0) // end of list
		return 32767;		// max line bumber
	return *(line_pointer + 1) | *(line_pointer + 2) << 8;
}

// Search line by line number
unsigned char *search_line_by_line_number(short line_number)
{
	unsigned char *line_pointer;

	for (line_pointer = list_area; *line_pointer; line_pointer += *line_pointer)
		if (get_line_number_by_line_pointer(line_pointer) >= line_number)
			break;
	return line_pointer;
}

// Return free memory size
short return_free_memory_size()
{
	unsigned char *line_pointer;

	for (line_pointer = list_area; *line_pointer; line_pointer += *line_pointer)
		;
	return list_area + SIZE_LIST_BUFFER - line_pointer - 1;
}

// Insert i-code to the list
// Preconditions to do *icode_conversion_buffer = len
void insert_icode_to_the_list_preconditions()
{
	unsigned char *insp;
	unsigned char *p1, *p2;
	short len;

	if (return_free_memory_size() < *icode_conversion_buffer)
	{
		err = ERR_LBUFOF; // List buffer overflow
		return;
	}

	insp = search_line_by_line_number(get_line_number_by_line_pointer(icode_conversion_buffer));

	if (get_line_number_by_line_pointer(insp) == get_line_number_by_line_pointer(icode_conversion_buffer))
	{ // line number agree
		p1 = insp;
		p2 = p1 + *p1;
		while (len = *p2)
		{
			while (len--)
				*p1++ = *p2++;
		}
		*p1 = 0;
	}

	// Case line number only
	if (*icode_conversion_buffer == 4)
		return;

	// Make space
	for (p1 = insp; *p1; p1 += *p1)
		;
	len = p1 - insp + 1;
	p2 = p1 + *icode_conversion_buffer;
	while (len--)
		*p2-- = *p1--;

	// Insert
	len = *icode_conversion_buffer;
	p1 = insp;
	p2 = icode_conversion_buffer;
	while (len--)
		*p1++ = *p2++;
}

// Listing 1 line of i-code
void listing_1_line_of_icode(unsigned char *ip)
{
	unsigned char i;

	while (*ip != I_EOL)
	{
		// Case keyword
		if (*ip < SIZE_KEYWORD_TABLE)
		{
			c_puts(keyword_table[*ip]);
			if (!nospacea(*ip))
				c_putch(' ');
			if (*ip == I_REM)
			{
				ip++;
				i = *ip++;
				while (i--)
				{
					c_putch(*ip++);
				}
				return;
			}
			ip++;
		}
		else if (*ip == I_NUM) // Case numeric
		{
			ip++;
			print_numeric_specified_columns(*ip | *(ip + 1) << 8, 0);
			ip += 2;
			if (!nospaceb(*ip))
				c_putch(' ');
		}
		else if (*ip == I_VAR) // Case variable
		{
			ip++;
			c_putch(*ip++ + 'A');
			if (!nospaceb(*ip))
				c_putch(' ');
		}
		else if (*ip == I_STR) // Case string
		{
			char c;

			c = '\"';
			ip++;
			for (i = *ip; i; i--)
				if (*(ip + i) == '\"')
				{
					c = '\'';
					break;
				}

			c_putch(c);
			i = *ip++;
			while (i--)
			{
				c_putch(*ip++);
			}
			c_putch(c);
			if (*ip == I_VAR)
				c_putch(' ');
		}

		else // Nothing match, I think, such case is impossible
		{
			err = ERR_SYS;
			return;
		}
	}
}

// Get argument in parenthesis
short get_argument_in_parenthesis()
{
	short value;

	if (*current_icode != I_OPEN)
	{
		err = ERR_PAREN;
		return 0;
	}
	current_icode++;
	value = i_the_parser();
	if (err)
		return 0;

	if (*current_icode != I_CLOSE)
	{
		err = ERR_PAREN;
		return 0;
	}
	current_icode++;

	return value;
}

// Get value
short i_get_value()
{
	short value;

	switch (*current_icode)
	{
	case I_NUM:
		current_icode++;
		value = *current_icode | *(current_icode + 1) << 8;
		current_icode += 2;
		break;
	case I_PLUS:
		current_icode++;
		value = i_get_value();
		break;
	case I_MINUS:
		current_icode++;
		value = 0 - i_get_value();
		break;
	case I_VAR:
		current_icode++;
		value = variable_area[*current_icode++];
		break;
	case I_OPEN:
		value = get_argument_in_parenthesis();
		break;
	case I_ARRAY:
		current_icode++;
		value = get_argument_in_parenthesis();
		if (err)
			break;
		if (value >= SIZE_ARRAY_AREA)
		{
			err = ERR_SOR;
			break;
		}
		value = array_area[value];
		break;
	case I_RND:
		current_icode++;
		value = get_argument_in_parenthesis();
		if (err)
			break;
		value = get_random_number(value);
		break;
	case I_ABS:
		current_icode++;
		value = get_argument_in_parenthesis();
		if (err)
			break;
		if (value < 0)
			value *= -1;
		break;
	case I_SIZE:
		current_icode++;
		if ((*current_icode != I_OPEN) || (*(current_icode + 1) != I_CLOSE))
		{
			err = ERR_PAREN;
			break;
		}
		current_icode += 2;
		value = return_free_memory_size();
		break;

	default:
		err = ERR_SYNTAX;
		break;
	}
	return value;
}

// multiply or divide calculation
short i_multiply_or_divide_calculation()
{
	short value, tmp;

	value = i_get_value();
	if (err)
		return -1;

	while (1)
		switch (*current_icode)
		{
		case I_MUL:
			current_icode++;
			tmp = i_get_value();
			value *= tmp;
			break;
		case I_DIV:
			current_icode++;
			tmp = i_get_value();
			if (tmp == 0)
			{
				err = ERR_DIVBY0;
				return -1;
			}
			value /= tmp;
			break;
		default:
			return value;
		}
}

// add or subtract calculation
short i_add_or_subtract_calculation()
{
	short value, tmp;

	value = i_multiply_or_divide_calculation();
	if (err)
		return -1;

	while (1)
		switch (*current_icode)
		{
		case I_PLUS:
			current_icode++;
			tmp = i_multiply_or_divide_calculation();
			value += tmp;
			break;
		case I_MINUS:
			current_icode++;
			tmp = i_multiply_or_divide_calculation();
			value -= tmp;
			break;
		default:
			return value;
		}
}

// The parser
short i_the_parser()
{
	short value, tmp;

	value = i_add_or_subtract_calculation();
	if (err)
		return -1;

	// conditional expression
	while (1)
		switch (*current_icode)
		{
		case I_EQ:
			current_icode++;
			tmp = i_add_or_subtract_calculation();
			value = (value == tmp);
			break;
		case I_SHARP:
			current_icode++;
			tmp = i_add_or_subtract_calculation();
			value = (value != tmp);
			break;
		case I_LT:
			current_icode++;
			tmp = i_add_or_subtract_calculation();
			value = (value < tmp);
			break;
		case I_LTE:
			current_icode++;
			tmp = i_add_or_subtract_calculation();
			value = (value <= tmp);
			break;
		case I_GT:
			current_icode++;
			tmp = i_add_or_subtract_calculation();
			value = (value > tmp);
			break;
		case I_GTE:
			current_icode++;
			tmp = i_add_or_subtract_calculation();
			value = (value >= tmp);
			break;
		default:
			return value;
		}
}

// PRINT handler
void i_print_handler()
{
	short value;
	short len;
	unsigned char i;

	len = 0;
	while (*current_icode != I_SEMI && *current_icode != I_EOL)
	{
		switch (*current_icode)
		{
		case I_STR:
			current_icode++;
			i = *current_icode++;
			while (i--)
				c_putch(*current_icode++);
			break;
		case I_SHARP:
			current_icode++;
			len = i_the_parser();
			if (err)
				return;
			break;
		default:
			value = i_the_parser();
			if (err)
				return;
			print_numeric_specified_columns(value, len);
			break;
		}

		if (*current_icode == I_COMMA)
		{
			current_icode++;
			if (*current_icode == I_SEMI || *current_icode == I_EOL)
				return;
		}
		else
		{
			if (*current_icode != I_SEMI && *current_icode != I_EOL)
			{
				err = ERR_SYNTAX;
				return;
			}
		}
	}
	newline();
}

// INPUT handler
void i_input_handler()
{
	short value;
	short index;
	unsigned char i;
	unsigned char prompt;

	while (1)
	{
		prompt = 1;

		if (*current_icode == I_STR)
		{
			current_icode++;
			i = *current_icode++;
			while (i--)
				c_putch(*current_icode++);
			prompt = 0;
		}

		switch (*current_icode)
		{
		case I_VAR:
			current_icode++;
			if (prompt)
			{
				c_putch(*current_icode + 'A');
				c_putch(':');
			}
			value = input_numeric_and_return_value();
			if (err)
				return;
			variable_area[*current_icode++] = value;
			break;
		case I_ARRAY:
			current_icode++;
			index = get_argument_in_parenthesis();
			if (err)
				return;
			if (index >= SIZE_ARRAY_AREA)
			{
				err = ERR_SOR;
				return;
			}
			if (prompt)
			{
				c_puts("@(");
				print_numeric_specified_columns(index, 0);
				c_puts("):");
			}
			value = input_numeric_and_return_value();
			if (err)
				return;
			array_area[index] = value;
			break;
		default:
			err = ERR_SYNTAX;
			return;
		}

		switch (*current_icode)
		{
		case I_COMMA:
			current_icode++;
			break;
		case I_SEMI:
		case I_EOL:
			return;
		default:
			err = ERR_SYNTAX;
			return;
		}
	}
}

// Variable assignment handler
void i_variable_assignment_handler()
{
	short value;
	short index;

	index = *current_icode++;
	if (*current_icode != I_EQ)
	{
		err = ERR_VWOEQ;
		return;
	}
	current_icode++;

	value = i_the_parser();
	if (err)
		return;

	variable_area[index] = value;
}

// Array assignment handler
void i_array_assignment_handler()
{
	short value;
	short index;

	index = get_argument_in_parenthesis();
	if (err)
		return;

	if (index >= SIZE_ARRAY_AREA)
	{
		err = ERR_SOR;
		return;
	}

	if (*current_icode != I_EQ)
	{
		err = ERR_VWOEQ;
		return;
	}
	current_icode++;

	value = i_the_parser();
	if (err)
		return;

	array_area[index] = value;
}

// LET handler
void i_let_handler()
{
	switch (*current_icode)
	{
	case I_VAR:
		current_icode++;
		i_variable_assignment_handler(); // Variable assignment
		break;
	case I_ARRAY:
		current_icode++;
		i_array_assignment_handler(); // Array assignment
		break;
	default:
		err = ERR_LETWOV;
		break;
	}
}

// Execute a series of i-code
unsigned char *i_execute_a_series_of_icode()
{
	short line_number;			 // line number
	unsigned char *line_pointer; // temporary line pointer
	short index, vto, vstep;	 // FOR-NEXT items
	short condition;			 // IF condition

	while (*current_icode != I_EOL)
	{

		if (c_kbhit()) // check keyin
			if (c_getch() == 27)
			{ // ESC ?
				err = ERR_ESC;
				return NULL;
			}

		switch (*current_icode)
		{

		case I_GOTO:
			current_icode++;
			line_number = i_the_parser(); // get line number
			if (err)
				break;
			line_pointer = search_line_by_line_number(line_number); // search line
			if (line_number != get_line_number_by_line_pointer(line_pointer))
			{ // if not found
				err = ERR_ULN;
				break;
			}

			current_line = line_pointer;	  // update line pointer
			current_icode = current_line + 3; // update i-code pointer
			break;

		case I_GOSUB:
			current_icode++;
			line_number = i_the_parser(); // get line number
			if (err)
				break;
			line_pointer = search_line_by_line_number(line_number); // search line
			if (line_number != get_line_number_by_line_pointer(line_pointer))
			{ // if not found
				err = ERR_ULN;
				break;
			}

			// push pointers
			if (gosub_stack_index >= SIZE_GOSUB_STACK - 2)
			{ // stack overflow ?
				err = ERR_GSTKOF;
				break;
			}
			gosub_stack[gosub_stack_index++] = current_line;  // push line pointer
			gosub_stack[gosub_stack_index++] = current_icode; // push i-code pointer

			current_line = line_pointer;	  // update line pointer
			current_icode = current_line + 3; // update i-code pointer
			break;

		case I_RETURN:
			if (gosub_stack_index < 2)
			{ // stack empty ?
				err = ERR_GSTKUF;
				break;
			}
			current_icode = gosub_stack[--gosub_stack_index]; // pop line pointer
			current_line = gosub_stack[--gosub_stack_index];  // pop i-code pointer
			break;

		case I_FOR:
			current_icode++;

			if (*current_icode++ != I_VAR)
			{ // no variable
				err = ERR_FORWOV;
				break;
			}

			index = *current_icode;			 // get variable index
			i_variable_assignment_handler(); // variable_area = value
			if (err)
				break;

			if (*current_icode == I_TO)
			{
				current_icode++;
				vto = i_the_parser(); // get TO value
			}
			else
			{
				err = ERR_FORWOTO;
				break;
			}

			if (*current_icode == I_STEP)
			{
				current_icode++;
				vstep = i_the_parser(); // get STEP value
			}
			else
				vstep = 1; // default STEP value

			// overflow check
			if (((vstep < 0) && (-32767 - vstep > vto)) ||
				((vstep > 0) && (32767 - vstep < vto)))
			{
				err = ERR_VOF;
				break;
			}

			// push pointers
			if (for_stack_index >= SIZE_LSTK - 5)
			{ // stack overflow ?
				err = ERR_LSTKOF;
				break;
			}
			for_stack[for_stack_index++] = current_line;					  // push line pointer
			for_stack[for_stack_index++] = current_icode;					  // push i-code pointer
																			  // Special thanks hardyboy
			for_stack[for_stack_index++] = (unsigned char *)(uintptr_t)vto;   // push TO value
			for_stack[for_stack_index++] = (unsigned char *)(uintptr_t)vstep; // push STEP value
			for_stack[for_stack_index++] = (unsigned char *)(uintptr_t)index; // push variable index
			break;

		case I_NEXT:
			current_icode++;

			if (for_stack_index < 5)
			{ // stack empty ?
				err = ERR_LSTKUF;
				break;
			}

			index = (short)(uintptr_t)for_stack[for_stack_index - 1]; // read variable index
			if (*current_icode++ != I_VAR)
			{ // no variable
				err = ERR_NEXTWOV;
				break;
			}
			if (*current_icode++ != index)
			{ // not equal index
				err = ERR_NEXTUM;
				break;
			}

			vstep = (short)(uintptr_t)for_stack[for_stack_index - 2]; // read STEP value
			variable_area[index] += vstep;							  // update loop counter
			vto = (short)(uintptr_t)for_stack[for_stack_index - 3];   // read TO value

			// loop end
			if (((vstep < 0) && (variable_area[index] < vto)) ||
				((vstep > 0) && (variable_area[index] > vto)))
			{
				for_stack_index -= 5; // resume stack
				break;
			}

			// loop continue
			current_icode = for_stack[for_stack_index - 4]; // read line pointer
			current_line = for_stack[for_stack_index - 5];  // read i-code pointer
			break;

		case I_IF:
			current_icode++;
			condition = i_the_parser(); // get condition
			if (err)
			{
				err = ERR_IFWOC;
				break;
			}
			if (condition) // if true continue
				break;
			// If false, same as REM

		case I_REM:
			// Seek pointer to I_EOL
			// No problem even if it points not realy end of line
			while (*current_icode != I_EOL)
				current_icode++; // seek end of line
			break;

		case I_STOP:
			while (*current_line)
				current_line += *current_line; // seek end
			return current_line;

		case I_VAR:
			current_icode++;
			i_variable_assignment_handler();
			break;
		case I_ARRAY:
			current_icode++;
			i_array_assignment_handler();
			break;
		case I_LET:
			current_icode++;
			i_let_handler();
			break;
		case I_PRINT:
			current_icode++;
			i_print_handler();
			break;
		case I_INPUT:
			current_icode++;
			i_input_handler();
			break;

		case I_SEMI:
			current_icode++;
			break;

		case I_LIST:
		case I_NEW:
		case I_RUN:
			err = ERR_COM;
			break;

		default:
			err = ERR_SYNTAX;
			break;
		}

		if (err)
			return NULL;
	}
	return current_line + *current_line;
}

// RUN command handler
void i_run_command_handler()
{
	unsigned char *line_pointer;

	gosub_stack_index = 0;
	for_stack_index = 0;
	current_line = list_area;

	while (*current_line)
	{
		current_icode = current_line + 3;
		line_pointer = i_execute_a_series_of_icode();
		if (err)
			return;
		current_line = line_pointer;
	}
}

// LIST command handler
void i_list_handler()
{
	short line_number;

	line_number = (*current_icode == I_NUM) ? get_line_number_by_line_pointer(current_icode) : 0;

	for (current_line = list_area;
		 *current_line && (get_line_number_by_line_pointer(current_line) < line_number);
		 current_line += *current_line)
		;

	while (*current_line)
	{
		print_numeric_specified_columns(get_line_number_by_line_pointer(current_line), 0);
		c_putch(' ');
		listing_1_line_of_icode(current_line + 3);
		if (err)
			break;
		newline();
		current_line += *current_line;
	}
}

// NEW command handler
void i_new_command_handler(void)
{
	unsigned char i;

	for (i = 0; i < 26; i++)
		variable_area[i] = 0;
	for (i = 0; i < SIZE_ARRAY_AREA; i++)
		array_area[i] = 0;
	gosub_stack_index = 0;
	for_stack_index = 0;
	*list_area = 0;
	current_line = list_area;
}

// Command processor
void i_command_processor()
{
	current_icode = icode_conversion_buffer;
	switch (*current_icode)
	{
	case I_NEW:
		current_icode++;
		if (*current_icode == I_EOL)
			i_new_command_handler();
		else
			err = ERR_SYNTAX;
		break;
	case I_LIST:
		current_icode++;
		if (*current_icode == I_EOL || *(current_icode + 3) == I_EOL)
			i_list_handler();
		else
			err = ERR_SYNTAX;
		break;
	case I_RUN:
		current_icode++;
		i_run_command_handler();
		break;
	default:
		i_execute_a_series_of_icode();
		break;
	}
}

// Print OK or error message
void error()
{
	if (err)
	{
		if (current_icode >= list_area && current_icode < list_area + SIZE_LIST_BUFFER && *current_line)
		{
			newline();
			c_puts("LINE:");
			print_numeric_specified_columns(get_line_number_by_line_pointer(current_line), 0);
			c_putch(' ');
			listing_1_line_of_icode(current_line + 3);
		}
		else
		{
			newline();
			c_puts("YOU TYPE: ");
			c_puts(command_line_buffer);
		}
	}

	newline();
	c_puts(errmsg[err]);
	newline();
	err = 0;
}

/*
TOYOSHIKI Tiny BASIC
The BASIC entry point
*/
void basic()
{
	unsigned char len;

	i_new_command_handler();
	c_puts("TOYOSHIKI TINY BASIC");
	newline();
	c_puts(STR_EDITION);
	c_puts(" EDITION");
	newline();
	error(); // Print OK, and Clear error flag

	// Input 1 line and execute
	while (1)
	{
		c_putch('>');					// Prompt
		c_gets();						// Input 1 line
		len = convert_token_to_icode(); // Convert token to i-code
		if (err)
		{ // Error
			error();
			continue; // Do nothing
		}

		// Quit if i-code is "SYSTEM"
		if (*icode_conversion_buffer == I_SYSTEM)
		{
			return;
		}

		// If the line starts with a number, store i-code in list
		if (*icode_conversion_buffer == I_NUM)
		{											  // Case the top includes line number
			*icode_conversion_buffer = len;			  // Change I_NUM to byte length
			insert_icode_to_the_list_preconditions(); // Insert list
			if (err)								  // Error
				error();							  // Print error message
			continue;
		}

		// Simply execude the code in the the entered statement
		i_command_processor(); // Execute direct
		error();			   // Print OK, and Clear error flag
	}
}
