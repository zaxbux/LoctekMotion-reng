/** Represents the 8th bit flag, indicating a decimal follows the digit. */
static const uint8_t HEIGHT_DECIMAL = 0x80; // (1 << 7)

const uint8_t display_byte[] = {
	// 0-9
	0x3f,
	0x06,
	0x5b,
	0x4f,
	0x66,
	0x6d,
	0x7d,
	0x07,
	0x7f,
	0x6f,

	// letters/symbols
	0x09,
	0x31,
	0x40,
	0x50,
	0x5C,
	0x54,
	0x71,
	0x77,
	0x78,
	0x79,
	0x80,
};

const char display_char[] = {
	// 0-9
	'0',
	'1',
	'2',
	'3',
	'4',
	'5',
	'6',
	'7',
	'8',
	'9',

	// letters/symbols
	'=',
	'R',
	'-',
	'r',
	'o',
	'n',
	'F',
	'A',
	't',
	'E',
	'.',
};

bool display_is_decimal(uint8_t b)
{
	return (b & HEIGHT_DECIMAL) == HEIGHT_DECIMAL;
}

/**
 * @brief
 *
 * @param s
 * @return int
 */
int display_to_int(uint8_t s)
{
	// Remove decimal bit
	uint8_t i = s & ~(HEIGHT_DECIMAL);

	// determine which 7-segment digit the byte represents
	for (int k = 0; k < 10; k++)
	{
		if (i == display_byte[k])
		{
			return k;
		}
	}

	// invalid value
	return -1;
}

float display_to_height(uint8_t h1, uint8_t h2, uint8_t h3)
{
	if (
		!display_is_decimal(h1) &&
		!display_is_decimal(h2) &&
		!display_is_decimal(h3))
	{
		// height must always contain a decimal, otherwise this is a non-height value
		return -1;
	}

	int height1 = display_to_int(h1);
	int height2 = display_to_int(h2);
	int height3 = display_to_int(h3);

	if (height1 == -1 || height2 == -1 || height3 == -1)
	{
		return -1;
	}

	float height = (height1 * 100) + (height2 * 10) + height3;

	return height / 10;
}

/**
 * @brief
 *
 * @param s
 * @return int
 */
char display_to_char(uint8_t b)
{
	// Remove decimal bit
	uint8_t i = b & ~(HEIGHT_DECIMAL);

	// determine which 7-segment digit the byte represents
	for (int k = 0; k < sizeof(display_byte); k++)
	{
		if (i == display_byte[k])
		{
			return display_char[k];
		}
	}

	// unknown char
	return '\0';
}