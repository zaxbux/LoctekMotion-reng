#include "keypad.h"
#include "message.h"
#include "crc16_modbus.h"

#define ARG_MAX 32
#define ARG_TIMEOUT 1500

#define CB_ACTIVE_TIMEOUT 5000

#define displayPin20 4

HardwareSerial *s[3] = {&Serial, &Serial1, &Serial2};

bool debugMode = false;

uint8_t hs_rx_buf[UINT8_MAX];
uint8_t hs_rx_buf_len = 0;
uint8_t hs_rx_buf_pos = 0;

bool hs_connected = false;
bool cb_connected = false;

long cb_on_millis = 0;
float cb_last_height = 0.0;

uint8_t cb_rx_buf[UINT8_MAX];
uint8_t cb_rx_buf_len = 0;
uint8_t cb_rx_buf_pos = 0;

/**
 * Pull PIN20 HIGH/LOW before sending, or after sending to control box
 */
void cb_activate(bool on = true)
{
	if (on)
	{
		if (cb_on_millis == 0)
		{
			digitalWrite(displayPin20, HIGH);
			s[0]->print("\n[CB]  [ACTIVE] 1");

			delay(250);
		}

		cb_on_millis = millis();
	}
	else
	{
		if (cb_on_millis != 0)
		{
			digitalWrite(displayPin20, LOW);
			s[0]->print("\n[CB]  [ACTIVE] 0");
		}

		cb_on_millis = 0;
	}
}

void hs_tx(const uint8_t *buffer, size_t size)
{
	// sending too fast causes nothing to happen
	delay(250);

	s[1]->flush();
	s[1]->write(buffer, size);
}

void cb_tx(const uint8_t *buffer, size_t size)
{
	// make sure control box is ready
	cb_activate(true);

	// sending too fast causes nothing to happen
	// delay(250);

	s[2]->flush();
	s[2]->write(buffer, size);
}

/**
 * Activates the screen.
 *
 * Send this in order to receive height from control box.
 *
 */
void cb_tx_btn()
{
	cb_tx(command_none, sizeof(command_none));
	s[0]->print("\n[CB] <[x02] *");
}

void hs_rx_print()
{

	if (hs_rx_buf[2] == MessageType::HSx02)
	{
		uint16_t state = ((uint16_t)hs_rx_buf[3] << 8) | hs_rx_buf[4];

		if (state == ButtonState::None)
		{
			if (!hs_connected)
			{
				hs_connected = true;
				s[0]->print("\n[HS]  FOUND");
			}
			return;
		}

		s[0]->print("\n[HS]> [x02] ");

		switch (state)
		{
		case ButtonState::None:
			s[0]->print("*");
			return;
		case ButtonState::Raise:
			s[0]->print("UP");
			return;
		case ButtonState::Lower:
			s[0]->print("DOWN");
			return;
		case ButtonState::Preset1:
			s[0]->print("M1");
			return;
		case ButtonState::Preset2:
			s[0]->print("M2");
			return;
		case ButtonState::Preset3:
			s[0]->print("M3");
			return;
		case ButtonState::Save:
			s[0]->print("M");
			return;
		case ButtonState::Remind:
			s[0]->print("A");
			return;
		}

		// An unknown button
		for (int i = 0; i < hs_rx_buf_len; i++)
		{
			s[0]->print("0x");
			s[0]->print(hs_rx_buf[i + 2], HEX);
			s[0]->print(" ");
		}
	}
}

void cb_rx_display_char(byte b)
{
	char c = display_to_char(b);

	if (c != '\0')
	{
		// known char
		s[0]->print(c);
	}
	else if (b == 0x00)
	{
		// blank
		s[0]->print("\u25a1");
	}
	else
	{
		// unknown char
		s[0]->print("0x");
		s[0]->print(b < 16 ? "0" : "");
		s[0]->print(b & ~(HEIGHT_DECIMAL), HEX);
	}

	s[0]->print(" ");

	if ((b & HEIGHT_DECIMAL) != 0)
	{
		s[0]->print(". ");
	}
}

void cb_rx_print()
{
	if (cb_rx_buf[2] == MessageType::CBx11)
	{
		if (!cb_connected)
		{
			cb_connected = true;
			s[0]->print("\n[CB]  FOUND");
			cb_tx_btn();
		}

		// s[0]->print("\n[CB]> [x11]");
	}
	else if (cb_rx_buf[2] == MessageType::CBx12)
	{
		float height = display_to_height(cb_rx_buf[3], cb_rx_buf[4], cb_rx_buf[5]);

		if (height == -1)
		{
			return;
		}

		if (cb_last_height != height)
		{
			cb_last_height = height;

			s[0]->print("\n[CB]> [x12] ");
			s[0]->print(height);

			if (height > 0)
			{
				// update timestamp. if PIN20 pulled LOW while desk is moving, this causes "rst" error
				cb_on_millis = millis();
				// cb_activate(true);
			}
		}
	}
	// else if (cb_rx_buf[2] == MessageType::CBx14)
	// {
	// 	s[0]->print("\n[CB]> [x14]");
	// }
	// else if (cb_rx_buf[2] == MessageType::CBx15)
	// {
	// 	s[0]->print("\n[CB]> [x15]");
	// }
}

void hs_rx(uint8_t in)
{
	// s[2]->write(in);

	if (in == MESSAGE_BEGIN)
	{
		hs_rx_buf_len = 0;
		hs_rx_buf_pos = 0;
	}

	hs_rx_buf[hs_rx_buf_pos] = in;
	hs_rx_buf_pos++;

	if (hs_rx_buf_len == 0 && hs_rx_buf[0] == MESSAGE_BEGIN)
	{
		// Start of packet
		if (hs_rx_buf_pos == 1)
		{
			return;
		}

		// Second byte defines the length
		if (hs_rx_buf_pos == 2)
		{
			hs_rx_buf_len = in + 2;
			return;
		}
	}

	if (hs_rx_buf_pos == hs_rx_buf_len)
	{
		if (in == MESSAGE_FINISH && hs_rx_buf_len >= 6)
		{
			s[2]->flush();
			s[2]->write(hs_rx_buf, hs_rx_buf_len);
			// cb_last_tx = millis();

			if (debugMode)
			{
				s[0]->print("\n[HS]> ");
				for (int i = 0; i < hs_rx_buf_len; i++)
				{
					s[0]->print(hs_rx_buf[i] < 16 ? "0" : "");
					s[0]->print(hs_rx_buf[i], HEX);
					s[0]->print(" ");
				}

				s[0]->print("\n[CB]<");
				s[0]->print(hs_rx_buf_len);
			}

			hs_rx_print();

			// reset
			hs_rx_buf_len = 0;
			hs_rx_buf_pos = 0;
		}
	}
}

void cb_rx(uint8_t in)
{
	// s[1]->write(in);

	if (in == MESSAGE_BEGIN)
	{
		cb_rx_buf_len = 0;
		cb_rx_buf_pos = 0;
	}

	cb_rx_buf[cb_rx_buf_pos] = in;
	cb_rx_buf_pos++;

	if (cb_rx_buf_len == 0 && cb_rx_buf[0] == MESSAGE_BEGIN)
	{
		// Start of packet
		if (cb_rx_buf_pos == 1)
		{
			return;
		}

		// Second byte defines the length
		if (cb_rx_buf_pos == 2)
		{
			cb_rx_buf_len = in + 2;
			return;
		}
	}

	if (cb_rx_buf_pos == cb_rx_buf_len)
	{
		if (in == MESSAGE_FINISH && cb_rx_buf_len >= 6)
		{
			s[1]->flush();
			s[1]->write(cb_rx_buf, cb_rx_buf_len);
			// hs_last_tx = millis();

			if (debugMode)
			{
				s[0]->print("\n[CB]> ");
				for (int i = 0; i < cb_rx_buf_len; i++)
				{
					s[0]->print(cb_rx_buf[i] < 16 ? "0" : "");
					s[0]->print(cb_rx_buf[i], HEX);
					s[0]->print(" ");
				}

				s[0]->print("\n[HS]<");
				s[0]->print(cb_rx_buf_len);
			}

			cb_rx_print();

			// reset
			cb_rx_buf_len = 0;
			cb_rx_buf_pos = 0;
		}
	}
}

void rx_command(const char arg[])
{
	if (strstr(arg, "*") == &arg[0])
	{
		cb_tx_btn();
		cb_tx(command_none, sizeof(command_none));
		s[0]->print("\n[CB] <[x02] *");
	}
	else if (strstr(arg, "UP") == &arg[0])
	{
		cb_tx_btn();
		for (int i = 0; i < 4; i++)
		{
			cb_tx(command_raise, sizeof(command_raise));
			s[0]->print("\n[CB] <[x02] UP");
			delay(40);
		}
	}
	else if (strstr(arg, "DN") == &arg[0])
	{
		cb_tx_btn();
		for (int i = 0; i < 4; i++)
		{
			cb_tx(command_lower, sizeof(command_lower));
			s[0]->print("\n[CB] <[x02] DN");
			delay(40);
		}
	}
	else if (strstr(arg, "M1") == &arg[0])
	{
		cb_tx_btn();
		delay(40);
		cb_tx(command_preset1, sizeof(command_preset1));
		s[0]->print("\n[CB] <[x02] M1");
	}
	else if (strstr(arg, "M2") == &arg[0])
	{
		cb_tx_btn();
		delay(40);
		cb_tx(command_preset2, sizeof(command_preset2));
		s[0]->print("\n[CB] <[x02] M2");
	}
	else if (strstr(arg, "M3") == &arg[0])
	{
		cb_tx_btn();
		delay(40);
		cb_tx(command_preset3, sizeof(command_preset3));
		s[0]->print("\n[CB] <[x02] M3");
	}
	else if (strstr(arg, "M4") == &arg[0])
	{
		cb_tx_btn();
		delay(40);
		cb_tx(command_preset4, sizeof(command_preset4));
		s[0]->print("\n[CB] <[x02] M4");
	}
	else if (strstr(arg, "M") == &arg[0])
	{
		cb_tx_btn();
		delay(40);
		cb_tx(command_save, sizeof(command_save));
		s[0]->print("\n[CB] <[x02] M");
	}
	else if (strstr(arg, "R0") == &arg[0])
	{
		hs_tx(command_alarm_off, sizeof(command_alarm_off));
		s[0]->print("\n[HS] <[x15]");
	}
	else if (strstr(arg, "R1") == &arg[0])
	{
		hs_tx(command_alarm_on, sizeof(command_alarm_on));
		s[0]->print("\n[HS] <[x14]");
	}
	else if (strstr(arg, "hs:0") == &arg[0])
	{
		s[1]->flush();
		s[1]->end();

		hs_connected = false;

		s[0]->print("\n[HS] OFF");
	}
	else if (strstr(arg, "hs:1") == &arg[0])
	{
		s[1]->begin(9600);

		s[0]->print("\n[HS] ON");
	}
	else if (strstr(arg, "cb:0") == &arg[0])
	{
		s[2]->flush();
		s[2]->end();

		cb_activate(false);
		cb_connected = false;

		s[0]->print("\n[CB] OFF");
	}
	else if (strstr(arg, "cb:1") == &arg[0])
	{
		s[2]->begin(9600);
		cb_tx_btn();

		s[0]->print("\n[CB] ON");
	}
	else if (strstr(arg, "d:0") == &arg[0])
	{
		debugMode = false;
	}
	else if (strstr(arg, "d:1") == &arg[0])
	{
		debugMode = true;
	}
}

void rx_read()
{
	int i = 0;
	long time = 0;
	char arg[ARG_MAX] = {'\0'};
	char currentByte = '\0';
	bool overflow = false;

	while (true)
	{
		if (s[0]->available() > 0)
		{
			//
			// break before array overflow.
			//
			if (i == ARG_MAX)
			{
				overflow = true;
				break;
			}

			time = millis();
			currentByte = (char)(s[0]->read());
			arg[i] = currentByte;

			//
			// we expect only one newline per command
			//
			if (currentByte == '\n')
			{
				break;
			}
			++i;
		}
		else if (time > 0 && millis() - time > ARG_TIMEOUT)
		{
			// console input timeout
			break;
		}
	}
	//
	// when there's a buffer overflow, the input buffer might be empty although
	// there's still more data on the way
	//
	if (overflow)
	{
		delay(200);
	}

	if (s[0]->available())
	{
		//
		// we get here when there's still data in the pipeline but we already
		// exited the read loop. this happens when there's a newline character
		// in the command string before EOL or when we receive more than ARG_MAX
		// bytes. We flush the pipeline and flag the command to prevent execution.
		// in case of a buffer overflow, the command is already flagged.
		//
		while (s[0]->available() > 0)
		{
			s[0]->read();
		}

		return;
	}

	rx_command(arg);
}

void hs_rx_read()
{
	for (int i = 1; i <= s[1]->available(); ++i)
	{
		hs_rx((uint8_t)s[1]->read());
	}
}

void cb_rx_read()
{
	for (int i = 1; i <= s[2]->available(); ++i)
	{
		cb_rx((uint8_t)s[2]->read());
	}
}

void setup()
{
	s[0]->begin(115200);
	//
	// we start the arduino in inject mode
	//
	// pinMode(2, OUTPUT);
	// digitalWrite(2, LOW);

	pinMode(displayPin20, OUTPUT);
	digitalWrite(displayPin20, LOW);

	// hs
	s[1]->begin(9600);

	// cb
	s[2]->begin(9600);
	// cb_tx_btn();
	//  delay(1000);
}

/**
 * Forward data
 * - to console
 * - from hand set to control box
 * - from control box to hand set
 */
void loop()
{
	if (!cb_connected)
	{
		cb_activate(true);
		delay(100);
	}

	if (s[0]->available() > 0)
		rx_read();
	if (s[1]->available() > 0)
		hs_rx_read();
	if (s[2]->available() > 0)
		cb_rx_read();

	if (cb_on_millis > 0 && millis() - cb_on_millis > CB_ACTIVE_TIMEOUT)
	{
		s[0]->print("\n[CB]  TIMEOUT");
		cb_activate(false);
	}
}